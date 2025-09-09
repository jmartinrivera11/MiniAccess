#include "MainWindow.h"
#include <QTabWidget>
#include <QLabel>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QDir>
#include <QIcon>
#include "ObjectsDock.h"
#include "DatasheetPage.h"
#include "DesignPage.h"
#include "QueryBuilderPage.h"
#include "RelationDesignerPage.h"
#include <QFont>
#include <QApplication>
#include <QFileDialog>
#include <QInputDialog>
#include <QTableView>
#include <QHeaderView>
#include <QToolButton>
#include <QMessageBox>
#include <QDateTime>
#include <QMetaObject>
#include <QScreen>
#include <QShowEvent>
#include <algorithm>
#include <QFileInfo>
#include <QFile>
#include <QCoreApplication>
#include <QThread>

using namespace ma;

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi() {
    setWindowTitle("MiniAccess");
    resize(1200, 800);

    // --- Centro: tabs ---
    tabs_ = new QTabWidget(this);
    tabs_->setTabsClosable(true);
    tabs_->setDocumentMode(true);
    setCentralWidget(tabs_);

    connect(tabs_, &QTabWidget::tabCloseRequested, this, [this](int idx){
        QWidget* w = tabs_->widget(idx);

        auto eraseByWidget = [&](QHash<QString, QPointer<QWidget>>& map){
            QString keyToRemove;
            for (auto it = map.begin(); it != map.end(); ++it) {
                if (it.value() == w) { keyToRemove = it.key(); break; }
            }
            if (!keyToRemove.isEmpty()) map.remove(keyToRemove);
        };
        eraseByWidget(openDatasheets_);
        eraseByWidget(openDesigns_);
        if (queryBuilderTab_ == w) queryBuilderTab_.clear();
        if (relationDesignerTab_ == w) relationDesignerTab_.clear();

        tabs_->removeTab(idx);
        if (w) w->deleteLater();
    });

    // --- Dock izquierdo ---
    dock_ = new ObjectsDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, dock_);
    connect(dock_, &ObjectsDock::openDatasheet, this, &MainWindow::openFromDockDatasheet);
    connect(dock_, &ObjectsDock::openDesign,    this, &MainWindow::openFromDockDesign);

    // ===== MENÚ SUPERIOR =====
    // File
    QMenu* fileMenu = menuBar()->addMenu("&File");
    QAction* actNewProject   = fileMenu->addAction(QIcon(":/icons/icons/new.svg"),  "New Project...");
    QAction* actOpenProject  = fileMenu->addAction(QIcon(":/icons/icons/open.svg"), "Open Project...");
    QAction* actCloseProject = fileMenu->addAction(QIcon(":/icons/icons/close.svg"), "Close Current Project");
    QAction* actDeleteProj   = fileMenu->addAction(QIcon(":/icons/icons/delete.svg"), "Delete Project...");
    fileMenu->addSeparator();
    QAction* actQuit         = fileMenu->addAction("Quit");

    actNewProject->setShortcut(QKeySequence::New);    // Ctrl+N
    actOpenProject->setShortcut(QKeySequence::Open);  // Ctrl+O
    actCloseProject->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W)); // Ctrl+W
    actQuit->setShortcut(QKeySequence::Quit);         // Ctrl+Q

    // View
    QMenu* viewMenu = menuBar()->addMenu("&View");
    QAction* actZoomIn  = viewMenu->addAction(QIcon(":/icons/icons/zoom-in.svg"),  "Zoom In");
    QAction* actZoomOut = viewMenu->addAction(QIcon(":/icons/icons/zoom-out.svg"), "Zoom Out");
    actZoomIn->setShortcut(QKeySequence::ZoomIn);     // Ctrl + '+'
    actZoomOut->setShortcut(QKeySequence::ZoomOut);   // Ctrl + '-'

    // ===== RIBBON =====
    QToolBar* ribbon = addToolBar("Ribbon");
    ribbon->setObjectName("MainRibbon");
    ribbon->setIconSize(QSize(28,28));
    ribbon->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    ribbon->setMovable(false);

    auto makeGroup = [&](const QString& text){
        QWidget* sep = new QWidget(this); sep->setFixedWidth(1); ribbon->addWidget(sep);
        auto* lab = new QLabel(text, this); lab->setObjectName("RibbonGroupLabel"); ribbon->addWidget(lab);
    };

    makeGroup("Records");
    QAction* actInsert  = new QAction(QIcon(":/icons/icons/insert.svg"),  "Insert",  this);
    QAction* actDelete  = new QAction(QIcon(":/icons/icons/delete.svg"),  "Delete",  this);
    QAction* actRefresh = new QAction(QIcon(":/icons/icons/refresh.svg"), "Refresh", this);
    ribbon->addAction(actInsert);
    ribbon->addAction(actDelete);
    ribbon->addAction(actRefresh);

    makeGroup("Tools");
    QAction* actQuery = new QAction(QIcon(":/icons/icons/query.svg"), "Query Builder", this);
    ribbon->addAction(actQuery);
    QAction* actRelations = new QAction(QIcon(":/icons/icons/relation.svg"), "Relation Designer", this);
    ribbon->addAction(actRelations);

    makeGroup("Tables");
    QAction* actDeleteTable = new QAction(QIcon(":/icons/icons/delete.svg"), "Delete Table...", this);
    ribbon->addAction(actDeleteTable);

    for (auto* btn : ribbon->findChildren<QToolButton*>()) {
        btn->setAutoRaise(false);
    }

    // ===== Status bar =====
    statusLabel_ = new QLabel("Ready");
    statusBar()->addPermanentWidget(statusLabel_);

    // ===== Conexiones =====
    connect(actNewProject,   &QAction::triggered, this, &MainWindow::newProject);
    connect(actOpenProject,  &QAction::triggered, this, &MainWindow::openProject);
    connect(actCloseProject, &QAction::triggered, this, &MainWindow::closeCurrentProject);
    connect(actDeleteProj,   &QAction::triggered, this, &MainWindow::deleteCurrentProject);
    connect(actQuit,         &QAction::triggered, this, &QWidget::close);

    connect(dock_, &ObjectsDock::deleteTableRequested, this, &MainWindow::deleteTableByBase);

    connect(actZoomIn,       &QAction::triggered, this, &MainWindow::zoomIn);
    connect(actZoomOut,      &QAction::triggered, this, &MainWindow::zoomOut);

    connect(actDeleteTable, &QAction::triggered, this, &MainWindow::deleteSelectedTable);

    connect(actQuery,        &QAction::triggered, this, &MainWindow::openQueryBuilder);
    connect(actRelations, &QAction::triggered, this, &MainWindow::openRelationDesigner);
    connect(actInsert,       &QAction::triggered, this, &MainWindow::insertRecord);
    connect(actDelete,       &QAction::triggered, this, &MainWindow::deleteRecord);
    connect(actRefresh,      &QAction::triggered, this, &MainWindow::refreshView);

    // ===== Proyecto inicial =====
    setProjectPathAndReload(QString());
}

int MainWindow::ensureDatasheetTab(const QString& basePath) {
    if (openDatasheets_.contains(basePath)) {
        QWidget* w = openDatasheets_.value(basePath);
        if (w) {
            int i = tabs_->indexOf(w);
            if (i >= 0) { tabs_->setCurrentIndex(i); return i; }
        }
        openDatasheets_.remove(basePath);
    }
    auto* page = new DatasheetPage(basePath, this);
    const QString title = QFileInfo(basePath).fileName() + " [Data]";
    int idx = tabs_->addTab(page, QIcon(":/icons/icons/insert.svg"), title);
    tabs_->setCurrentIndex(idx);
    openDatasheets_.insert(basePath, page);
    return idx;
}

int MainWindow::ensureDesignTab(const QString& basePath) {
    if (openDesigns_.contains(basePath)) {
        QWidget* w = openDesigns_.value(basePath);
        if (w) {
            int i = tabs_->indexOf(w);
            if (i >= 0) { tabs_->setCurrentIndex(i); return i; }
        }
        openDesigns_.remove(basePath);
    }
    auto* page = new DesignPage(basePath, this);
    const QString title = QFileInfo(basePath).fileName() + " [Design]";
    int idx = tabs_->addTab(page, QIcon(":/icons/icons/index-str.svg"), title);
    tabs_->setCurrentIndex(idx);
    openDesigns_.insert(basePath, page);
    return idx;
}

void MainWindow::openFromDockDatasheet(const QString& basePath) {
    if (openDatasheets_.contains(basePath)) {
        QWidget* w = openDatasheets_.value(basePath);
        int i = tabs_->indexOf(w);
        if (i >= 0) {
            tabs_->setCurrentIndex(i);
            return;
        }
        openDatasheets_.remove(basePath);
    }

    auto* page = new DatasheetPage(basePath, this);
    const QString title = QFileInfo(basePath).completeBaseName();
    int i = tabs_->addTab(page, title);
    tabs_->setCurrentIndex(i);

    openDatasheets_.insert(basePath, page);
}

void MainWindow::openQueryBuilder() {
    if (queryBuilderTab_) {
        int i = tabs_->indexOf(queryBuilderTab_);
        if (i >= 0) { tabs_->setCurrentIndex(i); return; }
        queryBuilderTab_.clear();
    }

    QString projDir = currentProjectPath_.isEmpty() ? QDir::currentPath() : currentProjectPath_;
    auto* page = new QueryBuilderPage(projDir, this);
    int idx = tabs_->addTab(page, QIcon(":/icons/icons/query.svg"), "Query Builder");
    tabs_->setCurrentIndex(idx);
    queryBuilderTab_ = page;
}

void MainWindow::openRelationDesigner() {
    if (currentProjectPath_.isEmpty()) {
        QMessageBox::information(this, "Relations", "Open or create a Project first.");
        return;
    }
    if (relationDesignerTab_) {
        int i = tabs_->indexOf(relationDesignerTab_);
        if (i >= 0) { tabs_->setCurrentIndex(i); return; }
        relationDesignerTab_.clear();
    }
    auto* page = new RelationDesignerPage(currentProjectPath_, this);
    int idx = tabs_->addTab(page, QIcon(":/icons/icons/relation.svg"), "Relations");
    tabs_->setCurrentIndex(idx);
    relationDesignerTab_ = page;
}

void MainWindow::openFromDockDesign(const QString& basePath) {
    if (openDesigns_.contains(basePath)) {
        QWidget* w = openDesigns_.value(basePath);
        int i = tabs_->indexOf(w);
        if (i >= 0) {
            tabs_->setCurrentIndex(i);
            return;
        }
        openDesigns_.remove(basePath);
    }

    auto* page = new DesignPage(basePath, this);
    const QString title = QFileInfo(basePath).completeBaseName() + " (Design)";
    int i = tabs_->addTab(page, title);
    tabs_->setCurrentIndex(i);

    openDesigns_.insert(basePath, page);
}

void MainWindow::applyGlobalZoomRefresh() {
    const int rowh = QFontMetrics(QApplication::font()).height() + 6;
    for (auto* v : this->findChildren<QTableView*>()) {
        v->verticalHeader()->setDefaultSectionSize(rowh);
        v->viewport()->update();
    }
    for (auto* tlw : QApplication::topLevelWidgets()) {
        tlw->setUpdatesEnabled(false);
        tlw->setUpdatesEnabled(true);
        tlw->update();
    }
}

void MainWindow::zoomIn() {
    if (QWidget* page = tabs_->currentWidget()) {
        if (QMetaObject::invokeMethod(page, "zoomInView", Qt::DirectConnection)) {
            double z = 0.0;
            if (QMetaObject::invokeMethod(page, "zoom", Qt::DirectConnection, Q_RETURN_ARG(double, z))) {
                statusLabel_->setText(QString("Zoom: %1%").arg(int(z*100)));
            } else {
                statusLabel_->setText("Zoomed in");
            }
            return;
        }
    }
    statusLabel_->setText("No datasheet to zoom");
}

void MainWindow::zoomOut() {
    if (QWidget* page = tabs_->currentWidget()) {
        if (QMetaObject::invokeMethod(page, "zoomOutView", Qt::DirectConnection)) {
            double z = 0.0;
            if (QMetaObject::invokeMethod(page, "zoom", Qt::DirectConnection, Q_RETURN_ARG(double, z))) {
                statusLabel_->setText(QString("Zoom: %1%").arg(int(z*100)));
            } else {
                statusLabel_->setText("Zoomed out");
            }
            return;
        }
    }
    statusLabel_->setText("No datasheet to zoom");
}

void MainWindow::insertRecord() {
    if (QWidget* page = tabs_->currentWidget()) {
        if (QMetaObject::invokeMethod(page, "insertRow", Qt::DirectConnection)) {
            statusLabel_->setText("Inserted new row");
            return;
        }
        statusLabel_->setText("Insert not supported in current view");
        return;
    }
    statusLabel_->setText("No view");
}

void MainWindow::deleteRecord() {
    if (QWidget* page = tabs_->currentWidget()) {
        if (QMetaObject::invokeMethod(page, "deleteRows", Qt::DirectConnection)) {
            statusLabel_->setText("Deleted selection");
            return;
        }
        statusLabel_->setText("Delete not supported in current view");
        return;
    }
    statusLabel_->setText("No view");
}

void MainWindow::refreshView() {
    if (QWidget* page = tabs_->currentWidget()) {
        if (QMetaObject::invokeMethod(page, "refresh", Qt::DirectConnection)) {
            statusLabel_->setText("Refreshed");
            return;
        }
        statusLabel_->setText("Nothing to refresh");
        return;
    }
    statusLabel_->setText("No view");
}

void MainWindow::showEvent(QShowEvent* e) {
    QMainWindow::showEvent(e);
    if (firstShow_) {
        firstShow_ = false;
        QScreen* sc = screen() ? screen() : QGuiApplication::primaryScreen();
        if (sc) {
            const QRect r = sc->availableGeometry();
            move(r.center() - QPoint(width()/2, height()/2));
        }
    }
}

void MainWindow::setProjectPathAndReload(const QString& dir) {
    currentProjectPath_ = dir.isEmpty() ? QString() : QDir(dir).absolutePath();

    while (tabs_->count() > 0) {
        QWidget* w = tabs_->widget(0);
        tabs_->removeTab(0);
        if (w) w->deleteLater();
    }
    openDatasheets_.clear();
    openDesigns_.clear();
    queryBuilderTab_.clear();
    relationDesignerTab_.clear();

    refreshDock();

    const QString titleProj = currentProjectPath_.isEmpty()
                                  ? "(no project)"
                                  : QFileInfo(currentProjectPath_).fileName();
    setWindowTitle(QString("MiniAccess — %1").arg(titleProj));
    if (statusLabel_) statusLabel_->setText(
            QString("Project: %1").arg(currentProjectPath_.isEmpty() ? "—" : currentProjectPath_)
            );
}

void MainWindow::refreshDock() {
    if (dock_) {
        dock_->setProjectPath(currentProjectPath_);
    }
}

void MainWindow::newProject() {
    QString parent = QFileDialog::getExistingDirectory(
        this,
        "Select parent folder (will create a new subfolder here)",
        currentProjectPath_.isEmpty() ? QDir::homePath() : currentProjectPath_,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
    if (parent.isEmpty()) return;

    bool ok = false;
    QString name = QInputDialog::getText(
                       this,
                       "New Project",
                       "Project folder name:",
                       QLineEdit::Normal,
                       "MyProject",
                       &ok
                       ).trimmed();
    if (!ok || name.isEmpty()) return;

    static const QRegularExpression invalid(R"([\\/:*?"<>|])");
    if (name.contains(invalid)) {
        QMessageBox::warning(this, "New Project",
                             "Invalid folder name. Avoid characters: \\ / : * ? \" < > |");
        return;
    }

    QDir dir(parent);
    const QString full = dir.filePath(name);

    if (QFileInfo::exists(full)) {
        auto ans = QMessageBox::question(
            this, "New Project",
            "That folder already exists. Do you want to use it as the project?",
            QMessageBox::Yes | QMessageBox::No
            );
        if (ans != QMessageBox::Yes) return;
        setProjectPathAndReload(full);
        return;
    }

    if (!QDir().mkpath(full)) {
        QMessageBox::warning(this, "New Project",
                             "Cannot create the selected folder.");
        return;
    }

    setProjectPathAndReload(full);
}

void MainWindow::openProject() {
    QString folder = QFileDialog::getExistingDirectory(
        this,
        "Open Project (select a folder containing your tables)",
        currentProjectPath_.isEmpty() ? QDir::homePath() : currentProjectPath_,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
    if (folder.isEmpty()) return;

    QFileInfo fi(folder);
    if (!fi.exists() || !fi.isDir()) {
        QMessageBox::warning(this, "Open Project",
                             "Please select a valid folder.");
        return;
    }
    setProjectPathAndReload(folder);
}

void MainWindow::closeCurrentProject() {
    setProjectPathAndReload(QString());
}

bool MainWindow::isMiniAccessProjectDir(const QString& dir) const {
    if (dir.isEmpty()) return false;
    QDir d(dir);
    if (!d.exists()) return false;
    if (d.exists(".miniaccess")) return true;

    const QStringList metas = d.entryList(QStringList() << "*.meta", QDir::Files);
    const QStringList mads  = d.entryList(QStringList() << "*.mad",  QDir::Files);
    return (!metas.isEmpty() || !mads.isEmpty());
}

void MainWindow::closeTabsForBase(const QString& base) {
    if (openDatasheets_.contains(base)) {
        if (QWidget* w = openDatasheets_.value(base)) {
            if (auto* ds = qobject_cast<DatasheetPage*>(w)) {
                ds->prepareForClose();
            }
            int idx = tabs_->indexOf(w);
            if (idx >= 0) tabs_->removeTab(idx);
            delete w;
        }
        openDatasheets_.remove(base);
    }

    if (openDesigns_.contains(base)) {
        if (QWidget* w = openDesigns_.value(base)) {
            int idx = tabs_->indexOf(w);
            if (idx >= 0) tabs_->removeTab(idx);
            delete w;
        }
        openDesigns_.remove(base);
    }

    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);
}

bool MainWindow::removeTableFiles(const QString& base) {
    QStringList failed;
    auto tryRemove = [&](const QString& path){
        if (!QFileInfo::exists(path)) return true;
        for (int i = 0; i < 5; ++i) {
            if (QFile::remove(path)) return true;
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 5);
            QThread::msleep(30);
        }
        failed << path;
        return false;
    };

    bool ok = true;
    ok &= tryRemove(base + ".mad");
    ok &= tryRemove(base + ".meta");

    QFileInfo bi(base);
    const QString dir = bi.dir().absolutePath();
    const QString pref = bi.fileName() + ".";
    QDir d(dir);
    const QStringList idxs = d.entryList(QStringList() << (pref + "*.idx"), QDir::Files);
    for (const QString& f : idxs) {
        ok &= tryRemove(d.filePath(f));
    }

    if (!ok) {
        QMessageBox::critical(this, "Delete Table",
                              "Some files could not be removed. Check permissions.\n\n"
                                  + failed.join('\n'));
    }
    return ok;
}

void MainWindow::deleteTableByBase(const QString& base) {
    if (base.isEmpty()) return;

    const QString tableName = QFileInfo(base).fileName();
    const auto ret = QMessageBox::warning(
        this, "Delete Table",
        QString("Delete table \"%1\"?\nThis will permanently remove its data and indexes.")
            .arg(tableName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    closeTabsForBase(base);

    if (!removeTableFiles(base)) {
        QMessageBox::critical(this, "Delete Table",
                              "Some files could not be removed. Check permissions.");
        refreshDock();
        return;
    }

    if (statusLabel_) statusLabel_->setText(QString("Table \"%1\" deleted").arg(tableName));
    refreshDock();
}

void MainWindow::deleteCurrentProject() {
    QString dir = projectDir_;
    if (dir.isEmpty()) {
        if (!openDatasheets_.isEmpty()) {
            const QString anyBase = openDatasheets_.keys().first();
            dir = QFileInfo(anyBase).dir().absolutePath();
        } else if (!openDesigns_.isEmpty()) {
            const QString anyBase = openDesigns_.keys().first();
            dir = QFileInfo(anyBase).dir().absolutePath();
        }
    }

    if (dir.isEmpty() || !QDir(dir).exists()) {
        QMessageBox::information(this, "Delete Project", "No project is open (folder not found).");
        return;
    }

    QDir d(dir);
    if (d.isRoot()) {
        QMessageBox::warning(this, "Delete Project",
                             "Refusing to delete a drive root. Pick a normal folder.");
        return;
    }

    const auto ret = QMessageBox::warning(
        this, "Delete Project",
        QString("This will permanently delete the entire project folder:\n\n%1\n\n"
                "All tables and indexes will be lost. Continue?")
            .arg(dir),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    auto closeAllForMap = [&](QHash<QString, QPointer<QWidget>>& map){
        const auto keys = map.keys();
        for (const QString& base : keys) {
            if (QWidget* w = map.value(base)) {
                if (auto* ds = qobject_cast<DatasheetPage*>(w)) {
                    ds->prepareForClose();
                }
                int idx = tabs_->indexOf(w);
                if (idx >= 0) tabs_->removeTab(idx);
                delete w;
            }
            map.remove(base);
        }
    };
    closeAllForMap(openDatasheets_);
    closeAllForMap(openDesigns_);

    if (queryBuilderTab_) {
        int qi = tabs_->indexOf(queryBuilderTab_);
        if (qi >= 0) tabs_->removeTab(qi);
        delete queryBuilderTab_;
        queryBuilderTab_.clear();
    }
    while (tabs_->count() > 0) {
        QWidget* w = tabs_->widget(0);
        tabs_->removeTab(0);
        delete w;
    }
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);

    bool removed = false;
    for (int i=0; i<3 && !removed; ++i) {
        removed = d.removeRecursively();
        if (!removed) {
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);
            QThread::msleep(40);
        }
    }

    if (!removed) {
        QMessageBox::critical(this, "Delete Project",
                              "The folder could not be removed. Check that files are not in use.");
        return;
    }

    projectDir_.clear();
    setWindowTitle("MiniAccess");
    refreshDock();
    if (statusLabel_) statusLabel_->setText("Project deleted");
}

void MainWindow::deleteSelectedTable() {
    if (!dock_) return;
    const QString base = dock_->currentSelectedBase();
    if (base.isEmpty()) {
        QMessageBox::information(this, "Delete Table", "Select a table in the left panel first.");
        return;
    }
    deleteTableByBase(base);
}

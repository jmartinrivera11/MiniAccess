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
#include <QMessageBox>
#include <QFont>
#include <QApplication>
#include <QFileDialog>
#include <QDir>
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
    connect(actQuit,         &QAction::triggered, this, &QWidget::close);

    connect(actZoomIn,       &QAction::triggered, this, &MainWindow::zoomIn);
    connect(actZoomOut,      &QAction::triggered, this, &MainWindow::zoomOut);

    connect(actQuery,        &QAction::triggered, this, &MainWindow::openQueryBuilder);
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
    auto* page = new RelationDesignerPage(this);
    int idx = tabs_->addTab(page, QIcon(":/icons/icons/relation.svg"), "Relations");
    tabs_->setCurrentIndex(idx);
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

    QDir d(currentProjectPath_);
    const QStringList metas = d.entryList(QStringList() << "*.meta", QDir::Files, QDir::Name);
    for (const QString& m : metas) {
        QString base = d.absoluteFilePath(m);
        if (base.endsWith(".meta")) base.chop(5);
        ensureDatasheetTab(base);
    }
}

void MainWindow::closeCurrentProject() {
    setProjectPathAndReload(QString());
}

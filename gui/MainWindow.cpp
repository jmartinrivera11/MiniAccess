#include "MainWindow.h"
#include <QTableView>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QInputDialog>
#include <QHeaderView>
#include <QKeySequence>
#include <QWheelEvent>
#include <QApplication>

#include "TableModel.h"
#include "TableDesignerDialog.h"
#include "QueryBuilderDialog.h"
#include "RelationDesignerDialog.h"
#include "BoolDelegate.h"

using namespace ma;

MainWindow::MainWindow(QWidget* parent): QMainWindow(parent) {
    setupUi();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi() {
    setWindowTitle("MiniAccess - GUI");
    resize(1000, 650);

    view_ = new QTableView(this);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view_->horizontalHeader()->setStretchLastSection(true);
    setCentralWidget(view_);

    view_->installEventFilter(this);

    auto* fileMenu  = menuBar()->addMenu("&File");
    auto* toolsMenu = menuBar()->addMenu("&Tools");
    auto* viewMenu  = menuBar()->addMenu("&View");

    actNew_  = fileMenu->addAction("New Table...");
    actOpen_ = fileMenu->addAction("Open Table...");
    fileMenu->addSeparator();
    auto* actQuit = fileMenu->addAction("Quit");

    actCreateIdxI_ = toolsMenu->addAction("Create Int32 Index...");
    actCreateIdxS_ = toolsMenu->addAction("Create String Index...");
    toolsMenu->addSeparator();
    actQuery_    = toolsMenu->addAction("Query Builder...");
    actRelation_ = toolsMenu->addAction("Relation Designer...");

    actInsert_  = viewMenu->addAction("Insert Row");
    actDelete_  = viewMenu->addAction("Delete Selected Rows");
    actRefresh_ = viewMenu->addAction("Refresh");
    viewMenu->addSeparator();

    actZoomIn_    = viewMenu->addAction("Zoom +");
    actZoomOut_   = viewMenu->addAction("Zoom -");
    actZoomReset_ = viewMenu->addAction("Zoom 100%");

    actZoomIn_->setShortcuts({QKeySequence::ZoomIn, QKeySequence("Ctrl++"), QKeySequence("Ctrl+=")});
    actZoomOut_->setShortcuts({QKeySequence::ZoomOut, QKeySequence("Ctrl+-")});
    actZoomReset_->setShortcut(QKeySequence("Ctrl+0"));

    auto* tb = addToolBar("Main");
    tb->addAction(actNew_);
    tb->addAction(actOpen_);
    tb->addSeparator();
    tb->addAction(actInsert_);
    tb->addAction(actDelete_);
    tb->addAction(actRefresh_);
    tb->addSeparator();
    tb->addAction(actCreateIdxI_);
    tb->addAction(actCreateIdxS_);
    tb->addSeparator();
    tb->addAction(actQuery_);
    tb->addAction(actRelation_);
    tb->addSeparator();
    tb->addAction(actZoomIn_);
    tb->addAction(actZoomOut_);
    tb->addAction(actZoomReset_);

    statusLabel_ = new QLabel("Ready", this);
    statusBar()->addPermanentWidget(statusLabel_);

    connect(actNew_,       &QAction::triggered, this, &MainWindow::newTable);
    connect(actOpen_,      &QAction::triggered, this, &MainWindow::openTable);
    connect(actCreateIdxI_,&QAction::triggered, this, &MainWindow::createIntIndex);
    connect(actCreateIdxS_,&QAction::triggered, this, &MainWindow::createStrIndex);
    connect(actInsert_,    &QAction::triggered, this, &MainWindow::insertRow);
    connect(actDelete_,    &QAction::triggered, this, &MainWindow::deleteSelectedRows);
    connect(actRefresh_,   &QAction::triggered, this, &MainWindow::refreshView);
    connect(actQuery_,     &QAction::triggered, this, &MainWindow::openQueryBuilder);
    connect(actRelation_,  &QAction::triggered, this, &MainWindow::openRelationDesigner);
    connect(actQuit,       &QAction::triggered, this, &QWidget::close);

    connect(actZoomIn_,  &QAction::triggered, this, [this]{ gridZoom_ = std::min(gridZoom_+1, 8); applyGridScale(); });
    connect(actZoomOut_, &QAction::triggered, this, [this]{ gridZoom_ = std::max(gridZoom_-1, -4); applyGridScale(); });
    connect(actZoomReset_,&QAction::triggered, this, [this]{ gridZoom_ = 0; applyGridScale(); });

    applyGridScale();
}

QString MainWindow::chooseBasePathForNew() {
    QString dir = QFileDialog::getSaveFileName(this, "Base file (without extension)", "", "MiniAccess (*.meta)");
    if (dir.isEmpty()) return {};
    if (dir.endsWith(".meta")) dir.chop(5);
    return dir;
}

QString MainWindow::chooseExistingMeta() {
    return QFileDialog::getOpenFileName(this, "Open Table (.meta)", "", "MiniAccess Meta (*.meta)");
}

void MainWindow::newTable() {
    auto chosen = chooseBasePathForNew();
    if (chosen.isEmpty()) return;

    TableDesignerDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    schema_ = dlg.schema();
    basePath_ = chosen;

    try {
        table_ = std::make_unique<Table>();
        table_->create(basePath_.toStdString(), schema_);
        table_->setFitStrategy(FitStrategy::FirstFit);
        bindTableToView();
        statusLabel_->setText("Created table: " + basePath_);
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Error creating table", ex.what());
    }
}

void MainWindow::openTable() {
    auto meta = chooseExistingMeta();
    if (meta.isEmpty()) return;
    QString base = meta;
    if (base.endsWith(".meta")) base.chop(5);

    try {
        table_ = std::make_unique<Table>();
        table_->open(base.toStdString());
        schema_ = table_->getSchema();
        basePath_ = base;
        bindTableToView();
        statusLabel_->setText("Opened table: " + basePath_);
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Error opening table", ex.what());
    }
}

void MainWindow::bindTableToView() {
    delete model_;
    model_ = nullptr;
    if (!table_) return;
    model_ = new TableModel(table_.get(), this);
    view_->setModel(model_);
    view_->resizeColumnsToContents();

    const auto& s = table_->getSchema();
    for (int c=0; c<(int)s.fields.size(); ++c) {
        if (s.fields[c].type == ma::FieldType::Bool) {
            view_->setItemDelegateForColumn(c, new BoolDelegate(view_));
            view_->setColumnWidth(c, 80);
        }
    }

    applyGridScale();
}

void MainWindow::createIntIndex() {
    if (!table_) return;
    const auto& s = table_->getSchema();
    QStringList choices;
    QList<int> fieldIdxs;
    for (int i=0;i<(int)s.fields.size();++i) {
        if (s.fields[i].type == FieldType::Int32) {
            choices << QString::fromStdString(s.fields[i].name);
            fieldIdxs << i;
        }
    }
    if (choices.isEmpty()) { QMessageBox::information(this,"Index","No Int32 fields"); return; }
    bool ok=false;
    QString name = QInputDialog::getItem(this,"Int32 Index","Field", choices, 0, false, &ok);
    if (!ok) return;
    int pos = choices.indexOf(name);
    int fidx = fieldIdxs[pos];

    QString idxName = QInputDialog::getText(this, "Int32 Index", "Index name:", QLineEdit::Normal, "idx_int");
    if (idxName.isEmpty()) return;

    bool res = table_->createInt32Index(fidx, idxName.toStdString());
    QMessageBox::information(this, "Int32 Index", res ? "Created" : "Failed");
}

void MainWindow::createStrIndex() {
    if (!table_) return;
    const auto& s = table_->getSchema();
    QStringList choices;
    QList<int> fieldIdxs;
    for (int i=0;i<(int)s.fields.size();++i) {
        if (s.fields[i].type == FieldType::String || s.fields[i].type == FieldType::CharN) {
            choices << QString::fromStdString(s.fields[i].name);
            fieldIdxs << i;
        }
    }
    if (choices.isEmpty()) { QMessageBox::information(this,"Index","No String/CharN fields"); return; }
    bool ok=false;
    QString name = QInputDialog::getItem(this,"String Index","Field", choices, 0, false, &ok);
    if (!ok) return;
    int pos = choices.indexOf(name);
    int fidx = fieldIdxs[pos];

    QString idxName = QInputDialog::getText(this, "String Index", "Index name:", QLineEdit::Normal, "idx_str");
    if (idxName.isEmpty()) return;

    bool res = table_->createStringIndex(fidx, idxName.toStdString());
    QMessageBox::information(this, "String Index", res ? "Created" : "Failed");
}

void MainWindow::insertRow() {
    if (!table_ || !model_) return;
    model_->insertRows(model_->rowCount(), 1);
}

void MainWindow::deleteSelectedRows() {
    if (!table_ || !model_) return;
    auto sel = view_->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;
    std::sort(sel.begin(), sel.end(), [](const QModelIndex& a, const QModelIndex& b){ return a.row()>b.row(); });
    for (const auto& idx: sel) {
        model_->removeRows(idx.row(), 1);
    }
}

void MainWindow::refreshView() {
    if (!model_) return;
    model_->reload();
    view_->resizeColumnsToContents();
    applyGridScale();
}

void MainWindow::openQueryBuilder() {
    if (!table_) return;
    QueryBuilderDialog dlg(table_.get(), this);
    dlg.exec();
}

void MainWindow::openRelationDesigner() {
    RelationDesignerDialog dlg(this);
    dlg.exec();
}

void MainWindow::applyGridScale() {
    if (!view_) return;

    if (baseGridPt_ < 0) {
        int ps = view_->font().pointSize();
        baseGridPt_ = (ps > 0 ? ps : 11);
    }

    int targetPt = std::max(8, baseGridPt_ + gridZoom_);
    QFont f = view_->font();
    f.setPointSize(targetPt);
    view_->setFont(f);

    int rowH = targetPt * 2 + 10;
    view_->verticalHeader()->setDefaultSectionSize(rowH);
    view_->resizeColumnsToContents();
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == view_) {
        if (ev->type() == QEvent::Wheel && (QApplication::keyboardModifiers() & Qt::ControlModifier)) {
            auto* we = static_cast<QWheelEvent*>(ev);
            if (we->angleDelta().y() > 0) gridZoom_ = std::min(gridZoom_+1, 8);
            else                          gridZoom_ = std::max(gridZoom_-1, -4);
            applyGridScale();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

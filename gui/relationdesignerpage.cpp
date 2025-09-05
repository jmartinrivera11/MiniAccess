#include "RelationDesignerPage.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsPathItem>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QTimer>
#include <QGraphicsPathItem>

#include "../core/Table.h"
#include "../core/Schema.h"

using namespace ma;

static QFont titleFont() {
    QFont f; f.setBold(true); f.setPointSize(10); return f;
}
static QFont fieldFont() {
    QFont f; f.setPointSize(9); return f;
}

RelationDesignerPage::RelationDesignerPage(const QString& projectDir, QWidget* parent)
    : QWidget(parent), projectDir_(projectDir)
{
    buildUi();
    loadTables();
    layoutNodes();
    loadFromJson();

    connect(scene_, &QGraphicsScene::changed, this, &RelationDesignerPage::onSceneChanged);
}

void RelationDesignerPage::buildUi() {
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(6,6,6,6);
    lay->setSpacing(6);

    scene_ = new QGraphicsScene(this);
    view_  = new QGraphicsView(scene_, this);
    view_->setRenderHint(QPainter::Antialiasing, true);
    view_->setDragMode(QGraphicsView::RubberBandDrag);
    view_->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    view_->setMinimumSize(640, 420);

    QWidget* panel = new QWidget(this);
    auto* pv = new QVBoxLayout(panel);
    pv->setContentsMargins(6,6,6,6);
    pv->setSpacing(8);

    auto* form1 = new QFormLayout();
    form1->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    leftTable_ = new QComboBox(panel);
    leftField_ = new QComboBox(panel);
    rightTable_ = new QComboBox(panel);
    rightField_ = new QComboBox(panel);
    joinType_ = new QComboBox(panel);
    joinType_->addItems({"INNER","LEFT"});

    cbCascadeDel_ = new QCheckBox("Cascade Delete", panel);
    cbCascadeUpd_ = new QCheckBox("Cascade Update", panel);

    form1->addRow("Left table:", leftTable_);
    form1->addRow("Left field:", leftField_);
    form1->addRow("Right table:", rightTable_);
    form1->addRow("Right field:", rightField_);
    form1->addRow("Join type:", joinType_);
    pv->addLayout(form1);
    pv->addWidget(cbCascadeDel_);
    pv->addWidget(cbCascadeUpd_);

    auto* rowBtns = new QHBoxLayout();
    btnAdd_ = new QPushButton("Add relation", panel);
    btnRemove_ = new QPushButton("Remove selected", panel);
    btnSave_ = new QPushButton("Save", panel);
    rowBtns->addWidget(btnAdd_);
    rowBtns->addWidget(btnRemove_);
    rowBtns->addStretch();
    rowBtns->addWidget(btnSave_);
    pv->addLayout(rowBtns);

    grid_ = new QTableWidget(0, 5, panel);
    grid_->setHorizontalHeaderLabels({"Left","Right","Type","Cascade D","Cascade U"});
    grid_->horizontalHeader()->setStretchLastSection(true);
    grid_->verticalHeader()->setVisible(false);
    grid_->setSelectionBehavior(QAbstractItemView::SelectRows);
    pv->addWidget(grid_, 1);

    auto* zoomRow = new QHBoxLayout();
    auto* bIn = new QPushButton("Zoom +", panel);
    auto* bOut= new QPushButton("Zoom -", panel);
    zoomRow->addWidget(bIn); zoomRow->addWidget(bOut); zoomRow->addStretch();
    pv->addLayout(zoomRow);
    connect(bIn, &QPushButton::clicked, this, &RelationDesignerPage::zoomIn);
    connect(bOut,&QPushButton::clicked, this, &RelationDesignerPage::zoomOut);

    lay->addWidget(view_, 1);
    lay->addWidget(panel);

    connect(leftTable_,  &QComboBox::currentTextChanged, this, &RelationDesignerPage::onLeftTableChanged);
    connect(rightTable_, &QComboBox::currentTextChanged, this, &RelationDesignerPage::onRightTableChanged);
    connect(btnAdd_,     &QPushButton::clicked,          this, &RelationDesignerPage::onAddRelation);
    connect(btnRemove_,  &QPushButton::clicked,          this, &RelationDesignerPage::onRemoveSelected);
    connect(btnSave_,    &QPushButton::clicked,          this, &RelationDesignerPage::onSave);
}

void RelationDesignerPage::loadTables() {
    schemas_.clear();
    leftTable_->clear();
    rightTable_->clear();

    if (projectDir_.isEmpty()) return;
    QDir d(projectDir_);
    const auto metas = d.entryList(QStringList() << "*.meta", QDir::Files, QDir::Name);
    for (const auto& m : metas) {
        QString base = d.absoluteFilePath(m);
        if (base.endsWith(".meta")) base.chop(5);
        try {
            ma::Table t; t.open(base.toStdString());
            ma::Schema s = t.getSchema();
            QString name = QString::fromStdString(s.tableName);
            schemas_.insert(name, s);
            leftTable_->addItem(name);
            rightTable_->addItem(name);
        } catch (...) {
        }
    }
    if (leftTable_->count()>0) onLeftTableChanged(leftTable_->currentText());
    if (rightTable_->count()>0) onRightTableChanged(rightTable_->currentText());
}

void RelationDesignerPage::layoutNodes() {
    scene_->clear();
    qDeleteAll(nodes_); nodes_.clear();

    const int colW = 260;
    const int rowH = 220;
    int col = 0, row = 0;

    for (const auto& name : schemas_.keys()) {
        const auto& s = schemas_[name];

        QRectF rect(0,0,240, 60 + 18*int(s.fields.size()));
        auto* box = scene_->addRect(rect, QPen(Qt::gray, 1.0), QBrush(Qt::white));
        box->setFlag(QGraphicsItem::ItemIsMovable, true);
        box->setFlag(QGraphicsItem::ItemIsSelectable, true);

        auto* title = scene_->addSimpleText(name);
        title->setFont(titleFont());
        title->setPos(rect.left()+8, rect.top()+6);
        title->setParentItem(box);

        QMap<QString, QGraphicsSimpleTextItem*> labels;
        int y = 30;
        for (const auto& f : s.fields) {
            QString fname = QString::fromStdString(f.name);
            auto* lbl = scene_->addSimpleText(fname);
            lbl->setFont(fieldFont());
            lbl->setPos(rect.left()+12, rect.top()+y);
            lbl->setParentItem(box);
            labels.insert(fname, lbl);
            y += 16;
        }

        box->setPos(col*colW + 40, row*rowH + 40);

        Node* n = new Node{ name, box, labels };
        nodes_.insert(name, n);

        col++;
        if (col>=2) { col=0; row++; }
    }
}

void RelationDesignerPage::onLeftTableChanged(const QString& t) {
    rebuildFieldsFor(t, leftField_);
}
void RelationDesignerPage::onRightTableChanged(const QString& t) {
    rebuildFieldsFor(t, rightField_);
}

void RelationDesignerPage::rebuildFieldsFor(const QString& table, QComboBox* fieldCombo) {
    fieldCombo->clear();
    if (!schemas_.contains(table)) return;
    const auto& s = schemas_[table];
    for (const auto& f : s.fields) {
        fieldCombo->addItem(QString::fromStdString(f.name));
    }
}

void RelationDesignerPage::onAddRelation() {
    if (leftTable_->currentText().isEmpty() || rightTable_->currentText().isEmpty() ||
        leftField_->currentText().isEmpty() || rightField_->currentText().isEmpty()) {
        QMessageBox::information(this, "Relation", "Please select both tables and fields.");
        return;
    }
    Rel r;
    r.lt = leftTable_->currentText();
    r.lf = leftField_->currentText();
    r.rt = rightTable_->currentText();
    r.rf = rightField_->currentText();
    r.type = joinType_->currentText();
    r.cascadeDel = cbCascadeDel_->isChecked();
    r.cascadeUpd = cbCascadeUpd_->isChecked();

    for (const auto& e : relations_) {
        if (e.lt==r.lt && e.lf==r.lf && e.rt==r.rt && e.rf==r.rf) {
            QMessageBox::information(this, "Relation", "That relation already exists.");
            return;
        }
    }
    addRelation(r);
}

void RelationDesignerPage::addRelation(const Rel& r) {
    relations_.push_back(r);

    int row = grid_->rowCount();
    grid_->insertRow(row);
    grid_->setItem(row, 0, new QTableWidgetItem(r.lt + "." + r.lf));
    grid_->setItem(row, 1, new QTableWidgetItem(r.rt + "." + r.rf));
    grid_->setItem(row, 2, new QTableWidgetItem(r.type));
    grid_->setItem(row, 3, new QTableWidgetItem(r.cascadeDel ? "Yes":"No"));
    grid_->setItem(row, 4, new QTableWidgetItem(r.cascadeUpd ? "Yes":"No"));

    auto p1 = fieldAnchor(r.lt, r.lf);
    auto p2 = fieldAnchor(r.rt, r.rf);

    QPainterPath path;
    const qreal dx = (p2.x() - p1.x()) * 0.3;
    path.moveTo(p1);
    path.cubicTo(p1 + QPointF(dx, 0), p2 - QPointF(dx, 0), p2);

    auto* link = scene_->addPath(path, QPen(QColor(15,143,102), 2.0));
    relations_.back().link = link;
}

void RelationDesignerPage::onRemoveSelected() {
    auto sel = grid_->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;
    std::sort(sel.begin(), sel.end(), [](const QModelIndex& a, const QModelIndex& b){ return a.row()>b.row(); });
    for (const auto& idx : sel) removeRelationAtRow(idx.row());
}

void RelationDesignerPage::removeRelationAtRow(int row) {
    if (row<0 || row>=relations_.size()) return;
    if (relations_[row].link) {
        scene_->removeItem(relations_[row].link);
        delete relations_[row].link;
        relations_[row].link = nullptr;
    }
    relations_.remove(row);
    grid_->removeRow(row);
}

void RelationDesignerPage::onSave() {
    saveToJson();
    QMessageBox::information(this, "Relations", "Relations saved.");
}

QString RelationDesignerPage::jsonPath() const {
    return QDir(projectDir_).filePath("relations.json");
}

void RelationDesignerPage::saveToJson() {
    QJsonObject root; root["version"] = 1;

    QJsonArray jnodes;
    for (auto it = nodes_.cbegin(); it != nodes_.cend(); ++it) {
        const Node* n = it.value();
        if (!n || !n->box) continue;
        QJsonObject jn;
        jn["table"] = n->table;
        jn["x"] = n->box->pos().x();
        jn["y"] = n->box->pos().y();
        jnodes.push_back(jn);
    }
    root["nodes"] = jnodes;

    // relations
    QJsonArray jrels;
    for (const auto& r : relations_) {
        QJsonObject jr;
        jr["leftTable"]  = r.lt;
        jr["leftField"]  = r.lf;
        jr["rightTable"] = r.rt;
        jr["rightField"] = r.rf;
        jr["type"]       = r.type;
        jr["cascadeDelete"] = r.cascadeDel;
        jr["cascadeUpdate"] = r.cascadeUpd;
        jrels.push_back(jr);
    }
    root["relations"] = jrels;

    QFile f(jsonPath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.close();
    }
}

void RelationDesignerPage::loadFromJson() {
    QFile f(jsonPath());
    if (!f.exists()) return;
    if (!f.open(QIODevice::ReadOnly)) return;
    auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return;

    auto root = doc.object();

    if (root.contains("nodes") && root["nodes"].isArray()) {
        auto arr = root["nodes"].toArray();
        for (const auto& v : arr) {
            auto o = v.toObject();
            QString t = o["table"].toString();
            if (!nodes_.contains(t)) continue;
            auto* n = nodes_[t];
            qreal x = o["x"].toDouble(n->box->pos().x());
            qreal y = o["y"].toDouble(n->box->pos().y());
            n->box->setPos(QPointF(x,y));
        }
    }

    if (root.contains("relations") && root["relations"].isArray()) {
        auto arr = root["relations"].toArray();
        for (const auto& v : arr) {
            auto o = v.toObject();
            Rel r;
            r.lt = o["leftTable"].toString();
            r.lf = o["leftField"].toString();
            r.rt = o["rightTable"].toString();
            r.rf = o["rightField"].toString();
            r.type = o["type"].toString("INNER");
            r.cascadeDel = o["cascadeDelete"].toBool(false);
            r.cascadeUpd = o["cascadeUpdate"].toBool(false);

            if (!nodes_.contains(r.lt) || !nodes_.contains(r.rt)) continue;
            if (!schemas_.contains(r.lt) || !schemas_.contains(r.rt)) continue;
            bool lfOk=false, rfOk=false;
            for (const auto& f : schemas_[r.lt].fields) if (QString::fromStdString(f.name)==r.lf) { lfOk=true; break; }
            for (const auto& f : schemas_[r.rt].fields) if (QString::fromStdString(f.name)==r.rf) { rfOk=true; break; }
            if (!lfOk || !rfOk) continue;

            addRelation(r);
        }
    }
    redrawAllLinks();
}

QPointF RelationDesignerPage::fieldAnchor(const QString& table, const QString& field) const {
    auto itN = nodes_.find(table);
    if (itN == nodes_.end()) return QPointF();
    const Node* n = itN.value();
    if (!n || !n->box) return QPointF();
    auto itL = n->fieldLabels.find(field);
    if (itL == n->fieldLabels.end()) {
        return n->box->sceneBoundingRect().center();
    }
    return itL.value()->sceneBoundingRect().center();
}

void RelationDesignerPage::redrawAllLinks() {
    for (auto& r : relations_) {
        if (!r.link) continue;
        auto p1 = fieldAnchor(r.lt, r.lf);
        auto p2 = fieldAnchor(r.rt, r.rf);
        QPainterPath path;
        const qreal dx = (p2.x() - p1.x()) * 0.3;
        path.moveTo(p1);
        path.cubicTo(p1 + QPointF(dx, 0), p2 - QPointF(dx, 0), p2);
        r.link->setPath(path);
    }
}

void RelationDesignerPage::onSceneChanged() {
    redrawAllLinks();
}

void RelationDesignerPage::zoomIn()  { view_->scale(1.1, 1.1); }
void RelationDesignerPage::zoomOut() { view_->scale(0.9, 0.9); }

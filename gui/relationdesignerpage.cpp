#include "relationdesignerpage.h"
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QGraphicsSceneMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QListWidgetItem>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPen>
#include <QBrush>
#include <QtMath>
#include <QMimeData>
#include <QDrag>
#include <QDataStream>
#include <QMessageBox>
#include <QSizePolicy>
#include <QFont>
#include <qevent.h>
#include "../core/pk_utils.h"
#include "../core/relations_io.h"
#include "../core/metadata.h"

static QFont safeUiFontRegular(int pt = 10) {
#ifdef Q_OS_WIN
    QFont f; f.setFamilies(QStringList() << "Segoe UI" << "Arial" << "Tahoma" << "Sans Serif");
#else
    QFont f; f.setFamilies(QStringList() << "Noto Sans" << "DejaVu Sans" << "Sans Serif" << "Arial");
#endif
    f.setPointSize(pt);
    f.setStyleHint(QFont::SansSerif);
    return f;
}
static QFont safeUiFontBold(int pt = 10) { QFont f = safeUiFontRegular(pt); f.setBold(true); return f; }

class RelationDesignerPage::FieldItem : public QGraphicsRectItem {
public:
    FieldItem(RelationDesignerPage* owner, const QString& table, const QString& name, bool isPk, QGraphicsItem* parent=nullptr)
        : QGraphicsRectItem(parent), owner_(owner), table_(table), name_(name), pk_(isPk)
    {
        setRect(0, 0, 240, 22);
        setPen(QPen(QColor(180,180,180)));
        setBrush(QBrush(Qt::white));
        setAcceptHoverEvents(true);

        text_ = new QGraphicsTextItem(name_, this);
        text_->setDefaultTextColor(Qt::black);
        text_->setFont(safeUiFontRegular());
        text_->setPos(8, 2);

        if (pk_) {
            pkLabel_ = new QGraphicsTextItem(QStringLiteral("PK"), this);
            pkLabel_->setDefaultTextColor(Qt::black);
            pkLabel_->setFont(safeUiFontBold());
            pkLabel_->setPos(218-10, 2);
        }
    }

    bool isPicked() const { return picked_; }
    void setPicked(bool v) {
        if (picked_ == v) return;
        picked_ = v;
        if (picked_) { setPen(QPen(QColor(30,120,200), 2)); setBrush(QBrush(QColor(235,245,255))); }
        else         { setPen(QPen(QColor(180,180,180), 1)); setBrush(QBrush(Qt::white)); }
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* ev) override {
        if (ev->button() == Qt::LeftButton) {
            owner_->onFieldClicked(this);
            ev->accept(); return;
        }
        QGraphicsRectItem::mousePressEvent(ev);
    }

public:
    QString table() const { return table_; }
    QString name()  const { return name_; }
    bool    isPk()  const { return pk_; }

private:
    RelationDesignerPage* owner_{nullptr};
    QString table_;
    QString name_;
    bool pk_{false};
    bool picked_{false};
    QGraphicsTextItem* text_{nullptr};
    QGraphicsTextItem* pkLabel_{nullptr};
};

class RelationDesignerPage::TableBox : public QGraphicsRectItem {
public:
    TableBox(RelationDesignerPage* owner, const QString& table, const QStringList& fields, const QString& pk, QGraphicsItem* parent=nullptr)
        : QGraphicsRectItem(parent), owner_(owner), table_(table), pk_(pk)
    {
        const int rows = fields.size();
        const int h = qMax(44, 28 + rows*24);
        setRect(0, 0, 260, h);
        setBrush(QBrush(QColor(245,249,255)));
        setPen(QPen(QColor(40,90,140), 1.4));
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);

        auto* header = new QGraphicsRectItem(0,0,260,24, this);
        header->setBrush(QBrush(QColor(220,235,255)));
        header->setPen(QPen(QColor(40,90,140)));
        auto* title = new QGraphicsTextItem(table_, header);
        title->setDefaultTextColor(QColor(20,40,60));
        title->setFont(safeUiFontBold());
        title->setPos(8, 2);

        int y = 28;
        for (const auto& f : fields) {
            const bool isPk = (!pk_.isEmpty() && f == pk_);
            auto* fi = new FieldItem(owner_, table, f, isPk, this);
            fi->setPos(10, y);
            items_.push_back(fi);
            y += 24;
        }
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override {
        if (change == ItemPositionHasChanged) {
            owner_->updateRelationsForTable(table_);
        }
        return QGraphicsRectItem::itemChange(change, value);
    }

    QString table() const { return table_; }
    QString pk() const { return pk_; }
    QList<FieldItem*> fieldItems() const { return items_; }

private:
    RelationDesignerPage* owner_{nullptr};
    QString table_;
    QString pk_;
    QList<FieldItem*> items_;
};

class RelationDesignerPage::CanvasView : public QGraphicsView {
public:
    explicit CanvasView(QGraphicsScene* s, RelationDesignerPage* owner, QWidget* parent=nullptr)
        : QGraphicsView(s, parent), owner_(owner) {
        setAcceptDrops(true);
        setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        setDragMode(QGraphicsView::RubberBandDrag);
    }
protected:
    void dragEnterEvent(QDragEnterEvent* e) override {
        if (e->mimeData()->hasFormat("application/x-miniaccess-table")) e->acceptProposedAction();
        else QGraphicsView::dragEnterEvent(e);
    }
    void dragMoveEvent(QDragMoveEvent* e) override {
        if (e->mimeData()->hasFormat("application/x-miniaccess-table")) e->acceptProposedAction();
        else QGraphicsView::dragMoveEvent(e);
    }
    void dropEvent(QDropEvent* e) override {
        if (!e->mimeData()->hasFormat("application/x-miniaccess-table")) {
            QGraphicsView::dropEvent(e); return;
        }
        const QByteArray bytes = e->mimeData()->data("application/x-miniaccess-table");
        const QString table = QString::fromUtf8(bytes);
        const QPointF scenePos = mapToScene(e->position().toPoint());
        owner_->addTableBoxAt(table, scenePos);
        e->acceptProposedAction();
    }
private:
    RelationDesignerPage* owner_;
};

class TablesListWidget : public QListWidget {
public:
    using QListWidget::QListWidget;
protected:
    void startDrag(Qt::DropActions actions) override {
        Q_UNUSED(actions);
        auto* it = currentItem();
        if (!it) return;
        const QString table = it->data(Qt::UserRole).toString();
        auto* mime = new QMimeData();
        mime->setData("application/x-miniaccess-table", table.toUtf8());
        auto* drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->exec(Qt::CopyAction);
    }
};

RelationDesignerPage::RelationDesignerPage(const QString& projectDir, QWidget* parent)
    : QWidget(parent)
    , scene_(new QGraphicsScene(this))
    , view_(nullptr)
    , tablesList_(nullptr)
    , cbRelType_(nullptr)
    , cbEnforce_(nullptr)
    , cbCascadeUpd_(nullptr)
    , cbCascadeDel_(nullptr)
    , btnCreate_(nullptr)
    , btnDelete_(nullptr)
    , btnSave_(nullptr)
    , relationsGrid_(nullptr)
    , projectDir_(projectDir)
{
    buildUi();
    loadTablesList();
    migrateJsonIfNeeded();
    loadFromJsonV2();
}

RelationDesignerPage::~RelationDesignerPage() = default;

void RelationDesignerPage::buildUi() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0,0,0,0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    view_ = new CanvasView(scene_, this, this);
    auto* leftPane = new QWidget(this);
    auto* leftLay = new QVBoxLayout(leftPane);
    leftLay->setContentsMargins(0,0,0,0);
    leftLay->addWidget(view_);

    auto* rightPane = new QWidget(this);
    auto* rightLay = new QVBoxLayout(rightPane);
    rightLay->setContentsMargins(8,8,8,8);

    tablesList_ = new TablesListWidget(this);
    tablesList_->setSelectionMode(QAbstractItemView::SingleSelection);
    tablesList_->setDragEnabled(true);
    tablesList_->setDefaultDropAction(Qt::IgnoreAction);
    tablesList_->setDragDropMode(QAbstractItemView::DragOnly);
    tablesList_->setFixedWidth(300);
    tablesList_->setStyleSheet("QListWidget { background: #e8f0ea; }");
    tablesList_->setFont(safeUiFontRegular());

    rightLay->addWidget(new QLabel(tr("Arrastra una tabla al lienzo:"), this));
    rightLay->addWidget(tablesList_, 1);

    auto* relBox = new QWidget(this);
    auto* relLay = new QHBoxLayout(relBox);
    relLay->setContentsMargins(0,0,0,0);
    relBox->setMaximumHeight(40);

    cbRelType_ = new QComboBox(this);
    cbRelType_->addItems(QStringList() << "1:1" << "1:N" << "N:M");
    cbRelType_->setMinimumWidth(70);
    cbRelType_->setFont(safeUiFontRegular());

    cbEnforce_    = new QCheckBox(tr("RI"), this);
    cbCascadeUpd_ = new QCheckBox(tr("CUpd"), this);
    cbCascadeDel_ = new QCheckBox(tr("CDel"), this);
    cbEnforce_->setFont(safeUiFontRegular());
    cbCascadeUpd_->setFont(safeUiFontRegular());
    cbCascadeDel_->setFont(safeUiFontRegular());

    auto* lblTipo = new QLabel(tr("Tipo:"), this);
    lblTipo->setFont(safeUiFontRegular());

    relLay->addWidget(lblTipo);
    relLay->addWidget(cbRelType_);
    relLay->addSpacing(8);
    relLay->addWidget(cbEnforce_);
    relLay->addWidget(cbCascadeUpd_);
    relLay->addWidget(cbCascadeDel_);
    rightLay->addWidget(relBox);

    auto* btnRow = new QHBoxLayout();
    btnCreate_ = new QPushButton(tr("Crear relación"), this);
    btnDelete_ = new QPushButton(tr("Eliminar relación"), this);
    btnSave_   = new QPushButton(tr("Guardar"), this);
    for (auto* b : {btnCreate_, btnDelete_, btnSave_}) {
        b->setMinimumHeight(32);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setFont(safeUiFontRegular());
    }
    btnRow->addWidget(btnCreate_);
    btnRow->addWidget(btnDelete_);
    btnRow->addWidget(btnSave_);
    rightLay->addLayout(btnRow);

    relationsGrid_ = new QTableWidget(this);
    relationsGrid_->setFont(safeUiFontRegular());
    relationsGrid_->setColumnCount(9);
    relationsGrid_->setHorizontalHeaderLabels(
        { tr("LeftTable"), tr("LeftField"), tr("RightTable"), tr("RightField"),
         tr("Tipo"), tr("RI"), tr("CUpd"), tr("CDel"), tr("Etiquetas") });
    relationsGrid_->horizontalHeader()->setStretchLastSection(true);
    relationsGrid_->verticalHeader()->setVisible(false);
    relationsGrid_->setSelectionBehavior(QAbstractItemView::SelectRows);
    relationsGrid_->setSelectionMode(QAbstractItemView::SingleSelection);
    relationsGrid_->setMinimumHeight(180);
    relationsGrid_->setMaximumHeight(220);
    for (int c=0;c<relationsGrid_->columnCount();++c)
        relationsGrid_->setColumnWidth(c, (c<=3)?95:60);
    rightLay->addWidget(relationsGrid_, 0);

    splitter->addWidget(leftPane);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 1);
    rightPane->setMaximumWidth(360);
    root->addWidget(splitter);

    view_->setScene(scene_);
    scene_->setSceneRect(QRectF(0,0,4000,2500));

    QList<int> sizes; sizes << 1000 << 340; splitter->setSizes(sizes);

    connect(btnCreate_, &QPushButton::clicked, this, &RelationDesignerPage::onBtnCreateRelation);
    connect(btnDelete_, &QPushButton::clicked, this, &RelationDesignerPage::onBtnDeleteRelation);
    connect(btnSave_,   &QPushButton::clicked, this, &RelationDesignerPage::onBtnSave);
}

QStringList RelationDesignerPage::allTablesInProject() const {
    QStringList out;
    QDir dir(projectDir_);
    const auto metas = dir.entryList(QStringList() << "*.meta" << "*.meta.json" << "*.meta.bin",
                                     QDir::Files, QDir::Name);
    for (const auto& m : metas) {
        QFileInfo fi(dir.absoluteFilePath(m));
        QString base = fi.completeBaseName();
        if (base.endsWith(".meta")) base.chop(5);
        out << base;
    }
    out.removeDuplicates();
    return out;
}

QStringList RelationDesignerPage::fieldsForTable(const QString& table) {
    if (!schemas_.contains(table)) {
        schemas_.insert(table, meta::readTableMeta(projectDir_, table));
    }
    const auto& tm = schemas_[table];
    auto names = meta::fieldNames(tm);
    if (names.isEmpty()) names = QStringList() << "ID" << "Campo1" << "Campo2";
    return names;
}

QString RelationDesignerPage::primaryKeyForTable(const QString& table) {
    if (!schemas_.contains(table)) {
        schemas_.insert(table, meta::readTableMeta(projectDir_, table));
    }
    const auto& tm = schemas_[table];
    if (!tm.pkName.isEmpty()) return tm.pkName;
    return QString();
}

quint16 RelationDesignerPage::typeIdFor(const QString& table, const QString& field) {
    if (!schemas_.contains(table)) {
        schemas_.insert(table, meta::readTableMeta(projectDir_, table));
    }
    const auto& tm = schemas_[table];
    return meta::fieldTypeId(tm, field);
}

void RelationDesignerPage::addTableBoxAt(const QString& table, const QPointF& scenePos) {
    if (table.isEmpty()) return;
    if (boxes_.contains(table)) { boxes_[table]->setPos(scenePos); return; }

    const auto names = fieldsForTable(table);
    const auto pk    = primaryKeyForTable(table);

    auto* box = new TableBox(this, table, names, pk);
    box->setPos(scenePos);
    scene_->addItem(box);
    boxes_.insert(table, box);
}

void RelationDesignerPage::onFieldClicked(FieldItem* fi) {
    if (fi->isPicked()) {
        fi->setPicked(false);
        selected_.removeAll(fi);
        return;
    }
    for (auto* other : selected_) {
        if (other->table() == fi->table()) {
            other->setPicked(false);
            selected_.removeAll(other);
            break;
        }
    }
    if (selected_.size() >= 2) {
        auto* oldest = selected_.front();
        oldest->setPicked(false);
        selected_.pop_front();
    }
    fi->setPicked(true);
    selected_.push_back(fi);
}

void RelationDesignerPage::clearFieldSelection() {
    for (auto* box : boxes_) for (auto* f : box->fieldItems()) f->setPicked(false);
    selected_.clear();
}

RelationDesignerPage::FieldItem* RelationDesignerPage::pickFirstSelected() const {
    return selected_.size() >= 1 ? selected_[0] : nullptr;
}
RelationDesignerPage::FieldItem* RelationDesignerPage::pickSecondSelected() const {
    return selected_.size() >= 2 ? selected_[1] : nullptr;
}

int RelationDesignerPage::findRelationIndex(const QString& lt, const QString& lf,
                                            const QString& rt, const QString& rf,
                                            const QString& type) const {
    for (int i=0;i<relations_.size();++i) {
        const auto& r = relations_[i];
        if (r.leftTable==lt && r.leftField==lf && r.rightTable==rt && r.rightField==rf && r.relType==type)
            return i;
    }
    return -1;
}

bool RelationDesignerPage::canFormRelation(const QString& type, const QString& lt, const QString& lf,
                                           const QString& rt, const QString& rf, QString& whyNot) const {
    if (!boxes_.contains(lt) || !boxes_.contains(rt)) { whyNot = tr("Ambas tablas deben estar en el lienzo."); return false; }
    if (lt == rt) { whyNot = tr("Las relaciones deben ser entre tablas distintas."); return false; }
    if (lt == rt && lf == rf) { whyNot = tr("No puedes relacionar un campo consigo mismo."); return false; }

    auto* self = const_cast<RelationDesignerPage*>(this);
    if (!self->schemas_.contains(lt)) self->schemas_.insert(lt, meta::readTableMeta(projectDir_, lt));
    if (!self->schemas_.contains(rt)) self->schemas_.insert(rt, meta::readTableMeta(projectDir_, rt));

    const quint16 tL = self->typeIdFor(lt, lf);
    const quint16 tR = self->typeIdFor(rt, rf);
    if (tL != 0xFFFF && tR != 0xFFFF && tL != tR) {
        whyNot = tr("Tipos incompatibles entre %1.%2 (typeId=%3) y %4.%5 (typeId=%6).")
        .arg(lt, lf).arg(tL).arg(rt, rf).arg(tR);
        return false;
    }

    const QString pkRight = self->primaryKeyForTable(rt);
    if (type == "1:N") {
        if (rf != pkRight) { whyNot = tr("En 1:N el campo del lado derecho debe ser la PK del padre."); return false; }
    } else if (type == "1:1") {
        if (rf != pkRight) { whyNot = tr("En 1:1 el campo del lado derecho debe ser PK."); return false; }
    } else if (type == "N:M") {
    }
    return true;
}

void RelationDesignerPage::updateRelationGeometry(VisualRelation& vr) {
    if (!vr.leftItem || !vr.rightItem) return;

    auto* boxL = boxes_.value(vr.leftTable, nullptr);
    auto* boxR = boxes_.value(vr.rightTable, nullptr);
    if (!boxL || !boxR) return;

    const QPointF pL = boxL->scenePos() + vr.leftItem->pos()  + QPointF(120, 11);
    const QPointF pR = boxR->scenePos() + vr.rightItem->pos() + QPointF(120, 11);

    if (!vr.line) vr.line = scene_->addLine(QLineF(pL,pR), QPen(Qt::darkGray, 1.6));
    else          vr.line->setLine(QLineF(pL, pR));

    if (!vr.labelLeft)  vr.labelLeft  = scene_->addText("", safeUiFontBold());
    if (!vr.labelRight) vr.labelRight = scene_->addText("", safeUiFontBold());

    QString labL = "1", labR = "1";
    if (vr.relType == "1:N") { labL = "N"; labR = "1"; }
    else if (vr.relType == "N:M") { labL = "N"; labR = "N"; }

    vr.labelLeft->setPlainText(labL);
    vr.labelRight->setPlainText(labR);
    vr.labelLeft->setDefaultTextColor(Qt::black);
    vr.labelRight->setDefaultTextColor(Qt::black);
    vr.labelLeft->setPos(pL + QPointF(-6, -18));
    vr.labelRight->setPos(pR + QPointF(-6, -18));
}

void RelationDesignerPage::updateRelationsForTable(const QString& tableName) {
    for (auto& vr : relations_) {
        if (vr.leftTable == tableName || vr.rightTable == tableName) {
            updateRelationGeometry(vr);
        }
    }
}

void RelationDesignerPage::drawRelation(VisualRelation& vr) {
    updateRelationGeometry(vr);
}

void RelationDesignerPage::addRelationToGrid(const VisualRelation& vr) {
    const int row = relationsGrid_->rowCount();
    relationsGrid_->insertRow(row);

    auto mkItem = [&](const QString& s){ auto* it = new QTableWidgetItem(s); it->setFlags(it->flags() ^ Qt::ItemIsEditable); return it; };
    auto mkChk  = [&](bool v){ auto* it = new QTableWidgetItem(); it->setFlags(it->flags() | Qt::ItemIsUserCheckable); it->setCheckState(v?Qt::Checked:Qt::Unchecked); return it; };

    relationsGrid_->setItem(row, 0, mkItem(vr.leftTable));
    relationsGrid_->setItem(row, 1, mkItem(vr.leftField));
    relationsGrid_->setItem(row, 2, mkItem(vr.rightTable));
    relationsGrid_->setItem(row, 3, mkItem(vr.rightField));
    relationsGrid_->setItem(row, 4, mkItem(vr.relType));
    relationsGrid_->setItem(row, 5, mkChk(vr.enforceRI));
    relationsGrid_->setItem(row, 6, mkChk(vr.cascadeUpdate));
    relationsGrid_->setItem(row, 7, mkChk(vr.cascadeDelete));

    QString labels = (vr.relType=="1:N") ? "N ↔ 1" : (vr.relType=="N:M") ? "N ↔ N" : "1 ↔ 1";
    relationsGrid_->setItem(row, 8, mkItem(labels));
}

void RelationDesignerPage::removeRelationVisualOnly(int idx) {
    if (idx < 0 || idx >= relations_.size()) return;
    auto& vr = relations_[idx];
    if (vr.labelLeft)  { scene_->removeItem(vr.labelLeft);  delete vr.labelLeft;  vr.labelLeft=nullptr; }
    if (vr.labelRight) { scene_->removeItem(vr.labelRight); delete vr.labelRight; vr.labelRight=nullptr; }
    if (vr.line)       { scene_->removeItem(vr.line);       delete vr.line;       vr.line=nullptr; }
}

void RelationDesignerPage::onBtnCreateRelation() {
    auto* a = pickFirstSelected();
    auto* b = pickSecondSelected();

    if (!a || !b) {
        QMessageBox::information(this, tr("Relaciones"),
                                 tr("Selecciona un campo de una tabla y un campo de otra (máx. 2)."));
        return;
    }
    if (a->table() == b->table()) {
        QMessageBox::warning(this, tr("Relaciones"), tr("Debe ser entre tablas distintas."));
        return;
    }

    QString type = cbRelType_->currentText();

    FieldItem *left = a, *right = b;
    if ((a->isPk() && !b->isPk()) || (!a->isPk() && b->isPk())) {
        if (a->isPk()) { left=b; right=a; } else { left=a; right=b; }
    }

    QString why;
    if (!canFormRelation(type, left->table(), left->name(), right->table(), right->name(), why)) {
        QMessageBox::warning(this, tr("Relaciones"), why);
        return;
    }

    if (type == "N:M") {
        QString junction, fkA, fkB;
        if (!ensureJunctionTable(left->table(), right->table(), junction, fkA, fkB)) {
            QMessageBox::critical(this, tr("Relaciones"), tr("No se pudo crear la tabla intermedia."));
            return;
        }

        auto* boxA = boxes_.value(left->table(),  nullptr);
        auto* boxB = boxes_.value(right->table(), nullptr);
        QPointF pos = (boxA && boxB) ? (boxA->pos() + (boxB->pos()-boxA->pos())*0.5 + QPointF(60,20))
                                     : QPointF(100,100);
        addTableBoxAt(junction, pos);

        VisualRelation r1;
        r1.leftTable  = junction;
        r1.leftField  = fkA;
        r1.rightTable = left->table();
        r1.rightField = primaryKeyForTable(left->table());
        r1.relType    = "1:N";
        r1.enforceRI     = cbEnforce_->isChecked();
        r1.cascadeUpdate = cbCascadeUpd_->isChecked();
        r1.cascadeDelete = cbCascadeDel_->isChecked();

        VisualRelation r2;
        r2.leftTable  = junction;
        r2.leftField  = fkB;
        r2.rightTable = right->table();
        r2.rightField = primaryKeyForTable(right->table());
        r2.relType    = "1:N";
        r2.enforceRI     = cbEnforce_->isChecked();
        r2.cascadeUpdate = cbCascadeUpd_->isChecked();
        r2.cascadeDelete = cbCascadeDel_->isChecked();

        auto* boxJ = boxes_.value(junction, nullptr);
        auto findFieldItem = [](TableBox* box, const QString& name)->FieldItem*{
            if (!box) return nullptr;
            for (auto* f : box->fieldItems()) if (f->name()==name) return f;
            return nullptr;
        };
        r1.leftItem  = findFieldItem(boxJ,   r1.leftField);
        r1.rightItem = findFieldItem(boxA,   r1.rightField);
        r2.leftItem  = findFieldItem(boxJ,   r2.leftField);
        r2.rightItem = findFieldItem(boxB,   r2.rightField);

        drawRelation(r1); relations_.push_back(r1); addRelationToGrid(r1);
        drawRelation(r2); relations_.push_back(r2); addRelationToGrid(r2);

        clearFieldSelection();
        return;
    }

    VisualRelation vr;
    vr.leftTable  = left->table();
    vr.leftField  = left->name();
    vr.rightTable = right->table();
    vr.rightField = right->name();
    vr.relType    = type;
    vr.enforceRI     = cbEnforce_->isChecked();
    vr.cascadeUpdate = cbCascadeUpd_->isChecked();
    vr.cascadeDelete = cbCascadeDel_->isChecked();
    vr.leftItem  = left;
    vr.rightItem = right;

    if (findRelationIndex(vr.leftTable, vr.leftField, vr.rightTable, vr.rightField, vr.relType) >= 0) {
        QMessageBox::information(this, tr("Relaciones"), tr("Esa relación ya existe."));
        return;
    }

    drawRelation(vr);
    relations_.push_back(vr);
    addRelationToGrid(vr);

    clearFieldSelection();
}

void RelationDesignerPage::onBtnDeleteRelation() {
    const int row = relationsGrid_->currentRow();
    if (row < 0 || row >= relations_.size()) return;
    removeRelationVisualOnly(row);
    relationsGrid_->removeRow(row);
    relations_.remove(row);
}

void RelationDesignerPage::onBtnSave() {
    if (!saveToJsonV2()) {
        QMessageBox::critical(this, tr("Relaciones"), tr("No se pudo guardar relations.json"));
        return;
    }
    QMessageBox::information(this, tr("Relaciones"), tr("Relaciones guardadas."));
}

bool RelationDesignerPage::writeMetaUtf8Len(const QString& tableName,
                                            const QVector<meta::FieldInfo>& fields)
{
    const QString metaPath = QDir(projectDir_).filePath(tableName + ".meta");
    QFile f(metaPath);
    if (!f.open(QIODevice::WriteOnly)) return false;

    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);

    quint32 magic = 0x4D455431u;
    quint16 ver   = 1;
    ds << magic << ver;

    const QByteArray tbytes = tableName.toUtf8();
    quint16 tlen = static_cast<quint16>(qMin<int>(tbytes.size(), 65535));
    ds << tlen;
    if (tlen > 0) ds.writeRawData(tbytes.constData(), tlen);

    quint16 n = static_cast<quint16>(qMin<int>(fields.size(), 65535));
    ds << n;

    for (quint16 i=0;i<n;++i) {
        const auto& fi = fields[i];
        const QByteArray fb = fi.name.toUtf8();
        quint16 flen = static_cast<quint16>(qMin<int>(fb.size(), 65535));
        ds << flen;
        if (flen > 0) ds.writeRawData(fb.constData(), flen);

        quint8  t = static_cast<quint8>(fi.typeId & 0xFF);
        quint16 s = fi.size;
        ds << t << s;
    }

    f.close();
    schemas_.insert(tableName, meta::readTableMeta(projectDir_, tableName));
    return true;
}

bool RelationDesignerPage::ensureJunctionTable(const QString& aTable, const QString& bTable,
                                               QString& junctionName,
                                               QString& fkAName, QString& fkBName)
{
    const QString pkA = primaryKeyForTable(aTable);
    const QString pkB = primaryKeyForTable(bTable);
    if (pkA.isEmpty() || pkB.isEmpty()) return false;

    const quint16 tA = typeIdFor(aTable, pkA);
    const quint16 tB = typeIdFor(bTable, pkB);
    const quint16 sA = 0;
    const quint16 sB = 0;

    junctionName = aTable + "_" + bTable + "_link";
    fkAName      = aTable + "_" + pkA;
    fkBName      = bTable + "_" + pkB;

    const QString metaPath = QDir(projectDir_).filePath(junctionName + ".meta");
    if (!QFile::exists(metaPath)) {
        QVector<meta::FieldInfo> fields;
        meta::FieldInfo f1; f1.name = fkAName; f1.typeId = tA; f1.size = sA;
        meta::FieldInfo f2; f2.name = fkBName; f2.typeId = tB; f2.size = sB;
        fields << f1 << f2;

        if (!writeMetaUtf8Len(junctionName, fields)) return false;
    }

    return true;
}

QString RelationDesignerPage::jsonPath() const { return QDir(projectDir_).filePath("relations.json"); }
bool RelationDesignerPage::migrateJsonIfNeeded() const { return migrateRelationsToV2(jsonPath()); }

bool RelationDesignerPage::saveToJsonV2() const {
    QJsonObject root; root.insert("version", 2);

    QJsonArray nodesArr;
    for (auto it = boxes_.cbegin(); it != boxes_.cend(); ++it) {
        const auto* box = it.value();
        const QPointF p = box->pos();
        QJsonObject o; o.insert("table", it.key()); o.insert("x", p.x()); o.insert("y", p.y());
        nodesArr.push_back(o);
    }
    root.insert("nodes", nodesArr);

    QJsonArray rels;
    for (const auto& vr : relations_) {
        QJsonObject r;
        r.insert("leftTable", vr.leftTable);
        r.insert("leftField", vr.leftField);
        r.insert("rightTable", vr.rightTable);
        r.insert("rightField", vr.rightField);
        r.insert("relType",   vr.relType);
        r.insert("enforceRI", vr.enforceRI);
        r.insert("cascadeUpdate", vr.cascadeUpdate);
        r.insert("cascadeDelete", vr.cascadeDelete);
        rels.push_back(r);
    }
    root.insert("relations", rels);

    return writeRelationsV2(jsonPath(), root);
}

bool RelationDesignerPage::loadFromJsonV2() {
    QJsonObject root; if (!readRelationsV2Object(jsonPath(), root)) return false;

    const auto nodes = root.value("nodes").toArray();
    for (const auto& v : nodes) {
        const QJsonObject o = v.toObject();
        const QString table = o.value("table").toString();
        const qreal x = o.value("x").toDouble(50.0);
        const qreal y = o.value("y").toDouble(50.0);
        if (!allTablesInProject().contains(table)) continue;
        addTableBoxAt(table, QPointF(x, y));
    }

    const auto rels = root.value("relations").toArray();
    for (const auto& v : rels) {
        const auto o  = v.toObject();
        VisualRelation vr;
        vr.leftTable  = o.value("leftTable").toString();
        vr.leftField  = o.value("leftField").toString();
        vr.rightTable = o.value("rightTable").toString();
        vr.rightField = o.value("rightField").toString();
        vr.relType    = o.value("relType").toString("1:N");
        vr.enforceRI  = o.value("enforceRI").toBool(false);
        vr.cascadeUpdate = o.value("cascadeUpdate").toBool(false);
        vr.cascadeDelete = o.value("cascadeDelete").toBool(false);

        auto* boxL = boxes_.value(vr.leftTable, nullptr);
        auto* boxR = boxes_.value(vr.rightTable, nullptr);
        if (!boxL || !boxR) continue;
        for (auto* f : boxL->fieldItems()) if (f->name()==vr.leftField)  vr.leftItem = f;
        for (auto* f : boxR->fieldItems()) if (f->name()==vr.rightField) vr.rightItem = f;
        if (!vr.leftItem || !vr.rightItem) continue;

        drawRelation(vr);
        relations_.push_back(vr);
        addRelationToGrid(vr);
    }
    return true;
}

void RelationDesignerPage::loadTablesList() {
    if (!tablesList_) return;

    tablesList_->clear();

    const auto tables = allTablesInProject();
    for (const auto& t : tables) {
        auto* it = new QListWidgetItem(t);
        it->setData(Qt::UserRole, t);
        tablesList_->addItem(it);
    }

    tablesList_->viewport()->setAcceptDrops(false);
    tablesList_->setDragEnabled(true);
    tablesList_->setDefaultDropAction(Qt::IgnoreAction);
    tablesList_->setDragDropMode(QAbstractItemView::DragOnly);
    tablesList_->setSelectionMode(QAbstractItemView::SingleSelection);
}

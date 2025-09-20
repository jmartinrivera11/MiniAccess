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
#include <QDir>
#include "../core/relations_io.h"
#include "../core/metadata.h"
#include "relationeditdialog.h"
#include "../core/table.h"

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

static inline QString basePathForTableName(const QString& projectDir, const QString& tableName) {
    return QDir(projectDir).filePath(tableName);
}

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
    connect(relationsGrid_, &QTableWidget::cellDoubleClicked,
            this, &RelationDesignerPage::onGridCellDoubleClicked);
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

quint16 RelationDesignerPage::sizeFor(const QString& table, const QString& field) {
    if (!schemas_.contains(table)) {
        schemas_.insert(table, meta::readTableMeta(projectDir_, table));
    }
    const auto& tm = schemas_[table];
    for (const auto& fi : tm.fields) {
        if (fi.name == field) return fi.size;
    }
    return 0;
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
    for (int i = 0; i < relations_.size(); ++i) {
        const auto& R = relations_[i];
        const bool sameDir = (R.leftTable == lt && R.leftField == lf &&
                              R.rightTable == rt && R.rightField == rf &&
                              R.relType == type);
        const bool reversed = (R.leftTable == rt && R.leftField == rf &&
                               R.rightTable == lt && R.rightField == lf &&
                               R.relType == type);
        if (sameDir || reversed) return i;
    }
    return -1;
}

bool RelationDesignerPage::canFormRelation(const QString& type, const QString& lt, const QString& lf,
                                           const QString& rt, const QString& rf, QString& whyNot) const {
    if (!boxes_.contains(lt) || !boxes_.contains(rt)) {
        whyNot = tr("Ambas tablas deben estar en el lienzo.");
        return false;
    }
    if (lt == rt) {
        whyNot = tr("Las relaciones deben ser entre tablas distintas.");
        return false;
    }
    if (lt == rt && lf == rf) {
        whyNot = tr("No puedes relacionar un campo consigo mismo.");
        return false;
    }

    if (findRelationIndex(lt, lf, rt, rf, type) >= 0) {
        whyNot = tr("Esa relación ya existe.");
        return false;
    }

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

    const quint16 sL = self->sizeFor(lt, lf);
    const quint16 sR = self->sizeFor(rt, rf);
    if (sL > 0 && sR > 0 && sL > sR) {
        whyNot = tr("Longitud incompatible: %1.%2(size=%3) no puede referenciar %4.%5(size=%6).")
        .arg(lt, lf).arg(sL).arg(rt, rf).arg(sR);
        return false;
    }

    const QString pkRight = self->primaryKeyForTable(rt);
    if (type == "1:N" || type == "1:1") {
        if (pkRight.isEmpty() || rf != pkRight) {
            whyNot = tr("El campo del lado derecho debe ser la PK de la tabla padre (%1.%2).")
            .arg(rt, pkRight.isEmpty()? QStringLiteral("<sin PK>") : pkRight);
            return false;
        }
    }

    if (type == "1:N" && lf != rf) {
        whyNot = tr("1:N requiere mismo nombre de campo: FK '%1' debe llamarse igual que la PK '%2'.")
        .arg(lf, rf);
        return false;
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

    const bool openA = tableIsOpen(a->table());
    const bool openB = tableIsOpen(b->table());
    if (openA || openB) {
        QString msg = tr("No se puede modificar relaciones mientras haya tablas abiertas.\nAbiertas: ");
        if (openA) msg += a->table();
        if (openA && openB) msg += ", ";
        if (openB) msg += b->table();
        QMessageBox::warning(this, tr("Relaciones"), msg);
        return;
    }

    QString type = cbRelType_ ? cbRelType_->currentText() : QStringLiteral("1:N");

    FieldItem *left = a, *right = b;
    if ((a->isPk() && !b->isPk()) || (!a->isPk() && b->isPk())) {
        if (a->isPk()) { left = b; right = a; } else { left = a; right = b; }
    }

    QString why;
    if (!canFormRelation(type, left->table(), left->name(), right->table(), right->name(), why)) {
        QMessageBox::warning(this, tr("Relaciones"), why);
        return;
    }

    if (type != "N:M") {
        QString whyData;
        if (!validarDatosExistentes(left->table(), left->name(),
                                    right->table(), right->name(),
                                    type, whyData)) {
            QMessageBox::warning(this, tr("Relaciones"), whyData);
            return;
        }
    }

    if (type == "N:M") {
        QString junction, fkA, fkB;
        if (!ensureJunctionTable(left->table(), right->table(), junction, fkA, fkB)) {
            QMessageBox::critical(this, tr("Relaciones"), tr("No se pudo crear la tabla intermedia."));
            return;
        }

        if (tableIsOpen(junction)) {
            QMessageBox::warning(this, tr("Relaciones"),
                                 tr("No se puede crear la relación mientras la tabla intermedia '%1' esté abierta.").arg(junction));
            return;
        }

        auto* boxA = boxes_.value(left->table(),  nullptr);
        auto* boxB = boxes_.value(right->table(), nullptr);
        QPointF pos = (boxA && boxB)
                          ? (boxA->pos() + (boxB->pos()-boxA->pos())*0.5 + QPointF(60,20))
                          : QPointF(100,100);
        addTableBoxAt(junction, pos);

        auto findFieldItem = [](TableBox* box, const QString& name)->FieldItem*{
            if (!box) return nullptr;
            for (auto* f : box->fieldItems()) if (f->name()==name) return f;
            return nullptr;
        };
        auto* boxJ = boxes_.value(junction, nullptr);

        VisualRelation r1;
        r1.leftTable  = junction;       r1.leftField  = fkA;
        r1.rightTable = left->table();  r1.rightField = primaryKeyForTable(left->table());
        r1.relType    = "1:N";
        r1.enforceRI     = cbEnforce_ && cbEnforce_->isChecked();
        r1.cascadeUpdate = cbCascadeUpd_ && cbCascadeUpd_->isChecked();
        r1.cascadeDelete = cbCascadeDel_ && cbCascadeDel_->isChecked();
        r1.leftItem  = findFieldItem(boxJ,   r1.leftField);
        r1.rightItem = findFieldItem(boxA,   r1.rightField);

        VisualRelation r2;
        r2.leftTable  = junction;       r2.leftField  = fkB;
        r2.rightTable = right->table(); r2.rightField = primaryKeyForTable(right->table());
        r2.relType    = "1:N";
        r2.enforceRI     = cbEnforce_ && cbEnforce_->isChecked();
        r2.cascadeUpdate = cbCascadeUpd_ && cbCascadeUpd_->isChecked();
        r2.cascadeDelete = cbCascadeDel_ && cbCascadeDel_->isChecked();
        r2.leftItem  = findFieldItem(boxJ,   r2.leftField);
        r2.rightItem = findFieldItem(boxB,   r2.rightField);

        if (findRelationIndex(r1.leftTable, r1.leftField, r1.rightTable, r1.rightField, r1.relType) < 0) {
            drawRelation(r1); relations_.push_back(r1); addRelationToGrid(r1);
        }
        if (findRelationIndex(r2.leftTable, r2.leftField, r2.rightTable, r2.rightField, r2.relType) < 0) {
            drawRelation(r2); relations_.push_back(r2); addRelationToGrid(r2);
        }
        clearFieldSelection();
        emit relacionCreada();
        return;
    }

    VisualRelation vr;
    vr.leftTable  = left->table();
    vr.leftField  = left->name();
    vr.rightTable = right->table();
    vr.rightField = right->name();
    vr.relType    = type;
    vr.enforceRI     = cbEnforce_ && cbEnforce_->isChecked();
    vr.cascadeUpdate = cbCascadeUpd_ && cbCascadeUpd_->isChecked();
    vr.cascadeDelete = cbCascadeDel_ && cbCascadeDel_->isChecked();
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
    emit relacionCreada();
}

void RelationDesignerPage::onBtnDeleteRelation() {
    if (!relationsGrid_) return;
    const int row = relationsGrid_->currentRow();
    if (row < 0 || row >= relations_.size()) {
        QMessageBox::information(this, tr("Relaciones"), tr("Selecciona una relación para eliminar."));
        return;
    }

    const auto& r = relations_[row];

    const bool openL = tableIsOpen(r.leftTable);
    const bool openR = tableIsOpen(r.rightTable);
    if (openL || openR) {
        QString msg = tr("No se puede eliminar relaciones mientras haya tablas abiertas.\nAbiertas: ");
        if (openL) msg += r.leftTable;
        if (openL && openR) msg += ", ";
        if (openR) msg += r.rightTable;
        QMessageBox::warning(this, tr("Relaciones"), msg);
        return;
    }

    if (QMessageBox::question(this, tr("Eliminar relación"),
                              tr("¿Eliminar la relación %1.%2 → %3.%4 (%5)?")
                                  .arg(r.leftTable, r.leftField, r.rightTable, r.rightField, r.relType))
        != QMessageBox::Yes) {
        return;
    }

    removeRelationVisualOnly(row);
    relations_.removeAt(row);
    relationsGrid_->removeRow(row);

    emit relacionEliminada();
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

bool RelationDesignerPage::validarDatosExistentes(const QString& tablaOrigen,  const QString& campoOrigen,
                                                  const QString& tablaDestino, const QString& campoDestino,
                                                  const QString& tipoRelacion, QString& whyNot) const
{
    const int colO = indiceColumna(tablaOrigen,  campoOrigen);
    const int colD = indiceColumna(tablaDestino, campoDestino);
    if (colO < 0 || colD < 0) {
        whyNot = tr("No se pudo localizar la columna en el esquema.");
        return false;
    }

    Rows filasO = rowProvider_ ? rowProvider_(tablaOrigen)  : readRowsFromStorage(tablaOrigen);
    Rows filasD = rowProvider_ ? rowProvider_(tablaDestino) : readRowsFromStorage(tablaDestino);

    auto trimUltimaVacia = [](Rows& vv) {
        if (vv.isEmpty()) return;
        const auto& ult = vv.last();
        bool vacia = true;
        for (const auto& x : ult) {
            if (x.isValid() && !x.toString().trimmed().isEmpty()) { vacia = false; break; }
        }
        if (vacia) vv.removeLast();
    };
    trimUltimaVacia(filasO);
    trimUltimaVacia(filasD);

    QSet<QString> clavesOrigen;
    clavesOrigen.reserve(filasO.size());
    for (const auto& f : filasO) {
        if (colO >= 0 && colO < f.size()) {
            const QString v = f[colO].toString().trimmed();
            if (!v.isEmpty()) clavesOrigen.insert(v);
        }
    }

    QSet<QString> usadosDestino;
    const bool esUnoAUno = (tipoRelacion == "1:1");

    for (const auto& f : filasD) {
        if (colD < 0 || colD >= f.size()) continue;
        const QString v = f[colD].toString().trimmed();

        if (v.isEmpty()) continue;

        if (!clavesOrigen.contains(v)) {
            whyNot = tr("No se puede crear la relación: existen valores en '%1.%2' que no están en '%3.%4'.")
                         .arg(tablaDestino, campoDestino, tablaOrigen, campoOrigen);
            return false;
        }

        if (esUnoAUno) {
            if (usadosDestino.contains(v)) {
                whyNot = tr("No se puede crear la relación 1:1: el valor '%1' se repite en '%2.%3'.")
                             .arg(v, tablaDestino, campoDestino);
                return false;
            }
            usadosDestino.insert(v);
        }
    }
    return true;
}

bool RelationDesignerPage::validarValorFK(const QString& tablaDestino,
                                          const QString& campoDestino,
                                          const QString& valor,
                                          QString* outError) const
{
    for (const auto& r : relations_) {
        if (!r.rightTable.compare(tablaDestino, Qt::CaseInsensitive) &&
            !r.rightField.compare(campoDestino, Qt::CaseInsensitive))
        {
            const int colO = indiceColumna(r.leftTable,  r.leftField);
            if (colO < 0) continue;

            Rows filasO = rowProvider_ ? rowProvider_(r.leftTable) : readRowsFromStorage(r.leftTable);

            const QString v = valor.trimmed();
            if (v.isEmpty()) return true;

            QSet<QString> clavesO;
            clavesO.reserve(filasO.size());
            for (const auto& f : filasO) {
                if (colO >= 0 && colO < f.size()) {
                    const QString vv = f[colO].toString().trimmed();
                    if (!vv.isEmpty()) clavesO.insert(vv);
                }
            }
            if (!clavesO.contains(v)) {
                if (outError) {
                    *outError = tr("No se puede agregar o cambiar el registro porque no existe un "
                                   "registro relacionado en la tabla '%1'.").arg(r.leftTable);
                }
                return false;
            }
        }
    }
    return true;
}

RelationDesignerPage::Rows RelationDesignerPage::readRowsFromStorage(const QString& table) const {
    Rows rows;

    auto it = schemas_.find(table);
    if (it == schemas_.end()) {
        const_cast<RelationDesignerPage*>(this)->schemas_.insert(table, meta::readTableMeta(projectDir_, table));
        it = const_cast<RelationDesignerPage*>(this)->schemas_.find(table);
    }
    const auto& tm = it.value();

    try {
        const QString base = basePathForTableName(projectDir_, table);
        ma::Table t; t.open(base.toStdString());

        for (const auto& rid : t.scanAll()) {
            auto recOpt = t.read(rid);
            if (!recOpt) continue;
            const auto& rec = *recOpt;

            Row row;
            row.reserve(static_cast<int>(tm.fields.size()));

            for (int i = 0; i < tm.fields.size(); ++i) {
                const auto& vopt = rec.values[i];
                if (!vopt.has_value()) { row.push_back(QVariant()); continue; }

                const auto& v  = vopt.value();
                QVariant qv;

                if (std::holds_alternative<int>(v)) {
                    qv = std::get<int>(v);
                } else if (std::holds_alternative<double>(v)) {
                    qv = std::get<double>(v);
                } else if (std::holds_alternative<bool>(v)) {
                    qv = std::get<bool>(v);
                } else if (std::holds_alternative<std::string>(v)) {
                    qv = QString::fromStdString(std::get<std::string>(v));
                } else if (std::holds_alternative<long long>(v)) {
                    qv = QVariant::fromValue<long long>(std::get<long long>(v));
                } else {
                    qv = QVariant();
                }

                row.push_back(qv);
            }
            rows.push_back(std::move(row));
        }
        t.close();
    } catch (...) {
    }

    return rows;
}

int RelationDesignerPage::indiceColumna(const QString& table, const QString& field) const {
    auto it = schemas_.find(table);
    if (it == schemas_.end()) return -1;
    const auto& tm = it.value();
    for (int i = 0; i < tm.fields.size(); ++i) {
        if (tm.fields[i].name.compare(field, Qt::CaseInsensitive) == 0) return i;
    }
    return -1;
}

void RelationDesignerPage::refreshGridRow(int idx) {
    if (!relationsGrid_ || idx < 0 || idx >= relations_.size()) return;
    const auto& r = relations_[idx];

    relationsGrid_->setItem(idx, 0, new QTableWidgetItem(r.leftTable));
    relationsGrid_->setItem(idx, 1, new QTableWidgetItem(r.leftField));
    relationsGrid_->setItem(idx, 2, new QTableWidgetItem(r.rightTable));
    relationsGrid_->setItem(idx, 3, new QTableWidgetItem(r.rightField));
    relationsGrid_->setItem(idx, 4, new QTableWidgetItem(r.relType));
    relationsGrid_->setItem(idx, 5, new QTableWidgetItem(r.enforceRI ? "Sí" : "No"));
    relationsGrid_->setItem(idx, 6, new QTableWidgetItem(r.cascadeUpdate ? "Sí" : "No"));
    relationsGrid_->setItem(idx, 7, new QTableWidgetItem(r.cascadeDelete ? "Sí" : "No"));
}

void RelationDesignerPage::onGridCellDoubleClicked(int row, int /*col*/) {
    if (row < 0 || row >= relations_.size()) return;
    auto current = relations_[row];

    const bool openL = tableIsOpen(current.leftTable);
    const bool openR = tableIsOpen(current.rightTable);
    if (openL || openR) {
        QString msg = tr("No se puede editar relaciones mientras haya tablas abiertas.\nAbiertas: ");
        if (openL) msg += current.leftTable;
        if (openL && openR) msg += ", ";
        if (openR) msg += current.rightTable;
        QMessageBox::warning(this, tr("Relaciones"), msg);
        return;
    }

    RelationEditDialog::Model m;
    m.leftTable  = current.leftTable;   m.leftField  = current.leftField;
    m.rightTable = current.rightTable;  m.rightField = current.rightField;
    m.relType        = (current.relType == "1:1") ? "1:1" : "1:N";
    m.enforceRI      = current.enforceRI;
    m.cascadeUpdate  = current.cascadeUpdate;
    m.cascadeDelete  = current.cascadeDelete;

    RelationEditDialog dlg(m, this);
    if (dlg.exec() != QDialog::Accepted) return;

    auto edited = dlg.result();

    QString why;
    if (!canFormRelation(edited.relType, current.leftTable, current.leftField,
                         current.rightTable, current.rightField, why)) {
        QMessageBox::warning(this, tr("Relaciones"), why);
        return;
    }

    QString whyData;
    if (!validarDatosExistentes(current.leftTable, current.leftField,
                                current.rightTable, current.rightField,
                                edited.relType, whyData)) {
        QMessageBox::warning(this, tr("Relaciones"), whyData);
        return;
    }

    current.relType       = edited.relType;
    current.enforceRI     = edited.enforceRI;
    current.cascadeUpdate = edited.cascadeUpdate;
    current.cascadeDelete = edited.cascadeDelete;

    if (current.labelLeft)  current.labelLeft->setPlainText( current.relType == "1:1" ? "1" : "N" );
    if (current.labelRight) current.labelRight->setPlainText("1");

    relations_[row] = current;
    refreshGridRow(row);
}

void RelationDesignerPage::removeTableBox(const QString& table) {
    auto it = boxes_.find(table);
    if (it == boxes_.end()) return;
    if (auto* box = it.value()) {
        scene_->removeItem(box);
        delete box;
    }
    boxes_.erase(it);
}

RelationDesignerPage::FieldItem* RelationDesignerPage::findFieldItem(TableBox* box, const QString& name) const {
    if (!box) return nullptr;
    for (auto* f : box->fieldItems()) {
        if (f->name().compare(name, Qt::CaseInsensitive) == 0) return f;
    }
    return nullptr;
}

void RelationDesignerPage::rebindPointersForTable(const QString& table) {
    auto* box = boxes_.value(table, nullptr);
    if (!box) return;
    for (auto& vr : relations_) {
        bool touched = false;
        if (vr.leftTable.compare(table, Qt::CaseInsensitive) == 0) {
            vr.leftItem = findFieldItem(box, vr.leftField);
            touched = true;
        }
        if (vr.rightTable.compare(table, Qt::CaseInsensitive) == 0) {
            auto* boxR = boxes_.value(vr.rightTable, nullptr);
            if (boxR) vr.rightItem = findFieldItem(boxR, vr.rightField);
            touched = true;
        }
        if (touched && vr.line) updateRelationGeometry(vr);
    }
}

bool RelationDesignerPage::canDeleteTable(const QString& table, QString* why) const {
    for (const auto& r : relations_) {
        if (r.leftTable.compare(table, Qt::CaseInsensitive) == 0 ||
            r.rightTable.compare(table, Qt::CaseInsensitive) == 0) {
            if (why) *why = tr("No se puede eliminar la tabla '%1' porque participa en relaciones. "
                          "Elimina primero las relaciones en la ventana de Relaciones.")
                           .arg(table);
            return false;
        }
    }
    return true;
}

bool RelationDesignerPage::canDeleteField(const QString& table, const QString& field, QString* why) const {
    for (const auto& r : relations_) {
        const bool hit =
            (r.leftTable.compare(table, Qt::CaseInsensitive) == 0  && r.leftField.compare(field, Qt::CaseInsensitive) == 0) ||
            (r.rightTable.compare(table, Qt::CaseInsensitive) == 0 && r.rightField.compare(field, Qt::CaseInsensitive) == 0);
        if (hit) {
            if (why) *why = tr("No se puede eliminar el campo '%1.%2' porque participa en relaciones. "
                          "Elimina primero las relaciones en la ventana de Relaciones.")
                           .arg(table, field);
            return false;
        }
    }
    return true;
}

bool RelationDesignerPage::applyRenameTable(const QString& oldName, const QString& newName, QString* why) {
    if (oldName.compare(newName, Qt::CaseInsensitive) == 0) return true;

    if (tableIsOpen(oldName) || tableIsOpen(newName)) {
        if (why) *why = tr("No se puede renombrar mientras haya pestañas abiertas de '%1' o '%2'.").arg(oldName, newName);
        return false;
    }

    if (boxes_.contains(newName)) {
        if (why) *why = tr("Ya existe una tabla visual con nombre '%1'.").arg(newName);
        return false;
    }

    QPointF pos(100,100);
    if (auto* oldBox = boxes_.value(oldName, nullptr)) pos = oldBox->pos();
    removeTableBox(oldName);

    if (schemas_.contains(oldName)) {
        auto tm = schemas_.take(oldName);
        schemas_.insert(newName, tm);
    } else {
        schemas_.insert(newName, meta::readTableMeta(projectDir_, newName));
    }

    addTableBoxAt(newName, pos);

    for (int i = 0; i < relations_.size(); ++i) {
        auto& r = relations_[i];
        bool touched = false;
        if (r.leftTable.compare(oldName, Qt::CaseInsensitive) == 0) { r.leftTable = newName; touched = true; }
        if (r.rightTable.compare(oldName, Qt::CaseInsensitive) == 0) { r.rightTable = newName; touched = true; }
        if (touched) {
            rebindPointersForTable(newName);
            refreshGridRow(i);
        }
    }

    saveToJsonV2();
    emit relationsChanged();
    return true;
}

bool RelationDesignerPage::applyRenameField(const QString& table, const QString& oldField, const QString& newField, QString* why) {
    if (oldField.compare(newField, Qt::CaseInsensitive) == 0) return true;

    if (tableIsOpen(table)) {
        if (why) *why = tr("No se puede renombrar el campo mientras la tabla '%1' esté abierta.").arg(table);
        return false;
    }

    schemas_.insert(table, meta::readTableMeta(projectDir_, table));

    QPointF pos(100,100);
    if (auto* box = boxes_.value(table, nullptr)) pos = box->pos();
    removeTableBox(table);
    addTableBoxAt(table, pos);

    for (int i = 0; i < relations_.size(); ++i) {
        auto& r = relations_[i];
        bool touched = false;
        if (r.leftTable.compare(table, Qt::CaseInsensitive) == 0 &&
            r.leftField.compare(oldField, Qt::CaseInsensitive) == 0) {
            r.leftField = newField; touched = true;
        }
        if (r.rightTable.compare(table, Qt::CaseInsensitive) == 0 &&
            r.rightField.compare(oldField, Qt::CaseInsensitive) == 0) {
            r.rightField = newField; touched = true;
        }
        if (touched) {
            rebindPointersForTable(table);
            refreshGridRow(i);
        }
    }

    saveToJsonV2();
    emit relationsChanged();
    return true;
}

void RelationDesignerPage::revalidateAllRelations() {
    QVector<int> toRemove;
    for (int i = 0; i < relations_.size(); ++i) {
        const auto& r = relations_[i];

        if (!schemas_.contains(r.leftTable))  schemas_.insert(r.leftTable,  meta::readTableMeta(projectDir_, r.leftTable));
        if (!schemas_.contains(r.rightTable)) schemas_.insert(r.rightTable, meta::readTableMeta(projectDir_, r.rightTable));

        const auto& lm = schemas_.value(r.leftTable);
        const auto& rm = schemas_.value(r.rightTable);

        const bool leftFieldOk  = std::any_of(lm.fields.begin(), lm.fields.end(),
                                             [&](const meta::FieldInfo& f){ return f.name.compare(r.leftField,  Qt::CaseInsensitive) == 0; });
        const bool rightFieldOk = std::any_of(rm.fields.begin(), rm.fields.end(),
                                              [&](const meta::FieldInfo& f){ return f.name.compare(r.rightField, Qt::CaseInsensitive) == 0; });

        if (!leftFieldOk || !rightFieldOk) {
            toRemove.push_back(i);
            continue;
        }

        QString why;
        if (!canFormRelation(r.relType, r.leftTable, r.leftField, r.rightTable, r.rightField, why)) {
            toRemove.push_back(i);
            continue;
        }
    }

    std::sort(toRemove.begin(), toRemove.end(), std::greater<int>());
    for (int row : toRemove) {
        removeRelationVisualOnly(row);
        relations_.removeAt(row);
        if (relationsGrid_) relationsGrid_->removeRow(row);
    }

    for (auto it = boxes_.cbegin(); it != boxes_.cend(); ++it) {
        rebindPointersForTable(it.key());
    }

    if (!toRemove.isEmpty()) {
        saveToJsonV2();
        emit relationsChanged();
    }
}

void RelationDesignerPage::refreshTableBox(const QString& table) {
    schemas_.insert(table, meta::readTableMeta(projectDir_, table));

    QPointF pos(100,100);
    if (auto* old = boxes_.value(table, nullptr)) pos = old->pos();

    removeTableBox(table);
    addTableBoxAt(table, pos);

    rebindPointersForTable(table);

    for (int i = 0; i < relations_.size(); ++i) {
        const auto& r = relations_[i];
        if (r.leftTable.compare(table, Qt::CaseInsensitive) == 0 ||
            r.rightTable.compare(table, Qt::CaseInsensitive) == 0) {
            refreshGridRow(i);
        }
    }

    for (auto& r : relations_) {
        if (r.leftTable.compare(table, Qt::CaseInsensitive) == 0 ||
            r.rightTable.compare(table, Qt::CaseInsensitive) == 0) {
            if (r.line) updateRelationGeometry(r);
        }
    }
}

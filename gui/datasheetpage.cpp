#include "DatasheetPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableView>
#include <QHeaderView>
#include <QLabel>
#include "../core/Table.h"
#include "TableModel.h"
#include "boolcheckdelegate.h"
#include "formatdelegate.h"
#include <QAbstractItemView>
#include <QCoreApplication>
#include <QPainter>
#include <QPixmap>
#include <QApplication>
#include <QFileInfo>
#include <qpainterpath.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QSaveFile>

using namespace ma;

static int fieldIndexByName(const ma::Schema& s, const QString& name) {
    for (int i = 0; i < (int)s.fields.size(); ++i) {
        if (QString::fromStdString(s.fields[i].name).compare(name, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

static inline QString pkSidecarPathForBase(const QString& basePath) {
    return basePath + ".pk.json";
}

static QString readPkNameForBase(const QString& basePath) {
    QFile f(pkSidecarPathForBase(basePath));
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return {};
    const auto doc = QJsonDocument::fromJson(f.readAll());
    const auto o   = doc.object();
    return o.value(QStringLiteral("primaryKey")).toString();
}

static bool writePkNameForBase(const QString& basePath, const QString& pkName) {
    QFile f(pkSidecarPathForBase(basePath));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QJsonObject o; o["primaryKey"] = pkName;
    QJsonDocument d(o);
    return f.write(d.toJson(QJsonDocument::Indented)) >= 0;
}

static QIcon makePkIconRuntime() {
    const QColor col(15,143,102);
    QPixmap pm(16,16); pm.fill(Qt::transparent);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(col, 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(6, 8), 3.5, 3.5);
    QPainterPath path; path.moveTo(9.5, 8); path.lineTo(14, 8);
    path.moveTo(12, 8); path.lineTo(12, 6.3);
    path.moveTo(13.3, 8); path.lineTo(13.3, 6.8);
    p.drawPath(path);
    return QIcon(pm);
}

static QIcon pkIcon() {
    QIcon ic(":/icons/icons/pk.svg");
    if (!ic.isNull()) return ic;
    return makePkIconRuntime();
}

static inline QString pkPathForBase(const QString& basePath) {
    return basePath + ".pk.json";
}

static inline QString loadPrimaryKeyNameForBase(const QString& basePath) {
    QFile f(pkPathForBase(basePath));
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return {};
    const auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    const auto o = doc.object();
    return o.value("primaryKey").toString();
}

static inline bool savePrimaryKeyNameForBase(const QString& basePath, const QString& name) {
    const QString path = pkPathForBase(basePath);
    if (name.trimmed().isEmpty()) {
        QFile::remove(path);
        return true;
    }
    QSaveFile sf(path);
    if (!sf.open(QIODevice::WriteOnly)) return false;
    QJsonObject o; o["primaryKey"] = name;
    QJsonDocument doc(o);
    sf.write(doc.toJson(QJsonDocument::Compact));
    return sf.commit();
}

DatasheetPage::DatasheetPage(const QString& basePath, QWidget* parent)
    : QWidget(parent), basePath_(basePath) {
    setupUi();

    table_ = std::make_unique<Table>();
    table_->open(basePath_.toStdString());

    model_ = new TableModel(table_.get(), basePath_, this);
    view_->setModel(model_);

    QFont f0 = view_->font();
    basePt_ = f0.pointSizeF();
    if (basePt_ <= 0) basePt_ = 10.0;
    zoom_ = 1.0;
    applyZoom();

    view_->setItemDelegate(new FormatDelegate(table_->getSchema(), view_));

    const auto& s = table_->getSchema();
    for (int c = 0; c < (int)s.fields.size(); ++c) {
        if (s.fields[c].type == ma::FieldType::Bool) {
            view_->setItemDelegateForColumn(c, new BoolCheckDelegate(view_));
        }
    }

    auto* hdr = view_->horizontalHeader();
    hdr->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(hdr, &QHeaderView::customContextMenuRequested,
            this, &DatasheetPage::onHeaderContextMenu);

    hdr->setSectionResizeMode(QHeaderView::Interactive);
    hdr->setSectionsMovable(true);
    hdr->setSectionsClickable(true);
    hdr->setStretchLastSection(false);

    refreshPkBadge();

    view_->setAlternatingRowColors(true);
    view_->verticalHeader()->setDefaultSectionSize(22);
    view_->horizontalHeader()->setHighlightSections(false);

    autoFitColumns(20);
    info_->setText(QString("Rows: %1").arg(model_->rowCount()));
}

void DatasheetPage::setupUi() {
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* top = new QWidget(this);
    auto* th  = new QHBoxLayout(top);
    th->setContentsMargins(0, 0, 0, 0);
    th->setSpacing(8);

    info_ = new QLabel("Rows: —", top);
    th->addWidget(info_);
    th->addStretch();

    view_ = new QTableView(this);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);

    lay->addWidget(top);
    lay->addWidget(view_, 1);
}

void DatasheetPage::insertRow() {
    if (!model_) return;
    const int newRow = model_->rowCount();
    model_->insertRows(newRow, 1);

    view_->selectRow(newRow);
    view_->scrollTo(model_->index(newRow, 0));

    info_->setText(QString("Rows: %1").arg(model_->rowCount()));
}

void DatasheetPage::deleteRows() {
    if (!model_) return;
    auto sel = view_->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;

    std::sort(sel.begin(), sel.end(), [](const QModelIndex& a, const QModelIndex& b){
        return a.row() > b.row();
    });
    for (const auto& idx : sel) model_->removeRows(idx.row(), 1);

    info_->setText(QString("Rows: %1").arg(model_->rowCount()));
}

void DatasheetPage::refresh() {
    if (!model_) return;
    model_->reload();
    autoFitColumns(20);
    info_->setText(QString("Rows: %1").arg(model_->rowCount()));
}

void DatasheetPage::applyZoom() {
    const double pt = std::clamp(basePt_ * zoom_, 6.0, 32.0);

    QFont f = view_->font();
    f.setPointSizeF(pt);
    view_->setFont(f);
    view_->horizontalHeader()->setFont(f);
    view_->verticalHeader()->setFont(f);

    const int rowh = QFontMetrics(f).height() + 6;
    view_->verticalHeader()->setDefaultSectionSize(rowh);
    autoFitColumns(20);
    view_->viewport()->update();
}

void DatasheetPage::setZoom(double factor) {
    zoom_ = std::clamp(factor, 0.5, 2.0);
    applyZoom();
}

void DatasheetPage::zoomInView() {
    setZoom(zoom_ + 0.10);
}

void DatasheetPage::zoomOutView() {
    setZoom(zoom_ - 0.10);
}

void DatasheetPage::prepareForClose() {
    if (view_) view_->setModel(nullptr);

    if (model_) {
        model_->disconnect();
        model_->deleteLater();
        model_ = nullptr;
    }

    if (table_) {
        table_->close();
        table_.reset();
    }

    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 5);
}

void DatasheetPage::refreshPkBadge() {
    if (model_) model_->notifyPrimaryKeyChanged();
}

void DatasheetPage::onHeaderContextMenu(const QPoint& pos) {
    auto* hdr = view_->horizontalHeader();
    const int logicalCol = hdr->logicalIndexAt(pos);
    if (logicalCol < 0) return;

    const QString diskPk = loadPrimaryKeyNameForBase(basePath_);
    if (model_->primaryKeyName().compare(diskPk, Qt::CaseInsensitive) != 0) {
        model_->setPrimaryKeyName(diskPk);
    }

    QMenu m(this);
    QAction* aSet   = m.addAction(tr("Set as Primary Key"));
    QAction* aClear = m.addAction(tr("Clear Primary Key"));

    const auto& s = table_->getSchema();
    const QString colName = QString::fromStdString(s.fields[logicalCol].name);
    const QString pkName  = model_->primaryKeyName();

    aSet->setEnabled(pkName.compare(colName, Qt::CaseInsensitive) != 0);
    aClear->setEnabled(!pkName.isEmpty());

    QAction* chosen = m.exec(hdr->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == aSet) {
        setPrimaryKeyColumn(logicalCol);
    } else if (chosen == aClear) {
        clearPrimaryKey();
    }
}

void DatasheetPage::setPrimaryKeyColumn(int logicalCol) {
    const auto& s = table_->getSchema();
    if (logicalCol < 0 || logicalCol >= (int)s.fields.size()) return;

    const QString colName = QString::fromStdString(s.fields[logicalCol].name);
    if (!savePrimaryKeyNameForBase(basePath_, colName)) {
        QMessageBox::warning(this, tr("Primary Key"),
                             tr("Could not save primary key file."));
        return;
    }
    model_->setPrimaryKeyName(colName);
}

void DatasheetPage::clearPrimaryKey() {
    if (!savePrimaryKeyNameForBase(basePath_, QString())) {
        QMessageBox::warning(this, tr("Primary Key"),
                             tr("Could not remove primary key file."));
        return;
    }
    model_->setPrimaryKeyName(QString());
}

void DatasheetPage::autoFitColumns(int extraPx) {
    if (!model_ || !view_) return;

    for (int c = 0; c < model_->columnCount(); ++c) {
        view_->resizeColumnToContents(c);
        int w = view_->columnWidth(c);
        view_->setColumnWidth(c, w + extraPx);
    }
}

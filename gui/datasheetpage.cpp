#include "DatasheetPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableView>
#include <QHeaderView>
#include <QLabel>

#include "../core/Table.h"
#include "TableModel.h"

#include "../core/DisplayFmt.h"
#include "delegates/CurrencyDelegate.h"
#include "delegates/DateTimeDelegate.h"
#include "booldelegate.h"

using namespace ma;

DatasheetPage::DatasheetPage(const QString& basePath, QWidget* parent)
    : QWidget(parent), basePath_(basePath) {
    setupUi();

    table_ = std::make_unique<Table>();
    table_->open(basePath_.toStdString());

    model_ = new TableModel(table_.get(), this);
    view_->setModel(model_);

    QFont f0 = view_->font();
    basePt_ = f0.pointSizeF();
    if (basePt_ <= 0) basePt_ = 10.0;
    zoom_ = 1.0;
    applyZoom();

    const auto& s = table_->getSchema();
    for (int c = 0; c < (int)s.fields.size(); ++c) {
        const auto& f = s.fields[c];
        if (f.type == FieldType::Double && isCurrencyFmt(f.size)) {
            view_->setItemDelegateForColumn(c, new CurrencyDelegate(view_));
            view_->setColumnWidth(c, 140);
        } else if (f.type == FieldType::String && isDateTimeFmt(f.size)) {
            view_->setItemDelegateForColumn(c, new DateTimeDelegate(view_));
            view_->setColumnWidth(c, 160);
        } else if (f.type == FieldType::Bool) {
            view_->setItemDelegateForColumn(c, new BoolDelegate(view_));
            view_->setColumnWidth(c, 80);
        }
    }

    view_->setAlternatingRowColors(true);
    view_->verticalHeader()->setDefaultSectionSize(22);
    view_->horizontalHeader()->setHighlightSections(false);

    view_->resizeColumnsToContents();
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
    view_->resizeColumnsToContents();
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
    view_->resizeColumnsToContents();
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

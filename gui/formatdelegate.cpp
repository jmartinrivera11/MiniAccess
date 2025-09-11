#include "formatdelegate.h"
#include <QTableWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QAbstractItemModel>
#include <algorithm>
#include "../core/DisplayFmt.h"

using namespace ma;

FormatDelegate::FormatDelegate(QTableWidget* owner, QObject* parent)
    : QStyledItemDelegate(parent), grid_(owner) {}

QString FormatDelegate::typeAt(const QModelIndex& index) const {
    if (!grid_) return {};
    const int row = index.row();
    if (auto* cb = qobject_cast<QComboBox*>(grid_->cellWidget(row, 2))) {
        return cb->currentText();
    }
    return index.sibling(row, 2).data(Qt::DisplayRole).toString();
}

QString FormatDelegate::labelFor(const QString& typeName, int code) {
    const QString t = typeName;

    if (t == "Yes/No") {
        switch (code) {
        case FMT_BOOL_TRUEFALSE: return "True/False";
        case FMT_BOOL_YESNO:     return "Yes/No";
        case FMT_BOOL_ONOFF:     return "On/Off";
        default: return "True/False";
        }
    }
    if (t == "Number") {
        switch (code) {
        case FMT_NUM_BYTE:  return "Byte";
        case FMT_NUM_INT16: return "Integer";
        case FMT_NUM_INT32: return "Long Integer";
        default: return "Long Integer";
        }
    }
    if (t == "Currency") {
        switch (code) {
        case FMT_CUR_LPS: return "Lempiras (L)";
        case FMT_CUR_USD: return "US Dollar ($)";
        case FMT_CUR_EUR: return "Euro (€)";
        default:          return "Currency";
        }
    }
    if (t == "Double") {
        return QString("%1 decimals").arg(std::max(0, code));
    }
    if (t == "Date/Time") {
        switch (code) {
        case FMT_DT_GENERAL:   return "General Date";
        case FMT_DT_LONGDATE:  return "Long Date";
        case FMT_DT_SHORTDATE: return "Short Date";
        case FMT_DT_LONGTIME:  return "Long Time";
        case FMT_DT_SHORTTIME: return "Short Time";
        default:               return "General Date";
        }
    }
    if (t == "CharN") {
        return QString("%1 chars").arg(std::max(1, code));
    }
    if (t == "Short Text") {
        return QString("%1 max").arg(std::max(1, code));
    }
    return {};
}

QWidget* FormatDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&,
                                      const QModelIndex& index) const {
    if (!grid_ || index.column() != 3) return nullptr;

    const QString t = typeAt(index);

    if (t == "Yes/No") {
        auto* cb = new QComboBox(parent);
        cb->addItem("True/False", FMT_BOOL_TRUEFALSE);
        cb->addItem("Yes/No",     FMT_BOOL_YESNO);
        cb->addItem("On/Off",     FMT_BOOL_ONOFF);
        return cb;
    }
    if (t == "Number") {
        auto* cb = new QComboBox(parent);
        cb->addItem("Byte",          FMT_NUM_BYTE);
        cb->addItem("Integer (16)",  FMT_NUM_INT16);
        cb->addItem("Long Integer",  FMT_NUM_INT32);
        return cb;
    }
    if (t == "Currency") {
        auto* cb = new QComboBox(parent);
        cb->addItem("Lempiras (L)",  FMT_CUR_LPS);
        cb->addItem("US Dollar ($)", FMT_CUR_USD);
        cb->addItem("Euro (€)",      FMT_CUR_EUR);
        return cb;
    }
    if (t == "Double") {
        auto* sp = new QSpinBox(parent);
        sp->setRange(0, 9);
        sp->setSuffix(" decimals");
        return sp;
    }
    if (t == "Date/Time") {
        auto* cb = new QComboBox(parent);
        cb->addItem("General Date", FMT_DT_GENERAL);
        cb->addItem("Long Date",    FMT_DT_LONGDATE);
        cb->addItem("Short Date",   FMT_DT_SHORTDATE);
        cb->addItem("Long Time",    FMT_DT_LONGTIME);
        cb->addItem("Short Time",   FMT_DT_SHORTTIME);
        return cb;
    }
    if (t == "CharN") {
        auto* sp = new QSpinBox(parent);
        sp->setRange(1, 255);
        sp->setSuffix(" chars");
        return sp;
    }
    if (t == "Short Text") {
        auto* sp = new QSpinBox(parent);
        sp->setRange(1, 255);
        sp->setSuffix(" max");
        return sp;
    }

    return nullptr;
}

void FormatDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    const int v = index.data(Qt::EditRole).toInt();
    if (auto* cb = qobject_cast<QComboBox*>(editor)) {
        int i = cb->findData(v);
        if (i < 0) i = 0;
        cb->setCurrentIndex(i);
        return;
    }
    if (auto* sp = qobject_cast<QSpinBox*>(editor)) {
        sp->setValue(v>0 ? v : sp->minimum());
        return;
    }
}

void FormatDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                  const QModelIndex& index) const {
    if (auto* cb = qobject_cast<QComboBox*>(editor)) {
        model->setData(index, cb->currentData(),  Qt::EditRole);
        model->setData(index, cb->currentText(),  Qt::DisplayRole);
        return;
    }
    if (auto* sp = qobject_cast<QSpinBox*>(editor)) {
        const int v = sp->value();
        model->setData(index, v, Qt::EditRole);
        model->setData(index, labelFor(typeAt(index), v), Qt::DisplayRole);
        return;
    }
}

void FormatDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& opt,
                                          const QModelIndex&) const {
    editor->setGeometry(opt.rect);
}

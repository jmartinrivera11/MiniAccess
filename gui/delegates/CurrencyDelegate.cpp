#include "CurrencyDelegate.h"
#include <QDoubleSpinBox>

QWidget* CurrencyDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const {
    auto* sp = new QDoubleSpinBox(parent);
    sp->setDecimals(2);
    sp->setRange(-1e12, 1e12);
    sp->setAlignment(Qt::AlignRight);
    return sp;
}

void CurrencyDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    auto* sp = qobject_cast<QDoubleSpinBox*>(editor);
    sp->setValue(index.model()->data(index, Qt::EditRole).toDouble());
}

void CurrencyDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    auto* sp = qobject_cast<QDoubleSpinBox*>(editor);
    model->setData(index, sp->value(), Qt::EditRole);
}

QString CurrencyDelegate::displayText(const QVariant& value, const QLocale& locale) const {
    return locale.toCurrencyString(value.toDouble());
}

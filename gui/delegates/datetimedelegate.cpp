#include "DateTimeDelegate.h"
#include <QDateTimeEdit>
#include <QDateTime>

QWidget* DateTimeDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const {
    auto* de = new QDateTimeEdit(parent);
    de->setDisplayFormat(kDispFmt);
    de->setCalendarPopup(true);
    return de;
}

void DateTimeDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    auto* de = qobject_cast<QDateTimeEdit*>(editor);
    const auto s = index.model()->data(index, Qt::EditRole).toString();
    QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
    if (!dt.isValid()) dt = QDateTime::fromString(s, kDispFmt);
    if (!dt.isValid()) dt = QDateTime::currentDateTime();
    de->setDateTime(dt);
}

void DateTimeDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    auto* de = qobject_cast<QDateTimeEdit*>(editor);
    const QString iso = de->dateTime().toString(Qt::ISODate);
    model->setData(index, iso, Qt::EditRole);
}

QString DateTimeDelegate::displayText(const QVariant& value, const QLocale& locale) const {
    Q_UNUSED(locale);
    QDateTime dt = QDateTime::fromString(value.toString(), Qt::ISODate);
    if (!dt.isValid()) return value.toString();
    return dt.toString(kDispFmt);
}

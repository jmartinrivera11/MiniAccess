#pragma once
#include <QStyledItemDelegate>

class DateTimeDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit DateTimeDelegate(QObject* parent=nullptr) : QStyledItemDelegate(parent) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    QString displayText(const QVariant& value, const QLocale& locale) const override;

private:
    const QString kDispFmt = "yyyy-MM-dd HH:mm";
};

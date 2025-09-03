#pragma once
#include <QStyledItemDelegate>
#include <QLocale>

class CurrencyDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit CurrencyDelegate(QObject* parent=nullptr) : QStyledItemDelegate(parent) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    QString displayText(const QVariant& value, const QLocale& locale) const override;
};

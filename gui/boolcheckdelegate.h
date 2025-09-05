#pragma once
#include <QStyledItemDelegate>

class BoolCheckDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override;
    bool editorEvent(QEvent* ev,
                     QAbstractItemModel* model,
                     const QStyleOptionViewItem& opt,
                     const QModelIndex& idx) override;
    QWidget* createEditor(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const override {
        return nullptr;
    }
};

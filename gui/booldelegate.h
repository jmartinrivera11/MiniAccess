#pragma once
#include <QStyledItemDelegate>

class BoolDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit BoolDelegate(QObject* parent=nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;
};

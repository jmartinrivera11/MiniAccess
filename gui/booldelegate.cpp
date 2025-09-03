#include "BoolDelegate.h"
#include <QApplication>
#include <QStyle>
#include <QMouseEvent>

static inline bool asBool(const QVariant& v) {
    if (v.typeId() == QMetaType::QString)
        return v.toString().compare("true", Qt::CaseInsensitive) == 0 || v.toInt() != 0;
    return v.toInt() != 0;
}

void BoolDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                         const QModelIndex& index) const {
    bool checked = asBool(index.data(Qt::DisplayRole));

    QStyleOptionButton cb;
    cb.state = QStyle::State_Enabled | (checked ? QStyle::State_On : QStyle::State_Off);
    cb.rect = QApplication::style()->subElementRect(QStyle::SE_CheckBoxIndicator, &cb);
    QRect r = option.rect;
    cb.rect.moveCenter(r.center());

    QApplication::style()->drawControl(QStyle::CE_CheckBox, &cb, painter);
}

bool BoolDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                               const QStyleOptionViewItem& option,
                               const QModelIndex& index) {
    if (event->type() == QEvent::MouseButtonRelease) {
        bool cur = asBool(model->data(index, Qt::DisplayRole));
        model->setData(index, !cur, Qt::EditRole);
        return true;
    }
    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key()==Qt::Key_Space || ke->key()==Qt::Key_Return || ke->key()==Qt::Key_Enter) {
            bool cur = asBool(model->data(index, Qt::DisplayRole));
            model->setData(index, !cur, Qt::EditRole);
            return true;
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

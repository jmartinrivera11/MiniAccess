#include "BoolCheckDelegate.h"

#include <QApplication>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QStyle>
#include <QStyleOptionButton>
#include <QAbstractItemModel>

void BoolCheckDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const {
    QStyleOptionViewItem base(opt);
    base.text.clear();
    QStyledItemDelegate::paint(p, base, idx);

    QStyleOptionButton cb;
    cb.state = QStyle::State_Enabled;
    if (opt.state & QStyle::State_Selected) cb.state |= QStyle::State_On;

    const int st = idx.data(Qt::CheckStateRole).toInt();
    cb.state &= ~QStyle::State_On;
    cb.state &= ~QStyle::State_Off;
    cb.state |= (st == Qt::Checked) ? QStyle::State_On : QStyle::State_Off;

    const int w = QApplication::style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, nullptr);
    const QSize ind(w, w);
    cb.rect = QStyle::alignedRect(opt.direction, Qt::AlignCenter, ind, opt.rect);

    QApplication::style()->drawControl(QStyle::CE_CheckBox, &cb, p, nullptr);
}

bool BoolCheckDelegate::editorEvent(QEvent* ev,
                                    QAbstractItemModel* model,
                                    const QStyleOptionViewItem&,
                                    const QModelIndex& idx) {
    if (!(idx.flags() & Qt::ItemIsUserCheckable)) return false;

    if (ev->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(ev);
        if (me->button() != Qt::LeftButton) return false;

        Qt::CheckState st = static_cast<Qt::CheckState>(idx.data(Qt::CheckStateRole).toInt());
        st = (st == Qt::Checked ? Qt::Unchecked : Qt::Checked);
        return model->setData(idx, st, Qt::CheckStateRole);
    }

    if (ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        if (ke->key() == Qt::Key_Space) {
            Qt::CheckState st = static_cast<Qt::CheckState>(idx.data(Qt::CheckStateRole).toInt());
            st = (st == Qt::Checked ? Qt::Unchecked : Qt::Checked);
            return model->setData(idx, st, Qt::CheckStateRole);
        }
    }

    return false;
}

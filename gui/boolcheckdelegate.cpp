#include "BoolCheckDelegate.h"

#include <QApplication>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QStyle>
#include <QStyleOptionButton>

static QRect indicatorRect(const QStyleOptionViewItem& opt) {
    QStyleOptionButton cbOpt;
    cbOpt.rect = opt.rect;
    cbOpt.state = QStyle::State_Enabled;
    QStyle* st = opt.widget ? opt.widget->style() : QApplication::style();
    return st->subElementRect(QStyle::SE_CheckBoxIndicator, &cbOpt, opt.widget);
}

void BoolCheckDelegate::paint(QPainter* p,
                              const QStyleOptionViewItem& opt,
                              const QModelIndex& idx) const {
    QStyledItemDelegate::paint(p, opt, idx);

    QStyle* st = opt.widget ? opt.widget->style() : QApplication::style();

    QStyleOptionButton cbOpt;
    cbOpt.state = QStyle::State_Enabled;
    const int stRole = idx.data(Qt::CheckStateRole).toInt();
    cbOpt.state |= (stRole == Qt::Checked) ? QStyle::State_On : QStyle::State_Off;
    cbOpt.rect = indicatorRect(opt);

    st->drawControl(QStyle::CE_CheckBox, &cbOpt, p, opt.widget);
}

bool BoolCheckDelegate::editorEvent(QEvent* ev,
                                    QAbstractItemModel* model,
                                    const QStyleOptionViewItem& opt,
                                    const QModelIndex& idx) {
    if (!(idx.flags() & Qt::ItemIsUserCheckable) || !idx.isValid())
        return false;

    if (ev->type() == QEvent::MouseButtonPress || ev->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(ev);
        if (me->button() != Qt::LeftButton) return false;

        if (!opt.rect.contains(me->pos())) return false;

        Qt::CheckState st = static_cast<Qt::CheckState>(idx.data(Qt::CheckStateRole).toInt());
        st = (st == Qt::Checked ? Qt::Unchecked : Qt::Checked);
        return model->setData(idx, st, Qt::CheckStateRole);
    }

    if (ev->type() == QEvent::MouseButtonDblClick) {
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

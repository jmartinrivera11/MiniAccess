#include "BoolCheckDelegate.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>

static inline QColor onBg()  { return QColor(15, 143, 102); }
static inline QColor offBg() { return QColor(230, 230, 230); }
static inline QColor border(){ return QColor(170, 170, 170); }

void BoolCheckDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt,
                              const QModelIndex& idx) const {
    const bool checked = (idx.data(Qt::CheckStateRole).toInt() == Qt::Checked);

    QRect r = opt.rect;
    const int sz = qMin(r.width(), r.height());
    QRect box = QRect(0,0, sz-8, sz-8);
    box.moveCenter(r.center());

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    p->setPen(border());
    p->setBrush(checked ? onBg() : offBg());
    p->drawRoundedRect(box, 4, 4);

    if (checked) {
        p->setPen(QPen(Qt::white, 2.0));
        const int m = 6;
        QPoint a(box.left() + m,              box.center().y());
        QPoint b(box.center().x() - m/2,      box.bottom() - m);
        QPoint c(box.right() - m,             box.top() + m);
        p->drawLine(a, b);
        p->drawLine(b, c);
    }

    p->restore();
}

bool BoolCheckDelegate::editorEvent(QEvent* ev, QAbstractItemModel* model,
                                    const QStyleOptionViewItem& opt,
                                    const QModelIndex& idx) {
    if (!idx.flags().testFlag(Qt::ItemIsUserCheckable)) return false;

    if (ev->type()==QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(ev);
        if (opt.rect.contains(me->pos())) {
            const int cur = idx.data(Qt::CheckStateRole).toInt();
            const int next = (cur==Qt::Checked) ? Qt::Unchecked : Qt::Checked;
            return model->setData(idx, next, Qt::CheckStateRole);
        }
    } else if (ev->type()==QEvent::KeyPress) {
        const auto key = static_cast<QKeyEvent*>(ev)->key();
        if (key==Qt::Key_Space || key==Qt::Key_Return || key==Qt::Key_Enter) {
            const int cur = idx.data(Qt::CheckStateRole).toInt();
            const int next = (cur==Qt::Checked) ? Qt::Unchecked : Qt::Checked;
            return model->setData(idx, next, Qt::CheckStateRole);
        }
    }
    return false;
}

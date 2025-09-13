#pragma once
#include <QStyledItemDelegate>
#include <QLocale>
#include "../core/Schema.h"
#include "../core/DisplayFmt.h"

class FormatDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit FormatDelegate(const ma::Schema& s, QObject* parent=nullptr)
        : QStyledItemDelegate(parent), schema_(s) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& opt,
                          const QModelIndex& index) const override;

    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;

    void paint(QPainter* p, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

private:
    const ma::Field& fieldFor(const QModelIndex& idx) const {
        return schema_.fields[idx.column()];
    }

    static QString fmtInteger(qlonglong v);
    static QString fmtDouble(double v, int decimals, const QLocale& loc);
    static QString fmtDateTime(uint16_t fmtCode, const QVariant& raw, const QLocale& loc);
    static inline void stripGroupSeparators(QString& s) {
        s.remove(','); s.remove('.'); s.remove(' ');
    }

private:
    ma::Schema schema_;
};

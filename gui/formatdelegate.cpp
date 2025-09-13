#include "FormatDelegate.h"

#include <QPainter>
#include <QLineEdit>
#include <QDoubleValidator>
#include <QIntValidator>
#include <QDateTimeEdit>
#include <QDateEdit>
#include <QTimeEdit>
#include <cmath>
#include <qapplication.h>

using namespace ma;

static inline QLocale us() { return QLocale(QLocale::English, QLocale::UnitedStates); }

QString FormatDelegate::fmtInteger(qlonglong v) {
    return us().toString(v);
}

QString FormatDelegate::fmtDouble(double v, int decimals, const QLocale& loc) {
    if (decimals < 0) decimals = 0;
    if (decimals > 19) decimals = 19;
    return loc.toString(v, 'f', decimals);
}

static inline QString shortTimePattern() { return "h:mm ap"; }
static inline QString longTimePattern()  { return "h:mm:ss ap"; }

QString FormatDelegate::fmtDateTime(uint16_t fmtCode, const QVariant& raw, const QLocale& loc) {
    auto formatOut = [&](const QDateTime& dt)->QString {
        switch (fmtCode) {
        case FMT_DT_GENERAL:    return loc.toString(dt, QLocale::ShortFormat);
        case FMT_DT_LONGDATE:  return loc.toString(dt.date(), QLocale::LongFormat);
        case FMT_DT_SHORTDATE: return loc.toString(dt.date(), QLocale::ShortFormat);
        case FMT_DT_LONGTIME:  return dt.time().toString(longTimePattern());
        case FMT_DT_SHORTTIME: return dt.time().toString(shortTimePattern());
        default:                return loc.toString(dt, QLocale::ShortFormat);
        }
    };

    if (raw.canConvert<QDateTime>()) {
        QDateTime dt = raw.toDateTime();
        if (dt.isValid()) return formatOut(dt);
    }
    if (raw.canConvert<QDate>()) {
        QDate d = raw.toDate();
        if (d.isValid()) {
            switch (fmtCode) {
            case FMT_DT_LONGDATE:  return loc.toString(d, QLocale::LongFormat);
            case FMT_DT_SHORTDATE: return loc.toString(d, QLocale::ShortFormat);
            default:                return loc.toString(QDateTime(d, QTime(0,0)), QLocale::ShortFormat);
            }
        }
    }
    if (raw.canConvert<QTime>()) {
        QTime t = raw.toTime();
        if (t.isValid()) {
            switch (fmtCode) {
            case FMT_DT_LONGTIME:  return t.toString(longTimePattern());
            case FMT_DT_SHORTTIME: return t.toString(shortTimePattern());
            default:                return t.toString(shortTimePattern());
            }
        }
    }

    const QString s = raw.toString();
    if (s.isEmpty()) return s;

    {
        const QDateTime iso = QDateTime::fromString(s, Qt::ISODate);
        if (iso.isValid()) return formatOut(iso);
    }

    {
        const QString dfShort = loc.dateTimeFormat(QLocale::ShortFormat);
        const QString dfLong  = loc.dateTimeFormat(QLocale::LongFormat);
        QDateTime dt = QDateTime::fromString(s, dfShort);
        if (dt.isValid()) return formatOut(dt);
        dt = QDateTime::fromString(s, dfLong);
        if (dt.isValid()) return formatOut(dt);
    }

    {
        QDate d = QDate::fromString(s, loc.dateFormat(QLocale::ShortFormat));
        if (!d.isValid()) d = QDate::fromString(s, loc.dateFormat(QLocale::LongFormat));
        if (!d.isValid()) d = QDate::fromString(s, Qt::ISODate);
        if (d.isValid()) {
            switch (fmtCode) {
            case FMT_DT_LONGDATE:  return loc.toString(d, QLocale::LongFormat);
            case FMT_DT_SHORTDATE: return loc.toString(d, QLocale::ShortFormat);
            default:                return loc.toString(QDateTime(d, QTime(0,0)), QLocale::ShortFormat);
            }
        }
    }

    {
        QTime t = QTime::fromString(s, longTimePattern());
        if (!t.isValid()) t = QTime::fromString(s, shortTimePattern());
        if (t.isValid()) {
            switch (fmtCode) {
            case FMT_DT_LONGTIME:  return t.toString(longTimePattern());
            case FMT_DT_SHORTTIME: return t.toString(shortTimePattern());
            default:                return t.toString(shortTimePattern());
            }
        }
    }

    return s;
}

QWidget* FormatDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&,
                                      const QModelIndex& index) const {
    const Field& f = fieldFor(index);

    if (f.type == FieldType::Bool) return nullptr;

    if (f.type == FieldType::String && !isDateTimeFmt(f.size)) {
        auto* e = new QLineEdit(parent);
        if (f.size > 0) e->setMaxLength(static_cast<int>(f.size));
        return e;
    }

    if (f.type == FieldType::CharN) {
        auto* e = new QLineEdit(parent);
        e->setMaxLength(static_cast<int>(f.size));
        return e;
    }

    if (f.type == FieldType::Int32) {
        auto* e = new QLineEdit(parent);
        e->setValidator(new QIntValidator(std::numeric_limits<int>::min(),
                                          std::numeric_limits<int>::max(), e));
        return e;
    }

    if (f.type == FieldType::Double && !isCurrencyFmt(f.size)) {
        auto* e = new QLineEdit(parent);
        auto* dv = new QDoubleValidator(e);
        int decimals = qBound(0, static_cast<int>(f.size), 19);
        dv->setDecimals(decimals);
        dv->setNotation(QDoubleValidator::StandardNotation);
        e->setValidator(dv);
        return e;
    }

    if (f.type == FieldType::Double && isCurrencyFmt(f.size)) {
        auto* e = new QLineEdit(parent);
        auto* dv = new QDoubleValidator(e);
        dv->setDecimals(2);
        dv->setNotation(QDoubleValidator::StandardNotation);
        e->setValidator(dv);
        return e;
    }

    if (isDateTimeFmt(f.size)) {
        switch (f.size) {
        case FMT_DT_GENERAL: {
            auto* e = new QDateTimeEdit(parent);
            e->setDisplayFormat(QLocale().dateTimeFormat(QLocale::ShortFormat));
            e->setCalendarPopup(true);
            return e;
        }
        case FMT_DT_LONGDATE: {
            auto* e = new QDateEdit(parent);
            e->setDisplayFormat(QLocale().dateFormat(QLocale::LongFormat));
            e->setCalendarPopup(true);
            return e;
        }
        case FMT_DT_SHORTDATE: {
            auto* e = new QDateEdit(parent);
            e->setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat));
            e->setCalendarPopup(true);
            return e;
        }
        case FMT_DT_LONGTIME: {
            auto* e = new QTimeEdit(parent);
            e->setDisplayFormat(longTimePattern());
            return e;
        }
        case FMT_DT_SHORTTIME: {
            auto* e = new QTimeEdit(parent);
            e->setDisplayFormat(shortTimePattern());
            return e;
        }
        default: {
            auto* e = new QDateTimeEdit(parent);
            e->setDisplayFormat(QLocale().dateTimeFormat(QLocale::ShortFormat));
            e->setCalendarPopup(true);
            return e;
        }
        }
    }

    return QStyledItemDelegate::createEditor(parent, {}, index);
}

void FormatDelegate::setEditorData(QWidget* ed, const QModelIndex& index) const {
    const Field& f   = fieldFor(index);
    const QVariant v = index.data(Qt::EditRole).isValid()
                           ? index.data(Qt::EditRole)
                           : index.data(Qt::DisplayRole);

    if (auto* le = qobject_cast<QLineEdit*>(ed)) {
        QString s = v.toString();
        if (f.type == FieldType::Int32) stripGroupSeparators(s);
        le->setText(s);
        return;
    }

    if (auto* de = qobject_cast<QDateEdit*>(ed)) {
        if (v.canConvert<QDate>()) de->setDate(v.toDate());
        else if (v.canConvert<QDateTime>()) de->setDate(v.toDateTime().date());
        else de->setDate(QDate::fromString(v.toString(), Qt::ISODate));
        return;
    }
    if (auto* te = qobject_cast<QTimeEdit*>(ed)) {
        if (v.canConvert<QTime>()) te->setTime(v.toTime());
        else if (v.canConvert<QDateTime>()) te->setTime(v.toDateTime().time());
        else te->setTime(QTime::fromString(v.toString(), Qt::ISODate));
        return;
    }
    if (auto* dte = qobject_cast<QDateTimeEdit*>(ed)) {
        if (v.canConvert<QDateTime>()) dte->setDateTime(v.toDateTime());
        else dte->setDateTime(QDateTime::fromString(v.toString(), Qt::ISODate));
        return;
    }

    QStyledItemDelegate::setEditorData(ed, index);
}

void FormatDelegate::setModelData(QWidget* ed, QAbstractItemModel* model,
                                  const QModelIndex& index) const {
    const Field& f = fieldFor(index);

    if (auto* le = qobject_cast<QLineEdit*>(ed)) {
        QString s = le->text().trimmed();

        if (f.type == FieldType::Int32) {
            stripGroupSeparators(s);
            model->setData(index, s, Qt::EditRole);
            return;
        }

        model->setData(index, s, Qt::EditRole);
        return;
    }

    const QLocale loc;
    if (auto* de = qobject_cast<QDateEdit*>(ed)) {
        const QDate d = de->date();
        if (f.size == FMT_DT_LONGDATE) model->setData(index, loc.toString(d, QLocale::LongFormat), Qt::EditRole);
        else                            model->setData(index, loc.toString(d, QLocale::ShortFormat), Qt::EditRole);
        return;
    }
    if (auto* te = qobject_cast<QTimeEdit*>(ed)) {
        const QTime t = te->time();
        if (f.size == FMT_DT_LONGTIME) model->setData(index, t.toString(longTimePattern()), Qt::EditRole);
        else                            model->setData(index, t.toString(shortTimePattern()), Qt::EditRole);
        return;
    }
    if (auto* dte = qobject_cast<QDateTimeEdit*>(ed)) {
        const QDateTime dt = dte->dateTime();
        model->setData(index, loc.toString(dt, QLocale::ShortFormat), Qt::EditRole);
        return;
    }

    QStyledItemDelegate::setModelData(ed, model, index);
}

void FormatDelegate::paint(QPainter* p, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const {
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const Field& f   = fieldFor(index);
    const QVariant v = index.data(Qt::DisplayRole);
    const QLocale loc;

    if (f.type == FieldType::Bool) {
        QStyledItemDelegate::paint(p, opt, index);
        return;
    }

    if (f.type == FieldType::Int32) {
        bool ok=false; const qlonglong n = v.toLongLong(&ok);
        if (ok) opt.text = fmtInteger(n);
    } else if (f.type == FieldType::Double && !isCurrencyFmt(f.size)) {
        bool ok=false; const double d = v.toDouble(&ok);
        if (ok) opt.text = fmtDouble(d, static_cast<int>(f.size), loc);
    } else if (isDateTimeFmt(f.size)) {
        opt.text = fmtDateTime(f.size, v, loc);
    } else {
        opt.text = v.toString();
    }

    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, p, opt.widget);
}

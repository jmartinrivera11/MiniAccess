#include "TableModel.h"
#include <QColor>
#include <QTableWidgetItem>
#include <QLocale>
#include <QDateTime>
#include <variant>
#include <optional>
#include <cstdint>
#include "../core/DisplayFmt.h"
#include <climits>
#include <algorithm>

using namespace ma;

static inline QString currencySymbol(uint16_t sz) {
    if (sz == FMT_CUR_LPS) return "L";
    if (sz == FMT_CUR_USD) return "$";
    if (sz == FMT_CUR_EUR) return "€";
    return QLocale().currencySymbol(QLocale::CurrencySymbol);
}

static QString formatCurrencyLocal(double v) {
    return QLocale().toString(v, 'f', 2);
}

static inline QString boolLabel(bool v, uint16_t fmt) {
    switch (fmt) {
    case FMT_BOOL_YESNO:     return v ? "Yes" : "No";
    case FMT_BOOL_ONOFF:     return v ? "On"  : "Off";
    case FMT_BOOL_TRUEFALSE:
    default:                 return v ? "True" : "False";
    }
}

TableModel::TableModel(Table* table, QObject* parent)
    : QAbstractTableModel(parent), table_(table), schema_(table->getSchema()) {
    reload();
}

void TableModel::reload() {
    beginResetModel();
    rids_.clear();
    cache_.clear();
    rids_ = table_->scanAll();
    cache_.reserve(rids_.size());
    for (const auto& rid : rids_) {
        auto rec = table_->read(rid);
        cache_.push_back(rec.value_or(Record::withFieldCount((int)schema_.fields.size())));
    }
    endResetModel();
}

int TableModel::rowCount(const QModelIndex&) const { return (int)rids_.size(); }
int TableModel::columnCount(const QModelIndex&) const { return (int)schema_.fields.size(); }

QVariant TableModel::headerData(int section, Qt::Orientation o, int role) const {
    if (role != Qt::DisplayRole) return {};
    if (o == Qt::Horizontal) {
        const auto& f = schema_.fields[section];
        QString name = QString::fromStdString(f.name);

        if (f.type == FieldType::Double) {
            if (isCurrencyFmt(f.size)) {
                QString cur = (f.size == FMT_CUR_LPS) ? "Lps"
                              : (f.size == FMT_CUR_USD) ? "USD"
                              : (f.size == FMT_CUR_EUR) ? "EUR" : "Cur";
                name += " (Currency " + cur + ")";
            } else if (isDoublePrecision(f.size)) {
                name += QString(" (%.%1f)").arg(f.size);
            }
        } else if (f.type == FieldType::String && isDateTimeFmt(f.size)) {
            QString d;
            switch (f.size) {
            case FMT_DT_GENERAL:   d = "General Date"; break;
            case FMT_DT_LONGDATE:  d = "Long Date";    break;
            case FMT_DT_SHORTDATE: d = "Short Date";   break;
            case FMT_DT_LONGTIME:  d = "Long Time";    break;
            case FMT_DT_SHORTTIME: d = "Short Time";   break;
            }
            if (!d.isEmpty()) name += " (" + d + ")";
        } else if (f.type == FieldType::Int32 && isNumberSubtype(f.size)) {
            QString nm = (f.size == FMT_NUM_BYTE)  ? "Byte" :
                             (f.size == FMT_NUM_INT16) ? "Int16" :
                             (f.size == FMT_NUM_INT32) ? "Int32" : "";
            if (!nm.isEmpty()) name += " (" + nm + ")";
        } else if (f.type == FieldType::Bool && isBoolFmt(f.size)) {
            QString b = (f.size == FMT_BOOL_YESNO) ? "Yes/No" :
                            (f.size == FMT_BOOL_ONOFF) ? "On/Off" : "True/False";
            name += " (" + b + ")";
        }
        return name;
    }
    return section + 1;
}

QVariant TableModel::toVariant(const std::optional<Value>& ov) const {
    if (!ov.has_value()) return {};
    const auto& var = *ov;

    if (std::holds_alternative<int32_t>(var))     return QVariant((int)std::get<int32_t>(var));
    if (std::holds_alternative<double>(var))      return QVariant(std::get<double>(var));
    if (std::holds_alternative<bool>(var))        return QVariant(std::get<bool>(var));
    if (std::holds_alternative<std::string>(var)) return QVariant(QString::fromStdString(std::get<std::string>(var)));
    if constexpr (std::is_same_v<long long, long long>) {
        if (std::holds_alternative<long long>(var))
            return QVariant::fromValue<qlonglong>((qlonglong)std::get<long long>(var));
    }
    return {};
}

Value TableModel::fromVariant(int col, const QVariant& qv) const {
    auto t = schema_.fields[col].type;
    switch (t) {
    case FieldType::Int32:  return (int32_t)qv.toInt();
    case FieldType::Double: return qv.toDouble();
    case FieldType::Bool: {
        const auto s = qv.toString();
        bool b = (s.compare("true", Qt::CaseInsensitive) == 0)
                 || (s.compare("yes",  Qt::CaseInsensitive) == 0)
                 || (s.compare("on",   Qt::CaseInsensitive) == 0)
                 || (qv.toInt() != 0);
        return b;
    }
    case FieldType::String: return qv.toString().toStdString();
    case FieldType::CharN: {
        std::string s = qv.toString().toStdString();
        uint16_t N = schema_.fields[col].size;
        if (N>0 && s.size()>N) s.resize(N);
        return s;
    }
    default: return std::string();
    }
}

QVariant TableModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid()) return {};
    const int row = idx.row();
    const int col = idx.column();
    const auto& f = schema_.fields[col];
    const auto& opt = cache_[row].values[col];

    if (f.type == ma::FieldType::Bool) {
        if (role == Qt::CheckStateRole) {
            if (!opt.has_value()) return Qt::Unchecked;
            const ma::Value& v = opt.value();
            bool b = false;
            if (std::holds_alternative<bool>(v)) b = std::get<bool>(v);
            else if (std::holds_alternative<int>(v)) b = (std::get<int>(v) != 0);
            return b ? Qt::Checked : Qt::Unchecked;
        }
        if (role == Qt::DisplayRole || role == Qt::EditRole) return {};
        return {};
    }

    if (role == Qt::DisplayRole) {
        if (!opt.has_value()) return {};

        const ma::Value& v = opt.value();

        if (f.type == ma::FieldType::Double && ma::isCurrencyFmt(f.size)) {
            double d = 0.0;
            if (std::holds_alternative<double>(v)) d = std::get<double>(v);
            else if (std::holds_alternative<int>(v)) d = static_cast<double>(std::get<int>(v));
            else if (std::holds_alternative<long long>(v)) d = static_cast<double>(std::get<long long>(v));

            // símbolo según formato
            QString sym;
            if (f.size == ma::FMT_CUR_LPS)      sym = "L ";
            else if (f.size == ma::FMT_CUR_USD) sym = "$ ";
            else if (f.size == ma::FMT_CUR_EUR) sym = "€ ";
            else                                 sym = QLocale().currencySymbol(QLocale::CurrencySymbol) + " ";

            return sym + QLocale().toString(d, 'f', 2);
        }

        if (f.type == ma::FieldType::String && ma::isDateTimeFmt(f.size)) {
            if (std::holds_alternative<std::string>(v))
                return QString::fromStdString(std::get<std::string>(v));
            return {};
        }

        if (f.type == ma::FieldType::Double) {
            double d = 0.0;
            if (std::holds_alternative<double>(v)) d = std::get<double>(v);
            else if (std::holds_alternative<int>(v)) d = static_cast<double>(std::get<int>(v));
            else if (std::holds_alternative<long long>(v)) d = static_cast<double>(std::get<long long>(v));
            int dec = std::clamp<int>(static_cast<int>(f.size), 0, 19);
            return QLocale().toString(d, 'f', dec);
        }

        if (std::holds_alternative<int>(v))         return std::get<int>(v);
        if (std::holds_alternative<long long>(v))   return static_cast<qlonglong>(std::get<long long>(v));
        if (std::holds_alternative<std::string>(v)) return QString::fromStdString(std::get<std::string>(v));
        return {};
    }

    if (role == Qt::EditRole) {
        if (!opt.has_value()) return {};
        const ma::Value& v = opt.value();

        if (f.type == ma::FieldType::Double) {
            if (std::holds_alternative<double>(v))      return std::get<double>(v);
            if (std::holds_alternative<int>(v))         return static_cast<double>(std::get<int>(v));
            if (std::holds_alternative<long long>(v))   return static_cast<double>(std::get<long long>(v));
        }
        if (f.type == ma::FieldType::String) {
            if (std::holds_alternative<std::string>(v)) return QString::fromStdString(std::get<std::string>(v));
        }
        if (f.type == ma::FieldType::Int32) {
            if (std::holds_alternative<int>(v))         return std::get<int>(v);
        }
        if (f.type == ma::FieldType::CharN) {
            if (std::holds_alternative<std::string>(v)) return QString::fromStdString(std::get<std::string>(v));
        }
        return {};
    }

    return {};
}

Qt::ItemFlags TableModel::flags(const QModelIndex& idx) const {
    Qt::ItemFlags fl = QAbstractTableModel::flags(idx);
    if (!idx.isValid()) return fl;

    const auto& f = schema_.fields[idx.column()];

    if (f.type == ma::FieldType::Bool) {
        fl |=  Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        fl &= ~Qt::ItemIsEditable;
        return fl;
    }

    fl |= Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    return fl;
}

static bool coerceValueForField(const ma::Field& f, const QVariant& in, ma::Value& out) {
    if ((in.typeId() == QMetaType::QString && in.toString().trimmed().isEmpty()) || !in.isValid()) {
        return true;
    }

    const QString str = in.toString().trimmed();

    switch (f.type) {
    case ma::FieldType::Int32: {
        bool ok=false;
        qlonglong n = str.isEmpty() ? in.toLongLong(&ok) : str.toLongLong(&ok, 10);
        if (!ok || n < INT_MIN || n > INT_MAX) return false;
        out = static_cast<int32_t>(n);
        return true;
    }
    case ma::FieldType::Double: {
        bool ok=false;
        QLocale loc;
        double d;
        if (str.isEmpty()) d = in.toDouble(&ok);
        else { d = loc.toDouble(str, &ok); if (!ok) d = str.toDouble(&ok); }
        if (!ok) return false;
        out = d;
        return true;
    }
    case ma::FieldType::Bool: {
        const QString v = str.toLower();
        if (v=="true" || v=="yes" || v=="si" || v=="on" || v=="1")  { out = true;  return true; }
        if (v=="false"|| v=="no"  || v=="off"|| v=="0")              { out = false; return true; }
        if (in.canConvert<bool>()) { out = in.toBool(); return true; }
        return false;
    }
    case ma::FieldType::String: {
        if (ma::isDateTimeFmt(f.size)) {
            static const char* const formats[] = {
                "yyyy-MM-dd HH:mm:ss", "yyyy-MM-dd HH:mm", "yyyy-MM-dd",
                "dd/MM/yyyy HH:mm:ss","dd/MM/yyyy HH:mm", "dd/MM/yyyy",
                "MM/dd/yyyy HH:mm:ss","MM/dd/yyyy HH:mm", "MM/dd/yyyy",
                "HH:mm:ss","HH:mm"
            };
            QDateTime dt;
            for (auto fmt : formats) {
                dt = QDateTime::fromString(str, fmt);
                if (dt.isValid()) break;
            }
            if (!dt.isValid()) dt = QDateTime::fromString(str, Qt::ISODate);
            if (!dt.isValid()) return false;
            out = dt.toString(Qt::ISODate).toStdString();
            return true;
        } else {
            out = str.toStdString();
            return true;
        }
    }
    case ma::FieldType::CharN: {
        if (f.size > 0 && str.size() > (int)f.size) return false;
        out = str.toStdString();
        return true;
    }
    default:
        return false;
    }
}

bool TableModel::setData(const QModelIndex& idx, const QVariant& v, int role) {
    if (!idx.isValid() || !table_) return false;

    const int row = idx.row();
    const int col = idx.column();
    const auto& f = schema_.fields[col];

    if (f.type == ma::FieldType::Bool && role == Qt::CheckStateRole) {
        ma::Record rec = cache_[row];
        const bool b = (v.toInt() == Qt::Checked);
        rec.values[col] = b;

        auto rid2 = table_->update(rids_[row], rec);
        if (!rid2) return false;

        rids_[row]  = *rid2;
        cache_[row] = rec;

        emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::CheckStateRole, Qt::EditRole});
        return true;
    }

    if (role != Qt::EditRole) return false;

    bool isNull = false;
    ma::Value coerced;
    if ((v.typeId() == QMetaType::QString && v.toString().trimmed().isEmpty()) || !v.isValid()) {
        isNull = true;
    } else {
        if (!coerceValueForField(f, v, coerced)) {
            return false;
        }
    }

    ma::Record rec = cache_[row];
    if (isNull) rec.values[col].reset();
    else        rec.values[col] = coerced;

    auto maybeNewRid = table_->update(rids_[row], rec);
    if (!maybeNewRid) return false;

    rids_[row]  = *maybeNewRid;
    cache_[row] = rec;

    emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::EditRole});
    return true;
}

bool TableModel::insertRows(int, int count, const QModelIndex&) {
    if (count<=0) return false;
    beginInsertRows(QModelIndex(), rowCount(), rowCount()+count-1);
    for (int i=0;i<count;i++) {
        Record rec = makeDefaultRecord();
        RID rid = table_->insert(rec);
        rids_.push_back(rid);
        cache_.push_back(rec);
    }
    endInsertRows();
    return true;
}

bool TableModel::removeRows(int row, int count, const QModelIndex&) {
    if (count<=0 || row<0 || row+count>rowCount()) return false;
    beginRemoveRows(QModelIndex(), row, row+count-1);
    for (int i=0;i<count;i++) table_->erase(rids_[row+i]);
    rids_.erase(rids_.begin()+row, rids_.begin()+row+count);
    cache_.erase(cache_.begin()+row, cache_.begin()+row+count);
    endRemoveRows();
    return true;
}

Record TableModel::makeDefaultRecord() const {
    Record r = Record::withFieldCount((int)schema_.fields.size());
    for (size_t i=0;i<schema_.fields.size();++i) {
        switch (schema_.fields[i].type) {
        case FieldType::Int32:  r.values[i] = int32_t(0); break;
        case FieldType::Double: r.values[i] = 0.0; break;
        case FieldType::Bool:   r.values[i] = false; break;
        case FieldType::String: r.values[i] = std::string(); break;
        case FieldType::CharN:  r.values[i] = std::string(); break;
        default: r.values[i] = std::string();
        }
    }
    return r;
}

#include "TableModel.h"
#include <QColor>
#include <QTableWidgetItem>
#include <QLocale>
#include <QDateTime>
#include <variant>
#include <optional>
#include <cstdint>
#include "../core/DisplayFmt.h"

using namespace ma;

static inline QString currencySymbol(uint16_t sz) {
    if (sz == FMT_CUR_LPS) return "L";
    if (sz == FMT_CUR_USD) return "$";
    if (sz == FMT_CUR_EUR) return "€";
    return QLocale().currencySymbol(QLocale::CurrencySymbol);
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
    const auto& f = schema_.fields[idx.column()];
    const auto& ov = cache_[idx.row()].values[idx.column()];

    if (role == Qt::EditRole) {
        return toVariant(ov);
    }

    if (role == Qt::DisplayRole) {
        if (f.type == FieldType::Double && ov.has_value()) {
            double d = 0.0;
            if (std::holds_alternative<double>(*ov)) d = std::get<double>(*ov);
            if (isCurrencyFmt(f.size)) {
                return QLocale().toCurrencyString(d, currencySymbol(f.size));
            } else if (isDoublePrecision(f.size)) {
                return QLocale().toString(d, 'f', f.size);
            }
        }
        if (f.type == FieldType::Bool && ov.has_value()) {
            if (std::holds_alternative<bool>(*ov)) {
                return boolLabel(std::get<bool>(*ov), f.size);
            }
        }
        if (f.type == FieldType::String && isDateTimeFmt(f.size) && ov.has_value()) {
            if (std::holds_alternative<std::string>(*ov))
                return QString::fromStdString(std::get<std::string>(*ov));
        }
        return toVariant(ov);
    }
    return {};
}

Qt::ItemFlags TableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
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
    if (role != Qt::EditRole || !idx.isValid()) return false;
    if (!table_) return false;

    const int row = idx.row();
    const int col = idx.column();
    const auto& f = schema_.fields[col];

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

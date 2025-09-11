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
#include <QFile>
#include <QTextStream>
#include <unordered_set>
#include <QStringConverter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>

using namespace ma;

struct Relation {
    QString childName;
    QString childField;
    QString parentName;
    QString parentField;
    bool cascadeDelete{false};
    bool cascadeUpdate{false};
};

static inline QString projectDirFromBase(const QString& basePath) {
    return QFileInfo(basePath).dir().absolutePath();
}

static inline QString basePathForTableName(const QString& projectDir, const QString& tableName) {
    return QDir(projectDir).filePath(tableName);
}

static QString loadPrimaryKeyNameForBase(const QString& basePath) {
    const QString fn = basePath + ".keys.json";
    QFile f(fn);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return {};
    const auto doc = QJsonDocument::fromJson(f.readAll());
    return doc.object().value("primaryKey").toString();
}

static QVector<Relation> loadAllRelationsInProjectForBase(const QString& basePath) {
    QVector<Relation> out;
    const QString relPath = QDir(projectDirFromBase(basePath)).filePath("relations.json");
    QFile f(relPath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return out;

    const auto arr = QJsonDocument::fromJson(f.readAll()).array();
    for (const auto& v : arr) {
        const auto o = v.toObject();
        Relation r;
        r.childName     = o.value("lt").toString();
        r.childField    = o.value("lf").toString();
        r.parentName    = o.value("rt").toString();
        r.parentField   = o.value("rf").toString();
        r.cascadeDelete = o.value("cascadeDelete").toBool();
        r.cascadeUpdate = o.value("cascadeUpdate").toBool();
        if (!r.childName.isEmpty() && !r.childField.isEmpty() &&
            !r.parentName.isEmpty() && !r.parentField.isEmpty()) {
            out.push_back(r);
        }
    }
    return out;
}

static QVector<Relation> relationsWhereBaseIsChild(const QString& basePath) {
    const QString myName = QFileInfo(basePath).fileName();
    QVector<Relation> out;
    for (const auto& r : loadAllRelationsInProjectForBase(basePath)) {
        if (r.childName.compare(myName, Qt::CaseInsensitive)==0) out.push_back(r);
    }
    return out;
}
static QVector<Relation> relationsWhereBaseIsParent(const QString& basePath) {
    const QString myName = QFileInfo(basePath).fileName();
    QVector<Relation> out;
    for (const auto& r : loadAllRelationsInProjectForBase(basePath)) {
        if (r.parentName.compare(myName, Qt::CaseInsensitive)==0) out.push_back(r);
    }
    return out;
}

static int fieldIndexByName(const ma::Schema& s, const QString& name) {
    for (int i=0;i<(int)s.fields.size();++i)
        if (QString::fromStdString(s.fields[i].name).compare(name, Qt::CaseInsensitive)==0)
            return i;
    return -1;
}

static bool valuesEqual(const std::optional<ma::Value>& a, const std::optional<ma::Value>& b) {
    if (!a.has_value() && !b.has_value()) return true;
    if (a.has_value() != b.has_value()) return false;
    const ma::Value& va = a.value();
    const ma::Value& vb = b.value();
    if (va.index()!=vb.index()) return false;

    if (std::holds_alternative<int>(va))           return std::get<int>(va)==std::get<int>(vb);
    if (std::holds_alternative<long long>(va))     return std::get<long long>(va)==std::get<long long>(vb);
    if (std::holds_alternative<double>(va))        return std::get<double>(va)==std::get<double>(vb);
    if (std::holds_alternative<bool>(va))          return std::get<bool>(va)==std::get<bool>(vb);
    if (std::holds_alternative<std::string>(va))   return std::get<std::string>(va)==std::get<std::string>(vb);
    return false;
}

static bool valueExistsInParent(const QString& basePathOfThis,
                                const Relation& rel,
                                const ma::Value& fkVal) {
    try {
        const QString pd    = projectDirFromBase(basePathOfThis);
        const QString pbase = basePathForTableName(pd, rel.parentName);
        ma::Table pt; pt.open(pbase.toStdString());
        const auto ps = pt.getSchema();
        const int col = fieldIndexByName(ps, rel.parentField);
        if (col < 0) return false;
        for (const auto& rid : pt.scanAll()) {
            auto rec = pt.read(rid);
            if (rec && valuesEqual(rec->values[col], fkVal)) return true;
        }
        return false;
    } catch (...) { return false; }
}

static void cascadeDeleteChildren(const QString& basePathOfThis,
                                  const Relation& rel,
                                  const ma::Value& parentKeyVal) {
    try {
        const QString pd    = projectDirFromBase(basePathOfThis);
        const QString cbase = basePathForTableName(pd, rel.childName);
        ma::Table ct; ct.open(cbase.toStdString());
        const auto cs = ct.getSchema();
        const int col = fieldIndexByName(cs, rel.childField);
        if (col < 0) return;
        auto rids = ct.scanAll();
        for (auto it = rids.rbegin(); it != rids.rend(); ++it) {
            auto rec = ct.read(*it);
            if (rec && valuesEqual(rec->values[col], parentKeyVal)) {
                ct.erase(*it);
            }
        }
        ct.close();
    } catch (...) {}
}

static void cascadeUpdateChildren(const QString& basePathOfThis,
                                  const Relation& rel,
                                  const ma::Value& oldParentKey,
                                  const ma::Value& newParentKey) {
    try {
        const QString pd    = projectDirFromBase(basePathOfThis);
        const QString cbase = basePathForTableName(pd, rel.childName);
        ma::Table ct; ct.open(cbase.toStdString());
        const auto cs = ct.getSchema();
        const int col = fieldIndexByName(cs, rel.childField);
        if (col < 0) return;
        auto rids = ct.scanAll();
        for (const auto& rid : rids) {
            auto rec = ct.read(rid);
            if (rec && valuesEqual(rec->values[col], oldParentKey)) {
                rec->values[col] = newParentKey;
                ct.update(rid, *rec);
            }
        }
        ct.close();
    } catch (...) {}
}

static QString pkSidecarPath(const QString& basePath) {
    return basePath + ".keys.json";
}

static QString relationsPathForProject(const QString& basePath) {
    return QDir(projectDirFromBase(basePath)).filePath("relations.json");
}

static inline void setUtf8(QTextStream& ts) {
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        ts.setEncoding(QStringConverter::Utf8);
    #else
        ts.setCodec("UTF-8");
    #endif
}

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
    : TableModel(table, QString(), parent) {}

TableModel::TableModel(Table* table, const QString& basePath, QObject* parent)
    : QAbstractTableModel(parent), table_(table), schema_(table->getSchema()), basePath_(basePath)
{
    std::vector<ma::RID> scanned = table_->scanAll();
    rids_ = mergeWithOrder(scanned);
    rebuildCacheFromRids();
}

void TableModel::reload() {
    beginResetModel();
    std::vector<ma::RID> scanned = table_->scanAll();
    rids_ = mergeWithOrder(scanned);
    rebuildCacheFromRids();
    endResetModel();
    saveOrder();
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
    if (!idx.isValid() || !table_) return {};

    const int row = idx.row();
    const int col = idx.column();
    if (row < 0 || row >= static_cast<int>(cache_.size())) return {};
    if (col < 0 || col >= static_cast<int>(schema_.fields.size())) return {};

    const auto& f = schema_.fields[col];
    const auto& opt = cache_[row].values[col];

    if (f.type == ma::FieldType::Bool) {
        if (role == Qt::CheckStateRole) {
            const bool b = (opt.has_value() ? std::get<bool>(opt.value()) : false);
            return b ? Qt::Checked : Qt::Unchecked;
        }
        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            return {};
        }
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
    if (!idx.isValid()) return Qt::NoItemFlags;

    Qt::ItemFlags fl = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    const int col = idx.column();
    if (col >= 0 && col < static_cast<int>(schema_.fields.size())) {
        const auto& f = schema_.fields[col];
        if (f.type == ma::FieldType::Bool) {
            fl |= Qt::ItemIsUserCheckable;
        } else {
            fl |= Qt::ItemIsEditable;
        }
    }
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
        const bool newB = (v.toInt() == Qt::Checked);

        ma::Record rec = cache_[row];
        rec.values[col] = newB;

        auto maybeNewRid = table_->update(rids_[row], rec);
        if (!maybeNewRid) return false;

        rids_[row]  = *maybeNewRid;
        cache_[row] = rec;

        emit dataChanged(idx, idx, {Qt::CheckStateRole, Qt::DisplayRole});
        saveOrder();
        return true;
    }

    if (role != Qt::EditRole) return false;

    ma::Value newVal = fromVariant(col, v);
    const bool setNull =
        (v.typeId() == QMetaType::QString && v.toString().trimmed().isEmpty()) || !v.isValid();

    const QString pkName = loadPrimaryKeyNameForBase(basePath_);
    const int pkCol = pkName.isEmpty() ? -1 : fieldIndexByName(schema_, pkName);
    const bool editingPK = (col == pkCol);

    if (editingPK) {
        if (setNull) return false;
        if (!pkWouldBeUnique(pkCol, std::optional<ma::Value>(newVal), row)) return false;
    }

    for (const auto& rel : relationsWhereBaseIsChild(basePath_)) {
        const int fkCol = fieldIndexByName(schema_, rel.childField);
        if (fkCol == col) {
            if (setNull) return false;
            if (!valueExistsInParent(basePath_, rel, newVal)) return false;
        }
    }

    std::optional<ma::Value> oldPkVal;
    if (editingPK) oldPkVal = cache_[row].values[pkCol];

    ma::Record rec = cache_[row];
    if (setNull) rec.values[col].reset();
    else         rec.values[col] = newVal;

    auto maybeNewRid = table_->update(rids_[row], rec);
    if (!maybeNewRid) return false;

    rids_[row]  = *maybeNewRid;
    cache_[row] = rec;

    if (editingPK && oldPkVal.has_value()) {
        for (const auto& rel : relationsWhereBaseIsParent(basePath_)) {
            if (rel.parentField.compare(pkName, Qt::CaseInsensitive) == 0) {
                if (rel.cascadeUpdate) {
                    cascadeUpdateChildren(basePath_, rel, oldPkVal.value(), newVal);
                } else {
                    bool hasChild = false;
                    try {
                        const QString pd    = projectDirFromBase(basePath_);
                        const QString cbase = basePathForTableName(pd, rel.childName);
                        ma::Table ct; ct.open(cbase.toStdString());
                        const int cCol = fieldIndexByName(ct.getSchema(), rel.childField);
                        for (const auto& crid : ct.scanAll()) {
                            auto childRec = ct.read(crid);
                            if (childRec && valuesEqual(childRec->values[cCol], oldPkVal)) {
                                hasChild = true; break;
                            }
                        }
                        ct.close();
                    } catch (...) {}

                    if (hasChild) {
                        ma::Record revert = cache_[row];
                        revert.values[pkCol] = oldPkVal;
                        table_->update(rids_[row], revert);
                        cache_[row] = revert;
                        emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::EditRole});
                        return false;
                    }
                }
            }
        }
    }

    emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::EditRole});
    saveOrder();
    return true;
}

bool TableModel::insertRows(int row, int count, const QModelIndex& parent) {
    Q_UNUSED(parent);
    if (!table_ || count <= 0) return false;

    beginInsertRows(QModelIndex(), rowCount(), rowCount()+count-1);

    for (int i=0; i<count; ++i) {
        ma::Record rec = makeDefaultRecord();
        auto rid = table_->insert(rec);
        rids_.push_back(rid);
        cache_.push_back(rec);
    }

    endInsertRows();
    saveOrder();
    return true;
}

bool TableModel::removeRows(int row, int count, const QModelIndex& parent) {
    Q_UNUSED(parent);
    if (!table_ || row < 0 || count <= 0 || row+count > (int)rids_.size()) return false;

    const QString pkName = loadPrimaryKeyNameForBase(basePath_);
    const int pkCol = pkName.isEmpty() ? -1 : fieldIndexByName(schema_, pkName);

    beginRemoveRows(QModelIndex(), row, row+count-1);

    for (int i=0; i<count; ++i) {
        const int r = row + (count-1-i);
        const auto rid = rids_[r];
        const auto rec = cache_[r];

        if (pkCol >= 0) {
            const auto& pkVal = rec.values[pkCol];
            for (const auto& rel : relationsWhereBaseIsParent(basePath_)) {
                if (rel.parentField.compare(pkName, Qt::CaseInsensitive)==0) {
                    bool hasChild = false;
                    try {
                        const QString pd    = projectDirFromBase(basePath_);
                        const QString cbase = basePathForTableName(pd, rel.childName);
                        ma::Table ct; ct.open(cbase.toStdString());
                        const int cCol = fieldIndexByName(ct.getSchema(), rel.childField);
                        for (const auto& crid : ct.scanAll()) {
                            auto childRec = ct.read(crid);
                            if (childRec && valuesEqual(childRec->values[cCol], pkVal)) { hasChild = true; break; }
                        }
                        ct.close();
                    } catch (...) {}

                    if (hasChild) {
                        if (rel.cascadeDelete) {
                            cascadeDeleteChildren(basePath_, rel, pkVal.value());
                        } else {
                            endRemoveRows();
                            return false;
                        }
                    }
                }
            }
        }

        table_->erase(rid);
        rids_.erase(rids_.begin() + r);
        cache_.erase(cache_.begin() + r);
    }

    endRemoveRows();
    saveOrder();
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

QString TableModel::orderFilePath() const {
    return basePath_.isEmpty() ? QString() : (basePath_ + ".ord");
}

std::vector<ma::RID> TableModel::loadOrder() const {
    std::vector<ma::RID> out;
    const QString fn = orderFilePath();
    if (fn.isEmpty() || !QFile::exists(fn)) return out;

    QFile f(fn);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return out;

    QTextStream ts(&f);
    setUtf8(ts);
    const QString magic = ts.readLine().trimmed();
    if (magic != "ORD1") return out;

    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line.isEmpty()) continue;
        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() != 2) continue;
        bool ok1=false, ok2=false;
        uint32_t pg = parts[0].toUInt(&ok1);
        uint16_t sl = parts[1].toUShort(&ok2);
        if (ok1 && ok2) out.push_back(ma::RID{pg, sl});
    }
    return out;
}

void TableModel::saveOrder() const {
    const QString fn = orderFilePath();
    if (fn.isEmpty()) return;

    QFile f(fn);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) return;

    QTextStream ts(&f);
    setUtf8(ts);
    ts << "ORD1\n";
    for (const auto& rid : rids_) {
        ts << rid.pageId << ' ' << rid.slotId << '\n';
    }
}

static inline quint64 packRid(uint32_t p, uint16_t s) {
    return (static_cast<quint64>(p) << 16) | s;
}

std::vector<ma::RID> TableModel::mergeWithOrder(const std::vector<ma::RID>& scanned) const {
    if (basePath_.isEmpty()) return scanned;

    std::unordered_set<quint64> present;
    present.reserve(scanned.size());
    for (const auto& r : scanned) present.insert(packRid(r.pageId, r.slotId));

    std::vector<ma::RID> out;
    auto ord = loadOrder();
    out.reserve(scanned.size());
    std::unordered_set<quint64> emitted;
    emitted.reserve(scanned.size());

    for (const auto& r : ord) {
        const quint64 k = packRid(r.pageId, r.slotId);
        if (present.count(k)) { out.push_back(r); emitted.insert(k); }
    }

    for (const auto& r : scanned) {
        const quint64 k = packRid(r.pageId, r.slotId);
        if (!emitted.count(k)) { out.push_back(r); emitted.insert(k); }
    }
    return out;
}

void TableModel::rebuildCacheFromRids() {
    cache_.clear();
    cache_.reserve(rids_.size());
    for (const auto& rid : rids_) {
        auto rec = table_->read(rid);
        cache_.push_back(rec.value_or(Record::withFieldCount((int)schema_.fields.size())));
    }
}

QString TableModel::loadPrimaryKeyNameForThisTable() const {
    const QString fn = pkSidecarPath(basePath_);
    QFile f(fn);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return {};
    const auto doc = QJsonDocument::fromJson(f.readAll());
    return doc.object().value("primaryKey").toString();
}

bool TableModel::pkWouldBeUnique(int pkCol, const std::optional<ma::Value>& candidate, int skipRow) const {
    if (pkCol < 0) return true;
    for (int r=0; r<(int)cache_.size(); ++r) {
        if (r == skipRow) continue;
        if (valuesEqual(cache_[r].values[pkCol], candidate)) return false;
    }
    return true;
}

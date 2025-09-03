#include "QueryModel.h"
#include <QLocale>
#include <QDateTime>
#include <algorithm>
#include <variant>

using namespace ma;

static QVariant valueToQVariant(const Value& var) {
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

QueryModel::QueryModel(QObject* parent) : QAbstractTableModel(parent) {}

bool QueryModel::run(const Spec& s, QString* err) {
    try {
        beginResetModel();
        table_ = std::make_unique<Table>();
        table_->open(s.basePath.toStdString());
        schema_ = table_->getSchema();

        proj_.clear();
        if (s.columns.empty()) {
            proj_.resize((int)schema_.fields.size());
            for (int i=0;i<(int)schema_.fields.size();++i) proj_[i] = i;
        } else {
            proj_ = s.columns;
        }

        conds_ = s.conds;

        rids_ = table_->scanAll();
        rows_.clear();
        rows_.reserve(rids_.size());
        for (const auto& rid : rids_) {
            auto recOpt = table_->read(rid);
            if (!recOpt) continue;
            const auto& rec = *recOpt;
            if (matchRecord(rec)) {
                Record out = Record::withFieldCount((int)proj_.size());
                for (int i=0;i<(int)proj_.size();++i) {
                    out.values[i] = rec.values[proj_[i]];
                }
                rows_.push_back(std::move(out));
            }
        }
        endResetModel();
        return true;
    } catch (const std::exception& ex) {
        if (err) *err = QString::fromUtf8(ex.what());
        return false;
    }
}

int QueryModel::rowCount(const QModelIndex&) const { return (int)rows_.size(); }
int QueryModel::columnCount(const QModelIndex&) const { return (int)proj_.size(); }

QVariant QueryModel::headerData(int section, Qt::Orientation o, int role) const {
    if (role != Qt::DisplayRole) return {};
    if (o == Qt::Horizontal) {
        int src = proj_.empty() ? section : proj_[section];
        if (src>=0 && src<(int)schema_.fields.size())
            return QString::fromStdString(schema_.fields[src].name);
        return QString("col%1").arg(section+1);
    }
    return section+1;
}

QVariant QueryModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || role!=Qt::DisplayRole) return {};
    const auto& rec = rows_[idx.row()];
    const auto& ov = rec.values[idx.column()];
    if (!ov.has_value()) return {};
    return valueToQVariant(*ov);
}

Qt::ItemFlags QueryModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant QueryModel::toVariant(const std::optional<Value>& ov) const {
    if (!ov.has_value()) return {};
    return valueToQVariant(*ov);
}

static int qCompare(const QVariant& a, const QVariant& b) {
    bool ok1=false, ok2=false;
    double da = a.toDouble(&ok1), db = b.toDouble(&ok2);
    if (ok1 && ok2) return (da<db?-1:(da>db?1:0));
    int r = QString::localeAwareCompare(a.toString(), b.toString());
    return (r<0?-1:(r>0?1:0));
}

bool QueryModel::matchOne(const Record& rec, const Cond& c) const {
    if (c.fieldIndex<0 || c.fieldIndex >= (int)schema_.fields.size()) return false;
    const auto& ov = rec.values[c.fieldIndex];
    const QVariant left = toVariant(ov);
    const QVariant& right = c.value;

    switch (c.op) {
    case Op::EQ:  return qCompare(left, right) == 0;
    case Op::NE:  return qCompare(left, right) != 0;
    case Op::LT:  return qCompare(left, right) < 0;
    case Op::LE:  return qCompare(left, right) <= 0;
    case Op::GT:  return qCompare(left, right) > 0;
    case Op::GE:  return qCompare(left, right) >= 0;
    case Op::CONTAINS: return left.toString().contains(right.toString(), Qt::CaseInsensitive);
    case Op::STARTS:   return left.toString().startsWith(right.toString(), Qt::CaseInsensitive);
    case Op::ENDS:     return left.toString().endsWith(right.toString(), Qt::CaseInsensitive);
    }
    return false;
}

bool QueryModel::matchRecord(const Record& rec) const {
    if (conds_.empty()) return true;
    bool acc = matchOne(rec, conds_[0]);
    for (size_t i=1;i<conds_.size();++i) {
        if (conds_[i-1].andWithNext)
            acc = acc && matchOne(rec, conds_[i]);
        else
            acc = acc || matchOne(rec, conds_[i]);
    }
    return acc;
}

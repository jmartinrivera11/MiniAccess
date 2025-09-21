#include "QueryModel.h"
#include <QLocale>
#include <QDateTime>
#include <algorithm>
#include <variant>
#include <limits>

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

        rows_.clear();
        rids_.clear();

        auto isIndexableOp = [](Op op)->bool {
            switch (op) { case Op::EQ: case Op::LT: case Op::LE: case Op::GT: case Op::GE: return true; default: return false; }
        };
        int idxCond = -1;
        for (int i=0;i<(int)conds_.size();++i) {
            if (conds_[i].fieldIndex>=0 && conds_[i].fieldIndex<(int)schema_.fields.size() && isIndexableOp(conds_[i].op)) {
                const auto& f = schema_.fields[conds_[i].fieldIndex];
                if (f.type == FieldType::Int32 || f.type == FieldType::String || f.type == FieldType::CharN) { idxCond = i; break; }
            }
        }

        if (idxCond == -1) {
            const auto all = table_->scanAll();
            rows_.reserve(all.size());
            for (const auto& rid : all) {
                auto rec = table_->read(rid);
                if (!rec) continue;
                if (matchRecord(*rec)) {
                    ma::Record row = ma::Record::withFieldCount((int)proj_.size());
                    for (int c=0;c<(int)proj_.size();++c) row.values[c] = (*rec).values[proj_[c]];
                    rows_.push_back(std::move(row));
                }
            }
            endResetModel();
            return true;
        }

        const Cond& c0 = conds_[idxCond];
        const int   fi = c0.fieldIndex;
        const auto& fld= schema_.fields[fi];

        int idxSecond = -1;
        for (int i=0;i<(int)conds_.size();++i) {
            if (i==idxCond) continue;
            if (conds_[i].fieldIndex==fi && isIndexableOp(conds_[i].op)) { idxSecond = i; break; }
        }

        std::vector<RID> candidates;

        if (fld.type == FieldType::Int32) {
            table_->createInt32Index(fi, "idx_" + schema_.fields[fi].name);

            auto toI32 = [](const QVariant& v)->int32_t { bool ok=false; int val = v.toInt(&ok); return ok?(int32_t)val:(int32_t)0; };

            if (idxSecond == -1) {
                switch (c0.op) {
                case Op::EQ: candidates = table_->findByInt32(fi, toI32(c0.value)); break;
                case Op::LT: candidates = table_->rangeByInt32(fi, std::numeric_limits<int32_t>::min(), toI32(c0.value)-1); break;
                case Op::LE: candidates = table_->rangeByInt32(fi, std::numeric_limits<int32_t>::min(), toI32(c0.value));   break;
                case Op::GT: candidates = table_->rangeByInt32(fi, toI32(c0.value)+1, std::numeric_limits<int32_t>::max()); break;
                case Op::GE: candidates = table_->rangeByInt32(fi, toI32(c0.value),   std::numeric_limits<int32_t>::max()); break;
                default: break;
                }
            } else {
                const Cond& c1 = conds_[idxSecond];
                int32_t lo = std::numeric_limits<int32_t>::min();
                int32_t hi = std::numeric_limits<int32_t>::max();
                auto apply = [&](const Cond& c){
                    switch (c.op) {
                    case Op::LT: hi = std::min<int32_t>(hi, toI32(c.value)-1); break;
                    case Op::LE: hi = std::min<int32_t>(hi, toI32(c.value));   break;
                    case Op::GT: lo = std::max<int32_t>(lo, toI32(c.value)+1); break;
                    case Op::GE: lo = std::max<int32_t>(lo, toI32(c.value));   break;
                    case Op::EQ: lo = hi = toI32(c.value); break;
                    default: break;
                    }
                };
                apply(c0); apply(c1);
                candidates = table_->rangeByInt32(fi, lo, hi);
            }
        } else if (fld.type == FieldType::String || fld.type == FieldType::CharN) {
            table_->createStringIndex(fi, "idx_" + schema_.fields[fi].name);
            auto toStd = [](const QVariant& v)->std::string { return v.toString().toStdString(); };

            if (idxSecond == -1) {
                switch (c0.op) {
                case Op::EQ: candidates = table_->findByString(fi, toStd(c0.value)); break;
                case Op::LT: candidates = table_->rangeByString(fi, std::string(), toStd(c0.value)); break;
                case Op::LE: candidates = table_->rangeByString(fi, std::string(), toStd(c0.value)); break;
                case Op::GT: candidates = table_->rangeByString(fi, toStd(c0.value), std::string(1, char(0x7f))); break;
                case Op::GE: candidates = table_->rangeByString(fi, toStd(c0.value), std::string(1, char(0x7f))); break;
                default: break;
                }
            } else {
                const Cond& c1 = conds_[idxSecond];
                std::string lo = "";
                std::string hi = std::string(1, char(0x7f));
                auto apply = [&](const Cond& c){
                    switch (c.op) {
                    case Op::LT: hi = std::min(hi, toStd(c.value)); break;
                    case Op::LE: hi = std::min(hi, toStd(c.value)); break;
                    case Op::GT: lo = std::max(lo, toStd(c.value)); break;
                    case Op::GE: lo = std::max(lo, toStd(c.value)); break;
                    case Op::EQ: lo = hi = toStd(c.value); break;
                    default: break;
                    }
                };
                apply(c0); apply(c1);
                candidates = table_->rangeByString(fi, lo, hi);
            }
        }

        std::sort(candidates.begin(), candidates.end(), [](const RID& a, const RID& b){
            return (a.pageId<b.pageId) || (a.pageId==b.pageId && a.slotId<b.slotId);
        });
        candidates.erase(std::unique(candidates.begin(), candidates.end(), [](const RID& a, const RID& b){
                             return a.pageId==b.pageId && a.slotId==b.slotId;
                         }), candidates.end());

        rows_.reserve(candidates.size());
        for (const auto& rid : candidates) {
            auto rec = table_->read(rid);
            if (!rec) continue;
            if (matchRecord(*rec)) {
                ma::Record row = ma::Record::withFieldCount((int)proj_.size());
                for (int c=0;c<(int)proj_.size();++c) row.values[c] = (*rec).values[proj_[c]];
                rows_.push_back(std::move(row));
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

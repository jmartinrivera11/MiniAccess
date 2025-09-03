#pragma once
#include <QAbstractTableModel>
#include <QVariant>
#include <QString>
#include <vector>
#include <optional>
#include <memory>
#include "../core/Schema.h"
#include "../core/Table.h"

class QueryModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum class Op {
        EQ, NE, LT, LE, GT, GE, CONTAINS, STARTS, ENDS
    };
    struct Cond {
        int fieldIndex = -1;
        Op  op = Op::EQ;
        QVariant value;
        bool andWithNext = true;
    };

    struct Spec {
        QString basePath;
        std::vector<int> columns;
        std::vector<Cond> conds;
    };

    explicit QueryModel(QObject* parent=nullptr);

    bool run(const Spec& s, QString* err=nullptr);

    // QAbstractTableModel
    int rowCount(const QModelIndex& = QModelIndex()) const override;
    int columnCount(const QModelIndex& = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;
    QVariant data(const QModelIndex& idx, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    const ma::Schema& schema() const { return schema_; }

private:
    QVariant toVariant(const std::optional<ma::Value>& ov) const;
    bool     matchRecord(const ma::Record& rec) const;
    bool     matchOne(const ma::Record& rec, const Cond& c) const;

private:
    std::unique_ptr<ma::Table> table_;
    ma::Schema schema_;
    std::vector<int> proj_;
    std::vector<Cond> conds_;
    std::vector<ma::RID> rids_;
    std::vector<ma::Record> rows_;
};

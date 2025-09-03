#pragma once
#include <QAbstractTableModel>
#include <vector>
#include <optional>
#include "../core/Table.h"

class TableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit TableModel(ma::Table* table, QObject* parent=nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

    void reload();

private:
    ma::Record makeDefaultRecord() const;
    QVariant toVariant(const std::optional<ma::Value>& ov) const;
    ma::Value fromVariant(int col, const QVariant& qv) const;

private:
    ma::Table* table_{nullptr};
    ma::Schema schema_{};
    std::vector<ma::RID> rids_;
    std::vector<ma::Record> cache_;
};

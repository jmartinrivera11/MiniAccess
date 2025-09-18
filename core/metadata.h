#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>

namespace meta {

struct FieldInfo {
    QString name;
    quint16 typeId{0};
    quint16 size{0};
    bool    isPk{false};
};

struct TableMeta {
    QString tableName;
    QVector<FieldInfo> fields;
    QString pkName;
};

TableMeta readTableMeta(const QString& projectDir, const QString& table);
QStringList fieldNames(const TableMeta& tm);
quint16     fieldTypeId(const TableMeta& tm, const QString& fieldName);

}

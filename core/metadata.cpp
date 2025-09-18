#include "metadata.h"
#include <QFile>
#include <QDir>
#include <QDataStream>
#include <QByteArray>
#include "pk_utils.h"

namespace meta {

static bool plausibleNames(const QStringList& names) {
    if (names.isEmpty() || names.size() > 512) return false;
    for (const auto& s : names) {
        if (s.isEmpty() || s.size() > 128) return false;
        for (const QChar& c : s) {
            ushort uc = c.unicode();
            if (uc < 0x20 && uc != 0x09) return false;
            if (uc == 0xFFFD) return false;
        }
    }
    return true;
}

static bool parseFormatQString(const QByteArray& raw, TableMeta& out) {
    QDataStream ds(raw);
    ds.setByteOrder(QDataStream::LittleEndian);

    quint32 magic=0; quint16 ver=0;
    ds >> magic >> ver;
    if (ds.status()!=QDataStream::Ok || magic!=0x4D455431u) return false;

    QString tname; ds >> tname; if (ds.status()!=QDataStream::Ok) return false;
    out.tableName = tname;

    quint16 n=0; ds >> n;
    if (ds.status()!=QDataStream::Ok || n==0 || n>1024) return false;

    QStringList names;
    QVector<FieldInfo> fields; fields.reserve(n);
    for (quint16 i=0;i<n;++i) {
        QString fname; quint8 ftype=0; quint16 fsize=0;
        ds >> fname; if (ds.status()!=QDataStream::Ok) return false;
        ds >> ftype; if (ds.status()!=QDataStream::Ok) return false;
        ds >> fsize; if (ds.status()!=QDataStream::Ok) return false;

        FieldInfo fi;
        fi.name   = fname;
        fi.typeId = static_cast<quint16>(ftype);
        fi.size   = fsize;
        fields.push_back(fi);
        names << fname;
    }
    if (!plausibleNames(names)) return false;

    out.fields = fields;
    return true;
}

static bool parseFormatUtf8Len(const QByteArray& raw, TableMeta& out) {
    QDataStream ds(raw);
    ds.setByteOrder(QDataStream::LittleEndian);

    quint32 magic=0; quint16 ver=0;
    ds >> magic >> ver;
    if (ds.status()!=QDataStream::Ok || magic!=0x4D455431u) return false;

    quint16 nameLen=0; ds >> nameLen; if (ds.status()!=QDataStream::Ok) return false;
    if (nameLen>0) {
        QByteArray nameBytes(nameLen, Qt::Uninitialized);
        ds.readRawData(nameBytes.data(), nameLen);
        if (ds.status()!=QDataStream::Ok) return false;
        out.tableName = QString::fromUtf8(nameBytes);
    }

    quint16 n=0; ds >> n;
    if (ds.status()!=QDataStream::Ok || n==0 || n>1024) return false;

    QStringList names;
    QVector<FieldInfo> fields; fields.reserve(n);
    for (quint16 i=0;i<n;++i) {
        quint16 flen=0; ds >> flen; if (ds.status()!=QDataStream::Ok) return false;
        QByteArray fnameBytes(flen, Qt::Uninitialized);
        if (flen>0) ds.readRawData(fnameBytes.data(), flen);
        if (ds.status()!=QDataStream::Ok) return false;

        quint8 ftype=0; ds >> ftype; if (ds.status()!=QDataStream::Ok) return false;
        quint16 fsize=0; ds >> fsize; if (ds.status()!=QDataStream::Ok) return false;

        FieldInfo fi;
        fi.name   = QString::fromUtf8(fnameBytes);
        fi.typeId = static_cast<quint16>(ftype);
        fi.size   = fsize;
        fields.push_back(fi);
        names << fi.name;
    }
    if (!plausibleNames(names)) return false;

    out.fields = fields;
    return true;
}

TableMeta readTableMeta(const QString& projectDir, const QString& table) {
    TableMeta tm;
    const QString metaPath = QDir(projectDir).filePath(table + ".meta");
    QFile f(metaPath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        tm.tableName = table;
        return tm;
    }
    const QByteArray raw = f.readAll();
    f.close();

    if (!parseFormatQString(raw, tm)) {
        tm = TableMeta{};
        if (!parseFormatUtf8Len(raw, tm)) {
            tm.tableName = table;
        }
    }
    tm.pkName = loadPk(QDir(projectDir).filePath(table));
    if (!tm.pkName.isEmpty()) {
        for (auto& fi : tm.fields)
            fi.isPk = (fi.name == tm.pkName);
    }
    return tm;
}

QStringList fieldNames(const TableMeta& tm) {
    QStringList out;
    out.reserve(tm.fields.size());
    for (const auto& f : tm.fields) out << f.name;
    return out;
}

quint16 fieldTypeId(const TableMeta& tm, const QString& fieldName) {
    for (const auto& f : tm.fields)
        if (f.name == fieldName) return f.typeId;
    return 0xFFFF;
}

}

#include "pk_utils.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

QString pkPathForBase(const QString& basePath) {
    return basePath + ".keys.json";
}

QString legacyPkPathForBase(const QString& basePath) {
    return basePath + ".pk.json";
}

static QString readPkFromFile(const QString& path) {
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return {};
    const auto doc = QJsonDocument::fromJson(f.readAll());
    const auto obj = doc.object();
    return obj.value(QStringLiteral("primaryKey")).toString();
}

static bool writePkToFileAtomic(const QString& path, const QString& pk) {
    if (pk.trimmed().isEmpty()) {
        QFile::remove(path);
        return true;
    }
    QSaveFile sf(path);
    if (!sf.open(QIODevice::WriteOnly)) return false;
    QJsonObject o; o.insert(QStringLiteral("primaryKey"), pk);
    QJsonDocument d(o);
    if (sf.write(d.toJson(QJsonDocument::Compact)) < 0) return false;
    return sf.commit();
}

bool migratePkIfNeeded(const QString& basePath) {
    const QString canonical = pkPathForBase(basePath);
    QFileInfo canInfo(canonical);
    if (canInfo.exists()) return true;

    const QString legacy = legacyPkPathForBase(basePath);
    QFileInfo legInfo(legacy);
    if (!legInfo.exists()) return true;

    const QString pk = readPkFromFile(legacy);
    if (pk.trimmed().isEmpty()) {
        QFile::remove(canonical);
        return true;
    }
    const bool ok = writePkToFileAtomic(canonical, pk);
    if (!ok) {
        qWarning() << "[pk_utils] Migration failed writing canonical file:" << canonical;
    }
    return ok;
}

QString loadPk(const QString& basePath) {
    migratePkIfNeeded(basePath);
    return readPkFromFile(pkPathForBase(basePath));
}

bool savePk(const QString& basePath, const QString& pkName) {
    const QString canonical = pkPathForBase(basePath);
    return writePkToFileAtomic(canonical, pkName);
}

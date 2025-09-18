#include "relations_io.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>

static QJsonDocument readJson(const QString& path) {
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return {};
    const auto bytes = f.readAll();
    f.close();
    QJsonParseError err{};
    auto doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError) return {};
    return doc;
}

bool readRelationsV2Object(const QString& path, QJsonObject& outRoot) {
    const auto doc = readJson(path);
    if (doc.isNull() || !doc.isObject()) return false;
    outRoot = doc.object();
    if (outRoot.value("version").toInt(0) != 2) return false;
    return true;
}

bool migrateRelationsToV2(const QString& path) {
    QFileInfo fi(path);
    if (!fi.exists()) return true;
    const auto doc = readJson(path);
    if (doc.isNull()) return true;

    if (doc.isObject()) {
        const auto obj = doc.object();
        if (obj.value("version").toInt(0) == 2) return true;

        QJsonObject root;
        root.insert("version", 2);
        if (obj.contains("nodes"))     root.insert("nodes", obj.value("nodes"));
        if (obj.contains("relations")) root.insert("relations", obj.value("relations"));
        return writeRelationsV2(path, root);
    }

    if (!doc.isArray()) return true;

    const auto arr = doc.array();
    QJsonArray relationsV2;
    for (const auto& v : arr) {
        const auto o  = v.toObject();
        const auto lt = o.value("lt").toString(o.value("leftTable").toString());
        const auto lf = o.value("lf").toString(o.value("leftField").toString());
        const auto rt = o.value("rt").toString(o.value("rightTable").toString());
        const auto rf = o.value("rf").toString(o.value("rightField").toString());
        const auto j  = o.value("join").toString(o.value("joinType").toString("INNER"));

        QJsonObject r;
        r.insert("leftTable", lt);
        r.insert("leftField", lf);
        r.insert("rightTable", rt);
        r.insert("rightField", rf);
        r.insert("joinType", j.isEmpty() ? "INNER" : j);
        r.insert("enforceRI", o.value("enforceRI").toBool(false));
        r.insert("cascadeUpdate", o.value("cascadeUpdate").toBool(false));
        r.insert("cascadeDelete", o.value("cascadeDelete").toBool(false));
        relationsV2.push_back(r);
    }

    QJsonObject root;
    root.insert("version", 2);
    root.insert("nodes", QJsonArray());
    root.insert("relations", relationsV2);
    return writeRelationsV2(path, root);
}

QJsonArray loadRelationsArrayFlexible(const QString& path) {
    const auto doc = readJson(path);
    if (doc.isArray()) return doc.array();
    if (doc.isObject()) {
        const auto root = doc.object();
        if (root.contains("relations")) return root.value("relations").toArray();
    }
    return {};
}

bool writeRelationsV2(const QString& path, const QJsonObject& rootV2) {
    QSaveFile sf(path);
    if (!sf.open(QIODevice::WriteOnly)) return false;
    QJsonDocument d(rootV2);
    const auto bytes = d.toJson(QJsonDocument::Indented);
    if (sf.write(bytes) != bytes.size()) return false;
    return sf.commit();
}

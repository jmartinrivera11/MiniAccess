#pragma once
#include <QString>
#include <QJsonArray>
#include <QJsonObject>

bool readRelationsV2Object(const QString& path, QJsonObject& outRoot);
bool migrateRelationsToV2(const QString& path);
QJsonArray loadRelationsArrayFlexible(const QString& path);
bool writeRelationsV2(const QString& path, const QJsonObject& rootV2);

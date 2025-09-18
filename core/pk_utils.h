#pragma once
#include <QString>

QString pkPathForBase(const QString& basePath);
QString legacyPkPathForBase(const QString& basePath);
bool migratePkIfNeeded(const QString& basePath);
QString loadPk(const QString& basePath);
bool savePk(const QString& basePath, const QString& pkName);

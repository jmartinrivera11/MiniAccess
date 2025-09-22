#pragma once
#include <QString>
#include <QStringList>
#include <QJsonObject>

namespace forms {

QStringList listForms(const QString& projectDir);
bool loadForm(const QString& projectDir, const QString& formName, QJsonObject& outDef);
bool saveOrUpdateForm(const QString& projectDir, const QJsonObject& def);
QJsonObject makeAutoFormDef(const QString& formName, const QString& baseTable, const QString& projectDir);
bool loadFormData(const QString& projectDir, const QString& formName, QJsonArray& outData);
bool saveFormData(const QString& projectDir, const QString& formName, const QJsonArray& data);

}

#include "forms_io.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "table.h"

namespace {
QString formsPath(const QString& proj){ return QDir(proj).filePath("forms.json"); }
QString formDataPath(const QString& proj, const QString& name){
    QDir d(proj); if (!d.exists("forms_data")) d.mkdir("forms_data");
    return d.filePath(QString("forms_data/%1.json").arg(name));
}

QJsonObject loadRoot(const QString& proj) {
    QFile f(formsPath(proj));
    if (!f.exists()) {
        QJsonObject root; root.insert("version", 1);
        root.insert("forms", QJsonArray{});
        return root;
    }
    if (!f.open(QIODevice::ReadOnly)) return {};
    const auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return {};
    return doc.object();
}

bool saveRoot(const QString& proj, const QJsonObject& root) {
    QFile f(formsPath(proj));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QJsonDocument doc(root);
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    return true;
}
}

namespace forms {

QStringList listForms(const QString& projectDir) {
    QJsonObject root = loadRoot(projectDir);
    QStringList out;
    const auto arr = root.value("forms").toArray();
    for (const auto& v : arr) {
        const auto o = v.toObject();
        out << o.value("name").toString();
    }
    out.sort(Qt::CaseInsensitive);
    return out;
}

bool loadForm(const QString& projectDir, const QString& formName, QJsonObject& outDef) {
    QJsonObject root = loadRoot(projectDir);
    const auto arr = root.value("forms").toArray();
    for (const auto& v : arr) {
        const auto o = v.toObject();
        if (o.value("name").toString().compare(formName, Qt::CaseInsensitive)==0) {
            outDef = o;
            return true;
        }
    }
    return false;
}

bool saveOrUpdateForm(const QString& projectDir, const QJsonObject& def) {
    if (!def.contains("name") || !def.contains("table")) return false;
    QJsonObject root = loadRoot(projectDir);
    QJsonArray arr = root.value("forms").toArray();

    const QString name = def.value("name").toString();
    bool replaced = false;
    for (int i=0; i<arr.size(); ++i) {
        QJsonObject o = arr.at(i).toObject();
        if (o.value("name").toString().compare(name, Qt::CaseInsensitive)==0) {
            arr.replace(i, def);
            replaced = true; break;
        }
    }
    if (!replaced) arr.push_back(def);
    root.insert("forms", arr);
    return saveRoot(projectDir, root);
}

QJsonObject makeAutoFormDef(const QString& formName, const QString& baseTable, const QString& projectDir) {
    QJsonObject def;
    def.insert("name", formName);
    def.insert("table", baseTable);
    def.insert("version", 1);
    QJsonArray ctrls;

    try {
        ma::Table t; t.open(QDir(projectDir).filePath(baseTable).toStdString());
        ma::Schema s = t.getSchema();
        t.close();

        int y = 10;
        for (const auto& f : s.fields) {
            const QString fname = QString::fromStdString(f.name);
            QString type = "TextBox";
            switch (f.type) {
            case ma::FieldType::Bool:   type = "CheckBox"; break;
            case ma::FieldType::Date:   type = "DateEdit"; break;
            case ma::FieldType::Double:
            case ma::FieldType::Int32:  type = "TextBox"; break;
            case ma::FieldType::String:
            case ma::FieldType::CharN:  type = "TextBox"; break;
            default: type = "TextBox"; break;
            }
            QJsonObject c;
            c.insert("id", fname);
            c.insert("type", type);
            c.insert("field", fname);
            QJsonObject r; r.insert("x", 16); r.insert("y", y); r.insert("w", 320); r.insert("h", 28);
            c.insert("rect", r);
            ctrls.push_back(c);
            y += 36;
        }
    } catch (...) {
    }
    def.insert("controls", ctrls);
    return def;
}

bool loadFormData(const QString& projectDir, const QString& formName, QJsonArray& outData) {
    QFile f(formDataPath(projectDir, formName));
    if (!f.exists()) { outData = QJsonArray{}; return true; }
    if (!f.open(QIODevice::ReadOnly)) return false;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isArray()) return false;
    outData = doc.array();
    return true;
}

bool saveFormData(const QString& projectDir, const QString& formName, const QJsonArray& data) {
    QFile f(formDataPath(projectDir, formName));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QJsonDocument doc(data);
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

}

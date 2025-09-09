#include "DesignPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QStackedWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QFileInfo>
#include <QMessageBox>
#include <QSignalBlocker>
#include "../core/Table.h"
#include "../core/DisplayFmt.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cctype>

using namespace ma;

static QStringList typeNames() {
    return {"Short Text","Number","Yes/No","Double","Currency","Date/Time","CharN"};
}

static FieldType typeToCore(const QString& n) {
    if (n=="Short Text") return FieldType::String;
    if (n=="Number")     return FieldType::Int32;
    if (n=="Yes/No")     return FieldType::Bool;
    if (n=="Double")     return FieldType::Double;
    if (n=="Currency")   return FieldType::Double;
    if (n=="Date/Time")  return FieldType::String;
    if (n=="CharN")      return FieldType::CharN;
    return FieldType::String;
}

static QString coreToTypeName(const Field& f) {
    if (f.type==FieldType::Double && isCurrencyFmt(f.size)) return "Currency";
    if (f.type==FieldType::String && isDateTimeFmt(f.size)) return "Date/Time";
    switch (f.type) {
    case FieldType::String: return "Short Text";
    case FieldType::Int32:  return "Number";
    case FieldType::Bool:   return "Yes/No";
    case FieldType::Double: return "Double";
    case FieldType::CharN:  return "CharN";
    default: return "Short Text";
    }
}

static int defaultCodeForType(const QString& t) {
    const QString s = t.toLower();
    if (s=="yes/no")  return FMT_BOOL_TRUEFALSE;
    if (s=="currency")return FMT_CUR_LPS;
    if (s=="number")  return FMT_NUM_INT32;
    if (s=="double")  return 2;
    if (s=="date/time") return FMT_DT_GENERAL;
    if (s=="charn")   return 16;
    return 0;
}

// --- Helpers de conversión: Value -> tipos nativos ---
static bool toInt32(const ma::Value& v, int32_t& out) {
    if (std::holds_alternative<int>(v)) { out = std::get<int>(v); return true; }
    if (std::holds_alternative<long long>(v)) { long long x = std::get<long long>(v);
        if (x < INT_MIN || x > INT_MAX) return false; out = (int32_t)x; return true; }
    if (std::holds_alternative<double>(v)) { out = static_cast<int32_t>(std::get<double>(v)); return true; }
    if (std::holds_alternative<bool>(v)) { out = std::get<bool>(v) ? 1 : 0; return true; }
    if (std::holds_alternative<std::string>(v)) {
        try { out = static_cast<int32_t>(std::stoll(std::get<std::string>(v))); return true; } catch (...) { return false; }
    }
    return false;
}

static bool toDouble(const ma::Value& v, double& out) {
    if (std::holds_alternative<double>(v)) { out = std::get<double>(v); return true; }
    if (std::holds_alternative<int>(v)) { out = static_cast<double>(std::get<int>(v)); return true; }
    if (std::holds_alternative<long long>(v)) { out = static_cast<double>(std::get<long long>(v)); return true; }
    if (std::holds_alternative<bool>(v)) { out = std::get<bool>(v) ? 1.0 : 0.0; return true; }
    if (std::holds_alternative<std::string>(v)) {
        try { out = std::stod(std::get<std::string>(v)); return true; } catch (...) { return false; }
    }
    return false;
}

static bool toBool(const ma::Value& v, bool& out) {
    if (std::holds_alternative<bool>(v)) { out = std::get<bool>(v); return true; }
    if (std::holds_alternative<int>(v)) { out = std::get<int>(v) != 0; return true; }
    if (std::holds_alternative<long long>(v)) { out = std::get<long long>(v) != 0; return true; }
    if (std::holds_alternative<double>(v)) { out = (std::get<double>(v) != 0.0); return true; }
    if (std::holds_alternative<std::string>(v)) {
        std::string s = std::get<std::string>(v);
        std::string t; t.reserve(s.size());
        for (char c : s) t.push_back(std::tolower(static_cast<unsigned char>(c)));
        if (t=="true" || t=="yes" || t=="si" || t=="on" || t=="1") { out = true;  return true; }
        if (t=="false"|| t=="no"  || t=="off"|| t=="0")             { out = false; return true; }
        return false;
    }
    return false;
}

static bool toString(const ma::Value& v, std::string& out) {
    if (std::holds_alternative<std::string>(v)) { out = std::get<std::string>(v); return true; }
    if (std::holds_alternative<int>(v)) { out = std::to_string(std::get<int>(v)); return true; }
    if (std::holds_alternative<long long>(v)) { out = std::to_string(std::get<long long>(v)); return true; }
    if (std::holds_alternative<double>(v)) { out = std::to_string(std::get<double>(v)); return true; }
    if (std::holds_alternative<bool>(v)) { out = std::get<bool>(v) ? "true" : "false"; return true; }
    return false;
}

// --- Mapear campos por nombre: newIdx -> oldIdx (o -1 si no existía) ---
static std::vector<int> buildFieldMap(const ma::Schema& oldS, const ma::Schema& newS) {
    std::vector<int> map(newS.fields.size(), -1);
    for (size_t i=0; i<newS.fields.size(); ++i) {
        const auto& nf = newS.fields[i].name;
        for (size_t j=0; j<oldS.fields.size(); ++j) {
            if (oldS.fields[j].name == nf) { map[i] = static_cast<int>(j); break; }
        }
    }
    return map;
}

// --- Convertir un valor antiguo al campo nuevo (con truncamientos seguros) ---
static std::optional<ma::Value> convertValueForNewField(const ma::Field& newF,
                                                        const std::optional<ma::Value>& oldVal,
                                                        const ma::Field* oldF /*puede ser null*/) {
    if (!oldVal.has_value()) return std::nullopt;

    const ma::Value& v = oldVal.value();
    switch (newF.type) {
    case ma::FieldType::Int32: {
        int32_t x; if (!toInt32(v, x)) return std::nullopt; return ma::Value{x};
    }
    case ma::FieldType::Double: {
        double d; if (!toDouble(v, d)) return std::nullopt; return ma::Value{d};
    }
    case ma::FieldType::Bool: {
        bool b; if (!toBool(v, b)) return std::nullopt; return ma::Value{b};
    }
    case ma::FieldType::String: {
        std::string s; if (!toString(v, s)) return std::nullopt; return ma::Value{s};
    }
    case ma::FieldType::CharN: {
        std::string s; if (!toString(v, s)) return std::nullopt;
        if (newF.size > 0 && s.size() > newF.size) s.resize(newF.size); // truncar
        return ma::Value{s};
    }
    default:
        return std::nullopt;
    }
}

DesignPage::DesignPage(const QString& basePath, QWidget* parent)
    : QWidget(parent), basePath_(basePath) {
    setupUi();
    loadSchema();
}

void DesignPage::setupUi() {
    auto* lay = new QVBoxLayout(this);

    banner_ = new QLabel(this);
    banner_->setWordWrap(true);
    banner_->setStyleSheet("background:#402020; border:1px solid #803030; padding:6px; color:#fff;");
    banner_->hide();
    lay->addWidget(banner_);

    grid_ = new QTableWidget(0, 3, this);
    grid_->setHorizontalHeaderLabels({"Field Name","Data Type","Size/Format"});
    grid_->horizontalHeader()->setStretchLastSection(true);
    grid_->verticalHeader()->setVisible(false);
    grid_->setSelectionBehavior(QAbstractItemView::SelectRows);
    lay->addWidget(grid_);

    props_ = new QStackedWidget(this);
    props_->addWidget(new QLabel("Short Text / CharN: tamaño máximo, collation", this)); // idx 0
    props_->addWidget(new QLabel("Number (Int32): subtipos: Byte/Int16/Int32", this));   // idx 1
    props_->addWidget(new QLabel("Yes/No: booleano (True/False, Yes/No, On/Off)", this));// idx 2
    props_->addWidget(new QLabel("Double: precisión (decimales)", this));                // idx 3
    props_->addWidget(new QLabel("Currency: Lempiras, USD, EUR", this));                 // idx 4
    props_->addWidget(new QLabel("Date/Time: General/Long/Short Date/Time", this));      // idx 5
    lay->addWidget(props_);

    auto* h = new QHBoxLayout();
    auto* bAdd   = new QPushButton("Add Field", this);
    auto* bDel   = new QPushButton("Remove Field", this);
    auto* bApply = new QPushButton("Save Design", this);
    h->addWidget(bAdd); h->addWidget(bDel); h->addStretch(); h->addWidget(bApply);
    lay->addLayout(h);

    connect(bAdd,   &QPushButton::clicked, this, &DesignPage::addField);
    connect(bDel,   &QPushButton::clicked, this, &DesignPage::removeField);
    connect(bApply, &QPushButton::clicked, this, &DesignPage::saveDesign);
}

void DesignPage::loadSchema() {
    try {
        Table t; t.open(basePath_.toStdString());
        setSchema(t.getSchema());
        banner_->setText("Editing design for: " + QFileInfo(basePath_).fileName());
        banner_->show();
    } catch (const std::exception& ex) {
        banner_->setText(QString("Error loading schema: %1").arg(ex.what()));
        banner_->show();
    }
}

void DesignPage::setSchema(const Schema& s) {
    grid_->setRowCount(0);
    for (const auto& f : s.fields) {
        addField();
        int r = grid_->rowCount() - 1;

        grid_->item(r,0)->setText(QString::fromStdString(f.name));

        auto* combo = qobject_cast<QComboBox*>(grid_->cellWidget(r,1));
        {
            QSignalBlocker block(combo);
            combo->setCurrentText(coreToTypeName(f));
        }

        onTypeChanged(r);

        QWidget* ed = grid_->cellWidget(r,2);
        if (auto* cb = qobject_cast<QComboBox*>(ed)) {
            int code = (int)f.size;
            if (f.type==FieldType::Int32 && !isNumberSubtype(code)) code = FMT_NUM_INT32;
            if (f.type==FieldType::Double && !isCurrencyFmt(code) && !isDoublePrecision(code)) code = 2;
            if (f.type==FieldType::String && !isDateTimeFmt(code)) code = FMT_NONE;
            int idx = cb->findData(code);
            if (idx<0) idx = 0;
            cb->setCurrentIndex(idx);
        } else if (auto* sp = qobject_cast<QSpinBox*>(ed)) {
            if (f.type==FieldType::CharN) {
                sp->setValue(f.size>0 ? f.size : 16);
            } else if (f.type==FieldType::Double) {
                sp->setValue(isDoublePrecision(f.size) ? f.size : 2);
            } else {
                sp->setValue(f.size);
            }
        }
    }
}

void DesignPage::addField() {
    int r = grid_->rowCount();
    grid_->insertRow(r);

    auto* itName = new QTableWidgetItem();
    grid_->setItem(r,0,itName);

    auto* combo = new QComboBox(grid_);
    combo->addItems(typeNames());
    grid_->setCellWidget(r,1,combo);
    connect(combo, &QComboBox::currentTextChanged,
            this,  &DesignPage::onComboTypeChanged,
            Qt::UniqueConnection);

    onTypeChanged(r);
}

void DesignPage::removeField() {
    auto sel = grid_->selectionModel()->selectedRows();
    for (const auto& idx : sel) grid_->removeRow(idx.row());
}

void DesignPage::onTypeChanged(int row) {
    auto* combo = qobject_cast<QComboBox*>(grid_->cellWidget(row,1));
    if (!combo) return;
    const QString t = combo->currentText();

    if (t=="Short Text")     { props_->setCurrentIndex(0); }
    else if (t=="Number")    { props_->setCurrentIndex(1); }
    else if (t=="Yes/No")    { props_->setCurrentIndex(2); }
    else if (t=="Double")    { props_->setCurrentIndex(3); }
    else if (t=="Currency")  { props_->setCurrentIndex(4); }
    else if (t=="Date/Time") { props_->setCurrentIndex(5); }
    else if (t=="CharN")     { props_->setCurrentIndex(0); }

    QWidget* old = grid_->cellWidget(row,2);
    if (old) old->deleteLater();

    QWidget* editor = nullptr;

    if (t=="Yes/No") {
        auto* cb = new QComboBox(grid_);
        cb->addItem("True/False", FMT_BOOL_TRUEFALSE);
        cb->addItem("Yes/No",     FMT_BOOL_YESNO);
        cb->addItem("On/Off",     FMT_BOOL_ONOFF);
        editor = cb;
    } else if (t=="Currency") {
        auto* cb = new QComboBox(grid_);
        cb->addItem("Lempiras (L)",  FMT_CUR_LPS);
        cb->addItem("US Dollar ($)", FMT_CUR_USD);
        cb->addItem("Euro (€)",      FMT_CUR_EUR);
        editor = cb;
    } else if (t=="Number") {
        auto* cb = new QComboBox(grid_);
        cb->addItem("Byte",          FMT_NUM_BYTE);
        cb->addItem("Integer (16)",  FMT_NUM_INT16);
        cb->addItem("Long Integer",  FMT_NUM_INT32);
        editor = cb;
    } else if (t=="Date/Time") {
        auto* cb = new QComboBox(grid_);
        cb->addItem("General Date",  FMT_DT_GENERAL);
        cb->addItem("Long Date",     FMT_DT_LONGDATE);
        cb->addItem("Short Date",    FMT_DT_SHORTDATE);
        cb->addItem("Long Time",     FMT_DT_LONGTIME);
        cb->addItem("Short Time",    FMT_DT_SHORTTIME);
        editor = cb;
    } else if (t=="Double") {
        auto* sp = new QSpinBox(grid_);
        sp->setRange(0, 9);
        sp->setValue(2);
        sp->setSuffix(" decimals");
        editor = sp;
    } else if (t=="CharN") {
        auto* sp = new QSpinBox(grid_);
        sp->setRange(1, 255);
        sp->setValue(16);
        sp->setSuffix(" chars");
        editor = sp;
    } else {
        auto* lbl = new QLabel("—", grid_);
        lbl->setEnabled(false);
        editor = lbl;
    }

    grid_->setCellWidget(row,2, editor);
}

Schema DesignPage::collectSchema(bool* ok) const {
    Schema s;
    s.tableName = "table";

    QSet<QString> usedNames;

    for (int r = 0; r < grid_->rowCount(); ++r) {
        auto* itName = grid_->item(r, 0);
        if (!itName || itName->text().trimmed().isEmpty()) { if (ok) *ok = false; return {}; }
        const QString name = itName->text().trimmed();

        const QString key = name.toLower();
        if (usedNames.contains(key)) { if (ok) *ok = false; return {}; }
        usedNames.insert(key);

        auto* combo = qobject_cast<QComboBox*>(grid_->cellWidget(r, 1));
        const QString tn = combo ? combo->currentText() : "Short Text";
        FieldType t = typeToCore(tn);

        uint16_t size = 0;
        QWidget* ed = grid_->cellWidget(r, 2);

        if (tn == "CharN") {
            if (auto* sp = qobject_cast<QSpinBox*>(ed)) {
                int v = sp->value();
                if (v <= 0) v = 16;
                size = static_cast<uint16_t>(v);
            } else {
                size = 16;
            }
        } else if (tn == "Double") {
            if (auto* sp = qobject_cast<QSpinBox*>(ed)) {
                int dec = sp->value();
                if (dec < 0) dec = 0;
                if (dec > 19) dec = 19;
                size = static_cast<uint16_t>(dec);
            } else {
                size = 2;
            }
        } else if (tn == "Number") {
            if (auto* cb = qobject_cast<QComboBox*>(ed)) {
                size = static_cast<uint16_t>(cb->currentData().toInt()); // FMT_NUM_*
            } else {
                size = FMT_NUM_INT32;
            }
        } else if (tn == "Yes/No") {
            if (auto* cb = qobject_cast<QComboBox*>(ed)) {
                size = static_cast<uint16_t>(cb->currentData().toInt()); // FMT_BOOL_*
            } else {
                size = FMT_BOOL_TRUEFALSE;
            }
        } else if (tn == "Currency") {
            if (auto* cb = qobject_cast<QComboBox*>(ed)) {
                size = static_cast<uint16_t>(cb->currentData().toInt()); // FMT_CUR_*
            } else {
                size = FMT_CUR_LPS;
            }
        } else if (tn == "Date/Time") {
            if (auto* cb = qobject_cast<QComboBox*>(ed)) {
                size = static_cast<uint16_t>(cb->currentData().toInt()); // FMT_DT_*
            } else {
                size = FMT_DT_GENERAL;
            }
        } else {
            size = 0; // Short Text
        }

        s.fields.push_back(Field{name.toStdString(), t, size});
    }

    if (ok) *ok = true;
    return s;
}

bool DesignPage::isTableEmpty() const {
    try { Table t; t.open(basePath_.toStdString()); return t.scanCount()==0; }
    catch (...) { return true; }
}

void DesignPage::saveDesign() {
    bool ok=false;
    Schema newS = collectSchema(&ok);
    if (!ok) {
        banner_->setText("Please complete all field names and avoid duplicates.");
        banner_->show();
        return;
    }

    const QString base = basePath_;
    const QString meta = base + ".meta";
    const QString mad  = base + ".mad";

    const bool exists = QFileInfo::exists(meta) || QFileInfo::exists(mad);
    if (!exists) {
        try {
            ma::Table t; t.create(base.toStdString(), newS);
            banner_->setText("Design saved (new table created).");
            banner_->show();
        } catch (const std::exception& ex) {
            banner_->setText(QString("Error creating table: %1").arg(ex.what()));
            banner_->show();
        }
        return;
    }

    ma::Schema oldS;
    size_t rowCount = 0;
    try {
        ma::Table told; told.open(base.toStdString());
        oldS = told.getSchema();
        rowCount = told.scanCount();
    } catch (const std::exception& ex) {
        banner_->setText(QString("Error opening current table: %1").arg(ex.what()));
        banner_->show();
        return;
    }

    if (rowCount == 0) {
        try {
            ma::Table t; t.create(base.toStdString(), newS);
            banner_->setText("Design saved (empty table recreated).");
            banner_->show();
            return;
        } catch (const std::exception& ex) {
            banner_->setText(QString("Error saving design: %1").arg(ex.what()));
            banner_->show();
            return;
        }
    }

    const QString tmpBase = base + ".tmp_schema";
    const QString tmpMeta = tmpBase + ".meta";
    const QString tmpMad  = tmpBase + ".mad";

    QFile::remove(tmpMeta);
    QFile::remove(tmpMad);

    try {
        ma::Table told; told.open(base.toStdString());
        ma::Table tnew; tnew.create(tmpBase.toStdString(), newS);

        auto fmap = buildFieldMap(oldS, newS);

        const auto rids = told.scanAll();
        for (const auto& rid : rids) {
            auto recOld = told.read(rid);
            if (!recOld) continue;

            ma::Record recNew = ma::Record::withFieldCount(static_cast<int>(newS.fields.size()));
            for (size_t i=0; i<newS.fields.size(); ++i) {
                int j = fmap[i];
                const ma::Field& nF = newS.fields[i];
                const ma::Field* oF = (j>=0 ? &oldS.fields[j] : nullptr);
                const std::optional<ma::Value> oldVal = (j>=0 ? (*recOld).values[j] : std::optional<ma::Value>{});

                recNew.values[i] = convertValueForNewField(nF, oldVal, oF);
            }
            tnew.insert(recNew);
        }

        told.close();
        tnew.close();

        QFileInfo bi(base);
        const QString dir = bi.dir().absolutePath();
        const QString pref = bi.fileName() + ".";
        QDir d(dir);
        const QStringList idxs = d.entryList(QStringList() << (pref + "*.idx"), QDir::Files);
        for (const QString& f : idxs) {
            QFile::remove(d.filePath(f));
        }

        if (!QFile::remove(mad) || !QFile::remove(meta)) {
            banner_->setText("Close any open Datasheet for this table and try Save again.");
            banner_->show();
            QFile::remove(tmpMeta);
            QFile::remove(tmpMad);
            return;
        }

        if (!QFile::rename(tmpMeta, meta) || !QFile::rename(tmpMad, mad)) {
            banner_->setText("Error swapping temp files. Design NOT saved.");
            banner_->show();
            QFile::remove(tmpMeta);
            QFile::remove(tmpMad);
            return;
        }

        banner_->setText("Design saved. Data migrated to new schema. Indexes were removed; recreate if needed.");
        banner_->show();

    } catch (const std::exception& ex) {
        QFile::remove(tmpMeta);
        QFile::remove(tmpMad);
        banner_->setText(QString("Error during migration: %1").arg(ex.what()));
        banner_->show();
        return;
    }
}

void DesignPage::onComboTypeChanged(const QString&) {
    auto* combo = qobject_cast<QComboBox*>(sender());
    if (!combo) return;
    int row = -1;
    for (int i=0; i<grid_->rowCount(); ++i) {
        if (grid_->cellWidget(i,1) == combo) { row = i; break; }
    }
    if (row >= 0) onTypeChanged(row);
}

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
#include "../core/pk_utils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cctype>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QCheckBox>
#include <QEvent>
#include <QMouseEvent>

using namespace ma;

static QComboBox* typeComboAt(QTableWidget* grid, int row) {
    return qobject_cast<QComboBox*>(grid->cellWidget(row, 2));
}

static QWidget* fmtWidgetAt(QTableWidget* grid, int row) {
    return grid->cellWidget(row, 3);
}

static void fillFormatEditor(QTableWidget* grid, int row, const QString& typeName) {
    if (auto* old = grid->cellWidget(row, 2)) {
        old->deleteLater();
        grid->setCellWidget(row, 2, nullptr);
    }

    if (typeName=="Yes/No") {
        auto* cb = new QComboBox(grid);
        cb->addItem("True/False",  FMT_BOOL_TRUEFALSE);
        cb->addItem("Yes/No",      FMT_BOOL_YESNO);
        cb->addItem("On/Off",      FMT_BOOL_ONOFF);
        grid->setCellWidget(row, 2, cb);
        cb->setCurrentIndex(0);
    } else if (typeName=="Currency") {
        auto* cb = new QComboBox(grid);
        cb->addItem("Lps",  FMT_CUR_LPS);
        cb->addItem("USD",  FMT_CUR_USD);
        cb->addItem("EUR",  FMT_CUR_EUR);
        grid->setCellWidget(row, 2, cb);
        cb->setCurrentIndex(0);
    } else if (typeName=="Number") {
        auto* cb = new QComboBox(grid);
        cb->addItem("Byte",         FMT_NUM_BYTE);
        cb->addItem("Integer",      FMT_NUM_INT16);
        cb->addItem("Long Integer", FMT_NUM_INT32);
        grid->setCellWidget(row, 2, cb);
        cb->setCurrentIndex(2);
    } else if (typeName=="Date/Time") {
        auto* cb = new QComboBox(grid);
        cb->addItem("General Date", FMT_DT_GENERAL);
        cb->addItem("Long Date",    FMT_DT_LONGDATE);
        cb->addItem("Short Date",   FMT_DT_SHORTDATE);
        cb->addItem("Long Time",    FMT_DT_LONGTIME);
        cb->addItem("Short Time",   FMT_DT_SHORTTIME);
        grid->setCellWidget(row, 2, cb);
        cb->setCurrentIndex(0);
    } else if (typeName=="Double") {
        auto* sp = new QSpinBox(grid);
        sp->setRange(0, 19);
        sp->setValue(2);
        sp->setSuffix(" decimals");
        sp->setAccelerated(true);
        grid->setCellWidget(row, 2, sp);
    } else if (typeName=="CharN") {
        auto* sp = new QSpinBox(grid);
        sp->setRange(1, 255);
        sp->setValue(16);
        sp->setSuffix(" chars");
        sp->setAccelerated(true);
        grid->setCellWidget(row, 2, sp);
    } else {
        auto* sp = new QSpinBox(grid);
        sp->setRange(1, 255);
        sp->setValue(50);
        sp->setSuffix(" max");
        sp->setAccelerated(true);
        grid->setCellWidget(row, 2, sp);
    }
}

static void applyExistingFormatToEditor(QTableWidget* grid, int row, const ma::Field& f) {
    QWidget* ed = grid->cellWidget(row, 3);
    if (!ed) return;

    using ma::FieldType;

    if (f.type == FieldType::CharN) {
        if (auto* sp = qobject_cast<QSpinBox*>(ed)) {
            sp->setValue( (f.size > 0) ? f.size : 16 );
        }
        return;
    }

    if (f.type == FieldType::Double) {
        if (ma::isCurrencyFmt(f.size)) {
            if (auto* cb = qobject_cast<QComboBox*>(ed)) {
                int i = cb->findData(f.size);
                if (i >= 0) cb->setCurrentIndex(i);
            }
        } else {
            if (auto* sp = qobject_cast<QSpinBox*>(ed)) {
                int dec = static_cast<int>(f.size);
                if (dec < 0) dec = 0; if (dec > 19) dec = 19;
                sp->setValue(dec);
            }
        }
        return;
    }

    if (f.type == FieldType::Int32) {
        if (auto* cb = qobject_cast<QComboBox*>(ed)) {
            int i = cb->findData(f.size);
            if (i >= 0) cb->setCurrentIndex(i);
        }
        return;
    }

    if (f.type == FieldType::Bool) {
        if (auto* cb = qobject_cast<QComboBox*>(ed)) {
            int i = cb->findData(f.size);
            if (i >= 0) cb->setCurrentIndex(i);
        }
        return;
    }

    if (f.type == FieldType::String) {
        if (ma::isDateTimeFmt(f.size)) {
            if (auto* cb = qobject_cast<QComboBox*>(ed)) {
                int i = cb->findData(f.size);
                if (i >= 0) cb->setCurrentIndex(i);
            }
        }
        return;
    }
}

static uint16_t formatSizeFromWidget(QTableWidget* grid, int row, const QString& typeName) {
    if (typeName=="Yes/No" || typeName=="Currency" || typeName=="Number" || typeName=="Date/Time") {
        if (auto* cb = qobject_cast<QComboBox*>(fmtWidgetAt(grid,row)))
            return static_cast<uint16_t>(cb->currentData().toInt());
        return 0;
    } else if (typeName=="Double") {
        if (auto* sp = qobject_cast<QSpinBox*>(fmtWidgetAt(grid,row)))
            return static_cast<uint16_t>(std::clamp(sp->value(),0,9));
        return 2;
    } else if (typeName=="CharN") {
        if (auto* sp = qobject_cast<QSpinBox*>(fmtWidgetAt(grid,row)))
            return static_cast<uint16_t>(std::clamp(sp->value(),1,255));
        return 16;
    } else {
        if (auto* sp = qobject_cast<QSpinBox*>(fmtWidgetAt(grid,row)))
            return static_cast<uint16_t>(std::clamp(sp->value(),1,255));
        return 50;
    }
}

static QString coreToTypeName(const ma::Field& f) {
    using namespace ma;
    if (f.type==FieldType::Double && ma::isCurrencyFmt(f.size)) return "Currency";
    if (f.type==FieldType::String && ma::isDateTimeFmt(f.size)) return "Date/Time";
    switch (f.type) {
    case FieldType::String: return "Short Text";
    case FieldType::Int32:  return "Number";
    case FieldType::Bool:   return "Yes/No";
    case FieldType::Double: return "Double";
    case FieldType::CharN:  return "CharN";
    default: return "Short Text";
    }
}

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

static std::optional<ma::Value> convertValueForNewField(const ma::Field& newF,
                                                        const std::optional<ma::Value>& oldVal,
                                                        const ma::Field* oldF) {
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
        if (newF.size > 0 && s.size() > newF.size) s.resize(newF.size);
        return ma::Value{s};
    }
    default:
        return std::nullopt;
    }
}

static QString pkFileForBase(const QString& basePath) { return basePath + ".keys.json"; }

static QString loadPrimaryKeyNameForBase(const QString& basePath) {
    migratePkIfNeeded(basePath);
    return loadPk(basePath);
}

static bool savePrimaryKeyNameForBase(const QString& basePath, const QString& pkName) {
    return savePk(basePath, pkName);
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
    grid_->setEditTriggers(QAbstractItemView::AllEditTriggers);
    lay->addWidget(grid_);

    props_ = new QStackedWidget(this);
    props_->addWidget(new QLabel("Short Text / CharN: tamaño máximo, collation", this));
    props_->addWidget(new QLabel("Number (Int32): tamaño fijo 32 bits", this));
    props_->addWidget(new QLabel("Yes/No: booleano", this));
    props_->addWidget(new QLabel("Double: número de doble precisión", this));
    props_->addWidget(new QLabel("Currency: visualización monetaria", this));
    props_->addWidget(new QLabel("Date/Time: visualización de fecha/hora", this));
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

void DesignPage::setSchema(const ma::Schema& s) {
    using namespace ma;
    grid_->setRowCount(0);

    for (const auto& f : s.fields) {
        addField();
        const int r = grid_->rowCount() - 1;

        grid_->item(r,0)->setText(QString::fromStdString(f.name));

        auto* combo = qobject_cast<QComboBox*>(grid_->cellWidget(r,1));
        {
            QSignalBlocker b(combo);
            combo->setCurrentText(coreToTypeName(f));
        }

        onTypeChanged(r);

        QWidget* ed = grid_->cellWidget(r,2);
        if (!ed) continue;

        if (f.type == FieldType::CharN) {
            if (auto* sp = qobject_cast<QSpinBox*>(ed)) {
                sp->setValue(f.size > 0 ? f.size : 16);
            }
        } else if (f.type == FieldType::String && !ma::isDateTimeFmt(f.size)) {
            if (auto* sp = qobject_cast<QSpinBox*>(ed)) {
                sp->setValue(f.size > 0 ? f.size : 50);
            }
        } else if (f.type == FieldType::Double && ma::isCurrencyFmt(f.size)) {
            if (auto* cb = qobject_cast<QComboBox*>(ed)) {
                int idx = cb->findData(static_cast<int>(f.size));
                if (idx >= 0) cb->setCurrentIndex(idx);
            }
        } else if (f.type == FieldType::String && ma::isDateTimeFmt(f.size)) {
            if (auto* cb = qobject_cast<QComboBox*>(ed)) {
                int idx = cb->findData(static_cast<int>(f.size));
                if (idx >= 0) cb->setCurrentIndex(idx);
            }
        } else if (f.type == FieldType::Double && ma::isDoublePrecision(f.size)) {
            if (auto* sp = qobject_cast<QSpinBox*>(ed)) {
                sp->setValue(std::clamp<int>(f.size, 0, 19));
            }
        } else if (f.type == FieldType::Int32 && ma::isNumberSubtype(f.size)) {
            if (auto* cb = qobject_cast<QComboBox*>(ed)) {
                int idx = cb->findData(static_cast<int>(f.size));
                if (idx >= 0) cb->setCurrentIndex(idx);
            }
        } else if (f.type == FieldType::Bool && ma::isBoolFmt(f.size)) {
            if (auto* cb = qobject_cast<QComboBox*>(ed)) {
                int idx = cb->findData(static_cast<int>(f.size));
                if (idx >= 0) cb->setCurrentIndex(idx);
            }
        }
    }
}

ma::Schema DesignPage::collectSchema(bool* ok) const {
    using namespace ma;
    Schema s; s.tableName = "table";

    for (int r=0; r<grid_->rowCount(); ++r) {
        auto* itName = grid_->item(r,0);
        if (!itName || itName->text().trimmed().isEmpty()) {
            if (ok) *ok=false;
            return {};
        }
        const QString name = itName->text().trimmed();

        auto* combo = qobject_cast<QComboBox*>(grid_->cellWidget(r,1));
        const QString tn = combo ? combo->currentText() : "Short Text";
        const FieldType t = typeToCore(tn);

        uint16_t size = 0;
        QWidget* ed = grid_->cellWidget(r,2);

        if (tn=="CharN") {
            if (auto* sp = qobject_cast<QSpinBox*>(ed)) {
                int v = sp->value(); if (v <= 0) v = 16;
                size = static_cast<uint16_t>(v);
            } else size = 16;
        } else if (tn=="Short Text") {
            if (auto* sp = qobject_cast<QSpinBox*>(ed)) {
                int v = std::clamp(sp->value(), 1, 255);
                size = static_cast<uint16_t>(v);
            } else size = 50;
        } else if (tn=="Double") {
            if (auto* sp = qobject_cast<QSpinBox*>(ed)) {
                int dec = std::clamp(sp->value(), 0, 19);
                size = static_cast<uint16_t>(dec);
            } else size = 2;
        } else if (tn=="Number") {
            if (auto* cb = qobject_cast<QComboBox*>(ed)) {
                size = static_cast<uint16_t>(cb->currentData().toInt());
            } else size = FMT_NUM_INT32;
        } else if (tn=="Yes/No") {
            if (auto* cb = qobject_cast<QComboBox*>(ed)) {
                size = static_cast<uint16_t>(cb->currentData().toInt());
            } else size = FMT_BOOL_TRUEFALSE;
        } else if (tn=="Currency") {
            if (auto* cb = qobject_cast<QComboBox*>(ed)) {
                size = static_cast<uint16_t>(cb->currentData().toInt());
            } else size = FMT_CUR_LPS;
        } else if (tn=="Date/Time") {
            if (auto* cb = qobject_cast<QComboBox*>(ed)) {
                size = static_cast<uint16_t>(cb->currentData().toInt());
            } else size = FMT_DT_GENERAL;
        } else {
            size = 0;
        }

        s.fields.push_back(Field{name.toStdString(), t, size});
    }
    if (ok) *ok = true;
    return s;
}

void DesignPage::addField() {
    const int r = grid_->rowCount();
    grid_->insertRow(r);

    grid_->setItem(r, 0, new QTableWidgetItem());

    auto* combo = new QComboBox(grid_);
    combo->addItems(QStringList({"Short Text","Number","Yes/No","Double","Currency","Date/Time","CharN"}));
    grid_->setCellWidget(r, 1, combo);

    fillFormatEditor(grid_, r, combo->currentText());

    connect(combo, &QComboBox::currentTextChanged,
            this,  &DesignPage::onComboTypeChanged,
            Qt::UniqueConnection);
}

void DesignPage::removeField() {
    auto sel = grid_->selectionModel()->selectedRows();
    for (const auto& idx : sel) grid_->removeRow(idx.row());
}

void DesignPage::onTypeChanged(int row) {
    auto* combo = qobject_cast<QComboBox*>(grid_->cellWidget(row, 1));
    if (!combo) return;
    const QString t = combo->currentText();

    fillFormatEditor(grid_, row, t);

    if      (t=="Short Text")  props_->setCurrentIndex(0);
    else if (t=="Number")      props_->setCurrentIndex(1);
    else if (t=="Yes/No")      props_->setCurrentIndex(2);
    else if (t=="Double")      props_->setCurrentIndex(3);
    else if (t=="Currency")    props_->setCurrentIndex(4);
    else if (t=="Date/Time")   props_->setCurrentIndex(5);
    else if (t=="CharN")       props_->setCurrentIndex(0);
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
            banner_->setText("Design saved");
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
            banner_->setText("Design saved");
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
            banner_->setText("Close any open datasheet");
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

        banner_->setText("Design saved");
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
        if (grid_->cellWidget(i, 1) == combo) {
            row = i; break;
        }
    }
    if (row >= 0) onTypeChanged(row);
}

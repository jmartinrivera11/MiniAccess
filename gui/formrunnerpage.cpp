#include "FormRunnerPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QDateEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QDate>
#include "../core/forms_io.h"

FormRunnerPage::FormRunnerPage(const QString& projectDir,
                               const QJsonObject& formDef,
                               QWidget* parent)
    : QWidget(parent),
    projectDir_(projectDir),
    formDef_(formDef)
{
    formName_  = formDef_.value("name").toString();
    baseTable_ = formDef_.value("table").toString();
    buildUi();
    rebuildControls();
    loadData();
    if (!data_.isEmpty()) { current_ = 0; bindRecordToUi(); }
    else { onAdd(); }
}

void FormRunnerPage::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8,8,8,8);

    auto* bar = new QHBoxLayout();
    btnFirst_ = new QPushButton("⏮ Primero", this);
    btnPrev_  = new QPushButton("◀ Anterior", this);
    btnNext_  = new QPushButton("Siguiente ▶", this);
    btnLast_  = new QPushButton("Último ⏭", this);
    btnAdd_   = new QPushButton("Agregar", this);
    btnDel_   = new QPushButton("Eliminar", this);
    btnSave_  = new QPushButton("Guardar", this);
    btnClose_ = new QPushButton("Cerrar", this);
    for (auto* b : {btnFirst_,btnPrev_,btnNext_,btnLast_,btnAdd_,btnDel_,btnSave_,btnClose_}) {
        b->setMinimumHeight(28);
    }
    bar->addWidget(btnFirst_);
    bar->addWidget(btnPrev_);
    bar->addWidget(btnNext_);
    bar->addWidget(btnLast_);
    bar->addSpacing(12);
    bar->addWidget(btnAdd_);
    bar->addWidget(btnDel_);
    bar->addWidget(btnSave_);
    bar->addWidget(btnClose_);
    root->addLayout(bar);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scrollBody_ = new QWidget(scroll);
    scroll->setWidget(scrollBody_);

    root->addWidget(scroll, 1);

    infoLabel_ = new QLabel(this);
    infoLabel_->setText("Ready");
    root->addWidget(infoLabel_);

    connect(btnFirst_, &QPushButton::clicked, this, &FormRunnerPage::onFirst);
    connect(btnPrev_,  &QPushButton::clicked, this, &FormRunnerPage::onPrev);
    connect(btnNext_,  &QPushButton::clicked, this, &FormRunnerPage::onNext);
    connect(btnLast_,  &QPushButton::clicked, this, &FormRunnerPage::onLast);
    connect(btnAdd_,   &QPushButton::clicked, this, &FormRunnerPage::onAdd);
    connect(btnDel_,   &QPushButton::clicked, this, &FormRunnerPage::onDelete);
    connect(btnSave_,  &QPushButton::clicked, this, &FormRunnerPage::onSave);
    connect(btnClose_, &QPushButton::clicked, this, &FormRunnerPage::onClose);
}

void FormRunnerPage::rebuildControls() {
    auto* body = new QVBoxLayout(scrollBody_);
    body->setContentsMargins(16,16,16,16);
    qDeleteAll(scrollBody_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));
    editors_.clear();

    const QJsonArray ctrls = formDef_.value("controls").toArray();
    for (const auto& v : ctrls) {
        const QJsonObject c = v.toObject();
        const QString type  = c.value("type").toString("TextBox");
        const QString field = c.value("field").toString();

        auto* row = new QHBoxLayout();
        auto* lab = new QLabel(field + ":", scrollBody_);
        row->addWidget(lab);

        QWidget* ed = nullptr;
        if (type.compare("CheckBox", Qt::CaseInsensitive)==0) {
            auto* cb = new QCheckBox(scrollBody_);
            connect(cb, &QCheckBox::stateChanged, this, &FormRunnerPage::onFieldEdited);
            ed = cb;
        } else if (type.compare("DateEdit", Qt::CaseInsensitive)==0) {
            auto* de = new QDateEdit(scrollBody_);
            de->setCalendarPopup(true);
            connect(de, &QDateEdit::dateChanged, this, &FormRunnerPage::onFieldEdited);
            ed = de;
        } else {
            auto* le = new QLineEdit(scrollBody_);
            connect(le, &QLineEdit::textChanged, this, &FormRunnerPage::onFieldEdited);
            ed = le;
        }
        row->addWidget(ed, 1);
        body->addLayout(row);

        if (!field.isEmpty() && ed) editors_.insert(field, ed);
    }
    body->addStretch(1);
}

bool FormRunnerPage::loadData() {
    QJsonArray arr;
    if (!forms::loadFormData(projectDir_, formName_, arr)) {
        QMessageBox::warning(this, "Forms", "Could not load form data file.");
        return false;
    }
    data_ = arr;
    infoLabel_->setText(QString("Loaded %1 record(s)").arg(data_.size()));
    return true;
}

bool FormRunnerPage::saveData() {
    if (!forms::saveFormData(projectDir_, formName_, data_)) {
        QMessageBox::warning(this, "Forms", "Could not save form data.");
        return false;
    }
    dirty_ = false;
    infoLabel_->setText("Saved.");
    return true;
}

void FormRunnerPage::bindRecordToUi() {
    if (current_ < 0 || current_ >= data_.size()) return;
    const QJsonObject rec = data_.at(current_).toObject();

    for (auto it = editors_.cbegin(); it != editors_.cend(); ++it) {
        const QString field = it.key();
        QWidget* ed = it.value();

        if (auto* cb = qobject_cast<QCheckBox*>(ed)) {
            cb->blockSignals(true);
            cb->setChecked(rec.value(field).toBool(false));
            cb->blockSignals(false);
        } else if (auto* de = qobject_cast<QDateEdit*>(ed)) {
            de->blockSignals(true);
            const QString s = rec.value(field).toString();
            QDate d = QDate::fromString(s, Qt::ISODate);
            if (!d.isValid()) d = QDate::currentDate();
            de->setDate(d);
            de->blockSignals(false);
        } else if (auto* le = qobject_cast<QLineEdit*>(ed)) {
            le->blockSignals(true);
            le->setText(rec.value(field).toVariant().toString());
            le->blockSignals(false);
        }
    }
    infoLabel_->setText(QString("Record %1 / %2").arg(current_+1).arg(data_.size()));
}

void FormRunnerPage::pullUiToRecord() {
    if (current_ < 0 || current_ >= data_.size()) return;
    QJsonObject rec = data_.at(current_).toObject();

    for (auto it = editors_.cbegin(); it != editors_.cend(); ++it) {
        const QString field = it.key();
        QWidget* ed = it.value();

        if (auto* cb = qobject_cast<QCheckBox*>(ed)) {
            rec.insert(field, cb->isChecked());
        } else if (auto* de = qobject_cast<QDateEdit*>(ed)) {
            rec.insert(field, de->date().toString(Qt::ISODate));
        } else if (auto* le = qobject_cast<QLineEdit*>(ed)) {
            rec.insert(field, le->text());
        }
    }
    data_.replace(current_, rec);
    dirty_ = true;
}

void FormRunnerPage::onFieldEdited() { pullUiToRecord(); }

void FormRunnerPage::onFirst() { if (data_.isEmpty()) return; current_ = 0; bindRecordToUi(); }
void FormRunnerPage::onPrev()  { if (current_>0) { --current_; bindRecordToUi(); } }
void FormRunnerPage::onNext()  { if (current_+1 < data_.size()) { ++current_; bindRecordToUi(); } }
void FormRunnerPage::onLast()  { if (!data_.isEmpty()) { current_ = data_.size()-1; bindRecordToUi(); } }

void FormRunnerPage::onAdd() {
    QJsonObject rec;
    for (auto it = editors_.cbegin(); it != editors_.cend(); ++it) {
        const QString field = it.key();
        rec.insert(field, QJsonValue());
    }
    data_.push_back(rec);
    current_ = data_.size()-1;
    bindRecordToUi();
    dirty_ = true;
}

void FormRunnerPage::onDelete() {
    if (current_ < 0 || current_ >= data_.size()) return;
    const auto ret = QMessageBox::question(this, "Forms", "Delete current record?",
                                           QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    data_.removeAt(current_);
    if (current_ >= data_.size()) current_ = data_.size()-1;
    dirty_ = true;
    if (current_ >= 0) bindRecordToUi();
    else infoLabel_->setText("No records");
}

void FormRunnerPage::onSave() { saveData(); }

void FormRunnerPage::onClose() {
    if (dirty_) {
        const auto ret = QMessageBox::question(this, "Forms",
                                               "Save changes before closing?",
                                               QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel, QMessageBox::Yes);
        if (ret == QMessageBox::Cancel) return;
        if (ret == QMessageBox::Yes) {
            if (!saveData()) return;
        }
    }
    emit requestClose(this);
}

bool FormRunnerPage::insertRow() { onAdd(); return true; }
bool FormRunnerPage::deleteRows() { onDelete(); return true; }
bool FormRunnerPage::refresh() { return loadData(); }
bool FormRunnerPage::zoomInView() { return true; }
bool FormRunnerPage::zoomOutView() { return true; }

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
#include <QDir>
#include <QFileInfo>
#include "../core/forms_io.h"
#include "../core/Table.h"
#include "../core/DisplayFmt.h"
#include "TableModel.h"

using namespace ma;

namespace {
static int fieldIndexByName(const ma::Schema& s, const QString& name) {
    for (int i=0;i<(int)s.fields.size();++i) {
        if (QString::fromStdString(s.fields[i].name).compare(name, Qt::CaseInsensitive)==0)
            return i;
    }
    return -1;
}
}

FormRunnerPage::FormRunnerPage(const QString& projectDir,
                               const QJsonObject& formDef,
                               QWidget* parent)
    : QWidget(parent),
    projectDir_(projectDir),
    formDef_(formDef)
{
    formName_  = formDef_.value("name").toString();
    baseTable_ = formDef_.value("table").toString();
    basePath_  = QDir(projectDir_).filePath(baseTable_);

    buildUi();

    table_  = std::make_unique<ma::Table>();
    table_->open(basePath_.toStdString());
    schema_ = std::make_unique<ma::Schema>(table_->getSchema());

    model_  = std::make_unique<TableModel>(table_.get(), basePath_, this);

    rebuildControls();

    if (rowCount() > 0) { current_ = 0; bindRowToUi(current_); }
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
    for (auto* b : {btnFirst_,btnPrev_,btnNext_,btnLast_,btnAdd_,btnDel_,btnSave_,btnClose_})
        b->setMinimumHeight(28);

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
    qDeleteAll(scrollBody_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));
    editors_.clear();

    auto* body = new QVBoxLayout(scrollBody_);
    body->setContentsMargins(16,16,16,16);

    const QJsonArray ctrls = formDef_.value("controls").toArray();
    for (const auto& v : ctrls) {
        const QJsonObject c = v.toObject();
        const QString type  = c.value("type").toString("TextBox");
        const QString field = c.value("field").toString();
        if (field.isEmpty()) continue;

        const int col = fieldIndexByName(*schema_, field);

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
            connect(le, &QLineEdit::editingFinished, this, &FormRunnerPage::onFieldEdited);
            ed = le;
        }

        row->addWidget(ed, 1);
        body->addLayout(row);
        editors_.insert(field, ed);
    }
    body->addStretch(1);
}

int FormRunnerPage::rowCount() const {
    return model_ ? model_->rowCount() : 0;
}

int FormRunnerPage::fieldColumn(const QString& fieldName) const {
    return schema_ ? fieldIndexByName(*schema_, fieldName) : -1;
}

void FormRunnerPage::bindRowToUi(int row) {
    if (!model_ || row < 0 || row >= rowCount()) return;

    for (auto it = editors_.cbegin(); it != editors_.cend(); ++it) {
        const QString field = it.key();
        QWidget* ed = it.value();
        const int col = fieldColumn(field);
        if (col < 0) continue;

        QModelIndex idx = model_->index(row, col);

        if (auto* cb = qobject_cast<QCheckBox*>(ed)) {
            const QVariant st = model_->data(idx, Qt::CheckStateRole);
            cb->blockSignals(true);
            cb->setCheckState(st.toInt()==Qt::Checked ? Qt::Checked : Qt::Unchecked);
            cb->blockSignals(false);
            continue;
        }

        if (auto* de = qobject_cast<QDateEdit*>(ed)) {
            const ma::Field& f = schema_->fields[col];
            de->blockSignals(true);
            if (f.type == ma::FieldType::String && ma::isDateTimeFmt(f.size)) {
                const QVariant v = model_->data(idx, Qt::EditRole);
                const QDate d = QDate::fromString(v.toString(), Qt::ISODate);
                de->setDate(d.isValid()? d : QDate::currentDate());
            } else {
                const QVariant v = model_->data(idx, Qt::EditRole);
                bool ok=false; qlonglong secs = v.toLongLong(&ok);
                QDate d = ok ? QDateTime::fromSecsSinceEpoch(secs).date() : QDate::currentDate();
                de->setDate(d);
            }
            de->blockSignals(false);
            continue;
        }

        if (auto* le = qobject_cast<QLineEdit*>(ed)) {
            const QVariant v = model_->data(idx, Qt::EditRole);
            le->blockSignals(true);
            le->setText(v.toString());
            le->blockSignals(false);
            continue;
        }
    }

    infoLabel_->setText(QString("Record %1 / %2").arg(row+1).arg(rowCount()));
}

void FormRunnerPage::commitField(const QString& fieldName) {
    if (!model_ || current_ < 0 || current_ >= rowCount()) return;

    QWidget* ed = editors_.value(fieldName, nullptr);
    if (!ed) return;

    const int col = fieldColumn(fieldName);
    if (col < 0) return;

    QModelIndex idx = model_->index(current_, col);
    const ma::Field& f = schema_->fields[col];

    bool ok = false;

    if (auto* cb = qobject_cast<QCheckBox*>(ed)) {
        const Qt::CheckState st = cb->checkState();
        ok = model_->setData(idx, st, Qt::CheckStateRole);
    } else if (auto* de = qobject_cast<QDateEdit*>(ed)) {
        if (f.type == ma::FieldType::String && ma::isDateTimeFmt(f.size)) {
            ok = model_->setData(idx, de->date().toString(Qt::ISODate), Qt::EditRole);
        } else if (f.type == ma::FieldType::Date) {
            const qlonglong secs = QDateTime(de->date().startOfDay()).toSecsSinceEpoch();
            ok = model_->setData(idx, secs, Qt::EditRole);
        } else {
            ok = model_->setData(idx, de->date().toString(Qt::ISODate), Qt::EditRole);
        }
    } else if (auto* le = qobject_cast<QLineEdit*>(ed)) {
        ok = model_->setData(idx, le->text(), Qt::EditRole);
    }

    if (ok) {
        dirty_ = true;
        infoLabel_->setText("Edited.");
    } else {
        infoLabel_->setText("Edit rejected by constraints.");
        bindRowToUi(current_);
    }
}

void FormRunnerPage::onFieldEdited() {
    QWidget* ed = qobject_cast<QWidget*>(sender());
    if (!ed) return;
    for (auto it = editors_.cbegin(); it != editors_.cend(); ++it) {
        if (it.value() == ed) { commitField(it.key()); break; }
    }
}

void FormRunnerPage::onFirst() { if (rowCount() == 0) return; current_ = 0; bindRowToUi(current_); }
void FormRunnerPage::onPrev()  { if (current_ > 0) { --current_; bindRowToUi(current_); } }
void FormRunnerPage::onNext()  { if (current_+1 < rowCount()) { ++current_; bindRowToUi(current_); } }
void FormRunnerPage::onLast()  { if (rowCount() > 0) { current_ = rowCount()-1; bindRowToUi(current_); } }

void FormRunnerPage::onAdd() {
    if (!model_) return;
    const int pos = rowCount();
    if (!model_->insertRows(pos, 1)) {
        infoLabel_->setText("Insert rejected.");
        return;
    }
    current_ = rowCount() - 1;
    bindRowToUi(current_);
    dirty_ = true;
}

void FormRunnerPage::onDelete() {
    if (!model_ || current_ < 0 || current_ >= rowCount()) return;
    const auto ret = QMessageBox::question(this, "Forms", "Delete current record?",
                                           QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    if (!model_->removeRows(current_, 1)) {
        infoLabel_->setText("Delete rejected (RI/cascade).");
        return;
    }
    if (current_ >= rowCount()) current_ = rowCount()-1;
    if (current_ >= 0) bindRowToUi(current_);
    else infoLabel_->setText("No records");
    dirty_ = true;
}

void FormRunnerPage::onSave() {
    dirty_ = false;
    if (model_) model_->reload();
    if (rowCount() == 0) { current_ = -1; infoLabel_->setText("No records"); }
    else {
        if (current_ < 0) current_ = 0;
        if (current_ >= rowCount()) current_ = rowCount()-1;
        bindRowToUi(current_);
    }
    infoLabel_->setText("Saved.");
}

void FormRunnerPage::onClose() {
    if (dirty_) {
        const auto ret = QMessageBox::question(this, "Forms",
                                               "Save changes before closing?",
                                               QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel, QMessageBox::Yes);
        if (ret == QMessageBox::Cancel) return;
        if (ret == QMessageBox::Yes) onSave();
    }
    emit requestClose(this);
}

bool FormRunnerPage::insertRow()  { onAdd();    return true; }
bool FormRunnerPage::deleteRows() { onDelete(); return true; }
bool FormRunnerPage::refresh()    {
    if (model_) model_->reload();
    if (rowCount()==0) { current_=-1; infoLabel_->setText("No records"); return true; }
    if (current_ < 0) current_ = 0;
    if (current_ >= rowCount()) current_ = rowCount()-1;
    bindRowToUi(current_);
    return true;
}
bool FormRunnerPage::zoomInView()  { return true; }
bool FormRunnerPage::zoomOutView() { return true; }

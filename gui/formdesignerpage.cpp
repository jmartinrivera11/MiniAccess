#include "FormDesignerPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QJsonArray>
#include "../core/forms_io.h"

FormDesignerPage::FormDesignerPage(const QString& projectDir,
                                   const QJsonObject& def,
                                   QWidget* parent)
    : QWidget(parent),
    projectDir_(projectDir),
    formDef_(def)
{
    buildUi();
    loadDef();
}

void FormDesignerPage::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8,8,8,8);

    auto* form = new QFormLayout();
    edName_  = new QLineEdit(this);
    edTable_ = new QLineEdit(this);
    edName_->setReadOnly(false);
    edTable_->setReadOnly(true);
    form->addRow("Form name:", edName_);
    form->addRow("Base table:", edTable_);
    root->addLayout(form);

    listControls_ = new QListWidget(this);
    root->addWidget(listControls_, 1);

    auto* bar = new QHBoxLayout();
    btnSave_  = new QPushButton("Save", this);
    btnClose_ = new QPushButton("Close", this);
    bar->addStretch(1);
    bar->addWidget(btnSave_);
    bar->addWidget(btnClose_);
    root->addLayout(bar);

    connect(btnSave_,  &QPushButton::clicked, this, &FormDesignerPage::onSave);
    connect(btnClose_, &QPushButton::clicked, this, &FormDesignerPage::onClose);
}

void FormDesignerPage::loadDef() {
    edName_->setText(formDef_.value("name").toString());
    edTable_->setText(formDef_.value("table").toString());

    listControls_->clear();
    const auto arr = formDef_.value("controls").toArray();
    for (const auto& v : arr) {
        const auto o = v.toObject();
        const QString type  = o.value("type").toString("TextBox");
        const QString field = o.value("field").toString();
        listControls_->addItem(QString("%1  —  %2").arg(field, type));
    }
}

void FormDesignerPage::onSave() {
    const QString newName = edName_->text().trimmed();
    if (newName.isEmpty()) {
        QMessageBox::warning(this, "Forms", "Form name cannot be empty.");
        return;
    }
    formDef_.insert("name", newName);
    if (!forms::saveOrUpdateForm(projectDir_, formDef_)) {
        QMessageBox::warning(this, "Forms", "Could not save form definition.");
        return;
    }
    QMessageBox::information(this, "Forms", "Form saved.");
}

void FormDesignerPage::onClose() {
    emit requestClose(this);
}

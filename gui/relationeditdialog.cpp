#include "relationeditdialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>

RelationEditDialog::RelationEditDialog(const Model& m, QWidget* parent)
    : QDialog(parent), model_(m)
{
    setWindowTitle(tr("Editar relación"));
    buildUi();
}

void RelationEditDialog::buildUi() {
    auto* main = new QVBoxLayout(this);

    auto* info = new QFormLayout();
    lblLeft_  = new QLabel(QString("%1.%2").arg(model_.leftTable,  model_.leftField), this);
    lblRight_ = new QLabel(QString("%1.%2").arg(model_.rightTable, model_.rightField), this);
    info->addRow(tr("Hija (FK):"), lblLeft_);
    info->addRow(tr("Padre (PK):"), lblRight_);
    main->addLayout(info);

    cbType_ = new QComboBox(this);
    cbType_->addItem("1:N");
    cbType_->addItem("1:1");
    int idx = cbType_->findText(model_.relType);
    if (idx < 0) idx = 0;
    cbType_->setCurrentIndex(idx);

    cbEnforce_ = new QCheckBox(tr("Exigir integridad referencial"), this);
    cbEnforce_->setChecked(model_.enforceRI);

    cbCUpd_ = new QCheckBox(tr("ON UPDATE CASCADE"), this);
    cbCUpd_->setChecked(model_.cascadeUpdate);

    cbCDel_ = new QCheckBox(tr("ON DELETE CASCADE"), this);
    cbCDel_->setChecked(model_.cascadeDelete);

    auto* form2 = new QFormLayout();
    form2->addRow(tr("Tipo:"), cbType_);
    form2->addRow(QString(), cbEnforce_);
    form2->addRow(QString(), cbCUpd_);
    form2->addRow(QString(), cbCDel_);
    main->addLayout(form2);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    btnOk_ = new QPushButton(tr("Aceptar"), this);
    btnCancel_ = new QPushButton(tr("Cancelar"), this);
    connect(btnOk_, &QPushButton::clicked, this, &RelationEditDialog::onAccept);
    connect(btnCancel_, &QPushButton::clicked, this, &RelationEditDialog::reject);
    buttons->addWidget(btnOk_);
    buttons->addWidget(btnCancel_);
    main->addLayout(buttons);
}

void RelationEditDialog::onAccept() {
    model_.relType       = cbType_->currentText();
    model_.enforceRI     = cbEnforce_->isChecked();
    model_.cascadeUpdate = cbCUpd_->isChecked();
    model_.cascadeDelete = cbCDel_->isChecked();
    accept();
}

#include "NewTableDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <qpushbutton.h>

NewTableDialog::NewTableDialog(QWidget* parent)
    : QDialog(parent) {

    setWindowTitle("New Table");
    setModal(true);
    resize(360, 120);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(12,12,12,12);
    lay->setSpacing(8);

    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel("Table name:", this));
    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText("e.g. Customers");
    nameEdit_->setText("NewTable");
    nameEdit_->setStyleSheet(
        "QLineEdit {"
        "  color: white;"
        "}"
        );
    row->addWidget(nameEdit_, 1);
    lay->addLayout(row);

    hint_ = new QLabel("Allowed: letters, digits, spaces, _ and -", this);
    lay->addWidget(hint_);

    static QRegularExpression rx(R"(^[A-Za-z0-9 _-]+$)");
    auto* val = new QRegularExpressionValidator(rx, this);
    nameEdit_->setValidator(val);

    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    lay->addWidget(buttons_);

    connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(nameEdit_, &QLineEdit::textChanged, this, [this]{ updateOkEnabled(); });

    updateOkEnabled();
    nameEdit_->setFocus();
    nameEdit_->selectAll();
}

QString NewTableDialog::tableName() const {
    return nameEdit_ ? nameEdit_->text().trimmed() : QString();
}

void NewTableDialog::updateOkEnabled() {
    const bool ok = nameEdit_ && !nameEdit_->text().trimmed().isEmpty() && nameEdit_->hasAcceptableInput();
    if (buttons_) {
        if (auto* okBtn = buttons_->button(QDialogButtonBox::Ok)) okBtn->setEnabled(ok);
    }
}

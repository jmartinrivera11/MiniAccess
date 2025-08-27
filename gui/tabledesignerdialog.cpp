#include "TableDesignerDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QMessageBox>

using namespace ma;

static QStringList typeNames() {
    return {"Int32","String","Bool","Double","CharN"};
}
static FieldType typeFromName(const QString& n) {
    if (n=="Int32") return FieldType::Int32;
    if (n=="String") return FieldType::String;
    if (n=="Bool") return FieldType::Bool;
    if (n=="Double") return FieldType::Double;
    if (n=="CharN") return FieldType::CharN;
    return FieldType::String;
}

TableDesignerDialog::TableDesignerDialog(QWidget* parent): QDialog(parent) {
    setWindowTitle("Table Designer");
    resize(700, 400);
    auto* lay = new QVBoxLayout(this);

    table_ = new QTableWidget(0, 3, this);
    table_->setHorizontalHeaderLabels({"Name","Type","Size (CharN)"});
    table_->horizontalHeader()->setStretchLastSection(true);
    lay->addWidget(table_);

    auto* h = new QHBoxLayout();
    btnAdd_ = new QPushButton("Add Field", this);
    btnRemove_ = new QPushButton("Remove Field", this);
    h->addWidget(btnAdd_);
    h->addWidget(btnRemove_);
    h->addStretch();
    lay->addLayout(h);

    auto* buttons = new QHBoxLayout();
    btnOk_ = new QPushButton("Create", this);
    btnCancel_ = new QPushButton("Cancel", this);
    buttons->addStretch();
    buttons->addWidget(btnOk_);
    buttons->addWidget(btnCancel_);
    lay->addLayout(buttons);

    connect(btnAdd_, &QPushButton::clicked, this, &TableDesignerDialog::addField);
    connect(btnRemove_, &QPushButton::clicked, this, &TableDesignerDialog::removeField);
    connect(btnOk_, &QPushButton::clicked, this, &TableDesignerDialog::acceptDesign);
    connect(btnCancel_, &QPushButton::clicked, this, &QDialog::reject);

    addField(); addField();
    table_->item(0,0)->setText("id");
    auto* c0 = qobject_cast<QComboBox*>(table_->cellWidget(0,1)); if (c0) c0->setCurrentText("Int32");
    table_->item(1,0)->setText("nombre");
    auto* c1 = qobject_cast<QComboBox*>(table_->cellWidget(1,1)); if (c1) c1->setCurrentText("String");
}

void TableDesignerDialog::addField() {
    int r = table_->rowCount();
    table_->insertRow(r);
    table_->setItem(r,0,new QTableWidgetItem());
    auto* combo = new QComboBox(table_);
    combo->addItems(typeNames());
    table_->setCellWidget(r,1, combo);
    table_->setItem(r,2,new QTableWidgetItem("0"));
}

void TableDesignerDialog::removeField() {
    auto sel = table_->selectionModel()->selectedRows();
    for (const auto& idx: sel) table_->removeRow(idx.row());
}

void TableDesignerDialog::acceptDesign() {
    if (table_->rowCount()==0) {
        QMessageBox::warning(this,"Designer","Add at least one field.");
        return;
    }
    Schema s;
    s.tableName = "table";
    for (int r=0;r<table_->rowCount();++r) {
        auto* itName = table_->item(r,0);
        if (!itName || itName->text().trimmed().isEmpty()) {
            QMessageBox::warning(this,"Designer","Field name required.");
            return;
        }
        QString name = itName->text().trimmed();
        auto* combo = qobject_cast<QComboBox*>(table_->cellWidget(r,1));
        FieldType t = FieldType::String;
        if (combo) t = typeFromName(combo->currentText());
        uint16_t size = 0;
        bool ok=false;
        if (t==FieldType::CharN) size = table_->item(r,2)->text().toUShort(&ok);

        s.fields.push_back(Field{name.toStdString(), t, size});
    }
    schema_ = s;
    accept();
}

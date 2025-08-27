#include "QueryBuilderDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>

using namespace ma;

QueryBuilderDialog::QueryBuilderDialog(Table* table, QWidget* parent)
    : QDialog(parent), table_(table), schema_(table->getSchema()) {
    setWindowTitle("Query Builder");
    resize(800, 500);

    auto* lay = new QVBoxLayout(this);

    auto* row = new QHBoxLayout();
    cbField_ = new QComboBox(this);
    for (auto& f: schema_.fields) cbField_->addItem(QString::fromStdString(f.name));
    cbOp_ = new QComboBox(this);
    cbOp_->addItems({"=", "!=", "<", "<=", ">", ">=", "contains", "startsWith"});
    edValue_ = new QLineEdit(this);
    auto* btnRun = new QPushButton("Run", this);

    row->addWidget(cbField_);
    row->addWidget(cbOp_);
    row->addWidget(edValue_);
    row->addWidget(btnRun);
    lay->addLayout(row);

    grid_ = new QTableWidget(this);
    grid_->setColumnCount((int)schema_.fields.size());
    QStringList headers;
    for (auto& f: schema_.fields) headers << QString::fromStdString(f.name);
    grid_->setHorizontalHeaderLabels(headers);
    grid_->horizontalHeader()->setStretchLastSection(true);
    lay->addWidget(grid_);

    connect(btnRun, &QPushButton::clicked, this, &QueryBuilderDialog::runQuery);
}

void QueryBuilderDialog::runQuery() {
    int fidx = cbField_->currentIndex();
    QString op = cbOp_->currentText();
    QString val = edValue_->text();

    auto rids = table_->scanAll();
    std::vector<Record> rows;
    rows.reserve(rids.size());
    for (auto& rid: rids) {
        auto rec = table_->read(rid);
        if (!rec) continue;
        const auto& V = rec->values[fidx];
        if (!V.has_value()) continue;

        bool match=false;
        const auto& var = V.value();
        if (std::holds_alternative<int32_t>(var)) {
            int v = std::get<int32_t>(var);
            int q = val.toInt();
            if (op=="=") match = (v==q);
            else if (op=="!=") match = (v!=q);
            else if (op=="<") match = (v<q);
            else if (op=="<=") match = (v<=q);
            else if (op==">") match = (v>q);
            else if (op==">=") match = (v>=q);
        } else if (std::holds_alternative<double>(var)) {
            double v = std::get<double>(var);
            double q = val.toDouble();
            if (op=="=") match = (v==q);
            else if (op=="!=") match = (v!=q);
            else if (op=="<") match = (v<q);
            else if (op=="<=") match = (v<=q);
            else if (op==">") match = (v>q);
            else if (op==">=") match = (v>=q);
        } else if (std::holds_alternative<bool>(var)) {
            bool v = std::get<bool>(var);
            bool q = (val.compare("true", Qt::CaseInsensitive)==0) || (val.toInt()!=0);
            match = (op=="=" ? v==q : v!=q);
        } else if (std::holds_alternative<std::string>(var)) {
            QString v = QString::fromStdString(std::get<std::string>(var));
            if (op=="=") match = (v==val);
            else if (op=="!=") match = (v!=val);
            else if (op=="contains") match = v.contains(val, Qt::CaseInsensitive);
            else if (op=="startsWith") match = v.startsWith(val, Qt::CaseInsensitive);
            else if (op=="<") match = v < val;
            else if (op=="<=") match = v <= val;
            else if (op==">") match = v > val;
            else if (op==">=") match = v >= val;
        }
        if (match) rows.push_back(*rec);
    }

    grid_->setRowCount((int)rows.size());
    for (int r=0;r<(int)rows.size();++r) {
        for (int c=0;c<(int)schema_.fields.size();++c) {
            const auto& V = rows[r].values[c];
            QString text;
            if (V.has_value()) {
                const auto& var = V.value();
                if (std::holds_alternative<int32_t>(var)) text = QString::number(std::get<int32_t>(var));
                else if (std::holds_alternative<double>(var)) text = QString::number(std::get<double>(var));
                else if (std::holds_alternative<bool>(var)) text = std::get<bool>(var) ? "true" : "false";
                else if (std::holds_alternative<std::string>(var)) text = QString::fromStdString(std::get<std::string>(var));
            }
            grid_->setItem(r,c,new QTableWidgetItem(text));
        }
    }
}

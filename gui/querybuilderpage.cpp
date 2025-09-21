#include "querybuilderpage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileInfo>
#include <QDir>
#include <QTableView>
#include <QMessageBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QSet>
#include <QItemSelectionModel>
#include "../core/Table.h"
#include "../core/Schema.h"
#include "../core/DisplayFmt.h"
#include "QueryModel.h"

using namespace ma;

static QString baseFromMeta(const QString& metaPath) {
    QString p = metaPath;
    if (p.endsWith(".meta")) p.chop(5);
    return p;
}

QueryBuilderPage::QueryBuilderPage(const QString& projectDir, QWidget* parent)
    : QWidget(parent), projectDir_(projectDir) {
    setupUi();
    loadTables();
}

void QueryBuilderPage::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8,8,8,8);
    layout->setSpacing(6);

    auto* row1 = new QWidget(this);
    auto* h1 = new QHBoxLayout(row1);
    h1->setContentsMargins(0,0,0,0);
    h1->setSpacing(8);

    h1->addWidget(new QLabel("Table:", row1));
    cbTable_ = new QComboBox(row1);
    h1->addWidget(cbTable_, 1);

    auto* btnClear = new QPushButton("Clear", row1);
    auto* btnRun   = new QPushButton("Run", row1);
    h1->addWidget(btnClear);
    h1->addWidget(btnRun);

    layout->addWidget(row1);

    auto* split = new QSplitter(Qt::Horizontal, this);

    auto* left = new QWidget(split);
    auto* lLay = new QVBoxLayout(left);
    lLay->setContentsMargins(0,0,0,0);
    lLay->setSpacing(6);
    auto* headL = new QHBoxLayout();
    headL->addWidget(new QLabel("Fields", left));
    headL->addStretch();
    lLay->addLayout(headL);

    lwFields_ = new QListWidget(left);
    lwFields_->setSelectionMode(QAbstractItemView::NoSelection);
    lLay->addWidget(lwFields_, 1);

    auto* right = new QWidget(split);
    auto* rLay = new QVBoxLayout(right);
    rLay->setContentsMargins(0,0,0,0);
    rLay->setSpacing(6);

    auto* rowCondHead = new QHBoxLayout();
    rowCondHead->addWidget(new QLabel("Criteria", right));
    auto* btnAdd = new QPushButton("Add", right);
    btnRemove_   = new QPushButton("Remove", right);
    rowCondHead->addStretch();
    rowCondHead->addWidget(btnAdd);
    rowCondHead->addWidget(btnRemove_);
    rLay->addLayout(rowCondHead);

    twConds_ = new QTableWidget(0, 4, right);
    twConds_->setHorizontalHeaderLabels(QStringList() << "Field" << "Operator" << "Value" << "Logic");
    twConds_->horizontalHeader()->setStretchLastSection(true);
    twConds_->verticalHeader()->setVisible(false);
    rLay->addWidget(twConds_);

    tvResult_ = new QTableView(right);
    tvResult_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tvResult_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    rLay->addWidget(tvResult_, 1);

    labInfo_ = new QLabel("Rows: 0", right);
    rLay->addWidget(labInfo_);

    split->addWidget(left);
    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    layout->addWidget(split, 1);

    model_ = new QueryModel(this);
    tvResult_->setModel(model_);

    connect(cbTable_, &QComboBox::currentIndexChanged, this, &QueryBuilderPage::onTableChanged);
    connect(btnAdd,   &QPushButton::clicked,            this, &QueryBuilderPage::onAddCondition);
    connect(btnRemove_, &QPushButton::clicked,          this, &QueryBuilderPage::onRemoveCondition);
    connect(btnRun,   &QPushButton::clicked,            this, &QueryBuilderPage::onRun);
    connect(btnClear, &QPushButton::clicked,            this, &QueryBuilderPage::onClear);

    btnRemove_->setEnabled(false);

    connect(twConds_, &QTableWidget::itemSelectionChanged,
            this,     &QueryBuilderPage::updateRemoveEnabled);
    connect(twConds_, &QTableWidget::currentCellChanged,
            this,     [this](int, int, int, int){ updateRemoveEnabled(); });
}

void QueryBuilderPage::loadTables() {
    cbTable_->clear();
    QDir d(projectDir_.isEmpty()? QDir::currentPath() : projectDir_);
    const auto metas = d.entryList(QStringList() << "*.meta", QDir::Files, QDir::Name);
    for (const auto& m : metas) {
        const QString base = d.absoluteFilePath(m);
        cbTable_->addItem(QFileInfo(base).completeBaseName(), baseFromMeta(base));
    }
    if (cbTable_->count() > 0) onTableChanged(0);
}

void QueryBuilderPage::onTableChanged(int) {
    currentBasePath_ = cbTable_->currentData().toString();
    loadFieldsForCurrent();
}

void QueryBuilderPage::loadFieldsForCurrent() {
    lwFields_->clear();
    columns_.clear();
    twConds_->setRowCount(0);

    if (currentBasePath_.isEmpty()) {
        updateRemoveEnabled();
        return;
    }
    try {
        Table t; t.open(currentBasePath_.toStdString());
        const Schema s = t.getSchema();

        for (int i=0;i<(int)s.fields.size();++i) {
            const auto& f = s.fields[i];
            QListWidgetItem* it = new QListWidgetItem(QString::fromStdString(f.name), lwFields_);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            it->setCheckState(Qt::Checked);
            lwFields_->addItem(it);
            columns_.push_back({QString::fromStdString(f.name), i, (int)f.type, f.size});
        }
    } catch (const std::exception& ex) {
        QMessageBox::warning(this, "Query Builder", QString("Cannot read schema:\n%1").arg(ex.what()));
    }

    updateRemoveEnabled();
}

void QueryBuilderPage::onAddCondition() {
    int r = twConds_->rowCount();
    twConds_->insertRow(r);

    auto* cbField = new QComboBox(twConds_);
    for (const auto& c : columns_) cbField->addItem(c.name, c.index);
    twConds_->setCellWidget(r, 0, cbField);

    auto* cbOp = new QComboBox(twConds_);
    cbOp->addItems(QStringList() << "=" << "!=" << "<" << "<=" << ">" << ">=" << "contains" << "starts with" << "ends with");
    twConds_->setCellWidget(r, 1, cbOp);

    QWidget* valEd = new QLineEdit(twConds_);
    twConds_->setCellWidget(r, 2, valEd);

    auto* cbLogic = new QComboBox(twConds_);
    cbLogic->addItems(QStringList() << "AND" << "OR");
    twConds_->setCellWidget(r, 3, cbLogic);

    setRowEditorTypes(r);
    connect(cbField, &QComboBox::currentIndexChanged, this, [this, r](int){ setRowEditorTypes(r); });

    updateRemoveEnabled();
}

void QueryBuilderPage::setRowEditorTypes(int row) {
    auto* cbField = qobject_cast<QComboBox*>(twConds_->cellWidget(row, 0));
    auto* cbOp    = qobject_cast<QComboBox*>(twConds_->cellWidget(row, 1));
    if (!cbField || !cbOp) return;
    int fieldIdx = cbField->currentData().toInt();
    if (fieldIdx < 0 || fieldIdx >= (int)columns_.size()) return;

    const auto& col = columns_[(size_t)fieldIdx];

    QWidget* old = twConds_->cellWidget(row, 2);
    if (old) old->deleteLater();

    QWidget* val = nullptr;
    if (col.type == (int)FieldType::Bool) {
        auto* cb = new QComboBox(twConds_);
        cb->addItems(QStringList() << "True" << "False");
        val = cb;
        cbOp->clear(); cbOp->addItems(QStringList() << "=" << "!=");
    } else if (col.type == (int)FieldType::Int32 || col.type == (int)FieldType::Double) {
        auto* le = new QLineEdit(twConds_);
        le->setPlaceholderText(col.type == (int)FieldType::Int32 ? "123" : "123.45");
        val = le;
        cbOp->clear(); cbOp->addItems(QStringList() << "=" << "!=" << "<" << "<=" << ">" << ">=");
    } else {
        auto* le = new QLineEdit(twConds_);
        le->setPlaceholderText("text...");
        val = le;
        cbOp->clear(); cbOp->addItems(QStringList() << "=" << "!=" << "contains" << "starts with" << "ends with");
    }
    twConds_->setCellWidget(row, 2, val);
}

void QueryBuilderPage::onRemoveCondition() {
    if (!twConds_ || twConds_->rowCount() == 0) {
        updateRemoveEnabled();
        return;
    }

    QList<int> rows;

    const auto selRows = twConds_->selectionModel() ? twConds_->selectionModel()->selectedRows() : QModelIndexList{};
    for (const auto& idx : selRows) rows << idx.row();

    if (rows.isEmpty() && twConds_->selectionModel()) {
        const auto selIdx = twConds_->selectionModel()->selectedIndexes();
        for (const auto& idx : selIdx) rows << idx.row();
    }

    if (rows.isEmpty()) {
        const int r = twConds_->currentRow();
        if (r >= 0) rows << r;
    }

    if (rows.isEmpty()) {
        QMessageBox::information(this, "Query Builder", "Select one or more condition rows to remove.");
        updateRemoveEnabled();
        return;
    }

    rows = QList<int>(QSet<int>(rows.begin(), rows.end()).values());
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int r : rows) {
        if (r >= 0 && r < twConds_->rowCount())
            twConds_->removeRow(r);
    }

    updateRemoveEnabled();
}

void QueryBuilderPage::onRun() {
    buildAndRun();
}

void QueryBuilderPage::onClear() {
    for (int i=0;i<lwFields_->count();++i) {
        auto* it = lwFields_->item(i);
        it->setCheckState(Qt::Unchecked);
    }
    twConds_->setRowCount(0);
    QueryModel::Spec s;
    model_->run(s);
    labInfo_->setText("Rows: 0");
    updateRemoveEnabled();
}

int QueryBuilderPage::currentFieldIndexByName(const QString& name) const {
    for (const auto& c : columns_) if (c.name == name) return c.index;
    return -1;
}

void QueryBuilderPage::buildAndRun() {
    if (currentBasePath_.isEmpty()) return;

    QueryModel::Spec s;
    s.basePath = currentBasePath_;

    for (int i=0;i<lwFields_->count();++i) {
        auto* it = lwFields_->item(i);
        if (it->checkState() == Qt::Checked) {
            const int src = currentFieldIndexByName(it->text());
            if (src >= 0) s.columns.push_back(src);
        }
    }
    if (s.columns.empty()) {
        for (const auto& c : columns_) s.columns.push_back(c.index);
    }

    for (int r=0; r<twConds_->rowCount(); ++r) {
        auto* cbField = qobject_cast<QComboBox*>(twConds_->cellWidget(r, 0));
        auto* cbOp    = qobject_cast<QComboBox*>(twConds_->cellWidget(r, 1));
        QWidget* valEd= twConds_->cellWidget(r, 2);
        auto* cbLogic = qobject_cast<QComboBox*>(twConds_->cellWidget(r, 3));
        if (!cbField || !cbOp || !valEd || !cbLogic) continue;

        QueryModel::Cond c;
        c.fieldIndex = cbField->currentData().toInt();

        const QString sop = cbOp->currentText();
        if (sop == "=") c.op = QueryModel::Op::EQ;
        else if (sop == "!=") c.op = QueryModel::Op::NE;
        else if (sop == "<")  c.op = QueryModel::Op::LT;
        else if (sop == "<=") c.op = QueryModel::Op::LE;
        else if (sop == ">")  c.op = QueryModel::Op::GT;
        else if (sop == ">=") c.op = QueryModel::Op::GE;
        else if (sop == "contains") c.op = QueryModel::Op::CONTAINS;
        else if (sop == "starts with") c.op = QueryModel::Op::STARTS;
        else if (sop == "ends with")   c.op = QueryModel::Op::ENDS;

        const auto& col = columns_[(size_t)c.fieldIndex];
        if (auto* cb = qobject_cast<QComboBox*>(valEd)) {
            c.value = (cb->currentText().compare("true", Qt::CaseInsensitive)==0);
        } else if (auto* le = qobject_cast<QLineEdit*>(valEd)) {
            if (col.type == (int)FieldType::Int32) c.value = le->text().toInt();
            else if (col.type == (int)FieldType::Double) c.value = le->text().toDouble();
            else c.value = le->text();
        }
        c.andWithNext = (cbLogic->currentText()=="AND");
        s.conds.push_back(std::move(c));
    }

    QString err;
    if (!model_->run(s, &err)) {
        QMessageBox::warning(this, "Query Builder", QString("Query failed:\n%1").arg(err));
        return;
    }
    labInfo_->setText(QString("Rows: %1").arg(model_->rowCount()));
}

void QueryBuilderPage::updateRemoveEnabled() {
    bool any = false;
    if (twConds_ && twConds_->rowCount() > 0) {
        if (auto* sm = twConds_->selectionModel()) {
            if (!sm->selectedRows().isEmpty() || !sm->selectedIndexes().isEmpty())
                any = true;
        }
        if (!any && twConds_->currentRow() >= 0)
            any = true;
    }
    if (btnRemove_) btnRemove_->setEnabled(any);
}

#include "ObjectsDock.h"
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QAction>
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QPushButton>
#include <QMessageBox>
#include <QIcon>
#include "NewTableDialog.h"
#include "../core/Table.h"
#include "../core/Schema.h"
#include <QKeyEvent>

using namespace ma;

ObjectsDock::ObjectsDock(QWidget* parent) : QDockWidget(parent) {
    setWindowTitle("All Access Objects");
    setObjectName("ObjectsDock");

    root_ = new QWidget(this);
    root_->setObjectName("ObjectsDockBody");

    auto* lay = new QVBoxLayout(root_);
    lay->setContentsMargins(6,6,6,6);
    lay->setSpacing(6);

    auto* header = new QWidget(root_);
    auto* hLay   = new QHBoxLayout(header);
    hLay->setContentsMargins(0,0,0,0);
    hLay->setSpacing(6);

    auto* labTables = new QLabel("Tables", header);
    labTables->setStyleSheet("font-weight:600; color:#5a5a5f; margin:6px 2px;");
    hLay->addWidget(labTables);
    hLay->addStretch();

    btnNewTable_ = new QPushButton(QIcon(":/icons/icons/new.svg"), "New Table", header);
    btnNewTable_->setCursor(Qt::PointingHandCursor);
    hLay->addWidget(btnNewTable_);

    lay->addWidget(header);

    search_ = new QLineEdit(root_);
    search_->setPlaceholderText("Search...");
    lay->addWidget(search_);

    list_ = new QListWidget(root_);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setContextMenuPolicy(Qt::CustomContextMenu);
    list_->viewport()->installEventFilter(this);
    lay->addWidget(list_);

    setWidget(root_);

    connect(search_, &QLineEdit::textChanged, this, [this]{ rebuild(); });
    connect(list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it){
        emit openDatasheet(it->data(Qt::UserRole).toString());
    });
    connect(list_, &QListWidget::customContextMenuRequested, this, [this](const QPoint& p){
        auto* it = list_->itemAt(p);
        if (!it) return;
        QString base = it->data(Qt::UserRole).toString();

        QMenu m(this);
        QAction* aData   = m.addAction("Open Datasheet");
        QAction* aDesign = m.addAction("Open Design View");
        m.addSeparator();
        QAction* aDelete = m.addAction(QIcon(":/icons/icons/delete.svg"), "Delete Table...");

        QAction* chosen = m.exec(list_->viewport()->mapToGlobal(p));
        if (chosen == aData)      emit openDatasheet(base);
        else if (chosen == aDesign) emit openDesign(base);
        else if (chosen == aDelete) emit deleteTableRequested(base);
    });
    connect(btnNewTable_, &QPushButton::clicked, this, &ObjectsDock::newTable);

    projectDir_.clear();
    btnNewTable_->setEnabled(false);
    rebuild();
}

void ObjectsDock::setTables(const QStringList& bases) {
    bases_ = bases;
    rebuild();
}

void ObjectsDock::setProjectPath(const QString& dir) {
    projectDir_ = dir;
    rebuild();
}

void ObjectsDock::rebuild() {
    list_->clear();

    const bool hasProject = !projectDir_.isEmpty();
    btnNewTable_->setEnabled(hasProject);

    if (!hasProject) return;

    const QString query = search_->text().trimmed();
    QDir d(projectDir_);
    const auto metas = d.entryList(QStringList() << "*.meta", QDir::Files, QDir::Name);

    for (const auto& m : metas) {
        QString base = d.absoluteFilePath(m);
        if (base.endsWith(".meta")) base.chop(5);

        QString name = QFileInfo(base).completeBaseName();
        if (!query.isEmpty() && !name.contains(query, Qt::CaseInsensitive)) continue;

        auto* it = new QListWidgetItem(name, list_);
        it->setData(Qt::UserRole, base);
        list_->addItem(it);
    }
}

void ObjectsDock::onContextMenuRequested(const QPoint& p) {
    auto* it = list_->itemAt(p);
    if (!it) return;
    QString base = it->data(Qt::UserRole).toString();
    QMenu m(this);
    QAction* aData   = m.addAction("Open Datasheet");
    QAction* aDesign = m.addAction("Open Design View");
    QAction* chosen  = m.exec(list_->viewport()->mapToGlobal(p));
    if (chosen==aData)   emit openDatasheet(base);
    else if (chosen==aDesign) emit openDesign(base);
}

void ObjectsDock::newTable() {
    if (projectDir_.isEmpty()) {
        QMessageBox::information(this, "New Table", "Open or create a Project first.");
        return;
    }

    NewTableDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    const QString name = dlg.tableName();
    if (name.isEmpty()) return;

    QDir d(projectDir_);
    const QString base = d.filePath(name);
    const QString meta = base + ".meta";
    const QString mad  = base + ".mad";

    if (QFileInfo::exists(meta) || QFileInfo::exists(mad)) {
        auto ans = QMessageBox::question(this, "New Table",
                                         "A table with that name already exists.\nDo you want to overwrite it?",
                                         QMessageBox::Yes | QMessageBox::No);
        if (ans != QMessageBox::Yes) return;
        QFile::remove(meta);
        QFile::remove(mad);
    }

    try {
        ma::Schema s; s.tableName = name.toStdString(); s.fields.clear();
        ma::Table t;  t.create(base.toStdString(), s);

        rebuild();
        emit openDesign(base);
    } catch (const std::exception& ex) {
        QMessageBox::warning(this, "New Table",
                             QString("Error creating table:\n%1").arg(ex.what()));
    }
}

QString ObjectsDock::currentSelectedBase() const {
    if (auto* it = list_->currentItem()) {
        return it->data(Qt::UserRole).toString();
    }
    return {};
}

bool ObjectsDock::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == list_->viewport() && ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        if (ke->key() == Qt::Key_Delete) {
            if (auto* it = list_->currentItem()) {
                const QString base = it->data(Qt::UserRole).toString();
                if (!base.isEmpty()) emit deleteTableRequested(base);
                return true;
            }
        }
    }
    return QDockWidget::eventFilter(obj, ev);
}

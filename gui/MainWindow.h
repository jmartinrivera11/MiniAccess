#pragma once
#include <QMainWindow>
#include <memory>
#include "../core/Table.h"

class QTableView;
class QAction;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);
    ~MainWindow();

private slots:
    void newTable();
    void openTable();
    void createIntIndex();
    void createStrIndex();
    void insertRow();
    void deleteSelectedRows();
    void refreshView();
    void openQueryBuilder();
    void openRelationDesigner();

private:
    void setupUi();
    void bindTableToView();
    QString chooseBasePathForNew();
    QString chooseExistingMeta();

private:
    std::unique_ptr<ma::Table> table_;
    ma::Schema schema_;
    QString basePath_;

    QTableView* view_{nullptr};
    class TableModel* model_{nullptr};
    QLabel* statusLabel_{nullptr};

    QAction* actNew_{nullptr};
    QAction* actOpen_{nullptr};
    QAction* actCreateIdxI_{nullptr};
    QAction* actCreateIdxS_{nullptr};
    QAction* actInsert_{nullptr};
    QAction* actDelete_{nullptr};
    QAction* actRefresh_{nullptr};
    QAction* actQuery_{nullptr};
    QAction* actRelation_{nullptr};
};

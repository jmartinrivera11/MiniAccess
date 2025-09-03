#pragma once
#include <QWidget>
#include <QString>
#include <vector>

class QComboBox;
class QListWidget;
class QTableWidget;
class QLabel;
class QTableView;
class QPushButton;

class QueryModel;

class QueryBuilderPage : public QWidget {
    Q_OBJECT
public:
    explicit QueryBuilderPage(const QString& projectDir, QWidget* parent=nullptr);

private slots:
    void onTableChanged(int);
    void onAddCondition();
    void onRemoveCondition();
    void onRun();
    void onClear();
    void onFieldsToggleAll();

private:
    void setupUi();
    void loadTables();
    void loadFieldsForCurrent();
    void buildAndRun();

    int  currentFieldIndexByName(const QString& name) const;
    void setRowEditorTypes(int row);

private:
    QString projectDir_;

    QComboBox*    cbTable_ {nullptr};
    QListWidget*  lwFields_ {nullptr};
    QPushButton*  btnToggleFields_ {nullptr};
    QTableWidget* twConds_ {nullptr};
    QTableView*   tvResult_ {nullptr};
    QLabel*       labInfo_ {nullptr};
    QueryModel*   model_ {nullptr};
    QString       currentBasePath_;
    struct Col { QString name; int index; int type; uint16_t size; };
    std::vector<Col> columns_;
};

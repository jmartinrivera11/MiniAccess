#pragma once
#include <QDialog>
#include "../core/Table.h"

class QComboBox;
class QLineEdit;
class QTableWidget;

class QueryBuilderDialog : public QDialog {
    Q_OBJECT
public:
    QueryBuilderDialog(ma::Table* table, QWidget* parent=nullptr);

private slots:
    void runQuery();

private:
    ma::Table* table_{nullptr};
    ma::Schema schema_;

    QComboBox* cbField_{nullptr};
    QComboBox* cbOp_{nullptr};
    QLineEdit*  edValue_{nullptr};
    QTableWidget* grid_{nullptr};
};

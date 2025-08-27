#pragma once
#include <QDialog>
#include "../core/Schema.h"

class QTableWidget;
class QPushButton;
class QComboBox;

class TableDesignerDialog : public QDialog {
    Q_OBJECT
public:
    explicit TableDesignerDialog(QWidget* parent=nullptr);
    ma::Schema schema() const { return schema_; }

private slots:
    void addField();
    void removeField();
    void acceptDesign();

private:
    QTableWidget* table_{nullptr};
    QPushButton* btnAdd_{nullptr};
    QPushButton* btnRemove_{nullptr};
    QPushButton* btnOk_{nullptr};
    QPushButton* btnCancel_{nullptr};
    ma::Schema schema_;
};

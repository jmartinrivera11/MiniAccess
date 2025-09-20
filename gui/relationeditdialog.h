#pragma once
#include <QDialog>
#include <QString>

class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;

class RelationEditDialog : public QDialog {
    Q_OBJECT
public:
    struct Model {
        QString leftTable;
        QString leftField;
        QString rightTable;
        QString rightField;
        QString relType;
        bool enforceRI{false};
        bool cascadeUpdate{false};
        bool cascadeDelete{false};
    };
    explicit RelationEditDialog(const Model& m, QWidget* parent = nullptr);
    Model result() const { return model_; }

private slots:
    void onAccept();

private:
    void buildUi();
    Model model_;
    QLabel* lblLeft_{nullptr};
    QLabel* lblRight_{nullptr};
    QComboBox* cbType_{nullptr};
    QCheckBox* cbEnforce_{nullptr};
    QCheckBox* cbCUpd_{nullptr};
    QCheckBox* cbCDel_{nullptr};
    QPushButton* btnOk_{nullptr};
    QPushButton* btnCancel_{nullptr};
};

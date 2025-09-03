#pragma once
#include <QDialog>

class QLineEdit;
class QLabel;
class QDialogButtonBox;

class NewTableDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewTableDialog(QWidget* parent = nullptr);

    QString tableName() const;

private:
    void updateOkEnabled();

private:
    QLineEdit* nameEdit_ = nullptr;
    QLabel*    hint_     = nullptr;
    QDialogButtonBox* buttons_ = nullptr;
};

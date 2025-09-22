#pragma once
#include <QWidget>
#include <QJsonObject>

class QLineEdit;
class QListWidget;
class QPushButton;

class FormDesignerPage : public QWidget {
    Q_OBJECT
public:
    explicit FormDesignerPage(const QString& projectDir,
                              const QJsonObject& formDef,
                              QWidget* parent=nullptr);

signals:
    void requestClose(QWidget* page);

private slots:
    void onSave();
    void onClose();

private:
    void buildUi();
    void loadDef();

    QString projectDir_;
    QJsonObject formDef_;

    QLineEdit*   edName_{nullptr};
    QLineEdit*   edTable_{nullptr};
    QListWidget* listControls_{nullptr};
    QPushButton* btnSave_{nullptr};
    QPushButton* btnClose_{nullptr};
};

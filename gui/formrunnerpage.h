#pragma once
#include <QWidget>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>

class QLineEdit;
class QCheckBox;
class QDateEdit;
class QPushButton;
class QLabel;

class FormRunnerPage : public QWidget {
    Q_OBJECT
public:
    explicit FormRunnerPage(const QString& projectDir,
                            const QJsonObject& formDef,
                            QWidget* parent=nullptr);

signals:
    void requestClose(QWidget* page);

public slots:
    bool insertRow();
    bool deleteRows();
    bool refresh();
    bool zoomInView();
    bool zoomOutView();
    double zoom() const { return 1.0; }

private slots:
    void onFirst();
    void onPrev();
    void onNext();
    void onLast();
    void onAdd();
    void onDelete();
    void onSave();
    void onClose();

    void onFieldEdited();

private:
    void buildUi();
    void rebuildControls();
    void bindRecordToUi();
    void pullUiToRecord();

    bool loadData();
    bool saveData();

    QString projectDir_;
    QJsonObject formDef_;
    QString formName_;
    QString baseTable_;

    QJsonArray data_;
    int current_{-1};
    bool dirty_{false};

    QMap<QString, QWidget*> editors_;

    QWidget* scrollBody_{nullptr};
    QLabel*  infoLabel_{nullptr};
    QPushButton* btnFirst_{nullptr};
    QPushButton* btnPrev_{nullptr};
    QPushButton* btnNext_{nullptr};
    QPushButton* btnLast_{nullptr};
    QPushButton* btnAdd_{nullptr};
    QPushButton* btnDel_{nullptr};
    QPushButton* btnSave_{nullptr};
    QPushButton* btnClose_{nullptr};
};

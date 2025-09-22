#pragma once
#include <QWidget>
#include <QMap>
#include <QJsonObject>
#include <memory>

class QLineEdit;
class QCheckBox;
class QDateEdit;
class QPushButton;
class QLabel;

namespace ma {
struct Schema;
struct Field;
class Table;
}

class TableModel;

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

    void bindRowToUi(int row);
    void commitField(const QString& fieldName);

    int  rowCount() const;
    int  fieldColumn(const QString& fieldName) const;

    QString projectDir_;
    QJsonObject formDef_;
    QString formName_;
    QString baseTable_;
    QString basePath_;

    std::unique_ptr<ma::Table>  table_;
    std::unique_ptr<TableModel> model_;
    std::unique_ptr<ma::Schema> schema_;

    int  current_{-1};
    bool dirty_{false};

    QMap<QString, QWidget*> editors_;

    QWidget*     scrollBody_{nullptr};
    QLabel*      infoLabel_{nullptr};
    QPushButton* btnFirst_{nullptr};
    QPushButton* btnPrev_{nullptr};
    QPushButton* btnNext_{nullptr};
    QPushButton* btnLast_{nullptr};
    QPushButton* btnAdd_{nullptr};
    QPushButton* btnDel_{nullptr};
    QPushButton* btnSave_{nullptr};
    QPushButton* btnClose_{nullptr};
};

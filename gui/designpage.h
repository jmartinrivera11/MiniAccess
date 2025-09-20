#pragma once
#include <QWidget>
#include <QStringList>
#include "../core/Schema.h"

class QTableWidget;
class QTableWidgetItem;
class QStackedWidget;
class QLabel;

class DesignPage : public QWidget {
    Q_OBJECT
public:
    explicit DesignPage(const QString& basePath, QWidget* parent=nullptr);

private slots:
    void addField();
    void removeField();
    void onTypeChanged(int row);
    void saveDesign();
    void onComboTypeChanged(const QString&);
    void onGridItemChanged(QTableWidgetItem* it);

private:
    void setupUi();
    void loadSchema();
    void setSchema(const ma::Schema& s);
    ma::Schema collectSchema(bool* ok) const;
    bool isTableEmpty() const;
    void updateLastNamesBuffer();

private:
    QString        basePath_;
    QTableWidget*  grid_{nullptr};
    QStackedWidget* props_{nullptr};
    QLabel*        banner_{nullptr};
    QStringList     lastNames_;
};

#pragma once
#include <QWidget>
#include <QString>
#include <memory>

class QTableView;
class QLabel;

namespace ma { class Table; }

class TableModel;

class DatasheetPage : public QWidget {
    Q_OBJECT
public:
    explicit DatasheetPage(const QString& basePath, QWidget* parent=nullptr);

public slots:
    void insertRow();
    void deleteRows();
    void refresh();

    void zoomInView();
    void zoomOutView();
    void setZoom(double factor);
    double zoom() const { return zoom_; }

private:
    void setupUi();
    QString basePath_;
    std::unique_ptr<ma::Table> table_;
    TableModel* model_ {nullptr};
    QTableView* view_ {nullptr};
    QLabel* info_ {nullptr};
    double basePt_ { 0.0 };
    double zoom_   { 1.0 };
    void applyZoom();
};

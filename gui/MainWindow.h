#pragma once
#include "../core/Table.h"
#include <QMainWindow>
#include <QHash>
#include <QPointer>
#include <memory>

class QTabWidget;
class QLabel;

class ObjectsDock;
class DatasheetPage;
class DesignPage;
class QueryBuilderPage;
class RelationDesignerPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);
    ~MainWindow();

private slots:
    void openFromDockDatasheet(const QString& basePath);
    void openFromDockDesign(const QString& basePath);
    void openQueryBuilder();
    void openRelationDesigner();
    void newProject();
    void openProject();
    void closeCurrentProject();
    void zoomIn();
    void zoomOut();
    void insertRecord();
    void deleteRecord();
    void refreshView();
    void deleteCurrentProject();
    void deleteTableByBase(const QString& base);
    void deleteSelectedTable();

private:
    void setupUi();
    int  ensureDatasheetTab(const QString& basePath);
    int  ensureDesignTab(const QString& basePath);
    void refreshDock();
    QString currentProjectPath_;
    void applyGlobalZoomRefresh();
    void setProjectPathAndReload(const QString& dir);
    bool firstShow_ = true;
    QPointer<QWidget> queryBuilderTab_;
    QPointer<QWidget> relationDesignerTab_;
    void closeTabsForBase(const QString& base);
    bool removeTableFiles(const QString& base);
    bool isMiniAccessProjectDir(const QString& dir) const;

private:
    QTabWidget* tabs_{nullptr};
    ObjectsDock* dock_{nullptr};
    QLabel* statusLabel_{nullptr};
    QString projectDir_;
    QHash<QString, QPointer<QWidget>> openDatasheets_;
    QHash<QString, QPointer<QWidget>> openDesigns_;

protected:
    void showEvent(QShowEvent* e) override;
};

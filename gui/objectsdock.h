#pragma once
#include <QDockWidget>
#include <QStringList>

class QListWidget;
class QLineEdit;
class QPushButton;

class ObjectsDock : public QDockWidget {
    Q_OBJECT
public:
    explicit ObjectsDock(QWidget* parent=nullptr);
    void setProjectPath(const QString& dir);
    void setTables(const QStringList& bases);
    QString currentSelectedBase() const;

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

signals:
    void openDatasheet(const QString& basePath);
    void openDesign(const QString& basePath);
    void deleteTableRequested(const QString& basePath);

private slots:
    void newTable();
    void onContextMenuRequested(const QPoint& p);

private:
    void rebuild();

private:
    QString      projectDir_;
    QWidget*     root_{nullptr};
    QLineEdit*   search_{nullptr};
    QListWidget* list_{nullptr};
    QPushButton* btnNewTable_{nullptr};
    QStringList  bases_;
};

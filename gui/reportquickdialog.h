#pragma once
#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QSet>

class QScrollArea;
class QWidget;
class QVBoxLayout;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QLabel;

class ReportQuickDialog : public QDialog {
    Q_OBJECT
public:
    explicit ReportQuickDialog(const QString& projectDir, QWidget* parent=nullptr);
    ~ReportQuickDialog() override;

private slots:
    void onExportPdf();

private:
    QString     projectDir_;
    QLineEdit*   edtTitle_{nullptr};
    QScrollArea* scroll_{nullptr};
    QWidget*     panel_{nullptr};
    QVBoxLayout* panelLayout_{nullptr};
    QPushButton* btnExport_{nullptr};
    QPushButton* btnClose_{nullptr};
    QLabel*      lblHint_{nullptr};
    QCheckBox*   chkRelationships_{nullptr};

    QMap<QString, QCheckBox*> tableChecks_;

    void buildUi();
    void loadTables();
    void applyCheckboxStyle();
    void refreshRelationshipsCheckbox();

    struct ColInfo {
        QString name;
        int index = -1;
    };
    bool readSchema(const QString& tableName, QVector<ColInfo>& cols);
    bool readAllRows(const QString& tableName,
                     const QVector<ColInfo>& cols,
                     QVector<QStringList>& outRows,
                     qsizetype& totalRows,
                     QString& whyNot);

    struct RelRow {
        QString leftTable, leftField, relType, rightTable, rightField;
        bool enforceRI=false, cascadeUpdate=false, cascadeDelete=false;
    };
    bool readRelationsForTables(const QSet<QString>& include, QVector<RelRow>& out, QString& whyNot) const;

    QString buildHtmlMulti(const QString& title,
                           const QStringList& tablesInOrder,
                           const QMap<QString, QVector<ColInfo>>& colsByTable,
                           const QMap<QString, QVector<QStringList>>& rowsByTable,
                           const QMap<QString, qsizetype>& totalRowsByTable,
                           const QVector<RelRow>& rels) const;

    bool exportHtmlPdf(const QString& outFile,
                       const QString& title,
                       const QStringList& tablesInOrder,
                       const QMap<QString, QVector<ColInfo>>& colsByTable,
                       const QMap<QString, QVector<QStringList>>& rowsByTable,
                       const QMap<QString, qsizetype>& totalRowsByTable,
                       const QVector<RelRow>& rels,
                       QString& whyNot) const;

    static QString baseForTable(const QString& projectDir, const QString& tableName);
    static QString escapeHtml(const QString& s);
};

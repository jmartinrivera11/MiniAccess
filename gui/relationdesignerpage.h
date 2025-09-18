#pragma once
#include <QWidget>
#include <QMap>
#include <QPointF>
#include <QStringList>
#include <QVector>
#include "../core/metadata.h"  // lector de .meta con typeId/size

class QGraphicsScene;
class QGraphicsView;
class QListWidget;
class QComboBox;
class QCheckBox;
class QPushButton;
class QTableWidget;
class QGraphicsLineItem;
class QGraphicsTextItem;

class RelationDesignerPage : public QWidget {
    Q_OBJECT
public:
    explicit RelationDesignerPage(const QString& projectDir, QWidget* parent = nullptr);
    ~RelationDesignerPage();

private:
    // ---------- Modelos visuales internos ----------
    class CanvasView;  // vista que acepta drops
    class TableBox;    // caja de tabla con campos
    class FieldItem;   // fila de campo (selección propia)

    struct VisualRelation {
        QString leftTable, leftField;
        QString rightTable, rightField;
        QString relType;                // "1:1" | "1:N" | "N:M"
        bool enforceRI{false};
        bool cascadeUpdate{false};
        bool cascadeDelete{false};
        QGraphicsLineItem* line{nullptr};
        QGraphicsTextItem* labelLeft{nullptr};   // "1" o "N"
        QGraphicsTextItem* labelRight{nullptr};  // "1" o "N"
        FieldItem* leftItem{nullptr};
        FieldItem* rightItem{nullptr};
    };

private slots:
    void onBtnCreateRelation();
    void onBtnDeleteRelation();
    void onBtnSave();

private:
    // ---------- Construcción ----------
    void buildUi();
    void loadTablesList();

    // ---------- Utilidades proyecto / esquema ----------
    QStringList allTablesInProject() const;
    QStringList fieldsForTable(const QString& table);      // usa metadata (cache)
    QString     primaryKeyForTable(const QString& table);  // usa metadata (cache)
    quint16     typeIdFor(const QString& table, const QString& field);

    // ---------- API para CanvasView ----------
    void addTableBoxAt(const QString& table, const QPointF& scenePos);

    // ---------- Selección de campos ----------
    void onFieldClicked(FieldItem* fi);       // control central de selección
    void clearFieldSelection();
    FieldItem* pickFirstSelected() const;
    FieldItem* pickSecondSelected() const;

    // ---------- Relaciones ----------
    bool canFormRelation(const QString& type, const QString& lt, const QString& lf,
                         const QString& rt, const QString& rf, QString& whyNot) const;
    void drawRelation(VisualRelation& vr);
    void updateRelationGeometry(VisualRelation& vr);
    void updateRelationsForTable(const QString& tableName);
    void addRelationToGrid(const VisualRelation& vr);
    void removeRelationVisualOnly(int idx);
    int  findRelationIndex(const QString& lt, const QString& lf,
                          const QString& rt, const QString& rf, const QString& type) const;

    // ---------- N:M (tabla intermedia) ----------
    bool ensureJunctionTable(const QString& aTable, const QString& bTable,
                             QString& junctionName,
                             QString& fkAName, QString& fkBName);
    bool writeMetaUtf8Len(const QString& tableName,
                          const QVector<meta::FieldInfo>& fields);

    // ---------- JSON v2 ----------
    QString jsonPath() const;
    bool saveToJsonV2() const;
    bool loadFromJsonV2();
    bool migrateJsonIfNeeded() const;

private:
    // ---------- UI ----------
    QGraphicsScene* scene_{nullptr};
    CanvasView*     view_{nullptr};
    QListWidget*    tablesList_{nullptr};

    QComboBox*  cbRelType_{nullptr};     // 1:1 | 1:N | N:M
    QCheckBox*  cbEnforce_{nullptr};
    QCheckBox*  cbCascadeUpd_{nullptr};
    QCheckBox*  cbCascadeDel_{nullptr};
    QPushButton* btnCreate_{nullptr};
    QPushButton* btnDelete_{nullptr};
    QPushButton* btnSave_{nullptr};
    QTableWidget* relationsGrid_{nullptr};

    // ---------- Datos ----------
    QString projectDir_;
    QMap<QString, TableBox*> boxes_;     // tabla -> caja
    QVector<VisualRelation>  relations_; // relaciones vivas

    // selección estricta: máx 2 campos, 1 por tabla
    QVector<FieldItem*> selected_;

    // cache de esquemas
    QMap<QString, meta::TableMeta> schemas_; // table -> schema
};

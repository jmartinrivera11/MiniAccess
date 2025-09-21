#pragma once
#include <QWidget>
#include <QMap>
#include <QPointF>
#include <QStringList>
#include <QVector>
#include <QVariant>
#include <functional>

#include "../core/metadata.h"

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

    using Row  = QVector<QVariant>;
    using Rows = QVector<Row>;
    using ProveedorFilas = std::function<Rows (const QString& tabla)>;
    void setProveedorFilas(ProveedorFilas pf) { rowProvider_ = std::move(pf); }

    using IsTableOpenFn = std::function<bool (const QString& tabla)>;
    void setIsTableOpen(IsTableOpenFn fn) { isTableOpen_ = std::move(fn); }

    bool canDeleteTable(const QString& table, QString* why = nullptr) const;
    bool canDeleteField(const QString& table, const QString& field, QString* why = nullptr) const;

    bool applyRenameTable(const QString& oldName, const QString& newName, QString* why = nullptr);
    bool applyRenameField(const QString& table, const QString& oldField, const QString& newField, QString* why = nullptr);

    void revalidateAllRelations();
    void refreshTableBox(const QString& table);

signals:
    void relacionCreada();
    void relacionEliminada();
    void relationsChanged();

private slots:
    void onBtnCreateRelation();
    void onBtnDeleteRelation();
    void onBtnSave();
    void onGridCellDoubleClicked(int row, int col);

private:
    void buildUi();
    void loadTablesList();

    QStringList allTablesInProject() const;
    QStringList fieldsForTable(const QString& table);
    QString     primaryKeyForTable(const QString& table);
    quint16     typeIdFor(const QString& table, const QString& field);
    quint16     sizeFor(const QString& table, const QString& field);
    int         indiceColumna(const QString& table, const QString& field) const;

    Rows readRowsFromStorage(const QString& table) const;

    void addTableBoxAt(const QString& table, const QPointF& scenePos);
    void removeTableBox(const QString& table);
    class CanvasView;
    class TableBox;
    class FieldItem;
    FieldItem* findFieldItem(TableBox* box, const QString& name) const;

    void onFieldClicked(FieldItem* fi);
    void clearFieldSelection();
    FieldItem* pickFirstSelected() const;
    FieldItem* pickSecondSelected() const;

    struct VisualRelation {
        QString leftTable, leftField;
        QString rightTable, rightField;
        QString relType;
        bool enforceRI{false};
        bool cascadeUpdate{false};
        bool cascadeDelete{false};
        QGraphicsLineItem*  line{nullptr};
        QGraphicsTextItem*  labelLeft{nullptr};
        QGraphicsTextItem*  labelRight{nullptr};
        FieldItem* leftItem{nullptr};
        FieldItem* rightItem{nullptr};
    };

    bool canFormRelation(const QString& type, const QString& lt, const QString& lf,
                         const QString& rt, const QString& rf, QString& whyNot) const;

    bool validarDatosExistentes(const QString& tablaOrigen,  const QString& campoOrigen,
                                const QString& tablaDestino, const QString& campoDestino,
                                const QString& tipoRelacion, QString& whyNot) const;

    bool validarValorFK(const QString& tablaDestino,
                        const QString& campoDestino,
                        const QString& valor,
                        QString* outError = nullptr) const;

    void drawRelation(VisualRelation& vr);
    void updateRelationGeometry(VisualRelation& vr);
    void updateRelationsForTable(const QString& tableName);
    void addRelationToGrid(const VisualRelation& vr);
    void refreshGridRow(int idx);
    void removeRelationVisualOnly(int idx);
    int  findRelationIndex(const QString& lt, const QString& lf,
                          const QString& rt, const QString& rf,
                          const QString& type) const;

    QString jsonPath() const;
    bool saveToJsonV2() const;
    bool loadFromJsonV2();
    bool migrateJsonIfNeeded() const;

    bool tableIsOpen(const QString& table) const { return isTableOpen_ ? isTableOpen_(table) : false; }
    void rebindPointersForTable(const QString& table);

private:
    QGraphicsScene* scene_{nullptr};
    CanvasView*     view_{nullptr};
    QListWidget*    tablesList_{nullptr};

    QComboBox*  cbRelType_{nullptr};
    QCheckBox*  cbEnforce_{nullptr};
    QCheckBox*  cbCascadeUpd_{nullptr};
    QCheckBox*  cbCascadeDel_{nullptr};
    QPushButton* btnCreate_{nullptr};
    QPushButton* btnDelete_{nullptr};
    QPushButton* btnSave_{nullptr};
    QTableWidget* relationsGrid_{nullptr};

    QString projectDir_;
    QMap<QString, TableBox*> boxes_;
    QVector<VisualRelation>  relations_;
    QVector<FieldItem*>      selected_;
    QMap<QString, meta::TableMeta> schemas_;

    ProveedorFilas rowProvider_{};
    IsTableOpenFn  isTableOpen_{};
};

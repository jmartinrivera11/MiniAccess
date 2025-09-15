#pragma once
#include <QWidget>
#include <QMap>
#include <QPointer>
#include <QVector>

class QGraphicsScene;
class QGraphicsView;
class QGraphicsRectItem;
class QGraphicsPathItem;
class QGraphicsSimpleTextItem;
class QTableWidget;
class QComboBox;
class QCheckBox;
class QPushButton;

namespace ma { struct Schema; }

class RelationDesignerPage : public QWidget {
    Q_OBJECT
public:
    explicit RelationDesignerPage(const QString& projectDir, QWidget* parent=nullptr);

private slots:
    void onLeftTableChanged(const QString& t);
    void onRightTableChanged(const QString& t);
    void onAddRelation();
    void onRemoveSelected();
    void onSave();
    void rebuildFieldsFor(const QString& table, QComboBox* fieldCombo);
    void onSceneChanged();
    void zoomIn();
    void zoomOut();

private:
    struct Node {
        QString table;
        QGraphicsRectItem* box = nullptr;
        QMap<QString, QGraphicsSimpleTextItem*> fieldLabels;
    };
    struct Rel {
        QString lt, lf;
        QString rt, rf;
        QString type;
        bool cascadeDel = false;
        bool cascadeUpd = false;
        QGraphicsPathItem* link = nullptr;
    };

    void buildUi();
    void loadTables();
    void layoutNodes();
    void loadFromJson();
    void saveToJson();
    void addRelation(const Rel& r);
    void removeRelationAtRow(int row);
    void redrawAllLinks();
    QPointF fieldAnchor(const QString& table, const QString& field) const;
    QString jsonPath() const;
    void loadRelations();
    void saveRelations();

private:
    QString projectDir_;
    QGraphicsScene* scene_ = nullptr;
    QGraphicsView*  view_  = nullptr;

    QComboBox* leftTable_ = nullptr;
    QComboBox* leftField_ = nullptr;
    QComboBox* rightTable_ = nullptr;
    QComboBox* rightField_ = nullptr;
    QComboBox* joinType_ = nullptr;
    QCheckBox* cbCascadeDel_ = nullptr;
    QCheckBox* cbCascadeUpd_ = nullptr;
    QPushButton* btnAdd_ = nullptr;
    QPushButton* btnRemove_ = nullptr;
    QPushButton* btnSave_ = nullptr;
    QTableWidget* grid_ = nullptr;

    QMap<QString, ma::Schema> schemas_;
    QMap<QString, Node*> nodes_;
    QVector<Rel> relations_;
    QMap<QString, QString> tableBase_;
};

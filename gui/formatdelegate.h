#pragma once
#include <QStyledItemDelegate>
class QTableWidget;

class FormatDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit FormatDelegate(QTableWidget* owner, QObject* parent=nullptr);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& opt,
                          const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& opt,
                              const QModelIndex& index) const override;

private:
    QTableWidget* grid_{nullptr};
    QString typeAt(const QModelIndex& index) const;
    static QString labelFor(const QString& typeName, int code);
};

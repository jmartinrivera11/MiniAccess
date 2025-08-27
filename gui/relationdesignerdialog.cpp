#include "RelationDesignerDialog.h"
#include <QVBoxLayout>
#include <QLabel>

RelationDesignerDialog::RelationDesignerDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Relation Designer");
    resize(500, 200);
    auto* lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel(
        "Relation Designer (preview)\n\n"
        "- Aquí podrás definir FKs entre tablas, políticas RESTRICT/CASCADE,\n"
        "- y validarlas. La lógica se conecta en Fase 3.\n\n"
        "Por ahora es un shell visual listo para integrar.",
        this));
}

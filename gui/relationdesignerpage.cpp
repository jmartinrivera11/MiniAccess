#include "RelationDesignerPage.h"
#include <QVBoxLayout>
#include <QLabel>

RelationDesignerPage::RelationDesignerPage(QWidget* parent) : QWidget(parent) {
    auto* lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel(
        "Relation Designer (preview)"
        ));
}

#include "Theme.h"
#include <QFontDatabase>

namespace gui {

void applyAppTheme(QApplication& app) {
    const char* RED  = "#d32f2f";  // rojo
    const char* WHITE= "#ffffff";  // texto principal
    const char* LG   = "#f2f2f2";  // gris claro
    const char* DG   = "#2b2b2b";  // gris oscuro
    const char* MG   = "#3a3a3a";  // gris medio

    QString robotoFamily = "Roboto";
    {
        int id = QFontDatabase::addApplicationFont(":/fonts/fonts/Roboto-Medium.ttf");
        if (id >= 0) {
            const auto fams = QFontDatabase::applicationFontFamilies(id);
            if (!fams.isEmpty()) robotoFamily = fams.first();
        }
    }
    QFont base(robotoFamily, 12);
    app.setFont(base);

    QString qss = QString(R"(
        /* Fuente y colores por defecto */
        * {
            font-family: '%1', 'Segoe UI', 'Arial';
            color: %2;
        }
        QMainWindow, QWidget { background: %4; }

        /* Menus */
        QMenuBar {
            background: %4;
        }
        QMenuBar::item {
            background: transparent;
            padding: 6px 10px;
        }
        QMenuBar::item:selected {
            background: %3;
            color: %2;
        }
        QMenu {
            background: %4;
            border: 1px solid %5;
        }
        QMenu::item {
            padding: 6px 14px;
        }
        QMenu::item:selected {
            background: %3;
        }

        /* ToolBar & StatusBar */
        QToolBar {
            background: %4;
            border: 0;
            spacing: 6px;
        }
        QStatusBar {
            background: %4;
            color: %2;
        }

        /* Botones */
        QPushButton {
            background: %5;
            border: 1px solid #555;
            border-radius: 6px;
            padding: 6px 12px;
        }
        QPushButton:hover {
            border-color: %3;
        }
        QPushButton:pressed {
            background: #2f2f2f;
        }
        QPushButton:default {
            background: %3;
            color: %2;
            border: none;
        }

        /* Controles de texto */
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            background: %5;
            border: 1px solid #555;
            border-radius: 6px;
            padding: 5px 8px;
            selection-background-color: %3;
            selection-color: %2;
        }
        QComboBox QAbstractItemView {
            background: %5;
            selection-background-color: %3;
            selection-color: %2;
        }

        /* Tabla */
        QTableView {
            background: %4;
            alternate-background-color: #333333;
            gridline-color: #444444;
            selection-background-color: %3;
            selection-color: %2;
            border: 1px solid #444;
        }
        QHeaderView::section {
            background: %5;
            color: %2;
            padding: 6px 8px;
            border: 0;
            border-right: 1px solid #4a4a4a;
        }

        /* Scrollbars (simple) */
        QScrollBar:vertical, QScrollBar:horizontal {
            background: %4;
        }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: %5;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover {
            background: #4a4a4a;
        }

        /* Diálogos */
        QDialog {
            background: %4;
        }
    )")
                      .arg(robotoFamily)
                      .arg(WHITE)
                      .arg(RED)
                      .arg(DG)
                      .arg(MG);

    app.setStyleSheet(qss);
}

}

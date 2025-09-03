#include "Theme.h"
#include <QFontDatabase>
#include <QApplication>
#include <QStringList>

namespace gui {

static const char* BG_APP    = "#ededf0"; // fondo general ligeramente más oscuro
static const char* BG_SURF   = "#ffffff"; // superficies (hoja/datasheet)

static const char* BG_RIB_L  = "#c0c0c5"; // ribbon (arriba)
static const char* BG_RIB_D  = "#b5b5ba"; // ribbon (abajo)
static const char* BG_NAV    = "#aeafb7"; // panel izquierdo (cuerpo)
static const char* BG_NAV_L  = "#b8b8bf"; // panel izquierdo (título)
static const char* BORD      = "#9a9ca6"; // bordes / gridlines

static const char* BG_HOV    = "#dcdde2"; // hover un paso más marcado
static const char* BG_PUSH   = "#d2d3da"; // pressed

static const char* TXT       = "#1f1f1f"; // texto principal
static const char* TXT_DIM   = "#505055"; // texto tenue un poco más oscuro
static const char* TXT_INV   = "#ffffff"; // texto invertido

static const char* ACCENT    = "#c83a3a"; // rojo Access
static const char* ACCENT_D  = "#9e2f2f"; // rojo más oscuro

static QString loadFallbackFont() {
    const QStringList candidates = {
        ":/fonts/Roboto-Regular.ttf",
        ":/fonts/Roboto-Medium.ttf",
        ":/fonts/fonts/Roboto-Regular.ttf",
        ":/fonts/fonts/Roboto-Medium.ttf"
    };
    for (const auto& p : candidates) {
        int id = QFontDatabase::addApplicationFont(p);
        if (id >= 0) {
            const auto fams = QFontDatabase::applicationFontFamilies(id);
            if (!fams.isEmpty()) return fams.first();
        }
    }
    return {};
}

void applyAppTheme(QApplication& app) {
    QString family = "Segoe UI";
    if (!QFontDatabase::families().contains(family, Qt::CaseInsensitive)) {
        family = "JetBrainsMono";
    }

    if (!QFontDatabase::families().contains("JetBrainsMono", Qt::CaseInsensitive)) {
        const int id = QFontDatabase::addApplicationFont(":/fonts/fonts/JetBrainsMono-Medium.ttf");
        if (id >= 0) {
            const auto fams = QFontDatabase::applicationFontFamilies(id);
            if (!fams.isEmpty()) family = fams.first();
        }
    }

    QFont base(family);
    base.setPointSizeF(11.0);
    app.setFont(base);

    const QString qss = QString(R"(
/* ===== Base ===== */
* { color:%1; font-family:'%2','Segoe UI','Arial'; }
QMainWindow, QWidget { background:%3; }

/* ===== Ribbon superior (QToolBar identificado) ===== */
QToolBar#MainRibbon {
  background: qlineargradient(x1:0,y1:0, x2:0,y2:1, stop:0 %4, stop:1 %5);
  border-bottom:1px solid %8;
  padding:4px 6px; spacing:6px; min-height:88px;
}
QToolBar#MainRibbon::separator { background:%8; width:1px; margin:0 8px; }
QToolBar#MainRibbon QToolButton {
  background:transparent; border:1px solid transparent; padding:6px 10px; border-radius:6px;
}
QToolBar#MainRibbon QToolButton:hover   { background:%6; border-color:%8; }
QToolBar#MainRibbon QToolButton:pressed { background:%7; border-color:%8; }
QToolBar#MainRibbon QToolButton:checked { background:%7; border-color:%8; }
QLabel#RibbonGroupLabel { color:%9; font-size:9pt; padding-top:2px; }

/* Menú / Status */
QMenuBar { background:%4; border-bottom:1px solid %8; }
QMenuBar::item { padding:6px 10px; background:transparent; }
QMenuBar::item:selected { background:%6; }
QMenu { background:%4; border:1px solid %8; }
QMenu::item { padding:6px 14px; }
QMenu::item:selected { background:%6; }
QStatusBar { background:%4; border-top:1px solid %8; color:%9; }

/* ===== Dock izquierdo (identificado) ===== */
QDockWidget#ObjectsDock {
  background:%3;
  border-right:1px solid %8;
}
QDockWidget#ObjectsDock::title {
  background:%10; padding:6px 8px; color:%1; border-bottom:1px solid %8;
}

/* Cuerpo del dock (barrido en grises) */
#ObjectsDockBody { background:%11; }
QDockWidget#ObjectsDock QLineEdit {
  background:%4; border:1px solid %8; border-radius:6px; padding:6px 8px;
  selection-background-color:%12; selection-color:%13;
}
QDockWidget#ObjectsDock QListWidget {
  background:%11; border:1px solid %8;
}
QDockWidget#ObjectsDock QListWidget::item { padding:6px 8px; }
QDockWidget#ObjectsDock QListWidget::item:hover { background:%6; }
QDockWidget#ObjectsDock QListWidget::item:selected {
  background:%12; color:%13; border-left:3px solid %7;
}

/* ===== Tabs documento ===== */
QTabWidget::pane { border:1px solid %8; top:-1px; background:%3; }
QTabBar::tab {
  background:%4; color:%1; border:1px solid %8; border-bottom:0;
  padding:6px 12px; margin-right:2px;
}
QTabBar::tab:selected { background:%3; font-weight:600; }
QTabBar::tab:hover    { background:%6; }

/* ===== Datasheet (zona central blanca) ===== */
QTableView {
  background:%14;
  alternate-background-color:#fafafa;
  gridline-color:%8;
  border:1px solid %8;
}
QHeaderView::section {
  background:%4; color:%1; padding:6px 8px; border:1px solid %8;
}
QTableView::item { padding:4px; }
QTableView::item:selected { background:#f7dcdc; color:%1; } /* rojo clarito, acorde al oscurecimiento */

/* ===== Controles ===== */
QPushButton {
  background:%4; border:1px solid %8; border-radius:6px; padding:6px 12px;
}
QPushButton:hover   { background:%6; }
QPushButton:pressed { background:%7; }
QPushButton:default { background:%12; color:%13; border:1px solid %7; }

/* ===== Scrollbars ===== */
QScrollBar:vertical, QScrollBar:horizontal { background:transparent; }
QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
  background:%4; border:1px solid %8; border-radius:4px;
}
QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover { background:%6; }
)")
                            .arg(TXT)                 // %1
                            .arg(app.font().family()) // %2
                            .arg(BG_APP)              // %3
                            .arg(BG_RIB_L)            // %4
                            .arg(BG_RIB_D)            // %5
                            .arg(BG_HOV)              // %6
                            .arg(BG_PUSH)             // %7
                            .arg(BORD)                // %8
                            .arg(TXT_DIM)             // %9
                            .arg(BG_NAV_L)            // %10
                            .arg(BG_NAV)              // %11
                            .arg(ACCENT)              // %12
                            .arg(TXT_INV)             // %13
                            .arg(BG_SURF);            // %14

    app.setStyleSheet(qss);
}

}

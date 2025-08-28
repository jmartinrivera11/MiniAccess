#include <QApplication>
#include "../gui/MainWindow.h"
#include "../gui/Theme.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    gui::applyAppTheme(app);
    MainWindow w;
    w.show();
    return app.exec();
}

#include <QApplication>
#include "../gui/MainWindow.h"
#include "../gui/Theme.h"
#include <QIcon>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    gui::applyAppTheme(app);
    app.setWindowIcon(QIcon(":/icons/icons/app.svg"));

    MainWindow w;
    w.show();
    return app.exec();
}

#include <QApplication>
#include <QByteArray>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArray("xcb"));
    }
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}

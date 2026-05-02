#include <QApplication>
#include <QByteArray>
#include <QtGlobal>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
#if defined(Q_OS_LINUX)
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArray("xcb"));
    }
#endif
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}

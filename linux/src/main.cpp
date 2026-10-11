#include <QApplication>
#include <QtGlobal>

#include "MainWindow.h"

int main(int argc, char** argv) {
    // mpv embeds into the window via a native X11 window id. On Wayland sessions
    // this runs through XWayland. (A render-API based path would enable native Wayland.)
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");

    QApplication app(argc, argv);
    QApplication::setOrganizationName("MPC-HC");
    QApplication::setApplicationName("mpc-hc");

    MainWindow w;
    w.show();

    const QStringList args = app.arguments();
    if (args.size() > 1)
        w.openPath(args.at(1));

    return app.exec();
}

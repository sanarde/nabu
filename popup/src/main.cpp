#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <Style.h>

#include "IconProvider.h"
#include "NotificationModel.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("nabu-popup");
    app.setApplicationVersion(PROJECT_VERSION);
    app.setDesktopFileName("org.nuros.sanar.nabu-popup");
    app.setQuitOnLastWindowClosed(false);

    SanarStyle::init();

    NotificationModel model;

    QQmlApplicationEngine engine;
    engine.addImageProvider("icon", new IconProvider);
    engine.rootContext()->setContextProperty("Notifications", &model);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Sanar.Nabu", "Main");

    return app.exec();
}

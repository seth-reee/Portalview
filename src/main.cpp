#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QStandardPaths>
#include <cstdio>
#include "instance.h"
#include "singleinstance.h"
#include "manager.h"
#include "theme.h"
#include "tray.h"
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setOrganizationName("Portalview"); app.setApplicationName("Portalview");
    SingleInstance sessionInstance;
    const auto sessionResult = sessionInstance.start();
    if (sessionResult == SingleInstance::Activated) return 0;
    if (sessionResult == SingleInstance::Error) {
        qCritical("Could not acquire Portalview session service or activate the running instance.");
        return 1;
    }
    Instance instance;
    const auto result = instance.start(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (result == Instance::Activated) return 0;
    if (result == Instance::Failed) {
        fprintf(stderr, "%s\n", qPrintable(instance.error()));
        return 1;
    }
    app.setApplicationVersion(PORTALVIEW_VERSION);
    app.setWindowIcon(QIcon(":/icons/portalview.png"));
    app.setDesktopFileName("portalview");
    QQuickStyle::setStyle("Basic");
    Manager manager;
    Theme theme;
    auto applyMenuTheme = [&app, &theme] {
        const auto colors = theme.colors();
        app.setStyleSheet(QString("QMenu { background: %1; color: %2; border: 1px solid %3; padding: 5px; } "
                                 "QMenu::item { padding: 8px 20px; border-radius: 4px; } "
                                 "QMenu::item:selected { background: %4; } "
                                 "QMenu::item:disabled { color: %5; } "
                                 "QMenu::separator { height: 1px; background: %3; margin: 4px; }")
                          .arg(colors["lighter_background"].toString(), colors["foreground"].toString(),
                               colors["muted"].toString(), colors["selection"].toString(), colors["light_foreground"].toString()));
    };
    applyMenuTheme();
    QObject::connect(&theme, &Theme::changed, &app, applyMenuTheme);
    Tray tray(&manager);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("manager", &manager);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("tray", &tray);
    bool activationPending = false;
    auto showWindow = [&engine, &activationPending] {
        if (engine.rootObjects().isEmpty()) activationPending = true;
        else QMetaObject::invokeMethod(engine.rootObjects().first(), "showMainMenu");
    };
    QObject::connect(&instance, &Instance::activationRequested, &engine, showWindow);
    QObject::connect(&sessionInstance, &SingleInstance::activationRequested, &engine, showWindow);
    engine.loadFromModule("Portalview", "Main");
    if (engine.rootObjects().isEmpty()) return 1;
    if (activationPending) showWindow();
    return app.exec();
}

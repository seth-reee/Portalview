#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QStandardPaths>
#include <cstdio>
#include <QCommandLineParser>
#include "instance.h"
#include "singleinstance.h"
#include "manager.h"
#include "theme.h"
#include "tray.h"
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setOrganizationName("Portalview"); app.setApplicationName("Portalview");
    app.setApplicationVersion(PORTALVIEW_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Saved RDP connections for Omarchy.");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"add-host", "Open the connection editor for a host.", "host"});
    parser.addOption({"name", "Initial connection name (with --add-host).", "name"});
    parser.process(app);
    const QString requestedHost = parser.value("add-host").trimmed();
    const QString requestedName = parser.value("name").trimmed();
    if ((parser.isSet("add-host") && !QRegularExpression("^[A-Za-z0-9_.:-]+$").match(requestedHost).hasMatch())
        || (parser.isSet("name") && requestedHost.isEmpty())
        || requestedName.contains('\n') || requestedName.contains('\r') || requestedName.contains(QChar(0))) {
        fprintf(stderr, "Enter a host without spaces or slashes, and a single-line name.\n");
        return 1;
    }
    SingleInstance sessionInstance;
    const auto sessionResult = sessionInstance.start(requestedHost, requestedName);
    if (sessionResult == SingleInstance::Activated) return 0;
    if (sessionResult == SingleInstance::Error) {
        qCritical("Could not contact Portalview. If it is already running, update and restart it to support adding hosts.");
        return 1;
    }
    Instance instance;
    const auto result = instance.start(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (result == Instance::Activated) {
        if (!requestedHost.isEmpty()) {
            fprintf(stderr, "Portalview storage is in use by another session; add the host from that session.\n");
            return 1;
        }
        return 0;
    }
    if (result == Instance::Failed) {
        fprintf(stderr, "%s\n", qPrintable(instance.error()));
        return 1;
    }
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
    QString pendingHost = requestedHost, pendingName = requestedName;
    auto addConnection = [&engine, &pendingHost, &pendingName](const QString &host, const QString &name) {
        if (engine.rootObjects().isEmpty()) { pendingHost = host; pendingName = name; }
        else QMetaObject::invokeMethod(engine.rootObjects().first(), "addHost",
                                      Q_ARG(QVariant, host), Q_ARG(QVariant, name));
    };
    QObject::connect(&sessionInstance, &SingleInstance::addConnectionRequested, &engine, addConnection);
    auto showWindow = [&engine, &activationPending] {
        if (engine.rootObjects().isEmpty()) activationPending = true;
        else QMetaObject::invokeMethod(engine.rootObjects().first(), "showMainMenu");
    };
    QObject::connect(&instance, &Instance::activationRequested, &engine, showWindow);
    QObject::connect(&sessionInstance, &SingleInstance::activationRequested, &engine, showWindow);
    engine.loadFromModule("Portalview", "Main");
    if (engine.rootObjects().isEmpty()) return 1;
    if (activationPending) showWindow();
    if (!pendingHost.isEmpty()) addConnection(pendingHost, pendingName);
    return app.exec();
}

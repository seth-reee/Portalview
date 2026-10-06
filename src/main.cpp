#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include "manager.h"
#include "theme.h"
#include "tray.h"
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setOrganizationName("Portalview"); app.setApplicationName("Portalview");
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
    engine.loadFromModule("Portalview", "Main");
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}

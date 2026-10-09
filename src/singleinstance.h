#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

// The session bus releases ownership automatically, including after a crash.
class SingleInstance : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.portalview.Application")
public:
    enum Result { Primary, Activated, Error };
    Result start() {
        auto bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) return Error;
        if (bus.registerService("org.portalview.Application")) {
            if (bus.registerObject("/Application", this, QDBusConnection::ExportAllSlots)) return Primary;
            bus.unregisterService("org.portalview.Application");
            return Error;
        }
        QDBusInterface existing("org.portalview.Application", "/Application",
                                "org.portalview.Application", bus);
        QDBusReply<void> reply = existing.call("activate");
        return reply.isValid() ? Activated : Error;
    }
public slots:
    void activate() { emit activationRequested(); }
signals:
    void activationRequested();
};

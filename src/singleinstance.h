#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QRegularExpression>

// The session bus releases ownership automatically, including after a crash.
class SingleInstance : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.portalview.Application")
public:
    enum Result { Primary, Activated, Error };
    Result start(const QString &host = {}, const QString &name = {}) {
        auto bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) return Error;
        if (bus.registerService("org.portalview.Application")) {
            if (bus.registerObject("/Application", this, QDBusConnection::ExportAllSlots)) return Primary;
            bus.unregisterService("org.portalview.Application");
            return Error;
        }
        QDBusInterface existing("org.portalview.Application", "/Application",
                                "org.portalview.Application", bus);
        if (!host.isEmpty()) {
            QDBusReply<bool> reply = existing.call("addConnection", host, name);
            return reply.isValid() && reply.value() ? Activated : Error;
        }
        QDBusReply<void> reply = existing.call("activate");
        return reply.isValid() ? Activated : Error;
    }
public slots:
    bool addConnection(const QString &host, const QString &name) {
        if (!QRegularExpression("^[A-Za-z0-9_.:-]+$").match(host).hasMatch()
            || name.contains('\n') || name.contains('\r') || name.contains(QChar(0))) return false;
        emit addConnectionRequested(host, name);
        return true;
    }
    void activate() { emit activationRequested(); }
signals:
    void activationRequested();
    void addConnectionRequested(QString host, QString name);
};

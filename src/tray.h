#pragma once
#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QSettings>
class Manager;
class Tray : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool canHide READ canHide NOTIFY availabilityChanged)
public:
    explicit Tray(Manager *manager, QObject *parent = nullptr);
    bool enabled() const { return m_enabled; }
    bool canHide() const { return m_enabled && QSystemTrayIcon::isSystemTrayAvailable(); }
    void setEnabled(bool enabled);
signals:
    void enabledChanged();
    void availabilityChanged();
    void openRequested();
    void connectionRequested(QString id);
private:
    void rebuildMenu();
    Manager *m_manager;
    QSettings m_settings;
    bool m_enabled = false;
    bool m_available = false;
    QMenu m_menu;
    QSystemTrayIcon m_icon;
};

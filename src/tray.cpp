#include "tray.h"
#include "manager.h"
#include <QApplication>
#include <QIcon>
#include <QMap>
#include <QTimer>
#include <algorithm>

Tray::Tray(Manager *manager, QObject *parent) : QObject(parent), m_manager(manager) {
    m_menu.setObjectName("trayMenu");
    m_icon.setIcon(QIcon(":/icons/portalview.png"));
    m_icon.setToolTip("Portalview");
    m_icon.setContextMenu(&m_menu);
    connect(&m_menu, &QMenu::aboutToShow, this, &Tray::rebuildMenu);
    connect(&m_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) emit openRequested();
    });
    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        bool available = QSystemTrayIcon::isSystemTrayAvailable();
        if (available != m_available) {
            m_available = available; emit availabilityChanged();
            if (!available && m_enabled) emit openRequested();
        }
    });
    timer->start(2000);
    m_available = QSystemTrayIcon::isSystemTrayAvailable();
    setEnabled(m_settings.value("tray/enabled", false).toBool());
}
void Tray::setEnabled(bool enabled) {
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    m_settings.setValue("tray/enabled", enabled);
    m_icon.setVisible(enabled);
    emit enabledChanged(); emit availabilityChanged();
    // Bring the window back before disabling the only way to access it.
    if (!enabled) emit openRequested();
}
void Tray::rebuildMenu() {
    m_menu.clear();
    auto open = m_menu.addAction("Open Main Menu");
    open->setObjectName("trayOpen");
    connect(open, &QAction::triggered, this, &Tray::openRequested);
    m_menu.addSeparator();
    QMap<QString, QList<QVariantMap>> groups;
    for (const auto &value : m_manager->connections()) {
        auto entry = value.toMap(); groups[entry.value("group").toString()].append(entry);
    }
    if (groups.isEmpty()) {
        auto empty = m_menu.addAction("No Saved Connections"); empty->setEnabled(false);
    }
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        // Keep ungrouped connections directly accessible.
        QMenu *menu = it.key().isEmpty() ? &m_menu : m_menu.addMenu(QString(it.key()).replace('&', "&&"));
        auto entries = it.value();
        std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
            return QString::localeAwareCompare(a["name"].toString(), b["name"].toString()) < 0;
        });
        for (const auto &entry : entries) {
            bool active = entry.value("active").toBool();
            QString label = entry["name"].toString().replace('&', "&&");
            auto action = menu->addAction(label + (active ? " (open)" : ""));
            action->setData(entry["id"]);
            action->setEnabled(!active);
            connect(action, &QAction::triggered, this, [this, id = entry["id"].toString()] {
                emit connectionRequested(id);
            });
        }
    }
    m_menu.addSeparator();
    auto close = m_menu.addAction("Close");
    close->setObjectName("trayClose");
    connect(close, &QAction::triggered, qApp, &QApplication::quit);
}

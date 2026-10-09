#include "manager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>
#include <QRegularExpression>
#include <QTimer>
#include <QProcessEnvironment>
Manager::Manager(QObject *parent) : QObject(parent) {
    m_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/connections.json";
    QFile file(m_path);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) { report("Cannot read saved connections: " + file.errorString()); return; }
        QJsonParseError error;
        auto doc = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isArray()) report("Saved connections file is invalid; repair it before saving.");
        else m_entries = doc.array().toVariantList();
    }
}
Manager::~Manager() {
    const auto sessions = m_sessions.values();
    for (auto process : sessions) { process->terminate(); if (!process->waitForFinished(1000)) { process->kill(); process->waitForFinished(1000); } }
}
void Manager::report(QString text) { m_message = text; emit messageChanged(); }
QString Manager::licenseText() const {
    QFile file(":/LICENSE");
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}
QVariantList Manager::connections() const {
    auto entries = m_entries;
    for (auto &value : entries) { auto entry = value.toMap(); entry["active"] = m_sessions.contains(entry["id"].toString()); value = entry; }
    return entries;
}
bool Manager::persist(const QVariantList &entries) {
    QFile existing(m_path);
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly) || !QJsonDocument::fromJson(existing.readAll()).isArray()) { report("Cannot overwrite unreadable or invalid connections file."); return false; }
    }
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) { report("Cannot create connection storage."); return false; }
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) { report(file.errorString()); return false; }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    auto bytes = QJsonDocument(QJsonArray::fromVariantList(entries)).toJson();
    if (file.write(bytes) != bytes.size() || !file.commit()) { report("Failed to save connections: " + file.errorString()); return false; }
    m_entries = entries; emit changed(); return true;
}
bool Manager::save(QVariantMap entry) {
    QVariantMap clean;
    for (auto key : {"id", "name", "host", "username", "domain", "group"}) {
        QString value = entry.value(key).toString().trimmed();
        if (value.contains('\n') || value.contains('\r') || value.contains(QChar(0))) { report("Fields must contain a single line."); return false; }
        clean[key] = value;
    }
    if (clean["name"].toString().isEmpty() || clean["host"].toString().isEmpty() || clean["username"].toString().isEmpty()) { report("Name, host and username are required."); return false; }
    if (clean["host"].toString().contains(QRegularExpression("\\s|/"))) { report("Enter a hostname or IP address without spaces or slashes."); return false; }
    int port = entry.value("port", 3389).toInt();
    if (port < 1 || port > 65535) { report("Port must be between 1 and 65535."); return false; }
    clean["port"] = port;
    clean["clipboard"] = entry.value("clipboard", true).toBool();
    clean["fullscreen"] = entry.value("fullscreen", false).toBool();
    clean["performanceMode"] = entry.value("performanceMode", false).toBool();
    clean["trustFirst"] = entry.value("trustFirst", false).toBool();
    if (clean["id"].toString().isEmpty()) clean["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto entries = m_entries; bool found = false;
    for (auto &value : entries) if (value.toMap()["id"] == clean["id"]) { value = clean; found = true; break; }
    if (!found) entries.append(clean);
    if (!persist(entries)) return false;
    report("Connection saved."); return true;
}
bool Manager::remove(QString id) {
    if (m_sessions.contains(id)) { report("Disconnect this session before deleting it."); return false; }
    auto entries = m_entries;
    for (int i = entries.size()-1; i >= 0; --i) if (entries[i].toMap()["id"].toString() == id) entries.removeAt(i);
    if (!persist(entries)) return false;
    report("Connection deleted."); return true;
}
void Manager::connectTo(QString id, QString password) {
    if (m_sessions.contains(id)) { report("This connection already has an open session."); return; }
    if (password.contains('\n') || password.contains('\r') || password.contains(QChar(0))) { report("Passwords containing line breaks are unsupported."); return; }
    QVariantMap entry;
    for (auto value : m_entries) if (value.toMap()["id"].toString() == id) entry = value.toMap();
    if (entry.isEmpty()) { report("Connection no longer exists."); return; }
    QString executable = QStandardPaths::findExecutable("sdl-freerdp3");
    if (executable.isEmpty()) { report("Install the freerdp package to connect."); return; }
    // SDL FreeRDP defaults to Right Shift hotkeys, including D to disconnect.
    // Seed a safe default while respecting explicitly configured shortcuts.
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/freerdp";
    const QString configPath = configDir + "/sdl-freerdp.json";
    QJsonObject config;
    QFile configFile(configPath);
    if (configFile.exists()) {
        if (!configFile.open(QIODevice::ReadOnly)) { report("Cannot read FreeRDP shortcut settings: " + configFile.errorString()); return; }
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(configFile.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) { report("FreeRDP shortcut settings are invalid; repair sdl-freerdp.json before connecting."); return; }
        config = document.object();
        configFile.close();
    }
    if (!config.contains("SDL_KeyModMask")) {
        config.insert("SDL_KeyModMask", QJsonArray {"KMOD_NONE"});
        if (!QDir().mkpath(configDir)) { report("Cannot create FreeRDP settings directory."); return; }
        QSaveFile output(configPath);
        const QByteArray data = QJsonDocument(config).toJson();
        if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit()) {
            report("Cannot save FreeRDP shortcut settings: " + output.errorString()); return;
        }
    }
    QString host = entry["host"].toString();
    if (host.contains(':') && !host.startsWith('[')) host = "[" + host + "]";
    QStringList args {"/v:" + host + ":" + entry["port"].toString(), "/u:" + entry["username"].toString(), "/p:" + password,
        "/t:Portalview — " + entry["name"].toString(), "/wm-class:portalview", "+dynamic-resolution", "/size:1280x800", "/timeout:10000", "/log-level:ERROR",
        entry["clipboard"].toBool() ? "+clipboard" : "-clipboard", entry["trustFirst"].toBool() ? "/cert:tofu" : "/cert:deny"};
    if (!entry["domain"].toString().isEmpty()) args << "/d:" + entry["domain"].toString();
    if (entry["fullscreen"].toBool()) args << "/f";
    if (entry.value("performanceMode", false).toBool())
        args << "/network:modem" << "/bpp:16" << "-wallpaper" << "-themes"
             << "-fonts" << "-aero" << "-window-drag" << "-menu-anims";
    auto process = new QProcess(this);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("SDL_APP_ID", "portalview");
    process->setProcessEnvironment(environment);
    process->setProcessChannelMode(QProcess::MergedChannels);
    m_sessions.insert(id, process); emit changed();
    connect(process, &QProcess::readyReadStandardOutput, process, [process] { process->readAllStandardOutput(); });
    connect(process, &QProcess::errorOccurred, this, [this, process, id](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) { m_sessions.remove(id); report("Could not start FreeRDP: " + process->errorString()); emit changed(); process->deleteLater(); }
    });
    connect(process, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this, [this, process, id](int code, QProcess::ExitStatus status) {
        m_sessions.remove(id); emit changed();
        report(status == QProcess::NormalExit && code == 0 ? "Session closed." : QString("FreeRDP session ended (code %1). Check the host, credentials and certificate settings.").arg(code)); process->deleteLater();
    });
    QByteArray input = args.join('\n').toUtf8() + '\n';
    connect(process, &QProcess::started, this, [this, process, input, name = entry["name"].toString()] {
        process->write(input);
        process->closeWriteChannel();
        report("FreeRDP running for " + name + ".");
    });
    report("Opening " + entry["name"].toString() + "…");
    process->start(executable, {"/args-from:stdin"});
}
void Manager::disconnectFrom(QString id) {
    auto process = m_sessions.value(id); if (!process) return;
    process->terminate(); QTimer::singleShot(2000, process, [process] { if (process->state() != QProcess::NotRunning) process->kill(); });
}

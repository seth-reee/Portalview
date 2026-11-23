#include "manager.h"
#include <QStandardPaths>
#include <QCoreApplication>
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
#include <QElapsedTimer>
#include <QProcessEnvironment>
#include <QStringDecoder>
#include <memory>

namespace {
constexpr qint64 kMaxImportFileBytes = 5 * 1024 * 1024;
constexpr qsizetype kMaxProcessOutputBytes = 16 * 1024;

QString decodeTextFile(const QByteArray &raw) {
    if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF && static_cast<unsigned char>(raw[1]) == 0xFE) {
        QStringDecoder decoder(QStringDecoder::Utf16LE);
        return decoder.decode(raw.mid(2));
    }
    if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFE && static_cast<unsigned char>(raw[1]) == 0xFF) {
        QStringDecoder decoder(QStringDecoder::Utf16BE);
        return decoder.decode(raw.mid(2));
    }
    if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF
        && static_cast<unsigned char>(raw[1]) == 0xBB && static_cast<unsigned char>(raw[2]) == 0xBF) {
        return QString::fromUtf8(raw.mid(3));
    }
    return QString::fromUtf8(raw);
}

QString shareNameForFolder(const QString &folder) {
    QString base = QFileInfo(folder).fileName();
    base.remove(QRegularExpression("[^A-Za-z0-9_-]"));
    return base.isEmpty() ? QStringLiteral("Shared") : base.left(32);
}

bool isDuplicateEntry(const QVariantList &entries, const QVariantMap &candidate) {
    const QString host = candidate["host"].toString().toLower();
    const int port = candidate["port"].toInt();
    const QString user = candidate["username"].toString().toLower();
    const QString domain = candidate["domain"].toString().toLower();
    for (const auto &value : entries) {
        const auto existing = value.toMap();
        if (existing["host"].toString().toLower() == host
            && existing.value("port", 3389).toInt() == port
            && existing["username"].toString().toLower() == user
            && existing["domain"].toString().toLower() == domain) {
            return true;
        }
    }
    return false;
}

QString extractErrorDetail(const QByteArray &output) {
    const QString text = QString::fromUtf8(output).replace('\r', '\n');
    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    static const QRegularExpression logPrefix(
        R"(^\[[^\]]+\]\s*(?:\[\d+:\d+\]\s*)?(?:\[[A-Z]+\](?:\[[^\]]+\])?\s*-\s*)?)");
    QString fallback;
    for (int i = lines.size() - 1; i >= 0; --i) {
        QString line = lines[i].trimmed();
        if (line.isEmpty()) continue;
        const bool isErrorLine = line.contains("[ERROR]") || line.contains("[FATAL]") || line.contains("ERRCONNECT_") || line.contains("ERRINFO_");
        line.remove(logPrefix);
        line = line.simplified();
        if (line.isEmpty()) continue;
        if (line.size() > 180) line = line.left(177) + "...";
        if (isErrorLine) return line;
        if (fallback.isEmpty()) fallback = line;
    }
    return fallback;
}

QString summarizeFreerdpFailure(const QByteArray &output, int code, QProcess::ExitStatus status) {
    const QString text = QString::fromUtf8(output);
    const QString prefix = status == QProcess::CrashExit
        ? QString("FreeRDP crashed (code %1)").arg(code)
        : QString("FreeRDP session ended (code %1)").arg(code);
    if (text.contains("ERRCONNECT_AUTHENTICATION_FAILED")
        || text.contains("ERRCONNECT_LOGON_FAILURE")
        || text.contains("ERRCONNECT_PASSWORD_EXPIRED")
        || text.contains("ERRCONNECT_ACCOUNT_LOCKED_OUT")
        || text.contains("ERRCONNECT_ACCOUNT_DISABLED")) {
        return prefix + ": authentication failed. Check your username, domain, and password.";
    }
    if (text.contains("ERRCONNECT_DNS_NAME_NOT_FOUND")) {
        return prefix + ": could not resolve host. Check the hostname and network connection.";
    }
    if (text.contains("ERRCONNECT_TLS_CONNECT_FAILED")
        || text.contains("ERRCONNECT_SECURITY_NEGO_CONNECT_FAILED")
        || text.contains("certificate verification failed", Qt::CaseInsensitive)
        || text.contains("Certificate check failed", Qt::CaseInsensitive)) {
        return prefix + ": certificate or TLS verification failed. Enable trust-on-first-use or verify the server certificate.";
    }
    if (text.contains("ERRCONNECT_CONNECT_FAILED")
        || text.contains("ERRCONNECT_CONNECT_TRANSPORT_FAILED")
        || text.contains("Connection refused", Qt::CaseInsensitive)
        || text.contains("timeout", Qt::CaseInsensitive)) {
        return prefix + ": could not reach the remote host. Check the host, port, and firewall.";
    }
    const QString detail = extractErrorDetail(output);
    if (!detail.isEmpty()) return prefix + ": " + detail;
    return prefix + ". Check the host, credentials and certificate settings.";
}

bool parseRdpFile(const QString &filePath, const QByteArray &raw, QVariantMap &entry, QString &error) {
    const QString text = decodeTextFile(raw);
    QString host;
    int port = 3389;
    bool hasExplicitPort = false;
    QString username, domain, sharedFolder;
    bool clipboard = true, fullscreen = false, multimon = false, captureShortcuts = false, performanceMode = false, microphone = false;
    QString audioMode = "none";
    int desktopScale = 0;

    for (const QString &rawLine : text.split('\n')) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) continue;
        const int firstColon = line.indexOf(':');
        if (firstColon <= 0) continue;
        const int secondColon = line.indexOf(':', firstColon + 1);
        if (secondColon <= firstColon + 1) continue;
        const QString key = line.left(firstColon).trimmed().toLower();
        const QString value = line.mid(secondColon + 1).trimmed();

        if (key == "full address") {
            if (value.startsWith('[')) {
                const int close = value.indexOf(']');
                if (close > 0) {
                    host = value.left(close + 1);
                    if (close + 1 < value.size() && value[close + 1] == ':') {
                        bool ok = false;
                        const int parsed = value.mid(close + 2).toInt(&ok);
                        if (ok) { port = parsed; hasExplicitPort = true; }
                    }
                } else {
                    host = value;
                }
            } else if (value.count(':') == 1) {
                const int sep = value.indexOf(':');
                bool ok = false;
                const int parsed = value.mid(sep + 1).toInt(&ok);
                if (ok) { host = value.left(sep); port = parsed; hasExplicitPort = true; }
                else host = value;
            } else {
                host = value;
            }
        } else if (key == "server port" && !hasExplicitPort) {
            bool ok = false;
            const int parsed = value.toInt(&ok, 0);
            if (ok) port = parsed;
        } else if (key == "username") {
            username = value;
        } else if (key == "domain") {
            domain = value;
        } else if (key == "screen mode id") {
            fullscreen = (value.toInt() == 2);
        } else if (key == "use multimon") {
            multimon = (value.toInt() != 0);
        } else if (key == "redirectclipboard") {
            clipboard = (value.toInt() != 0);
        } else if (key == "audiomode") {
            const int mode = value.toInt();
            audioMode = (mode == 0) ? "local" : (mode == 1 ? "remote" : "none");
        } else if (key == "audiocapturemode") {
            microphone = (value.toInt() != 0);
        } else if (key == "desktopscalefactor") {
            bool ok = false;
            const int scale = value.toInt(&ok);
            if (ok && scale >= 100 && scale <= 500) desktopScale = scale;
        } else if (key == "keyboardhook") {
            const int hook = value.toInt();
            captureShortcuts = (hook == 1 || hook == 2);
        } else if (key == "connection type") {
            performanceMode = (value.toInt() == 1);
        } else if (key == "drivestoredirect") {
            if (QDir::isAbsolutePath(value) && !value.contains(',') && QFileInfo(value).isDir())
                sharedFolder = QDir::cleanPath(value);
        }
    }

    if (host.isEmpty()) {
        error = "RDP file does not contain a host (full address).";
        return false;
    }
    if (domain.isEmpty() && username.contains('\\')) {
        const int slash = username.indexOf('\\');
        domain = username.left(slash);
        username = username.mid(slash + 1);
    }
    QString name = QFileInfo(filePath).completeBaseName().trimmed();
    if (name.isEmpty()) name = host;

    entry = {
        {"name", name},
        {"host", host},
        {"port", port},
        {"username", username},
        {"domain", domain},
        {"group", QString()},
        {"sharedFolder", sharedFolder},
        {"clipboard", clipboard},
        {"fullscreen", fullscreen},
        {"multimon", multimon},
        {"captureShortcuts", captureShortcuts},
        {"audioMode", audioMode},
        {"microphone", microphone},
        {"performanceMode", performanceMode},
        {"desktopScale", desktopScale},
        {"trustFirst", false}
    };
    return true;
}
}
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
    for (auto process : sessions) {
        process->disconnect(this);
        if (process->state() != QProcess::NotRunning) process->terminate();
    }
    QElapsedTimer timer;
    timer.start();
    for (auto process : sessions) {
        if (process->state() != QProcess::NotRunning)
            process->waitForFinished(qMax(0, 1000 - int(timer.elapsed())));
    }
    for (auto process : sessions) {
        if (process->state() != QProcess::NotRunning) process->kill();
    }
    for (auto process : sessions) {
        if (process->state() != QProcess::NotRunning) process->waitForFinished(1000);
    }
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
QString Manager::localPath(const QUrl &url) const {
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}
bool Manager::validateEntry(const QVariantMap &entry, QVariantMap &clean, bool requireUsername, QString &error) const {
    clean.clear();
    for (auto key : {"id", "name", "host", "username", "domain", "group", "sharedFolder"}) {
        QString value = entry.value(key).toString().trimmed();
        if (value.contains('\n') || value.contains('\r') || value.contains(QChar(0))) {
            error = "Fields must contain a single line.";
            return false;
        }
        clean[key] = value;
    }
    if (clean["name"].toString().isEmpty() || clean["host"].toString().isEmpty()
        || (requireUsername && clean["username"].toString().isEmpty())) {
        error = "Name, host and username are required.";
        return false;
    }
    if (!QRegularExpression("^(\\[[0-9A-Fa-f]*:[0-9A-Fa-f:]+\\]|[A-Za-z0-9_.:-]+)$").match(clean["host"].toString()).hasMatch()) {
        error = "Enter a valid hostname or IP address without spaces or slashes.";
        return false;
    }
    bool validPort = false;
    const int port = entry.contains("port") ? entry.value("port").toInt(&validPort) : (validPort = true, 3389);
    if (!validPort || port < 1 || port > 65535) {
        error = "Port must be between 1 and 65535.";
        return false;
    }
    bool validScale = false;
    const int desktopScale = entry.value("desktopScale", 0).toInt(&validScale);
    if (!validScale || (desktopScale != 0 && (desktopScale < 100 || desktopScale > 500))) {
        error = "Desktop scaling must be automatic or between 100 and 500 percent.";
        return false;
    }
    QString sharedFolder = clean["sharedFolder"].toString();
    if (!sharedFolder.isEmpty()) {
        if (sharedFolder == "~") sharedFolder = QDir::homePath();
        else if (sharedFolder.startsWith("~/")) sharedFolder = QDir::homePath() + sharedFolder.mid(1);
        sharedFolder = QDir::cleanPath(sharedFolder);
        if (!QDir::isAbsolutePath(sharedFolder) || sharedFolder.contains(',') || !QFileInfo(sharedFolder).isDir()) {
            error = "Shared folder must be an existing absolute directory path without commas.";
            return false;
        }
        clean["sharedFolder"] = sharedFolder;
    }
    QString audioMode = entry.value("audioMode", "none").toString().trimmed().toLower();
    if (audioMode.isEmpty()) audioMode = "none";
    if (audioMode != "none" && audioMode != "local" && audioMode != "remote") {
        error = "Audio redirection must be off, local, or remote.";
        return false;
    }
    clean["desktopScale"] = desktopScale;
    clean["port"] = port;
    clean["clipboard"] = entry.value("clipboard", true).toBool();
    clean["fullscreen"] = entry.value("fullscreen", false).toBool();
    clean["multimon"] = entry.value("multimon", false).toBool();
    clean["captureShortcuts"] = entry.value("captureShortcuts", false).toBool();
    clean["audioMode"] = audioMode;
    clean["microphone"] = entry.value("microphone", false).toBool();
    clean["performanceMode"] = entry.value("performanceMode", false).toBool();
    clean["trustFirst"] = entry.value("trustFirst", false).toBool();
    return true;
}
bool Manager::save(QVariantMap entry) {
    QVariantMap clean;
    QString error;
    if (!validateEntry(entry, clean, true, error)) { report(error); return false; }
    if (clean["id"].toString().isEmpty()) clean["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto entries = m_entries; bool found = false;
    for (auto &value : entries) if (value.toMap()["id"] == clean["id"]) { value = clean; found = true; break; }
    if (!found) entries.append(clean);
    if (!persist(entries)) return false;
    report("Connection saved."); return true;
}
int Manager::importFile(const QString &pathOrUrl) {
    const QUrl url(pathOrUrl);
    const QString path = url.isLocalFile() ? url.toLocalFile() : pathOrUrl.trimmed();
    if (path.isEmpty()) { report("Select a file to import."); return 0; }
    QFile file(path);
    if (!file.exists()) { report("Import file does not exist."); return 0; }
    if (file.size() > kMaxImportFileBytes) { report("Import file is too large."); return 0; }
    if (!file.open(QIODevice::ReadOnly)) { report("Cannot read import file: " + file.errorString()); return 0; }
    const QByteArray raw = file.readAll();
    file.close();

    const QString suffix = QFileInfo(path).suffix().toLower();
    QJsonParseError jsonError;
    const auto jsonDoc = QJsonDocument::fromJson(raw, &jsonError);
    const bool treatAsJson = (suffix == "json") || (suffix != "rdp" && jsonError.error == QJsonParseError::NoError && (jsonDoc.isArray() || jsonDoc.isObject()));

    if (treatAsJson) {
        if (jsonError.error != QJsonParseError::NoError || (!jsonDoc.isArray() && !jsonDoc.isObject())) {
            report("Import JSON file is invalid.");
            return 0;
        }
        QVariantList rawList;
        if (jsonDoc.isArray()) rawList = jsonDoc.array().toVariantList();
        else rawList.append(jsonDoc.object().toVariantMap());
        auto entries = m_entries;
        int imported = 0, skipped = 0, invalid = 0;
        for (const auto &item : rawList) {
            QVariantMap candidate = item.toMap();
            candidate.remove("id");
            QVariantMap clean;
            QString error;
            if (!validateEntry(candidate, clean, true, error)) { ++invalid; continue; }
            if (isDuplicateEntry(entries, clean)) { ++skipped; continue; }
            clean["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
            entries.append(clean);
            ++imported;
        }
        if (imported == 0) {
            if (skipped > 0 && invalid == 0) report(QString("No new connections imported (%1 duplicate%2 skipped).").arg(skipped).arg(skipped == 1 ? "" : "s"));
            else report("No valid connections found in import file.");
            return 0;
        }
        if (!persist(entries)) return 0;
        QString summary = QString("Imported %1 connection%2.").arg(imported).arg(imported == 1 ? "" : "s");
        QStringList notes;
        if (skipped > 0) notes << QString("%1 duplicate%2 skipped").arg(skipped).arg(skipped == 1 ? "" : "s");
        if (invalid > 0) notes << QString("%1 invalid skipped").arg(invalid);
        if (!notes.isEmpty()) summary.chop(1), summary += " (" + notes.join(", ") + ").";
        report(summary);
        return imported;
    }

    QVariantMap parsed;
    QString parseError;
    if (!parseRdpFile(path, raw, parsed, parseError)) {
        report(parseError);
        return 0;
    }
    QVariantMap clean;
    QString validationError;
    if (parsed["username"].toString().isEmpty()) {
        if (!validateEntry(parsed, clean, false, validationError)) { report(validationError); return 0; }
        report("Imported settings from RDP file. Enter a username and save.");
        emit importNeedsReview(clean);
        return 0;
    }
    if (!validateEntry(parsed, clean, true, validationError)) { report(validationError); return 0; }
    if (isDuplicateEntry(m_entries, clean)) {
        report("Connection already exists.");
        return 0;
    }
    clean["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto entries = m_entries;
    entries.append(clean);
    if (!persist(entries)) return 0;
    report("Imported " + clean["name"].toString() + ".");
    return 1;
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
    // Prefer an app-private runtime; never change the parent process library path.
    QString runtime;
    const QStringList runtimeCandidates {
        QCoreApplication::applicationDirPath() + "/../lib/portalview/freerdp-3.31.1",
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/portalview/freerdp-3.31.1"
    };
    for (const auto &candidate : runtimeCandidates) {
        if (QFileInfo(candidate + "/bin/xfreerdp3").isExecutable()) {
            runtime = QDir::cleanPath(candidate);
            break;
        }
    }
    QString executable = runtime.isEmpty() ? QStandardPaths::findExecutable("sdl-freerdp3")
                                           : runtime + "/bin/xfreerdp3";
    if (executable.isEmpty()) { report("Install the freerdp package to connect."); return; }
    // SDL FreeRDP defaults to Right Shift hotkeys, including D to disconnect.
    // Seed a safe default while respecting explicitly configured shortcuts.
    if (runtime.isEmpty()) {
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
    }
    const QString sharedFolder = entry.value("sharedFolder").toString().trimmed();
    if (!sharedFolder.isEmpty()) {
        if (!QDir::isAbsolutePath(sharedFolder) || sharedFolder.contains(',') || !QFileInfo(sharedFolder).isDir()) {
            report("Shared folder is unavailable: " + sharedFolder);
            return;
        }
    }
    QString host = entry["host"].toString();
    if (host.contains(':') && !(host.startsWith('[') && host.endsWith(']'))) host = "[" + host + "]";
    QStringList args {"/v:" + host + ":" + entry["port"].toString(), "/u:" + entry["username"].toString(), "/p:" + password,
        "/t:Portalview — " + entry["name"].toString(), "/wm-class:portalview", "+dynamic-resolution", "/size:1280x800", "/timeout:10000", "/log-level:ERROR",
        entry["clipboard"].toBool() ? "+clipboard" : "-clipboard", entry["trustFirst"].toBool() ? "/cert:tofu" : "/cert:deny",
        entry.value("captureShortcuts", false).toBool() ? "+grab-keyboard" : "-grab-keyboard"};
    const int desktopScale = entry.value("desktopScale", 0).toInt();
    if (desktopScale >= 100 && desktopScale <= 500)
        args << "/scale-desktop:" + QString::number(desktopScale) << "/scale-device:100";
    if (!entry["domain"].toString().isEmpty()) args << "/d:" + entry["domain"].toString();
    if (entry.value("multimon", false).toBool()) args << "/multimon";
    else if (entry["fullscreen"].toBool()) args << "/f";
    const QString audioMode = entry.value("audioMode", "none").toString();
    if (audioMode == "local") args << "/sound" << "/audio-mode:0";
    else if (audioMode == "remote") args << "/audio-mode:1";
    else args << "/audio-mode:2";
    if (entry.value("microphone", false).toBool()) args << "/microphone";
    if (!sharedFolder.isEmpty()) args << "/drive:" + shareNameForFolder(sharedFolder) + "," + sharedFolder;
    if (entry.value("performanceMode", false).toBool())
        args << "/network:modem" << "/bpp:16" << "-wallpaper" << "-themes"
             << "-fonts" << "-aero" << "-window-drag" << "-menu-anims";
    auto process = new QProcess(this);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("SDL_APP_ID", "portalview");
    if (!runtime.isEmpty() && QDir(runtime + "/lib").exists()) {
        const QString inherited = environment.value("LD_LIBRARY_PATH");
        environment.insert("LD_LIBRARY_PATH", runtime + "/lib" +
                           (inherited.isEmpty() ? QString() : ":" + inherited));
    }
    process->setProcessEnvironment(environment);
    process->setProcessChannelMode(QProcess::MergedChannels);
    m_sessions.insert(id, process); emit changed();
    auto outputBuffer = std::make_shared<QByteArray>();
    connect(process, &QProcess::readyReadStandardOutput, process, [process, outputBuffer] {
        outputBuffer->append(process->readAllStandardOutput());
        if (outputBuffer->size() > kMaxProcessOutputBytes)
            *outputBuffer = outputBuffer->right(kMaxProcessOutputBytes);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, id](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) { m_sessions.remove(id); report("Could not start FreeRDP: " + process->errorString()); emit changed(); process->deleteLater(); }
    });
    connect(process, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this, [this, process, id, outputBuffer](int code, QProcess::ExitStatus status) {
        outputBuffer->append(process->readAllStandardOutput());
        if (outputBuffer->size() > kMaxProcessOutputBytes)
            *outputBuffer = outputBuffer->right(kMaxProcessOutputBytes);
        m_sessions.remove(id); emit changed();
        report(status == QProcess::NormalExit && code == 0 ? "Session closed." : summarizeFreerdpFailure(*outputBuffer, code, status));
        process->deleteLater();
    });
    auto input = std::shared_ptr<QByteArray>(
        new QByteArray(args.join('\n').toUtf8() + '\n'),
        [](QByteArray *buffer) { buffer->fill(0); delete buffer; });
    password.fill(QChar(0));
    password.clear();
    for (auto &arg : args) { arg.fill(QChar(0)); arg.clear(); }
    connect(process, &QProcess::started, this, [this, process, input, name = entry["name"].toString()] {
        process->write(*input);
        input->fill(0);
        input->clear();
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

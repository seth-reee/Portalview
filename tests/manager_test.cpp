#include <QtTest>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "manager.h"
class ManagerTest : public QObject {
    Q_OBJECT
private slots:
    void persistenceAndLaunch() {
        QCoreApplication::setOrganizationName("PortalviewTests");
        QCoreApplication::setApplicationName("run-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
        const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/config";
        qputenv("XDG_CONFIG_HOME", configRoot.toUtf8());
        Manager manager;
        QVariantMap entry {{"name", "Office"}, {"host", "::1"}, {"username", "alice"}, {"port", 3389}};
        QVERIFY(manager.save(entry));
        QCOMPARE(manager.connections().size(), 1);
        QString id = manager.connections().first().toMap()["id"].toString();
        entry["id"] = id; entry["name"] = "Work";
        QVERIFY(manager.save(entry));
        Manager restored;
        QCOMPARE(restored.connections().first().toMap()["name"].toString(), QString("Work"));
        entry["host"] = "host\n/p:injected";
        QVERIFY(!manager.save(entry));
        entry["host"] = "[::1";
        QVERIFY(!manager.save(entry));
        entry["host"] = "bad;host";
        QVERIFY(!manager.save(entry));
        entry["host"] = "[::1]";
        QVERIFY(manager.save(entry));
        entry["host"] = "::1";
        QVERIFY(manager.save(entry));
        QCOMPARE(manager.connections().first().toMap()["host"].toString(), QString("::1"));
        QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QVERIFY(QDir().mkpath(root + "/bin"));
        QFile fake(root + "/bin/sdl-freerdp3");
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$PORTALVIEW_TEST_ROOT/argv\"\nprintf '%s\\n' \"$SDL_APP_ID\" > \"$PORTALVIEW_TEST_ROOT/app-id\"\ncat > \"$PORTALVIEW_TEST_ROOT/input\"\nsleep 1\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        qputenv("PORTALVIEW_TEST_ROOT", root.toUtf8());
        qputenv("PATH", (root + "/bin:" + qEnvironmentVariable("PATH")).toUtf8());
        manager.connectTo(id, "secret-test-value");
        QVERIFY(manager.connections().first().toMap()["active"].toBool());
        QVERIFY(!manager.remove(id));
        QTRY_COMPARE_WITH_TIMEOUT(manager.message(), QString("FreeRDP running for Work."), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(!manager.connections().first().toMap()["active"].toBool(), 4000);
        QCOMPARE(manager.message(), QString("Session closed."));
        QFile argv(root + "/argv"); QVERIFY(argv.open(QIODevice::ReadOnly));
        QCOMPARE(argv.readAll(), QByteArray("/args-from:stdin\n"));
        QFile input(root + "/input"); QVERIFY(input.open(QIODevice::ReadOnly));
        auto bytes = input.readAll();
        QVERIFY(bytes.contains("/p:secret-test-value\n"));
        QVERIFY(bytes.contains("/v:[::1]:3389\n"));
        QVERIFY(bytes.contains("/wm-class:portalview\n"));
        QVERIFY(bytes.contains("-grab-keyboard\n"));
        QVERIFY(bytes.contains("+clipboard\n"));
        QVERIFY(bytes.contains("/audio-mode:2\n"));
        QVERIFY(!bytes.contains("/sound\n"));
        QVERIFY(!bytes.contains("/microphone\n"));
        QVERIFY(!bytes.contains("/multimon\n"));
        QVERIFY(!bytes.contains("/drive:"));
        QFile appId(root + "/app-id"); QVERIFY(appId.open(QIODevice::ReadOnly));
        QCOMPARE(appId.readAll(), QByteArray("portalview\n"));
        QFile shortcuts(configRoot + "/freerdp/sdl-freerdp.json");
        QVERIFY(shortcuts.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(shortcuts.readAll()).object()["SDL_KeyModMask"].toArray(), QJsonArray {"KMOD_NONE"});
        shortcuts.close();
        // An explicit preference and other SDL settings must survive future launches.
        QVERIFY(shortcuts.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const QByteArray customSettings = "{\"SDL_KeyModMask\":[\"KMOD_RCTRL\"],\"SDL_Disconnect\":\"SDL_SCANCODE_F12\"}\n";
        QCOMPARE(shortcuts.write(customSettings), qint64(customSettings.size()));
        shortcuts.close();
        QVERIFY(!bytes.contains("/scale-desktop:"));
        QVERIFY(!bytes.contains("/scale-device:"));
        QVERIFY(bytes.contains("+dynamic-resolution\n"));
        entry["host"] = "::1";
        entry["desktopScale"] = 99;
        QVERIFY(!manager.save(entry));
        entry["desktopScale"] = 501;
        QVERIFY(!manager.save(entry));
        entry["desktopScale"] = "invalid";
        QVERIFY(!manager.save(entry));
        entry["desktopScale"] = 200;
        QVERIFY(!bytes.contains("/network:modem\n"));
        entry["host"] = "::1";
        entry["performanceMode"] = true;
        entry["captureShortcuts"] = true;
        QVERIFY(manager.save(entry));
        Manager performanceRestored;
        QCOMPARE(performanceRestored.connections().first().toMap()["desktopScale"].toInt(), 200);
        QVERIFY(performanceRestored.connections().first().toMap()["performanceMode"].toBool());
        QVERIFY(performanceRestored.connections().first().toMap()["captureShortcuts"].toBool());
        manager.connectTo(id, "secret-test-value");
        QTRY_VERIFY_WITH_TIMEOUT(!manager.connections().first().toMap()["active"].toBool(), 4000);
        input.close(); QVERIFY(input.open(QIODevice::ReadOnly));
        bytes = input.readAll();
        QVERIFY(bytes.contains("/scale-desktop:200\n"));
        QVERIFY(bytes.contains("/scale-device:100\n"));
        QVERIFY(bytes.contains("+dynamic-resolution\n"));
        QVERIFY(bytes.contains("+grab-keyboard\n"));
        QVERIFY(!bytes.contains("-grab-keyboard\n"));
        for (const auto &option : {"/network:modem", "/bpp:16", "-wallpaper", "-themes", "-fonts", "-aero", "-window-drag", "-menu-anims"})
            QVERIFY2(bytes.contains(QByteArray(option) + '\n'), option);
        entry["desktopScale"] = 0;
        entry["performanceMode"] = false;
        entry["captureShortcuts"] = false;
        QVERIFY(manager.save(entry));
        manager.connectTo(id, "secret-test-value");
        QTRY_VERIFY_WITH_TIMEOUT(!manager.connections().first().toMap()["active"].toBool(), 4000);
        input.close(); QVERIFY(input.open(QIODevice::ReadOnly));
        bytes = input.readAll();
        QVERIFY(!bytes.contains("/network:modem\n"));
        QVERIFY(!bytes.contains("/bpp:16\n"));
        QVERIFY(!bytes.contains("/scale-desktop:"));
        QVERIFY(!bytes.contains("/scale-device:"));
        QVERIFY(shortcuts.open(QIODevice::ReadOnly));
        QCOMPARE(shortcuts.readAll(), customSettings);
        shortcuts.close();
        QVERIFY(bytes.contains("-grab-keyboard\n"));
        QVERIFY(!bytes.contains("+grab-keyboard\n"));
        QFile storage(root + "/connections.json"); QVERIFY(storage.open(QIODevice::ReadOnly));
        QVERIFY(!storage.readAll().contains("secret-test-value"));
        QVERIFY(fake.open(QIODevice::WriteOnly | QIODevice::Truncate));
        fake.write("#!/bin/sh\ncat > /dev/null\nexit 7\n");
        fake.close();
        manager.connectTo(id, "secret-test-value");
        QTRY_VERIFY_WITH_TIMEOUT(!manager.connections().first().toMap()["active"].toBool(), 4000);
        QVERIFY(manager.message().contains("code 7"));
        QVERIFY(shortcuts.open(QIODevice::WriteOnly | QIODevice::Truncate));
        shortcuts.write("{\"SDL_Disconnect\":\"SDL_SCANCODE_F12\"}");
        shortcuts.close();
        manager.connectTo(id, "secret-test-value");
        QTRY_VERIFY_WITH_TIMEOUT(!manager.connections().first().toMap()["active"].toBool(), 4000);
        QVERIFY(shortcuts.open(QIODevice::ReadOnly));
        const auto seeded = QJsonDocument::fromJson(shortcuts.readAll()).object();
        QCOMPARE(seeded["SDL_KeyModMask"].toArray(), QJsonArray {"KMOD_NONE"});
        QCOMPARE(seeded["SDL_Disconnect"].toString(), QString("SDL_SCANCODE_F12"));
        shortcuts.close();
        QVERIFY(shortcuts.open(QIODevice::WriteOnly | QIODevice::Truncate));
        shortcuts.write("broken");
        shortcuts.close();
        manager.connectTo(id, "secret-test-value");
        QVERIFY(!manager.connections().first().toMap()["active"].toBool());
        QVERIFY(manager.message().contains("shortcut settings are invalid"));
        QVERIFY(shortcuts.open(QIODevice::ReadOnly));
        QCOMPARE(shortcuts.readAll(), QByteArray("broken"));
        shortcuts.close();
        // A private X11 runtime bypasses broken SDL settings and gets its own libraries.
        const QString runtime = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                                + "/portalview/freerdp-3.31.1";
        QVERIFY(QDir().mkpath(runtime + "/bin"));
        QVERIFY(QDir().mkpath(runtime + "/lib"));
        QFile x11(runtime + "/bin/xfreerdp3");
        QVERIFY(x11.open(QIODevice::WriteOnly));
        x11.write("#!/bin/sh\nprintf '%s' \"$LD_LIBRARY_PATH\" > \"$PORTALVIEW_TEST_ROOT/library-path\"\ncat > \"$PORTALVIEW_TEST_ROOT/x11-input\"\n");
        x11.close();
        QVERIFY(x11.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        const QByteArray parentLibraryPath = qgetenv("LD_LIBRARY_PATH");
        manager.connectTo(id, "private-runtime-secret");
        QTRY_VERIFY_WITH_TIMEOUT(!manager.connections().first().toMap()["active"].toBool(), 4000);
        QCOMPARE(manager.message(), QString("Session closed."));
        QFile libraryPath(root + "/library-path"); QVERIFY(libraryPath.open(QIODevice::ReadOnly));
        QVERIFY(libraryPath.readAll().startsWith((runtime + "/lib").toUtf8()));
        QCOMPARE(qgetenv("LD_LIBRARY_PATH"), parentLibraryPath);
        QFile x11Input(root + "/x11-input"); QVERIFY(x11Input.open(QIODevice::ReadOnly));
        QVERIFY(x11Input.readAll().contains("/p:private-runtime-secret\n"));
        QVERIFY(shortcuts.open(QIODevice::ReadOnly));
        QCOMPARE(shortcuts.readAll(), QByteArray("broken"));
        shortcuts.close();
        // A runtime without a private lib directory (e.g. Nix RPATH) leaves LD_LIBRARY_PATH untouched.
        QVERIFY(QDir(runtime + "/lib").removeRecursively());
        manager.connectTo(id, "nix-runtime-secret");
        QTRY_VERIFY_WITH_TIMEOUT(!manager.connections().first().toMap()["active"].toBool(), 4000);
        libraryPath.close(); QVERIFY(libraryPath.open(QIODevice::ReadOnly));
        QCOMPARE(libraryPath.readAll(), parentLibraryPath);
        QVERIFY(QDir(runtime).removeRecursively());
        QVERIFY(manager.remove(id));
        QVERIFY(manager.connections().isEmpty());
        storage.close(); QVERIFY(storage.open(QIODevice::WriteOnly | QIODevice::Truncate));
        storage.write("broken"); storage.close();
        Manager corrupt;
        entry["host"] = "localhost";
        QVERIFY(!corrupt.save(entry));
    }

    void audioMultimonFolderSharingAndErrors() {
        QCoreApplication::setOrganizationName("PortalviewTests");
        QCoreApplication::setApplicationName("features-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
        const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        qputenv("XDG_CONFIG_HOME", (root + "/config").toUtf8());
        QVERIFY(QDir().mkpath(root + "/bin"));
        QVERIFY(QDir().mkpath(root + "/My Docs"));
        QVERIFY(QDir().mkpath(root + "/bad,dir"));

        QFile fake(root + "/bin/sdl-freerdp3");
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\ncat > \"$PORTALVIEW_TEST_ROOT/input\"\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        qputenv("PORTALVIEW_TEST_ROOT", root.toUtf8());
        qputenv("PATH", (root + "/bin:" + qEnvironmentVariable("PATH")).toUtf8());

        Manager manager;
        QVariantMap entry {
            {"name", "Studio"},
            {"host", "10.0.0.5"},
            {"username", "carol"},
            {"port", 3389},
            {"audioMode", "bogus"}
        };
        QVERIFY(!manager.save(entry));
        entry["audioMode"] = "local";
        entry["sharedFolder"] = "relative/path";
        QVERIFY(!manager.save(entry));
        entry["sharedFolder"] = root + "/does-not-exist";
        QVERIFY(!manager.save(entry));
        entry["sharedFolder"] = root + "/bad,dir";
        QVERIFY(!manager.save(entry));
        entry["sharedFolder"] = root + "/My Docs";
        entry["microphone"] = true;
        entry["multimon"] = true;
        entry["fullscreen"] = true;
        QVERIFY(manager.save(entry));

        const QString id = manager.connections().first().toMap()["id"].toString();
        Manager reloaded;
        const auto saved = reloaded.connections().first().toMap();
        QCOMPARE(saved["sharedFolder"].toString(), QDir::cleanPath(root + "/My Docs"));
        QCOMPARE(saved["audioMode"].toString(), QString("local"));
        QVERIFY(saved["microphone"].toBool());
        QVERIFY(saved["multimon"].toBool());

        manager.connectTo(id, "pw");
        QTRY_VERIFY_WITH_TIMEOUT(!manager.connections().first().toMap()["active"].toBool(), 4000);
        QFile input(root + "/input");
        QVERIFY(input.open(QIODevice::ReadOnly));
        QByteArray bytes = input.readAll();
        input.close();
        QVERIFY(bytes.contains("/sound\n"));
        QVERIFY(bytes.contains("/audio-mode:0\n"));
        QVERIFY(bytes.contains("/microphone\n"));
        QVERIFY(bytes.contains("/multimon\n"));
        QVERIFY(!bytes.contains("/f\n"));
        QVERIFY(bytes.contains(("/drive:MyDocs," + QDir::cleanPath(root + "/My Docs") + "\n").toUtf8()));

        // Remote audio mode without microphone.
        entry["id"] = id;
        entry["audioMode"] = "remote";
        entry["microphone"] = false;
        entry["multimon"] = false;
        QVERIFY(manager.save(entry));
        manager.connectTo(id, "pw");
        QTRY_VERIFY_WITH_TIMEOUT(!manager.connections().first().toMap()["active"].toBool(), 4000);
        QVERIFY(input.open(QIODevice::ReadOnly));
        bytes = input.readAll();
        input.close();
        QVERIFY(bytes.contains("/audio-mode:1\n"));
        QVERIFY(!bytes.contains("/sound\n"));
        QVERIFY(!bytes.contains("/microphone\n"));
        QVERIFY(bytes.contains("/f\n"));

        // Deleted shared folder fails cleanly before launch.
        QVERIFY(QDir(root + "/My Docs").removeRecursively());
        manager.connectTo(id, "pw");
        QVERIFY(!manager.connections().first().toMap()["active"].toBool());
        QVERIFY(manager.message().contains("Shared folder is unavailable"));

        // Clear shared folder and verify structured FreeRDP error reporting.
        entry["sharedFolder"] = "";
        QVERIFY(manager.save(entry));

        auto runWithOutput = [&](const QByteArray &script, const QString &expectedSubstring) {
            QVERIFY(fake.open(QIODevice::WriteOnly | QIODevice::Truncate));
            fake.write(script);
            fake.close();
            manager.connectTo(id, "pw");
            QTRY_VERIFY_WITH_TIMEOUT(!manager.connections().first().toMap()["active"].toBool(), 4000);
            QVERIFY2(manager.message().contains(expectedSubstring, Qt::CaseInsensitive),
                     qPrintable(manager.message()));
        };
        runWithOutput("#!/bin/sh\ncat >/dev/null\necho '[12:00:00:000] [1:2] [ERROR][com.freerdp.core] - ERRCONNECT_LOGON_FAILURE [0x00020014]' >&2\nexit 131\n",
                      "authentication failed");
        runWithOutput("#!/bin/sh\ncat >/dev/null\necho '[12:00:00:000] [1:2] [ERROR][com.freerdp.core] - ERRCONNECT_DNS_NAME_NOT_FOUND' >&2\nexit 131\n",
                      "could not resolve host");
        runWithOutput("#!/bin/sh\ncat >/dev/null\necho '[12:00:00:000] [1:2] [ERROR][com.freerdp.core] - ERRCONNECT_TLS_CONNECT_FAILED' >&2\nexit 131\n",
                      "certificate or TLS verification failed");
        runWithOutput("#!/bin/sh\ncat >/dev/null\necho '[12:00:00:000] [1:2] [ERROR][com.freerdp.core] - Custom server rejected session' >&2\nexit 12\n",
                      "Custom server rejected session");
    }

    void importRdpAndJson() {
        QCoreApplication::setOrganizationName("PortalviewTests");
        QCoreApplication::setApplicationName("imports-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
        const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QVERIFY(QDir().mkpath(root));

        Manager manager;

        // Complete UTF-8 .rdp file.
        const QString rdpPath = root + "/Workstation.rdp";
        QFile rdp(rdpPath);
        QVERIFY(rdp.open(QIODevice::WriteOnly));
        rdp.write("full address:s:192.168.10.20:3390\r\n"
                  "username:s:CORP\\dave\r\n"
                  "screen mode id:i:2\r\n"
                  "use multimon:i:1\r\n"
                  "redirectclipboard:i:0\r\n"
                  "audiomode:i:0\r\n"
                  "audiocapturemode:i:1\r\n"
                  "desktopscalefactor:i:150\r\n"
                  "keyboardhook:i:1\r\n"
                  "connection type:i:1\r\n");
        rdp.close();
        QCOMPARE(manager.importFile(rdpPath), 1);
        QCOMPARE(manager.connections().size(), 1);
        const auto imported = manager.connections().first().toMap();
        QCOMPARE(imported["name"].toString(), QString("Workstation"));
        QCOMPARE(imported["host"].toString(), QString("192.168.10.20"));
        QCOMPARE(imported["port"].toInt(), 3390);
        QCOMPARE(imported["domain"].toString(), QString("CORP"));
        QCOMPARE(imported["username"].toString(), QString("dave"));
        QVERIFY(imported["fullscreen"].toBool());
        QVERIFY(imported["multimon"].toBool());
        QVERIFY(!imported["clipboard"].toBool());
        QCOMPARE(imported["audioMode"].toString(), QString("local"));
        QVERIFY(imported["microphone"].toBool());
        QCOMPARE(imported["desktopScale"].toInt(), 150);
        QVERIFY(imported["captureShortcuts"].toBool());
        QVERIFY(imported["performanceMode"].toBool());

        // Duplicate .rdp import is rejected.
        QCOMPARE(manager.importFile(rdpPath), 0);
        QCOMPARE(manager.connections().size(), 1);

        // UTF-16LE .rdp without username emits importNeedsReview.
        const QString utf16Path = root + "/AzureVM.rdp";
        const QString utf16Content = "full address:s:[::1]:3391\r\naudiomode:i:1\r\nuse multimon:i:1\r\n";
        QByteArray utf16Bytes;
        utf16Bytes.append('\xFF');
        utf16Bytes.append('\xFE');
        for (QChar ch : utf16Content) {
            const ushort u = ch.unicode();
            utf16Bytes.append(static_cast<char>(u & 0xFF));
            utf16Bytes.append(static_cast<char>((u >> 8) & 0xFF));
        }
        QFile utf16File(utf16Path);
        QVERIFY(utf16File.open(QIODevice::WriteOnly));
        utf16File.write(utf16Bytes);
        utf16File.close();

        QSignalSpy reviewSpy(&manager, &Manager::importNeedsReview);
        QCOMPARE(manager.importFile(QUrl::fromLocalFile(utf16Path).toString()), 0);
        QCOMPARE(reviewSpy.count(), 1);
        const auto partial = reviewSpy.first().first().toMap();
        QCOMPARE(partial["name"].toString(), QString("AzureVM"));
        QCOMPARE(partial["host"].toString(), QString("[::1]"));
        QCOMPARE(partial["port"].toInt(), 3391);
        QCOMPARE(partial["audioMode"].toString(), QString("remote"));
        QVERIFY(partial["multimon"].toBool());
        QVERIFY(partial["username"].toString().isEmpty());
        QCOMPARE(manager.connections().size(), 1);

        // JSON bulk import: 1 valid new, 1 duplicate, 1 invalid.
        const QString jsonPath = root + "/backup.json";
        QFile jsonFile(jsonPath);
        QVERIFY(jsonFile.open(QIODevice::WriteOnly));
        jsonFile.write(R"([
            {"name":"Workstation","host":"192.168.10.20","port":3390,"domain":"CORP","username":"dave"},
            {"name":"LabBox","host":"lab.local","port":3389,"username":"eve","group":"Lab","audioMode":"local","multimon":true},
            {"name":"Broken","host":"bad host","username":"frank"}
        ])");
        jsonFile.close();
        QCOMPARE(manager.importFile(jsonPath), 1);
        QCOMPARE(manager.connections().size(), 2);
        QVERIFY(manager.message().contains("Imported 1 connection"));
        QVERIFY(manager.message().contains("1 duplicate skipped"));
        QVERIFY(manager.message().contains("1 invalid skipped"));
    }
};
QTEST_GUILESS_MAIN(ManagerTest)
#include "manager_test.moc"

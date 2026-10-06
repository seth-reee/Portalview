#include <QtTest>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QUuid>
#include "manager.h"
class ManagerTest : public QObject {
    Q_OBJECT
private slots:
    void persistenceAndLaunch() {
        QCoreApplication::setOrganizationName("PortalviewTests");
        QCoreApplication::setApplicationName("run-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
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
        QCOMPARE(manager.connections().first().toMap()["host"].toString(), QString("::1"));
        QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QVERIFY(QDir().mkpath(root + "/bin"));
        QFile fake(root + "/bin/sdl-freerdp3");
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$PORTALVIEW_TEST_ROOT/argv\"\ncat > \"$PORTALVIEW_TEST_ROOT/input\"\nsleep 1\n");
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
        QVERIFY(!bytes.contains("/network:modem\n"));
        entry["host"] = "::1";
        entry["performanceMode"] = true;
        QVERIFY(manager.save(entry));
        Manager performanceRestored;
        QVERIFY(performanceRestored.connections().first().toMap()["performanceMode"].toBool());
        manager.connectTo(id, "secret-test-value");
        QTRY_VERIFY_WITH_TIMEOUT(!manager.connections().first().toMap()["active"].toBool(), 4000);
        input.close(); QVERIFY(input.open(QIODevice::ReadOnly));
        bytes = input.readAll();
        for (const auto &option : {"/network:modem", "/bpp:16", "-wallpaper", "-themes", "-fonts", "-aero", "-window-drag", "-menu-anims"})
            QVERIFY2(bytes.contains(QByteArray(option) + '\n'), option);
        entry["performanceMode"] = false;
        QVERIFY(manager.save(entry));
        manager.connectTo(id, "secret-test-value");
        QTRY_VERIFY_WITH_TIMEOUT(!manager.connections().first().toMap()["active"].toBool(), 4000);
        input.close(); QVERIFY(input.open(QIODevice::ReadOnly));
        bytes = input.readAll();
        QVERIFY(!bytes.contains("/network:modem\n"));
        QVERIFY(!bytes.contains("/bpp:16\n"));
        QFile storage(root + "/connections.json"); QVERIFY(storage.open(QIODevice::ReadOnly));
        QVERIFY(!storage.readAll().contains("secret-test-value"));
        QVERIFY(fake.open(QIODevice::WriteOnly | QIODevice::Truncate));
        fake.write("#!/bin/sh\ncat > /dev/null\nexit 7\n");
        fake.close();
        manager.connectTo(id, "secret-test-value");
        QTRY_VERIFY_WITH_TIMEOUT(!manager.connections().first().toMap()["active"].toBool(), 4000);
        QVERIFY(manager.message().contains("code 7"));
        QVERIFY(manager.remove(id));
        QVERIFY(manager.connections().isEmpty());
        storage.close(); QVERIFY(storage.open(QIODevice::WriteOnly | QIODevice::Truncate));
        storage.write("broken"); storage.close();
        Manager corrupt;
        entry["host"] = "localhost";
        QVERIFY(!corrupt.save(entry));
    }
};
QTEST_GUILESS_MAIN(ManagerTest)
#include "manager_test.moc"

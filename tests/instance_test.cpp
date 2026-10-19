#include <QtTest>
#include <QProcess>
#include <cstdio>
#include "singleinstance.h"

class InstanceTest : public QObject {
    Q_OBJECT
private:
    void launch(QProcess &process, const QStringList &request = {}) {
        process.start(QCoreApplication::applicationFilePath(), QStringList{"--helper"} + request);
    }
private slots:
    void duplicateActivatesAndCrashReleasesOwnership() {
        QProcess primary;
        launch(primary);
        QVERIFY(primary.waitForStarted());
        QVERIFY(primary.waitForReadyRead());
        QCOMPARE(primary.readAllStandardOutput(), QByteArray("ready\n"));
        for (int i = 0; i < 3; ++i) {
            QProcess secondary;
            launch(secondary);
            QVERIFY(secondary.waitForFinished());
            QCOMPARE(secondary.exitCode(), 0);
            QCOMPARE(secondary.readAllStandardOutput(), QByteArray());
            QTRY_VERIFY(primary.bytesAvailable() > 0);
            QCOMPARE(primary.readAllStandardOutput(), QByteArray("activated\n"));
            QCOMPARE(primary.state(), QProcess::Running);
        }
        primary.kill();
        QVERIFY(primary.waitForFinished());
        QProcess replacement;
        launch(replacement);
        QVERIFY(replacement.waitForReadyRead());
        QCOMPARE(replacement.readAllStandardOutput(), QByteArray("ready\n"));
        replacement.kill();
        QVERIFY(replacement.waitForFinished());
    }
    void addHostReachesRunningInstance() {
        QProcess primary; launch(primary);
        QVERIFY(primary.waitForReadyRead());
        QCOMPARE(primary.readAllStandardOutput(), QByteArray("ready\n"));
        QProcess request; launch(request, {"--request", "192.168.1.42", "Office PC"});
        QVERIFY(request.waitForFinished()); QCOMPARE(request.exitCode(), 0);
        QTRY_VERIFY(primary.bytesAvailable() > 0);
        QCOMPARE(primary.readAllStandardOutput(), QByteArray("add:192.168.1.42:Office PC\n"));
        QProcess invalid; launch(invalid, {"--request", "bad host", "Office PC"});
        QVERIFY(invalid.waitForFinished()); QCOMPARE(invalid.exitCode(), 1);
        QCOMPARE(primary.state(), QProcess::Running);
        primary.kill(); QVERIFY(primary.waitForFinished());
    }
    void simultaneousLaunchesChooseOneOwner() {
        QProcess first, second;
        launch(first);
        launch(second);
        QTRY_VERIFY(first.state() == QProcess::NotRunning || second.state() == QProcess::NotRunning);
        auto &owner = first.state() == QProcess::Running ? first : second;
        auto &duplicate = first.state() == QProcess::NotRunning ? first : second;
        QCOMPARE(duplicate.exitCode(), 0);
        QTRY_VERIFY(owner.bytesAvailable() > 0);
        QByteArray output = owner.readAllStandardOutput();
        QTRY_VERIFY((output += owner.readAllStandardOutput()).contains("activated\n"));
        QVERIFY(output.startsWith("ready\n"));
        owner.kill();
        QVERIFY(owner.waitForFinished());
    }
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().contains("--helper")) {
        SingleInstance instance;
        QObject::connect(&instance, &SingleInstance::activationRequested, [] {
            std::puts("activated"); std::fflush(stdout);
        });
        QObject::connect(&instance, &SingleInstance::addConnectionRequested, [](const QString &host, const QString &name) {
            const auto line = ("add:" + host + ":" + name).toUtf8();
            std::puts(line.constData()); std::fflush(stdout);
        });
        const auto args = app.arguments();
        const int requestIndex = args.indexOf("--request");
        const auto result = instance.start(requestIndex < 0 ? QString() : args.value(requestIndex + 1),
                                           requestIndex < 0 ? QString() : args.value(requestIndex + 2));
        if (result != SingleInstance::Primary) return result == SingleInstance::Activated ? 0 : 1;
        std::puts("ready"); std::fflush(stdout);
        return app.exec();
    }
    InstanceTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "instance_test.moc"

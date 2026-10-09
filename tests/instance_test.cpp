#include <QtTest>
#include <QProcess>
#include <cstdio>
#include "singleinstance.h"

class InstanceTest : public QObject {
    Q_OBJECT
private:
    void launch(QProcess &process) {
        process.start(QCoreApplication::applicationFilePath(), {"--helper"});
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
        const auto result = instance.start();
        if (result != SingleInstance::Primary) return result == SingleInstance::Activated ? 0 : 1;
        std::puts("ready"); std::fflush(stdout);
        return app.exec();
    }
    InstanceTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "instance_test.moc"

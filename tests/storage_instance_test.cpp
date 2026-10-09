#include <QtTest>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QLocalServer>
#include <QCryptographicHash>
#include "instance.h"

class InstanceTest : public QObject {
    Q_OBJECT
private slots:
    void invalidStorageFailsClosed() {
        QTemporaryFile file;
        QVERIFY(file.open());
        Instance instance;
        QCOMPARE(instance.start(file.fileName() + "/data"), Instance::Failed);
        QVERIFY(!instance.error().isEmpty());
    }
    void secondLaunchActivatesPrimary() {
        QTemporaryDir data;
        QVERIFY(data.isValid());
        {
            Instance first;
            QCOMPARE(first.start(data.path()), Instance::Primary);
            QSignalSpy activated(&first, &Instance::activationRequested);
            Instance second;
            QCOMPARE(second.start(data.path()), Instance::Activated);
            QTRY_COMPARE(activated.count(), 1);
            Instance third;
            QCOMPARE(third.start(data.path()), Instance::Activated);
            QTRY_COMPARE(activated.count(), 2);
        }
        Instance reopened;
        QCOMPARE(reopened.start(data.path()), Instance::Primary);
    }
    void separateStorageAllowsSeparateInstances() {
        QTemporaryDir a, b;
        Instance first, second;
        QCOMPARE(first.start(a.path()), Instance::Primary);
        QCOMPARE(second.start(b.path()), Instance::Primary);
    }
    void staleSocketIsRecovered() {
        QTemporaryDir data;
        const QString name = "portalview-" + QString::fromLatin1(QCryptographicHash::hash(
            QDir(data.path()).canonicalPath().toUtf8(), QCryptographicHash::Sha256).toHex());
        QLocalServer leftover;
        QVERIFY(leftover.listen(name));
        Instance instance;
        QCOMPARE(instance.start(data.path()), Instance::Primary);
    }
};
QTEST_GUILESS_MAIN(InstanceTest)
#include "storage_instance_test.moc"

#include <QtTest>
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQuickItem>
#include <QUuid>
#include "manager.h"
#include "theme.h"
#include "tray.h"

class DialogTest : public QObject {
    Q_OBJECT
private slots:
    void openDialogsWithoutBindingLoops() {
        Manager manager;
        Theme theme;
        Tray tray(&manager);
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("manager", &manager);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("tray", &tray);
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&warnings](const QList<QQmlError> &errors) {
            for (const auto &error : errors) warnings.append(error.toString());
        });
        engine.load(QUrl::fromLocalFile(QStringLiteral(PORTALVIEW_QML_PATH)));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        QVERIFY(window);
        window->hide();
        emit tray.openRequested();
        QTRY_VERIFY(window->isVisible());
        auto licenseText = window->findChild<QObject*>("aboutLicenseText");
        QVERIFY(licenseText);
        QVERIFY(licenseText->property("text").toString().contains("Permission is hereby granted"));
        QVariantMap entry {{"name", "Office"}, {"host", "localhost"}, {"username", "alice"}, {"group", " Work "}};
        QVERIFY(manager.save(entry));
        entry = manager.connections().first().toMap();
        QCOMPARE(entry["group"].toString(), QString("Work"));
        Manager restored;
        QCOMPARE(restored.connections().first().toMap()["group"].toString(), QString("Work"));
        QVariantMap ungrouped {{"name", "Home"}, {"host", "localhost"}, {"username", "bob"}};
        QVERIFY(manager.save(ungrouped));
        QMenu *trayMenu = nullptr;
        for (auto widget : QApplication::topLevelWidgets())
            if (widget->objectName() == "trayMenu") trayMenu = qobject_cast<QMenu*>(widget);
        QVERIFY(trayMenu);
        QVERIFY(QMetaObject::invokeMethod(trayMenu, "aboutToShow"));
        QVERIFY(trayMenu->findChild<QAction*>("trayOpen"));
        QVERIFY(trayMenu->findChild<QAction*>("trayClose"));
        QAction *officeAction = nullptr;
        for (auto action : trayMenu->actions()) {
            if (action->menu() && action->text() == "Work")
                for (auto child : action->menu()->actions())
                    if (child->data().toString() == entry["id"].toString()) officeAction = child;
        }
        QVERIFY(officeAction);
        window->hide();
        officeAction->trigger();
        QTRY_VERIFY(window->isVisible());
        auto trayPassword = window->findChild<QObject*>("passwordDialog");
        QVERIFY(trayPassword);
        QTRY_VERIFY(trayPassword->property("visible").toBool());
        QCOMPARE(trayPassword->property("connectionId").toString(), entry["id"].toString());
        QVERIFY(QMetaObject::invokeMethod(trayPassword, "reject"));
        auto list = window->findChild<QObject*>("connectionList");
        QVERIFY(list);
        QTRY_COMPARE(list->property("count").toInt(), 2);
        window->setProperty("selectedGroup", "Work");
        QTRY_COMPARE(list->property("count").toInt(), 1);
        window->setProperty("selectedId", entry["id"]);
        window->setProperty("selectedGroup", "");
        QTRY_COMPARE(list->property("count").toInt(), 1);
        QCOMPARE(window->property("selectedId").toString(), QString());
        entry["group"] = "Lab";
        QVERIFY(manager.save(entry));
        window->setProperty("selectedGroup", "Lab");
        QTRY_COMPARE(list->property("count").toInt(), 1);
        entry["group"] = "";
        QVERIFY(manager.save(entry));
        QTRY_COMPARE(list->property("count").toInt(), 2);
        entry["group"] = "bad\ngroup";
        QVERIFY(!manager.save(entry));
        auto filter = window->findChild<QObject*>("groupFilter");
        QVERIFY(filter);
        auto popup = filter->property("popup").value<QObject*>();
        QVERIFY(popup);
        QVERIFY(QMetaObject::invokeMethod(popup, "open"));
        QTest::qWait(100);
        QVERIFY(popup->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
        QTest::qWait(200);
        auto listItem = qobject_cast<QQuickItem*>(list);
        QVERIFY(listItem);
        window->setProperty("selectedId", manager.connections().last().toMap()["id"]);
        QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier,
                         listItem->mapToScene(QPointF(30, 23)).toPoint());
        auto contextMenu = window->findChild<QObject*>("entryContextMenu");
        QVERIFY(contextMenu);
        QTRY_VERIFY(contextMenu->property("visible").toBool());
        QVERIFY(contextMenu->property("width").toReal() >= 180);
        QVERIFY(contextMenu->property("height").toReal() >= 36);
        QCOMPARE(window->property("selectedId").toString(), entry["id"].toString());
        auto editAction = window->findChild<QObject*>("editConnectionMenuItem");
        QVERIFY(editAction);
        auto editItem = qobject_cast<QQuickItem*>(editAction);
        QVERIFY(editItem);
        QVERIFY(editItem->width() >= 150);
        QVERIFY(editItem->height() >= 36);
        QTest::qWait(200);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                         editItem->mapToScene(QPointF(editItem->width() / 2, editItem->height() / 2)).toPoint());
        auto editor = window->findChild<QObject*>("connectionEditor");
        QVERIFY(editor);
        QTRY_VERIFY(editor->property("visible").toBool());
        QCOMPARE(editor->property("entryId").toString(), entry["id"].toString());
        auto capture = window->findChild<QObject*>("captureShortcutsToggle");
        QVERIFY(capture);
        QVERIFY(!capture->property("checked").toBool());
        entry = manager.connections().first().toMap();
        entry["captureShortcuts"] = true;
        QVERIFY(manager.save(entry));
        QVERIFY(QMetaObject::invokeMethod(window, "edit", Q_ARG(QVariant, QVariant(entry))));
        QVERIFY(capture->property("checked").toBool());
        QVERIFY(QMetaObject::invokeMethod(window, "edit", Q_ARG(QVariant, QVariant())));
        QVERIFY(!capture->property("checked").toBool());
        QVERIFY(QMetaObject::invokeMethod(editor, "close"));
        QVERIFY(QMetaObject::invokeMethod(contextMenu, "close"));
        for (auto size : {QSize(1040, 650), QSize(760, 540)}) {
            window->resize(size);
            for (const auto &name : {"connectionEditor", "passwordDialog", "deleteDialog", "aboutDialog"}) {
                auto dialog = window->findChild<QObject*>(name);
                QVERIFY2(dialog, name);
                QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
                QTest::qWait(100);
                QVERIFY(dialog->property("visible").toBool());
                QVERIFY(dialog->property("width").toReal() > 0);
                QVERIFY(dialog->property("height").toReal() > 0);
                QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
                QTest::qWait(100);
            }
        }
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        QVERIFY(!tray.enabled());
        tray.setEnabled(true);
        QVERIFY(tray.enabled());
        {
            Tray restoredTray(&manager);
            QVERIFY(restoredTray.enabled());
        }
        QSignalSpy opened(&tray, &Tray::openRequested);
        tray.setEnabled(false);
        QVERIFY(!tray.enabled());
        QCOMPARE(opened.count(), 1);
        Tray disabledTray(&manager);
        QVERIFY(!disabledTray.enabled());
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("PortalviewTests");
    QCoreApplication::setApplicationName("dialogs-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QQuickStyle::setStyle("Basic");
    DialogTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "dialog_test.moc"

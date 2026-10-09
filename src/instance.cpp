#include "instance.h"
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QLocalSocket>
#include <QThread>

Instance::Result Instance::start(const QString &directory) {
    if (!QDir().mkpath(directory)) {
        m_error = "Cannot create connection storage.";
        return Failed;
    }
    const QString path = QDir(directory).canonicalPath();
    const QString socketName = "portalview-" + QString::fromLatin1(
        QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex());
    m_lock = std::make_unique<QLockFile>(path + "/instance.lock");
    // A live instance may remain open for days. Only remove locks of dead processes.
    m_lock->setStaleLockTime(0);
    if (!m_lock->tryLock()) {
        if (m_lock->error() != QLockFile::LockFailedError) {
            m_error = "Cannot lock connection storage.";
            return Failed;
        }
        // The first instance may still be starting its activation server.
        QElapsedTimer elapsed;
        elapsed.start();
        do {
            QLocalSocket socket;
            socket.connectToServer(socketName);
            if (socket.waitForConnected(100)) return Activated;
            QThread::msleep(10);
        } while (elapsed.elapsed() < 5000);
        m_error = "Portalview is already running, but its window could not be activated.";
        return Failed;
    }
    // The lock makes removing a socket left behind by a crash safe.
    QLocalServer::removeServer(socketName);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(socketName)) {
        m_error = "Cannot start window activation server: " + m_server.errorString();
        m_lock->unlock();
        return Failed;
    }
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (auto socket = m_server.nextPendingConnection()) {
            socket->close();
            socket->deleteLater();
            emit activationRequested();
        }
    });
    return Primary;
}

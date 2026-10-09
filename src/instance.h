#pragma once
#include <QObject>
#include <QLocalServer>
#include <QLockFile>
#include <memory>

class Instance : public QObject {
    Q_OBJECT
public:
    enum Result { Primary, Activated, Failed };
    explicit Instance(QObject *parent = nullptr) : QObject(parent) {}
    Result start(const QString &directory);
    QString error() const { return m_error; }
signals:
    void activationRequested();
private:
    std::unique_ptr<QLockFile> m_lock;
    QLocalServer m_server;
    QString m_error;
};

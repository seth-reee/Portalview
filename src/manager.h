#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QProcess>
#include <QHash>
#include <QUrl>
class Manager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList connections READ connections NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QString licenseText READ licenseText CONSTANT)
public:
    explicit Manager(QObject *parent = nullptr);
    ~Manager();
    QVariantList connections() const;
    QString message() const { return m_message; }
    QString licenseText() const;
    Q_INVOKABLE bool save(QVariantMap entry);
    Q_INVOKABLE bool remove(QString id);
    Q_INVOKABLE int importFile(const QString &pathOrUrl);
    Q_INVOKABLE QString localPath(const QUrl &url) const;
    Q_INVOKABLE void connectTo(QString id, QString password);
    Q_INVOKABLE void disconnectFrom(QString id);
signals:
    void changed();
    void messageChanged();
    void importNeedsReview(QVariantMap entry);
private:
    bool validateEntry(const QVariantMap &entry, QVariantMap &clean, bool requireUsername, QString &error) const;
    bool persist(const QVariantList &entries);
    void report(QString text);
    QVariantList m_entries;
    QString m_path, m_message;
    QHash<QString, QProcess*> m_sessions;
};

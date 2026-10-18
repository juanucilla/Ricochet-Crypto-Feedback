#pragma once
#include <QObject>
#include <QJsonObject>
#include <QStringList>
#include <QVariantList>
#include <QTimer>
#include <QMap>

// Experimental, fixed-roster groups. Transport identities MUST be authenticated.
// All peers must already be accepted contacts; this class never adds contacts.
class GroupManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList groups READ groups NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString self READ self NOTIFY changed)
    Q_PROPERTY(QVariantList contacts READ contacts NOTIFY changed)
public:
    GroupManager(QString self, QString storagePath, QObject *parent = nullptr);
    QString self() const { return m_self; }
    void setIdentity(const QString &identity);
    QString error() const { return m_error; }
    QVariantList groups() const;
    void setKnownPeers(const QStringList &peers) { m_peers = peers; }
    void setContactNames(const QMap<QString, QString> &names);
    QVariantList contacts() const;
    Q_INVOKABLE QString displayName(const QString &peer) const;
    // Returns false only for ordinary direct messages. Reserved frames are consumed.
    bool receive(const QString &authenticatedPeer, const QString &wire);
    Q_INVOKABLE QString create(const QString &title, const QStringList &peers);
    Q_INVOKABLE bool accept(const QString &id);
    Q_INVOKABLE bool leave(const QString &id);
    Q_INVOKABLE bool send(const QString &id, const QString &text, const QString &replyTo = QString());
    Q_INVOKABLE QVariantList messages(const QString &id, const QString &query = QString()) const;
    Q_INVOKABLE QVariantMap details(const QString &id) const;
    Q_INVOKABLE void markRead(const QString &id);
    Q_INVOKABLE void setMuted(const QString &id, bool muted);
    Q_INVOKABLE void setDraft(const QString &id, const QString &draft);
    Q_INVOKABLE void retry();
signals:
    void changed();
    void errorChanged();
    void outbound(const QString &peer, const QString &wire);
    void notification(const QString &title, const QString &body);
private:
    bool fail(const QString &message);
    bool persist(const QJsonObject &candidate);
    void transmit(const QString &peer, QJsonObject envelope);
    void broadcast(const QJsonObject &group, const QJsonObject &envelope);
    QJsonObject envelope(const QString &kind, const QString &id) const;
    bool knownRoster(const QStringList &members) const;
    bool validStored(const QJsonObject &data) const;
    void load();
    QString m_self, m_path, m_error;
    QStringList m_peers;
    QMap<QString, QString> m_names;
    QJsonObject m_groups;
    QTimer m_retry;
    bool m_storageHealthy = true;
};

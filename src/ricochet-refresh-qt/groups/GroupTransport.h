#pragma once
#include <QObject>
#include <QString>
#include <QSet>
class GroupManager;

class GroupTransport : public QObject
{
    Q_OBJECT
public:
    GroupTransport(GroupManager *manager, QObject *parent = nullptr);
    static bool receive(const QString &peer, const QString &wire);
    static bool acknowledge(const QString &peer, quint64 id);
private:
    void refreshPeers();
    GroupManager *m_manager;
    static GroupTransport *s_instance;
    QSet<QString> m_transportMessages;
};

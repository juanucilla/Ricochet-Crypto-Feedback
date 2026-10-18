#include "GroupTransport.h"
#include "GroupManager.h"
#include "shims/UserIdentity.h"
#include "shims/ContactsManager.h"
#include "shims/ContactUser.h"
#include <tego/tego.hpp>

GroupTransport *GroupTransport::s_instance = nullptr;

GroupTransport::GroupTransport(GroupManager *manager, QObject *parent)
    : QObject(parent), m_manager(manager)
{
    s_instance = this;
    connect(this, &QObject::destroyed, [] { s_instance = nullptr; });
    auto contacts = shims::UserIdentity::userIdentity->getContacts();
    connect(shims::UserIdentity::userIdentity, &shims::UserIdentity::hostOnionServiceStateChanged,
        this, [this] { refreshPeers(); });
    connect(contacts, &shims::ContactsManager::contactAdded, this, [this] { refreshPeers(); });
    connect(contacts, &shims::ContactsManager::contactStatusChanged, this, [this] { refreshPeers(); });
    connect(manager, &GroupManager::outbound, this, [this](const QString &peer, const QString &wire) {
        auto identity = shims::UserIdentity::userIdentity;
        auto contact = identity->getContacts()->getShimContactByContactId(peer);
        // The local persistent outbox retries when peers return online.
        if (!contact || contact->getStatus() != shims::ContactUser::Online || m_transportMessages.size() >= 4096) return;
        try {
            auto id = contact->toTegoUserId();
            const auto utf8 = wire.toUtf8();
            tego_message_id ignored = 0;
            tego_context_send_message(identity->getContext(), id.get(), utf8.constData(),
                static_cast<size_t>(utf8.size()), &ignored, tego::throw_on_error());
            m_transportMessages.insert(peer + QString::number(ignored));
        } catch (const std::exception &) {
            // No content/identity logging. GroupManager retains unacknowledged messages.
        }
    });
    refreshPeers();
}

void GroupTransport::refreshPeers()
{
    auto identity = shims::UserIdentity::userIdentity;
    if (identity->getHostOnionServiceState() != shims::UserIdentity::HostOnionServiceState_None)
        m_manager->setIdentity(identity->contactID());
    QStringList peers;
    QMap<QString, QString> names;
    for (auto contact : shims::UserIdentity::userIdentity->getContacts()->contacts()) {
        const auto status = contact->getStatus();
        if (status != shims::ContactUser::Online) {
            // Native chat message IDs are connection-scoped; retire stale receipts.
            for (auto it = m_transportMessages.begin(); it != m_transportMessages.end();) {
                if (it->startsWith(contact->getContactID())) it = m_transportMessages.erase(it);
                else ++it;
            }
        }
        if (status == shims::ContactUser::Online || status == shims::ContactUser::Offline
            || status == shims::ContactUser::RequestAccepted) {
            peers.append(contact->getContactID());
            names.insert(contact->getContactID(), contact->getNickname());
        }
    }
    m_manager->setKnownPeers(peers);
    m_manager->setContactNames(names);
}

bool GroupTransport::receive(const QString &peer, const QString &wire)
{
    if (!s_instance) return wire.startsWith(QStringLiteral("group-ricochet/v1 "));
    s_instance->refreshPeers();
    return s_instance->m_manager->receive(peer, wire);
}

bool GroupTransport::acknowledge(const QString &peer, quint64 id)
{
    return s_instance && s_instance->m_transportMessages.remove(peer + QString::number(id)) > 0;
}

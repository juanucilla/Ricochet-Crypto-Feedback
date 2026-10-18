#include "GroupManager.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <QSet>
#include <algorithm>

namespace {
const QString prefix = QStringLiteral("group-ricochet/v1 ");
constexpr int maxGroups = 32, maxMessages = 250, maxWire = 2000;
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QStringList strings(const QJsonValue &v) {
    QStringList result;
    for (const auto &item : v.toArray()) result.append(item.toString());
    return result;
}
bool validId(const QString &id) {
    static const QRegularExpression re(QStringLiteral("^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$"));
    return re.match(id).hasMatch();
}
bool validRoster(const QStringList &members) {
    static const QRegularExpression re(QStringLiteral("^ricochet:[a-z2-7]{56}$"));
    QSet<QString> unique;
    for (const auto &peer : members) {
        if (!re.match(peer).hasMatch() || unique.contains(peer)) return false;
        unique.insert(peer);
    }
    return members.size() >= 2 && members.size() <= 12;
}
bool sameRoster(const QStringList &a, const QStringList &b) {
    return QSet<QString>(a.begin(), a.end()) == QSet<QString>(b.begin(), b.end());
}
qint64 now() { return QDateTime::currentSecsSinceEpoch(); }
}

GroupManager::GroupManager(QString self, QString storagePath, QObject *parent)
    : QObject(parent), m_self(std::move(self)), m_path(std::move(storagePath))
{
    if (!m_self.isEmpty()) load();
    connect(&m_retry, &QTimer::timeout, this, &GroupManager::retry);
    m_retry.start(30000);
}

void GroupManager::setIdentity(const QString &identity)
{
    static const QRegularExpression re(QStringLiteral("^ricochet:[a-z2-7]{56}$"));
    if (!re.match(identity).hasMatch() || identity == m_self) return;
    if (!m_self.isEmpty()) { fail(tr("Identity changed. Restart with its own group profile.")); return; }
    m_self = identity;
    load();
    emit changed();
}

void GroupManager::load()
{
    QFile file(m_path);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024) {
            m_storageHealthy = false;
        } else {
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
            const auto root = doc.object();
            m_storageHealthy = error.error == QJsonParseError::NoError && doc.isObject()
                && root.value("version").toInt() == 1 && root.value("self").toString() == m_self
                && root.value("groups").isObject() && validStored(root.value("groups").toObject());
            if (m_storageHealthy) m_groups = root.value("groups").toObject();
        }
        if (!m_storageHealthy) fail(tr("Group storage cannot be read safely. Original file preserved."));
    }
}

bool GroupManager::validStored(const QJsonObject &data) const
{
    if (data.size() > maxGroups) return false;
    for (auto it = data.begin(); it != data.end(); ++it) {
        const auto g = it.value().toObject();
        const auto members = strings(g.value("members"));
        if (!validId(it.key()) || g.value("id").toString() != it.key()
            || !validRoster(members) || !members.contains(m_self)
            || !members.contains(g.value("owner").toString())
            || g.value("messages").toArray().size() > maxMessages) return false;
    }
    return true;
}

bool GroupManager::fail(const QString &message)
{
    m_error = message;
    emit errorChanged();
    return false;
}

bool GroupManager::persist(const QJsonObject &candidate)
{
    if (!m_storageHealthy) return fail(tr("Group storage is unavailable."));
    QJsonObject root{{"version", 1}, {"self", m_self}, {"groups", candidate}};
    const auto bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (bytes.size() > 16 * 1024 * 1024) return fail(tr("Group storage limit reached."));
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) return fail(tr("Cannot create group storage."));
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return fail(tr("Cannot save group state. No delivery acknowledgement was sent."));
    QFile::setPermissions(m_path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    m_groups = candidate;
    m_error.clear();
    emit errorChanged();
    emit changed();
    return true;
}

bool GroupManager::knownRoster(const QStringList &members) const
{
    if (!validRoster(members) || !members.contains(m_self)) return false;
    for (const auto &peer : members)
        if (peer != m_self && !m_peers.contains(peer)) return false;
    return true;
}

QJsonObject GroupManager::envelope(const QString &kind, const QString &id) const
{
    return {{"v", 1}, {"kind", kind}, {"group", id}};
}

void GroupManager::transmit(const QString &peer, QJsonObject e)
{
    if (!m_peers.contains(peer)) return;
    const auto wire = prefix + QString::fromUtf8(QJsonDocument(e).toJson(QJsonDocument::Compact));
    if (wire.size() > maxWire) { fail(tr("Group packet exceeds the transport limit.")); return; }
    emit outbound(peer, wire);
}

void GroupManager::broadcast(const QJsonObject &g, const QJsonObject &e)
{
    for (const auto &peer : strings(g.value("members")))
        if (peer != m_self) transmit(peer, e);
}

QString GroupManager::create(const QString &title, const QStringList &peers)
{
    QStringList members = peers;
    members.removeAll(m_self);
    members.prepend(m_self);
    if (!knownRoster(members) || title.trimmed().isEmpty() || title.size() > 80 || m_groups.size() >= maxGroups) {
        fail(tr("Choose a title and 1–11 distinct accepted contacts (maximum 32 groups)."));
        return {};
    }
    const auto id = uuid();
    QJsonObject g{{"id", id}, {"title", title.trimmed()}, {"owner", m_self},
        {"members", QJsonArray::fromStringList(members)}, {"state", "waiting"},
        {"accepted", QJsonArray{m_self}}, {"messages", QJsonArray()},
        {"created", double(now())}, {"unread", 0}, {"muted", false}};
    auto candidate = m_groups;
    candidate.insert(id, g);
    if (!persist(candidate)) return {};
    retry();
    return id;
}

bool GroupManager::accept(const QString &id)
{
    auto g = m_groups.value(id).toObject();
    if (g.value("state").toString() != "invited" || g.value("expires").toDouble() < now() || !knownRoster(strings(g.value("members"))))
        return fail(tr("All group members must first be accepted contacts."));
    g.insert("state", "waiting");
    g.insert("accepted", QJsonArray{m_self});
    auto candidate = m_groups;
    candidate.insert(id, g);
    if (!persist(candidate)) return false;
    auto e = envelope("accept", id);
    transmit(g.value("owner").toString(), e);
    return true;
}

bool GroupManager::leave(const QString &id)
{
    auto g = m_groups.value(id).toObject();
    if (g.isEmpty() || g.value("state").toString() == "closed") return false;
    g.insert("state", "closed");
    auto candidate = m_groups;
    candidate.insert(id, g);
    if (!persist(candidate)) return false;
    // Fixed-roster MVP: leaving closes the group, rather than silently changing keys/roster.
    if (g.value("owner").toString() == m_self) broadcast(g, envelope("close", id));
    else transmit(g.value("owner").toString(), envelope("leave", id));
    return true;
}

bool GroupManager::send(const QString &id, const QString &text, const QString &replyTo)
{
    auto g = m_groups.value(id).toObject();
    auto log = g.value("messages").toArray();
    if (g.value("state").toString() != "active" || !knownRoster(strings(g.value("members"))))
        return fail(tr("The group is not ready. Check invitations and contacts."));
    if (text.trimmed().isEmpty() || text.size() > 500 || log.size() >= maxMessages)
        return fail(tr("Use 1–500 characters. This preview stores up to 250 messages per group."));
    if (!replyTo.isEmpty() && !validId(replyTo)) return false;
    const auto mid = uuid();
    auto check = envelope("text", id);
    check.insert("id", mid); check.insert("text", text); check.insert("replyTo", replyTo);
    if ((prefix + QString::fromUtf8(QJsonDocument(check).toJson(QJsonDocument::Compact))).size() > maxWire)
        return fail(tr("Text contains too many escaped characters for the transport. Shorten it."));
    QJsonObject message{{"id", mid}, {"author", m_self}, {"text", text},
        {"replyTo", replyTo}, {"time", double(now())}, {"delivered", QJsonArray()},
        {"recipients", g.value("members")}};
    log.append(message);
    g.insert("messages", log);
    g.insert("draft", "");
    auto candidate = m_groups;
    candidate.insert(id, g);
    if (!persist(candidate)) return false;
    retry();
    return true;
}

bool GroupManager::receive(const QString &peer, const QString &wire)
{
    if (!wire.startsWith(prefix)) return false;
    if (wire.size() > maxWire || !m_peers.contains(peer)) return true;
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(wire.mid(prefix.size()).toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return true;
    const auto e = doc.object();
    const auto id = e.value("group").toString(), kind = e.value("kind").toString();
    if (e.value("v").toInt() != 1 || !validId(id)) return true;
    auto g = m_groups.value(id).toObject();
    auto candidate = m_groups;
    if (kind == "invite") {
        if (!g.isEmpty()) {
            if (g.value("owner").toString() == peer && g.value("state").toString() == "waiting")
                transmit(peer, envelope("accept", id));
            else if (g.value("owner").toString() == peer && g.value("state").toString() == "closed")
                transmit(peer, envelope("leave", id));
            return true;
        }
        const auto members = strings(e.value("members"));
        const auto title = e.value("title").toString();
        if (m_groups.size() >= maxGroups || !validRoster(members) || !members.contains(peer)
            || !members.contains(m_self) || title.isEmpty() || title.size() > 80
            || e.value("expires").toDouble() < now() || e.value("expires").toDouble() > now() + 7 * 86400)
            return true;
        g = {{"id", id}, {"title", title}, {"owner", peer}, {"members", e.value("members")}, {"expires", e.value("expires")},
            {"state", "invited"}, {"messages", QJsonArray()}, {"muted", false}, {"unread", 0}};
        candidate.insert(id, g);
        if (persist(candidate)) emit notification(tr("Group invitation"), title);
        return true;
    }
    if (g.isEmpty() || !strings(g.value("members")).contains(peer)) return true;
    const auto owner = g.value("owner").toString(), state = g.value("state").toString();
    if (kind == "close" && peer == owner) {
        if (state == "closed") return true;
        g.insert("state", "closed");
    } else if (kind == "leave" && owner == m_self) {
        g.insert("state", "closed");
        candidate.insert(id, g);
        if (persist(candidate)) broadcast(g, envelope("close", id));
        return true;
    } else if (state == "closed") {
        if (owner == m_self) transmit(peer, envelope("close", id));
        return true;
    } else if (kind == "accept" && owner == m_self && state == "waiting") {
        auto accepted = strings(g.value("accepted"));
        if (!accepted.contains(peer)) accepted.append(peer);
        g.insert("accepted", QJsonArray::fromStringList(accepted));
        if (sameRoster(accepted, strings(g.value("members")))) g.insert("state", "active");
    } else if (kind == "ready" && peer == owner && (state == "waiting" || state == "active")
               && sameRoster(strings(e.value("members")), strings(g.value("members")))) {
        if (state == "active") return true;
        g.insert("state", "active");
    } else if (kind == "text" && state == "active") {
        const auto mid = e.value("id").toString(), body = e.value("text").toString();
        const auto reply = e.value("replyTo").toString();
        if (!validId(mid) || body.trimmed().isEmpty() || body.size() > 500
            || (!reply.isEmpty() && !validId(reply))) return true;
        auto log = g.value("messages").toArray();
        for (const auto &item : log) {
            const auto stored = item.toObject();
            if (stored.value("id").toString() == mid && stored.value("author").toString() == peer) {
                if (stored.value("text").toString() == body && stored.value("replyTo").toString() == reply) {
                    auto ack = envelope("ack", id); ack.insert("id", mid); transmit(peer, ack);
                }
                return true;
            }
        }
        if (log.size() >= maxMessages) return true;
        log.append(QJsonObject{{"id", mid}, {"author", peer}, {"text", body},
            {"replyTo", reply}, {"time", double(now())}});
        g.insert("messages", log);
        g.insert("unread", g.value("unread").toInt() + 1);
        candidate.insert(id, g);
        if (persist(candidate)) {
            auto ack = envelope("ack", id); ack.insert("id", mid); transmit(peer, ack);
            if (!g.value("muted").toBool()) emit notification(g.value("title").toString(), tr("New group message"));
        }
        return true;
    } else if (kind == "ack" && state == "active") {
        auto log = g.value("messages").toArray();
        bool found = false;
        for (int i = 0; i < log.size(); ++i) {
            auto message = log.at(i).toObject();
            if (message.value("id") == e.value("id") && message.value("author").toString() == m_self) {
                auto delivered = strings(message.value("delivered"));
                if (delivered.contains(peer)) return true;
                delivered.append(peer);
                found = true;
                message.insert("delivered", QJsonArray::fromStringList(delivered));
                log.replace(i, message);
                break;
            }
        }
        if (!found) return true;
        g.insert("messages", log);
    } else return true;
    candidate.insert(id, g);
    if (persist(candidate) && g.value("state").toString() == "active" && owner == m_self) {
        auto ready = envelope("ready", id); ready.insert("members", g.value("members"));
        broadcast(g, ready);
    }
    return true;
}

void GroupManager::retry()
{
    // Copy: emitting outbound may synchronously deliver callbacks in tests/adapters.
    const auto snapshot = m_groups;
    for (auto it = snapshot.begin(); it != snapshot.end(); ++it) {
        const auto g = it.value().toObject();
        const auto state = g.value("state").toString(), owner = g.value("owner").toString();
        if (state == "closed") {
            if (owner == m_self) broadcast(g, envelope("close", it.key()));
            else transmit(owner, envelope("leave", it.key()));
        } else if (state == "waiting") {
            if (owner == m_self) {
                if (now() - g.value("created").toDouble() > 7 * 86400) continue;
                auto e = envelope("invite", it.key());
                e.insert("title", g.value("title")); e.insert("members", g.value("members"));
                e.insert("expires", g.value("created").toDouble() + 7 * 86400);
                for (const auto &peer : strings(g.value("members")))
                    if (peer != m_self && !strings(g.value("accepted")).contains(peer)) transmit(peer, e);
            } else transmit(owner, envelope("accept", it.key()));
        } else if (state == "active") {
            if (owner == m_self) {
                auto e = envelope("ready", it.key()); e.insert("members", g.value("members"));
                broadcast(g, e);
            }
            int budget = 20;
            for (const auto &item : g.value("messages").toArray()) {
                const auto msg = item.toObject();
                if (msg.value("author").toString() != m_self || now() - msg.value("time").toDouble() > 7 * 86400) continue;
                auto e = envelope("text", it.key());
                e.insert("id", msg.value("id")); e.insert("text", msg.value("text")); e.insert("replyTo", msg.value("replyTo"));
                for (const auto &peer : strings(msg.value("recipients"))) {
                    if (peer == m_self || strings(msg.value("delivered")).contains(peer)) continue;
                    if (budget-- <= 0) break;
                    transmit(peer, e);
                }
                if (budget <= 0) break;
            }
        }
    }
}

QVariantList GroupManager::groups() const
{
    QVariantList result;
    for (auto it = m_groups.begin(); it != m_groups.end(); ++it) {
        auto g = it.value().toObject(); g.remove("messages"); result.append(g.toVariantMap());
    }
    return result;
}

void GroupManager::setContactNames(const QMap<QString, QString> &names)
{
    if (m_names == names) return;
    m_names = names;
    emit changed();
}

QVariantList GroupManager::contacts() const
{
    QVariantList result;
    for (const auto &id : m_peers)
        result.append(QVariantMap{{"id", id}, {"name", displayName(id)}});
    return result;
}

QString GroupManager::displayName(const QString &peer) const
{
    if (peer == m_self) return tr("You");
    const auto name = m_names.value(peer).trimmed();
    return name.isEmpty() ? peer.mid(9, 10) + QStringLiteral("…") : name;
}

QVariantMap GroupManager::details(const QString &id) const
{
    auto g = m_groups.value(id).toObject(); g.remove("messages"); return g.toVariantMap();
}

QVariantList GroupManager::messages(const QString &id, const QString &query) const
{
    QVariantList result;
    const auto log = m_groups.value(id).toObject().value("messages").toArray();
    for (const auto &item : log) {
        auto msg = item.toObject();
        if (!query.isEmpty() && !msg.value("text").toString().contains(query, Qt::CaseInsensitive)) continue;
        msg.insert("mine", msg.value("author").toString() == m_self);
        msg.insert("deliveryCount", msg.value("delivered").toArray().size());
        msg.insert("recipientCount", std::max(0, msg.value("recipients").toArray().size() - 1));
        msg.insert("expired", now() - msg.value("time").toDouble() > 7 * 86400);
        if (!msg.value("replyTo").toString().isEmpty()) {
            msg.insert("replyPreview", tr("Original message unavailable"));
            for (const auto &original : log) {
                const auto quoted = original.toObject();
                if (quoted.value("id") == msg.value("replyTo")) {
                    msg.insert("replyPreview", displayName(quoted.value("author").toString())
                        + QStringLiteral(": ") + quoted.value("text").toString().left(120));
                    break;
                }
            }
        }
        result.append(msg.toVariantMap());
    }
    return result;
}

void GroupManager::markRead(const QString &id)
{
    auto g = m_groups.value(id).toObject();
    if (g.isEmpty() || g.value("unread").toInt() == 0) return;
    g.insert("unread", 0); auto candidate = m_groups; candidate.insert(id, g); persist(candidate);
}

void GroupManager::setMuted(const QString &id, bool muted)
{
    auto g = m_groups.value(id).toObject(); if (g.isEmpty()) return;
    g.insert("muted", muted); auto candidate = m_groups; candidate.insert(id, g); persist(candidate);
}

void GroupManager::setDraft(const QString &id, const QString &draft)
{
    auto g = m_groups.value(id).toObject(); if (g.isEmpty() || draft.size() > 500) return;
    if (g.value("draft").toString() == draft) return;
    g.insert("draft", draft); auto candidate = m_groups; candidate.insert(id, g); persist(candidate);
}

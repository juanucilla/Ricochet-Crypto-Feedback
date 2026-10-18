#include "GroupManager.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <deque>

namespace {
QString peer(QChar c) { return QStringLiteral("ricochet:") + QString(56, c); }
struct Packet { QString sender, recipient, wire; };
class Mesh {
public:
    QTemporaryDir directory;
    GroupManager a{peer('a'), directory.filePath("a.json")};
    GroupManager b{peer('b'), directory.filePath("b.json")};
    GroupManager c{peer('c'), directory.filePath("c.json")};
    std::deque<Packet> queue;
    QList<Packet> sent;
    QSet<QString> offline;
    Mesh() {
        for (auto node : {&a, &b, &c}) {
            node->setKnownPeers({peer('a'), peer('b'), peer('c')});
            QObject::connect(node, &GroupManager::outbound, node, [this, node](QString to, QString wire) {
                queue.push_back({node->self(), to, wire});
            });
        }
    }
    void pump() {
        int budget = 10000;
        while (!queue.empty() && --budget > 0) {
            auto packet = queue.front(); queue.pop_front(); sent.append(packet);
            if (offline.contains(packet.recipient)) continue;
            for (auto node : {&a, &b, &c})
                if (node->self() == packet.recipient) node->receive(packet.sender, packet.wire);
        }
        QVERIFY2(budget > 0, "Unexpected protocol loop");
    }
    QString activeGroup() {
        auto id = a.create("Team", {b.self(), c.self()}); pump();
        b.accept(id); c.accept(id); pump();
        return id;
    }
};
}

class GroupsTest : public QObject {
    Q_OBJECT
private slots:
    void identityBecomesAvailableAfterBootstrap() {
        QTemporaryDir dir;
        GroupManager first(peer('a'), dir.filePath("groups.json"));
        first.setKnownPeers({peer('b')});
        const auto id = first.create("Saved before restart", {peer('b')});
        QVERIFY(!id.isEmpty());
        GroupManager bootstrapping({}, dir.filePath("groups.json"));
        QVERIFY(bootstrapping.groups().isEmpty());
        bootstrapping.setIdentity(peer('a'));
        QCOMPARE(bootstrapping.groups().size(), 1);
        bootstrapping.setIdentity(peer('b'));
        QCOMPARE(bootstrapping.self(), peer('a'));
        QVERIFY(!bootstrapping.error().isEmpty());
    }
    void ownerCannotActivateAnUnacceptedInvitation() {
        Mesh m;
        auto id = m.a.create("Team", {m.b.self(), m.c.self()}); m.pump();
        const auto members = QJsonArray::fromVariantList(m.a.details(id).value("members").toList());
        QJsonObject ready{{"v", 1}, {"kind", "ready"}, {"group", id}, {"members", members}};
        const auto wire = QString("group-ricochet/v1 ") + QString::fromUtf8(QJsonDocument(ready).toJson(QJsonDocument::Compact));
        m.b.receive(m.a.self(), wire);
        QCOMPARE(m.b.details(id).value("state").toString(), QString("invited"));
    }
    void consentAndThreePeerDelivery() {
        Mesh m;
        auto id = m.a.create("Team", {m.b.self(), m.c.self()}); m.pump();
        QCOMPARE(m.b.details(id).value("state").toString(), QString("invited"));
        QVERIFY(!m.b.send(id, "without consent"));
        QVERIFY(m.b.accept(id)); m.pump();
        QCOMPARE(m.a.details(id).value("state").toString(), QString("waiting"));
        QVERIFY(m.c.accept(id)); m.pump();
        QCOMPARE(m.a.details(id).value("state").toString(), QString("active"));
        QVERIFY(m.b.send(id, "Hello everyone")); m.pump();
        QCOMPARE(m.a.messages(id).size(), 1);
        QCOMPARE(m.c.messages(id).size(), 1);
        QCOMPARE(m.b.messages(id).first().toMap().value("deliveryCount").toInt(), 2);
    }
    void rejectsOutsidersAndOversize() {
        Mesh m; auto id = m.activeGroup();
        QVERIFY(!m.a.send(id, QString(501, 'a')));
        QVERIFY(!m.a.send(id, QString(500, QChar(1))));
        QVERIFY(m.a.send(id, "Hello")); m.pump();
        Packet text;
        for (const auto &p : m.sent) if (p.wire.contains("\"kind\":\"text\"")) { text = p; break; }
        QCOMPARE(m.b.messages(id).size(), 1);
        QVERIFY(m.b.receive(peer('d'), text.wire));
        QCOMPARE(m.b.messages(id).size(), 1);
        QVERIFY(!m.b.receive(m.a.self(), "ordinary direct chat"));
        QVERIFY(m.b.receive(m.a.self(), "group-ricochet/v1 {bad json"));
        QCOMPARE(m.b.messages(id).size(), 1);
    }
    void retryDeduplicatesAndPersists() {
        Mesh m; auto id = m.activeGroup();
        m.offline.insert(m.c.self());
        QVERIFY(m.a.send(id, "Offline test")); m.pump();
        QCOMPARE(m.a.messages(id).first().toMap().value("deliveryCount").toInt(), 1);
        QCOMPARE(m.c.messages(id).size(), 0);
        m.offline.clear(); m.a.retry(); m.pump();
        QCOMPARE(m.c.messages(id).size(), 1);
        QCOMPARE(m.a.messages(id).first().toMap().value("deliveryCount").toInt(), 2);
        for (const auto &p : m.sent) {
            if (p.recipient == m.b.self() && p.wire.contains("\"kind\":\"text\"")) {
                m.b.receive(m.a.self(), p.wire); break;
            }
        }
        QCOMPARE(m.b.messages(id).size(), 1);
        GroupManager restored(m.a.self(), m.directory.filePath("a.json"));
        QCOMPARE(restored.messages(id).size(), 1);
        QCOMPARE(restored.messages(id).first().toMap().value("deliveryCount").toInt(), 2);
    }
    void draftsMuteSearchAndReplies() {
        Mesh m; auto id = m.activeGroup();
        m.a.setDraft(id, "unfinished");
        QCOMPARE(m.a.details(id).value("draft").toString(), QString("unfinished"));
        QSignalSpy notification(&m.b, &GroupManager::notification);
        m.b.setMuted(id, true);
        QVERIFY(m.a.send(id, "Question about TOR")); m.pump();
        QCOMPARE(notification.count(), 0);
        QCOMPARE(m.b.details(id).value("unread").toInt(), 1);
        m.b.markRead(id);
        QCOMPARE(m.b.details(id).value("unread").toInt(), 0);
        QCOMPARE(m.b.messages(id, "tor").size(), 1);
        auto mid = m.b.messages(id).first().toMap().value("id").toString();
        QVERIFY(m.b.send(id, "Answer", mid)); m.pump();
        QCOMPARE(m.c.messages(id).last().toMap().value("replyTo").toString(), mid);
    }
    void leaveClosesAndBlocksFurtherMessages() {
        Mesh m; auto id = m.activeGroup();
        QVERIFY(m.c.leave(id)); m.pump();
        QCOMPARE(m.a.details(id).value("state").toString(), QString("closed"));
        QCOMPARE(m.b.details(id).value("state").toString(), QString("closed"));
        QVERIFY(!m.a.send(id, "not allowed"));
    }
    void unknownContactCannotBeAccepted() {
        Mesh m;
        auto id = m.a.create("Team", {m.b.self(), m.c.self()}); m.pump();
        m.b.setKnownPeers({m.a.self()});
        QVERIFY(!m.b.accept(id));
        QVERIFY(m.a.create("Invalid", {peer('d')}).isEmpty());
        QVERIFY(m.a.create("Duplicates", {m.b.self(), m.b.self()}).isEmpty());
    }
    void damagedStorageAndDiskFailureNeverAcknowledge() {
        Mesh m;
        QFile file(m.directory.filePath("damaged.json"));
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write("corrupt"); file.close();
        GroupManager damaged(m.a.self(), file.fileName());
        damaged.setKnownPeers({m.b.self()});
        QVERIFY(damaged.create("no overwrite", {m.b.self()}).isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("corrupt"));
        file.close();
        auto id = m.activeGroup();
        QVERIFY(m.a.send(id, "Must persist before ack"));
        // Replace B's storage destination by a directory to force QSaveFile failure.
        QVERIFY(QFile::remove(m.directory.filePath("b.json")));
        QVERIFY(QDir().mkdir(m.directory.filePath("b.json")));
        m.pump();
        QCOMPARE(m.b.messages(id).size(), 0);
        QCOMPARE(m.a.messages(id).first().toMap().value("deliveryCount").toInt(), 1);
    }
};
QTEST_GUILESS_MAIN(GroupsTest)
#include "group_tests.moc"

#include "GroupManager.h"
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTimer>
#include <QDebug>
#include <QImage>
#include <deque>
#include <cstdio>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir directory;
    GroupManager manager("ricochet:" + QString(56, 'a'), directory.filePath("groups.json"));
    GroupManager other("ricochet:" + QString(56, 'b'), directory.filePath("other.json"));
    manager.setKnownPeers({other.self()}); other.setKnownPeers({manager.self()});
    manager.setContactNames({{other.self(), "Bianca"}});
    struct Packet { GroupManager *to; QString from, wire; };
    std::deque<Packet> queue;
    QObject::connect(&manager, &GroupManager::outbound, [&](QString, QString wire) { queue.push_back({&other, manager.self(), wire}); });
    QObject::connect(&other, &GroupManager::outbound, [&](QString, QString wire) { queue.push_back({&manager, other.self(), wire}); });
    auto pump = [&] { while (!queue.empty()) { auto p = queue.front(); queue.pop_front(); p.to->receive(p.from, p.wire); } };
    const auto id = manager.create("Team · Private groups", {other.self()}); pump();
    other.accept(id); pump();
    manager.send(id, "Welcome! Messages go directly to each group member over Tor."); pump();
    const auto mid = other.messages(id).first().toMap().value("id").toString();
    other.send(id, "The quoted replies make this easier to follow.", mid); pump();
    QQmlApplicationEngine engine;
    bool warning = false;
    QObject::connect(&engine, &QQmlEngine::warnings, [&warning](const QList<QQmlError> &errors) {
        for (const auto &e : errors) std::fprintf(stderr, "%s\n", qPrintable(e.toString()));
        warning = true;
    });
    engine.rootContext()->setContextProperty("groupManager", &manager);
    engine.load(QUrl::fromLocalFile(QStringLiteral(GROUP_QML_PATH)));
    if (engine.rootObjects().isEmpty()) return 1;
    auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window) return 2;
    if (!QMetaObject::invokeMethod(window, "selectGroup", Q_ARG(QVariant, QVariant(id)))) return 4;
    window->show();
    QTimer::singleShot(600, &app, [&] {
        if (qEnvironmentVariableIsSet("GROUP_SCREENSHOT")) {
            const auto shot = window->grabWindow();
            if (!shot.isNull()) shot.save(qEnvironmentVariable("GROUP_SCREENSHOT"));
        }
        app.quit();
    });
    app.exec();
    return warning ? 3 : 0;
}

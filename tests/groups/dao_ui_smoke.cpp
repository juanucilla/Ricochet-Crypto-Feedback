#include "DaoClient.h"
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QImage>
#include <QQmlError>
#include <QTemporaryDir>
#include <cstdio>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir temp;
    DaoClient client(nullptr, temp.filePath("pending.json"));
    bool submitted = false, booked = false;
    if (qEnvironmentVariableIsSet("DAO_TEST_TRANSFER")) {
        QObject::connect(&client, &DaoClient::changed, [&] {
            if (client.busy() || client.state().isEmpty()) return;
            const auto assets = client.state().value("assets").toList();
            if (!assets.isEmpty() && assets.first().toMap().value("balance").toInt() == 75) booked = true;
            if (!submitted) {
                submitted = true;
                client.submit({{"kind", "transfer"}, {"asset", "DAO"}, {"to", "bob"}, {"amount", 25}});
            }
        });
    }
    QQmlApplicationEngine engine;
    bool warning = false;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError> &errors) {
        warning = true;
        for (const auto &e : errors) std::fprintf(stderr, "%s\n", qPrintable(e.toString()));
    });
    engine.rootContext()->setContextProperty("daoClient", &client);
    engine.load(QUrl::fromLocalFile(QStringLiteral(DAO_QML_PATH)));
    if (engine.rootObjects().isEmpty()) return 1;
    auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window) return 2;
    window->show();
    if (qEnvironmentVariableIsSet("GROUP_DAO_TOKEN")) client.refresh();
    QTimer::singleShot(qEnvironmentVariableIsSet("DAO_TEST_TRANSFER") ? 4000 : 900, &app, [&] {
        if (qEnvironmentVariableIsSet("DAO_SCREENSHOT")) window->grabWindow().save(qEnvironmentVariable("DAO_SCREENSHOT"));
        app.quit();
    });
    app.exec();
    return warning ? 3 : (qEnvironmentVariableIsSet("DAO_TEST_TRANSFER") && !booked ? 4 : 0);
}

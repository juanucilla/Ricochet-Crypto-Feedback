#pragma once
#include <QObject>
#include <QVariantMap>
#include <QNetworkAccessManager>

// Explicitly local development service; no direct internet fallback from Tor chat.
class DaoClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
    explicit DaoClient(QObject *parent = nullptr, const QString &pendingPath = QString());
    QVariantMap state() const { return state_; }
    QString message() const { return message_; }
    bool busy() const { return busy_; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void submit(QVariantMap command);
    Q_INVOKABLE void retry();
private:
    void request(const QString &path, const QByteArray &body = {});
    QNetworkAccessManager network_;
    QByteArray token_, pending_;
    QString pendingPath_;
    QVariantMap state_;
    QString message_ = QStringLiteral("Test locale: configurare GROUP_DAO_TOKEN. Nessun collegamento Tor al wallet.");
    bool busy_ = false;
signals:
    void changed();
};

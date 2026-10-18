#include "DaoClient.h"
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QTimer>
#include <QFile>
#include <QSaveFile>
#include <QCryptographicHash>

DaoClient::DaoClient(QObject *parent, const QString &pendingPath) : QObject(parent), token_(qgetenv("GROUP_DAO_TOKEN")), pendingPath_(pendingPath) {
    network_.setProxy(QNetworkProxy::NoProxy);
    QFile file(pendingPath_);
    if (!pendingPath_.isEmpty() && file.exists()) {
        if (file.open(QIODevice::ReadOnly)) pending_ = file.readAll();
        if (pending_.isEmpty()) pending_ = "unreadable"; // Fail closed: never overwrite an uncertain command.
        message_ = QStringLiteral("Operazione sospesa: aggiorna e usa Riprova stessa operazione.");
    }
}

void DaoClient::refresh() {
    if (!busy_) request(QStringLiteral("/v1/state"));
}

void DaoClient::submit(QVariantMap command) {
    if (busy_ || !pending_.isEmpty()) return;
    command.insert(QStringLiteral("request_id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    const auto body = QJsonDocument::fromVariant(command).toJson(QJsonDocument::Compact);
    QJsonObject record;
    record.insert(QStringLiteral("credential"), QString::fromLatin1(QCryptographicHash::hash(token_, QCryptographicHash::Sha256).toHex()));
    record.insert(QStringLiteral("command"), QJsonDocument::fromJson(body).object());
    const auto bytes = QJsonDocument(record).toJson(QJsonDocument::Compact);
    QSaveFile file(pendingPath_);
    if (pendingPath_.isEmpty() || !file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        message_ = QStringLiteral("Impossibile salvare l'operazione: invio annullato."); emit changed(); return;
    }
    pending_ = bytes;
    request(QStringLiteral("/v1/commands"), body);
}

void DaoClient::retry() {
    if (busy_ || pending_.isEmpty()) return;
    const auto record = QJsonDocument::fromJson(pending_).object();
    const auto fingerprint = QString::fromLatin1(QCryptographicHash::hash(token_, QCryptographicHash::Sha256).toHex());
    if (record.value(QStringLiteral("credential")).toString() != fingerprint || !record.value(QStringLiteral("command")).isObject()) {
        message_ = QStringLiteral("Operazione sospesa non leggibile o di altra credenziale. Ripristina la credenziale originale; non reinviare con un nuovo ID."); emit changed(); return;
    }
    request(QStringLiteral("/v1/commands"), QJsonDocument(record.value(QStringLiteral("command")).toObject()).toJson(QJsonDocument::Compact));
}

void DaoClient::request(const QString &path, const QByteArray &body) {
    if (token_.size() < 32) {
        message_ = QStringLiteral("Credenziale locale assente: GROUP_DAO_TOKEN (almeno 32 caratteri).");
        emit changed();
        return;
    }
    QNetworkRequest req(QUrl(QStringLiteral("http://127.0.0.1:17842") + path));
    req.setRawHeader("Authorization", "Bearer " + token_);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    busy_ = true;
    emit changed();
    auto reply = body.isEmpty() ? network_.get(req) : network_.post(req, body);
    QTimer::singleShot(10000, reply, [reply] { if (reply->isRunning()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, body] {
        busy_ = false;
        const auto data = QJsonDocument::fromJson(reply->readAll()).object();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 200 && reply->error() == QNetworkReply::NoError) {
            if (body.isEmpty()) state_ = data.toVariantMap();
            else if (QFile::remove(pendingPath_)) pending_.clear();
            message_ = body.isEmpty() ? QStringLiteral("Crediti di prova locali aggiornati.") : QStringLiteral("Operazione registrata una sola volta.");
            if (!body.isEmpty()) QTimer::singleShot(0, this, &DaoClient::refresh);
        } else {
            // A later authentication failure does not prove a previous send was unbooked.
            if (!body.isEmpty() && status == 400 && !data.value(QStringLiteral("error")).toString().startsWith(QStringLiteral("Idempotency")) && QFile::remove(pendingPath_)) pending_.clear();
            message_ = data.value(QStringLiteral("error")).toString(QStringLiteral("Servizio non disponibile. Se l'esito è incerto, usa Riprova stessa operazione."));
        }
        reply->deleteLater();
        emit changed();
    });
}

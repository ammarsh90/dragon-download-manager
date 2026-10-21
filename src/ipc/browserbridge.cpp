// Dragon Download Manager — Brami Group. Distributed under LGPL v3.
#include "browserbridge.h"
#include <memory>
#include "browserprotocol.h"
#include <Core/JobFile>
#include <Core/Scheduler>
#include <Core/Settings>
#include <Core/ResourceItem>
#include <Core/Session>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QSettings>
#include <QtCore/QTimer>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>

BrowserBridge::BrowserBridge(Scheduler *scheduler, Settings *settings, QObject *parent)
    : QObject(parent), m_server(new QLocalServer(this)), m_scheduler(scheduler), m_settings(settings)
{
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (m_server->hasPendingConnections()) {
            auto *socket = m_server->nextPendingConnection();
            socket->setParent(this);
            socket->setReadBufferSize(DragonBrowser::MaximumMessageSize + 1);
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            auto buffer = std::make_shared<QByteArray>();
            const auto receive = [this, socket, buffer] {
                *buffer += socket->readAll();
                if (buffer->size() > DragonBrowser::MaximumMessageSize) { socket->disconnectFromServer(); return; }
                const auto end = buffer->indexOf('\n');
                if (end < 0) return;
                socket->disconnect(this);
                QJsonParseError error;
                const auto doc = QJsonDocument::fromJson(buffer->left(end), &error);
                const auto result = error.error == QJsonParseError::NoError && doc.isObject()
                    ? acceptDownload(doc.object()) : QJsonObject{{"accepted",false},{"error","Invalid download request"}};
                socket->write(QJsonDocument(result).toJson(QJsonDocument::Compact) + '\n');
                socket->flush();
                socket->disconnectFromServer();
            };
            connect(socket, &QLocalSocket::readyRead, this, receive);
            QTimer::singleShot(20000, socket, [socket] { socket->disconnectFromServer(); });
            if (socket->bytesAvailable()) receive();
        }
    });
    m_server->listen(DragonBrowser::serverName());
}

QJsonObject BrowserBridge::acceptDownload(const QJsonObject &message)
{
    const auto reject = [](const QString &reason) { return QJsonObject{{"accepted", false}, {"error", reason}}; };
    if (message["action"].toString() == "collect") {
        const auto text = message["text"].toString();
        if (text.size() > 65536 || (!text.contains("[LINKS]") && !text.contains("[MEDIA]"))) return reject("Invalid page collection");
        emit collectionReceived(text);
        return {{"accepted", true}};
    }
    const QUrl url(message["url"].toString());
    const auto requestId = message["requestId"].toString();
    if (message["action"].toString() != "download" || !url.isValid() || url.host().isEmpty()
        || (url.scheme() != "http" && url.scheme() != "https") || !url.userInfo().isEmpty()
        || !QRegularExpression("^[a-zA-Z0-9_-]{16,100}$").match(requestId).hasMatch()) return reject("Unsupported download request");
    for (auto *job : m_scheduler->jobs()) if (job->property("browserRequestId").toString() == requestId)
        return {{"accepted", true}, {"requestId", requestId}};
    QString name = message["filename"].toString().replace('\\', '/');
    name = QFileInfo(name).fileName();
    if (name.isEmpty()) name = url.fileName();
    name.replace(QRegularExpression(R"([<>:"/\\|?*\x00-\x1f])"), "_");
    while (name.endsWith('.') || name.endsWith(' ')) name.chop(1);
    if (name.isEmpty() || name == "." || name == "..") name = "download";
    if (name.size() > 200) name = name.left(160) + "." + QFileInfo(name).suffix().left(20);
    if (QRegularExpression(R"(^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$))", QRegularExpression::CaseInsensitiveOption).match(name).hasMatch()) name.prepend('_');
    const QDir folder(QSettings().value("Downloads/Folder", QDir::homePath() + "/Downloads").toString());
    if (!folder.isAbsolute() || !QDir().mkpath(folder.absolutePath()) || !QFileInfo(folder.absolutePath()).isWritable()) return reject("Download folder is not writable");
    const auto reserved = [this](const QString &path) {
        if (QFileInfo::exists(path)) return true;
        for (auto *job : m_scheduler->jobs()) if (job->localFullFileName().compare(path, Qt::CaseInsensitive) == 0) return true;
        return false;
    };
    const QFileInfo original(name);
    const auto extension = original.suffix().isEmpty() ? QString() : "." + original.suffix();
    int counter = 1;
    while (reserved(folder.filePath(name))) name = original.completeBaseName() + QString(" (%1)").arg(counter++) + extension;
    auto *resource = new ResourceItem;
    resource->setUrl(url.toString(QUrl::FullyEncoded));
    resource->setDestination(folder.absolutePath());
    resource->setCustomFileName(name);
    resource->setMask("*name*");
    const QUrl referer(message["referrer"].toString());
    if ((referer.scheme() == "https" || referer.scheme() == "http") && referer.userInfo().isEmpty()) resource->setReferringPage(referer.toString(QUrl::FullyEncoded));
    resource->setDescription(tr("Browser download"));
    auto *job = new JobFile(m_scheduler, resource);
    job->setProperty("browserRequestId", requestId);
    const auto totalBytes = message["totalBytes"].toInteger(0);
    if (totalBytes > 0) job->setBytesTotal(totalBytes);
    job->setBrowserContext(url, message["userAgent"].toString(), message["cookies"].toArray());
    m_scheduler->append({job}, false);
    if (!Session::write(m_scheduler->jobs(), m_settings->database())) {
        m_scheduler->model()->removeRow(m_scheduler->count() - 1);
        return reject("Cannot save the download queue");
    }
    emit downloadReceived(job, message["showDialog"].toBool(true));
    return {{"accepted",true},{"requestId",requestId}};
}

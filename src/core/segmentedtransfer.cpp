/* - ArrowDL - Copyright (C) 2019-present Sebastien Vavassori
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License (LGPL)
 * Version 3, 29 June 2007, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; If not, see <http://www.gnu.org/licenses/>.
 */

#include "segmentedtransfer.h"
#include "networkmanager.h"
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QRegularExpression>
#include <QtCore/QStandardPaths>

namespace {
QByteArray validator(QNetworkReply *reply)
{
    auto value = reply->rawHeader("ETag");
    if (value.startsWith("W/")) value.clear();
    return value.isEmpty() ? reply->rawHeader("Last-Modified") : value;
}
}

SegmentedTransfer::SegmentedTransfer(NetworkManager *network, QObject *parent)
    : QObject(parent), m_network(network)
{
    m_drainTimer.setInterval(100);
    connect(&m_drainTimer, &QTimer::timeout, this, [this] {
        m_readBudget = qint64(m_speedLimit) * 1024 / 10;
        const int count = m_parts.size();
        for (int offset = 0; m_active && offset < count && !m_parts.isEmpty(); ++offset) {
            const int i = (m_drainCursor + offset) % m_parts.size();
            auto reply = m_parts[i].reply;
            if (!reply) continue;
            consume(i, reply);
            if (m_active && i < m_parts.size() && m_parts[i].reply == reply
                && reply->isFinished() && reply->bytesAvailable() == 0) partFinished(i, reply);
        }
        ++m_drainCursor;
    });
    m_checkpoint.setInterval(2000);
    connect(&m_checkpoint, &QTimer::timeout, this, &SegmentedTransfer::checkpoint);
}

SegmentedTransfer::~SegmentedTransfer() { pause(); }

void SegmentedTransfer::start(const QUrl &url, const QString &referer,
                              const QString &destination, int connections)
{
    pause();
    m_parts.clear();
    m_total = -1;
    m_validator.clear();
    m_ranges = false;
    m_url = url;
    m_referer = referer;
    m_connections = qBound(1, connections, 16);
    const auto key = QCryptographicHash::hash(url.toEncoded() + '\n' + destination.toUtf8(),
                                             QCryptographicHash::Sha256).toHex();
    m_directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + "/partials/" + QString::fromLatin1(key);
    if (!QDir().mkpath(m_directory)) { fail(tr("Cannot create the partial download folder.")); return; }
    m_payload.setFileName(payloadPath());
    if (!m_payload.open(QIODevice::ReadWrite)) { fail(m_payload.errorString()); return; }
    m_fallback = false;
    m_active = true;
    m_checkpoint.start();
    m_drainTimer.start();
    probe();
}

void SegmentedTransfer::probe()
{
    m_probe = m_network->get(m_url, m_referer, 0, {}, 0);
    m_probe->setParent(this);
    // Keep a server that ignores Range bounded while converting its reply to a stream.
    m_probe->setReadBufferSize(128 * 1024);
    auto *reply = m_probe.data();
    connect(reply, &QNetworkReply::metaDataChanged, this, [this, reply] {
        if (!m_active || m_probe != reply) return;
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            m_probe.clear();
            reply->disconnect(this);
            startSingle(reply);
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] { probeFinished(reply); });
}

void SegmentedTransfer::probeFinished(QNetworkReply *reply)
{
    if (!m_active || m_probe != reply) return;
    if (reply->error() != QNetworkReply::NoError) { fail(reply->errorString()); return; }
    const auto match = QRegularExpression("^bytes 0-0/([0-9]+)$")
        .match(QString::fromLatin1(reply->rawHeader("Content-Range")));
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 206
        || !match.hasMatch() || reply->readAll().size() != 1) {
        fail(tr("The server returned an invalid range response.")); return;
    }
    m_total = match.captured(1).toLongLong();
    if (m_total < 1) { fail(tr("Invalid file size in the range response.")); return; }
    m_validator = validator(reply);
    m_ranges = true;
    release(m_probe);
    if (!restore()) resetParts();
    if (!m_active) return;
    checkpoint();
    emit progress(received(), m_total);
    if (received() == m_total) { complete(); return; }
    for (int i = 0; i < m_parts.size(); ++i) requestPart(i);
}

void SegmentedTransfer::resetParts()
{
    m_parts.clear();
    if (!m_payload.resize(0)) { fail(m_payload.errorString()); return; }
    // Small files use one connection. Validators prevent mixing different file versions.
    const int count = m_validator.isEmpty() ? 1
        : qMax(1, qMin(m_connections, int(qMin<qint64>(16, m_total / (256 * 1024)))));
    for (int i = 0; i < count; ++i) {
        Part part;
        part.start = m_total * i / count;
        part.end = m_total * (i + 1) / count - 1;
        m_parts.append(part);
    }
}

void SegmentedTransfer::requestPart(int index)
{
    if (!m_active || index < 0 || index >= m_parts.size()) return;
    auto &part = m_parts[index];
    if (part.received == part.end - part.start + 1) return;
    auto *reply = m_network->get(m_url, m_referer, part.start + part.received,
                                 m_validator, part.end);
    attach(index, reply);
}

void SegmentedTransfer::attach(int index, QNetworkReply *reply)
{
    auto &part = m_parts[index];
    part.reply = reply;
    part.validated = !m_ranges;
    reply->setParent(this);
    reply->setReadBufferSize(256 * 1024);
    reply->setProperty("rangeStart", part.start + part.received);
    connect(reply, &QNetworkReply::readyRead, this, [this, index, reply] { consume(index, reply); });
    connect(reply, &QNetworkReply::finished, this, [this, index, reply] { partFinished(index, reply); });
    consume(index, reply);
    if (reply->isFinished()) QTimer::singleShot(0, this, [this, index, reply] {
        if (m_active && index < m_parts.size() && m_parts[index].reply == reply)
            partFinished(index, reply);
    });
}

bool SegmentedTransfer::validate(int index, QNetworkReply *reply)
{
    auto &part = m_parts[index];
    if (part.validated) return true;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 200 && !m_fallback) {
        // Abort all ranged requests before truncating; never append a full body to a part.
        m_fallback = true;
        for (auto &item : m_parts) release(item.reply);
        startSingle();
        return false;
    }
    const auto expected = "bytes " + QByteArray::number(reply->property("rangeStart").toLongLong())
        + "-" + QByteArray::number(part.end) + "/" + QByteArray::number(m_total);
    if (status != 206 || reply->rawHeader("Content-Range") != expected
        || (!m_validator.isEmpty() && validator(reply) != m_validator)
        || (!reply->rawHeader("Content-Encoding").isEmpty()
            && reply->rawHeader("Content-Encoding") != "identity")) {
        fail(tr("The server changed the file or returned an invalid byte range. Retry the download."));
        return false;
    }
    part.validated = true;
    return true;
}

void SegmentedTransfer::consume(int index, QNetworkReply *reply)
{
    if (!m_active || index >= m_parts.size() || m_parts[index].reply != reply) return;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 0 || status >= 400 || reply->bytesAvailable() == 0) return;
    if (!validate(index, reply)) return;
    auto &part = m_parts[index];
    const qint64 allowed = m_speedLimit > 0 ? qMin(m_readBudget, reply->bytesAvailable()) : reply->bytesAvailable();
    if (allowed <= 0) return;
    const auto data = reply->read(allowed);
    if (m_speedLimit > 0) m_readBudget -= data.size();
    if (data.isEmpty()) return;
    if ((m_total >= 0 && part.start + part.received + data.size() > part.end + 1)
        || !m_payload.seek(part.start + part.received) || m_payload.write(data) != data.size()) {
        fail(tr("Cannot write the download, or the server sent too much data.")); return;
    }
    part.received += data.size();
    emit progress(received(), m_total);
}

void SegmentedTransfer::partFinished(int index, QNetworkReply *reply)
{
    if (!m_active || index >= m_parts.size() || m_parts[index].reply != reply) return;
    if (reply->error() == QNetworkReply::NoError) consume(index, reply);
    if (!m_active || index >= m_parts.size() || m_parts[index].reply != reply) return;
    if (reply->error() == QNetworkReply::NoError && reply->bytesAvailable() > 0) return;
    auto &part = m_parts[index];
    const bool completePart = reply->error() == QNetworkReply::NoError
        && (m_total < 0 || part.received == part.end - part.start + 1);
    const auto message = reply->error() == QNetworkReply::NoError
        ? tr("The server closed the download before the file was complete.") : reply->errorString();
    release(part.reply);
    if (!completePart) {
        checkpoint();
        if (m_ranges && !m_validator.isEmpty() && ++part.attempts <= 3) {
            const int generation = m_generation;
            QTimer::singleShot(1000, this, [this, index, generation] {
                if (generation == m_generation) requestPart(index);
            });
        } else fail(message);
        return;
    }
    if (m_total < 0) { m_total = part.received; part.end = m_total - 1; }
    if (received() == m_total) complete();
}

void SegmentedTransfer::startSingle(QNetworkReply *reply)
{
    ++m_generation;
    m_ranges = false;
    m_validator.clear();
    m_parts.clear();
    QFile::remove(m_directory + "/state.json");
    if (!m_payload.resize(0)) { fail(m_payload.errorString()); return; }
    auto *stream = reply ? reply : m_network->get(m_url, m_referer);
    const auto length = stream->header(QNetworkRequest::ContentLengthHeader);
    m_total = length.isValid() ? length.toLongLong() : -1;
    Part part;
    part.end = m_total - 1;
    m_parts.append(part);
    // With a new request, headers arrive after attach; learn its length then.
    connect(stream, &QNetworkReply::metaDataChanged, this, [this, stream] {
        if (!m_active || m_ranges || m_parts.isEmpty() || m_parts[0].reply != stream) return;
        const auto length = stream->header(QNetworkRequest::ContentLengthHeader);
        if (length.isValid()) { m_total = length.toLongLong(); m_parts[0].end = m_total - 1; }
    });
    attach(0, stream);
}

void SegmentedTransfer::complete()
{
    if (m_payload.size() != m_total) { fail(tr("The downloaded size does not match the expected file size.")); return; }
    m_active = false;
    m_checkpoint.stop();
    m_drainTimer.stop();
    checkpoint();
    m_payload.close();
    emit progress(received(), m_total);
    emit ready(payloadPath());
}

void SegmentedTransfer::fail(const QString &message)
{
    pause();
    emit failed(message);
}

void SegmentedTransfer::release(QPointer<QNetworkReply> &reply)
{
    if (!reply) return;
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();
    reply.clear();
}

void SegmentedTransfer::pause()
{
    ++m_generation;
    m_active = false;
    m_checkpoint.stop();
    m_drainTimer.stop();
    release(m_probe);
    for (auto &part : m_parts) release(part.reply);
    if (m_payload.isOpen()) { checkpoint(); m_payload.close(); }
}

qint64 SegmentedTransfer::received() const
{
    qint64 bytes = 0;
    for (const auto &part : m_parts) bytes += part.received;
    return bytes;
}

QString SegmentedTransfer::payloadPath() const { return m_directory + "/payload.part"; }

void SegmentedTransfer::setSpeedLimit(int kibibytes)
{
    m_speedLimit = qBound(0, kibibytes, 1048576);
    m_readBudget = qint64(m_speedLimit) * 1024 / 10;
}

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

#pragma once

#include <QtCore/QFile>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkReply>

class NetworkManager;

class SegmentedTransfer : public QObject
{
    Q_OBJECT
public:
    struct Part {
        qint64 start = 0;
        qint64 end = -1;
        qint64 received = 0;
        int attempts = 0;
        bool validated = false;
        QPointer<QNetworkReply> reply;
    };
    explicit SegmentedTransfer(NetworkManager *network, QObject *parent = nullptr);
    ~SegmentedTransfer() override;
    void start(const QUrl &url, const QString &referer, const QString &destination, int connections);
    void pause();
    void discard();
    void setSpeedLimit(int kibibytes);
    int speedLimit() const { return m_speedLimit; }
    QList<Part> parts() const { return m_parts; }
    bool supportsResume() const { return m_ranges; }
    qint64 received() const;
    qint64 total() const { return m_total; }
    QString payloadPath() const;

signals:
    void progress(qint64 received, qint64 total);
    void ready(const QString &path);
    void failed(const QString &message);

private:
    NetworkManager *m_network;
    QUrl m_url;
    QString m_referer;
    QString m_directory;
    QFile m_payload;
    QPointer<QNetworkReply> m_probe;
    QList<Part> m_parts;
    QTimer m_checkpoint;
    QTimer m_drainTimer;
    int m_speedLimit = 0;
    int m_drainCursor = 0;
    qint64 m_readBudget = 0;
    QByteArray m_validator;
    qint64 m_total = -1;
    int m_connections = 8;
    int m_generation = 0;
    bool m_ranges = false;
    bool m_active = false;
    bool m_fallback = false;

    void probe();
    void probeFinished(QNetworkReply *reply);
    void startSingle(QNetworkReply *reply = nullptr);
    void attach(int index, QNetworkReply *reply);
    void requestPart(int index);
    bool validate(int index, QNetworkReply *reply);
    void consume(int index, QNetworkReply *reply);
    void partFinished(int index, QNetworkReply *reply);
    void complete();
    void fail(const QString &message);
    void release(QPointer<QNetworkReply> &reply);
    bool restore();
    void checkpoint();
    void resetParts();
};

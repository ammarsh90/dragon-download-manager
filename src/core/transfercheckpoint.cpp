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
#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>

bool SegmentedTransfer::restore()
{
    // Without a validator a previous representation cannot be trusted across requests.
    if (m_validator.isEmpty() || m_payload.size() > m_total) return false;
    QFile state(m_directory + "/state.json");
    if (!state.open(QIODevice::ReadOnly)) return false;
    const auto json = QJsonDocument::fromJson(state.readAll()).object();
    if (json["url"].toString() != m_url.toString(QUrl::FullyEncoded)
        || json["validator"].toString().toLatin1() != m_validator
        || json["total"].toString().toLongLong() != m_total) return false;
    QList<Part> parts;
    qint64 next = 0;
    for (const auto &value : json["parts"].toArray()) {
        const auto object = value.toObject();
        Part part;
        part.start = object["start"].toString().toLongLong();
        part.end = object["end"].toString().toLongLong();
        part.received = object["received"].toString().toLongLong();
        if (part.start != next || part.end < part.start || part.end >= m_total
            || part.received < 0 || part.received > part.end - part.start + 1
            || (part.received > 0 && m_payload.size() < part.start + part.received)) return false;
        next = part.end + 1;
        parts.append(part);
    }
    if (parts.isEmpty() || parts.size() > 16 || next != m_total) return false;
    m_parts = parts;
    return true;
}

void SegmentedTransfer::checkpoint()
{
    if (m_directory.isEmpty() || !m_payload.isOpen() || m_parts.isEmpty() || !m_ranges) return;
    if (!m_payload.flush()) return;
    QJsonArray parts;
    for (const auto &part : m_parts) {
        parts.append(QJsonObject{{"start", QString::number(part.start)},
                                 {"end", QString::number(part.end)},
                                 {"received", QString::number(part.received)}});
    }
    const QJsonObject json{{"url", m_url.toString(QUrl::FullyEncoded)},
        {"validator", QString::fromLatin1(m_validator)}, {"total", QString::number(m_total)},
        {"parts", parts}};
    QSaveFile state(m_directory + "/state.json");
    if (state.open(QIODevice::WriteOnly)) {
        const auto data = QJsonDocument(json).toJson(QJsonDocument::Compact);
        if (state.write(data) == data.size()) state.commit();
    }
}

void SegmentedTransfer::discard()
{
    pause();
    if (m_directory.isEmpty()) return;
    // Delete only the two files owned by this transfer; never recursively delete paths.
    QFile::remove(payloadPath());
    QFile::remove(m_directory + "/state.json");
    QDir().rmdir(m_directory);
    m_parts.clear();
}

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
#include <Core/AbstractJob>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include "segmentedtransfer.h"
class Scheduler;
class NetworkManager;
class ResourceItem;

class JobFile : public AbstractJob
{
    Q_OBJECT
public:
    JobFile(QObject *parent, ResourceItem *resource);
    ~JobFile() override;
    void resume() override;
    void pause() override;
    void stop() override;
    void rename(const QString &newName) override;
    QList<SegmentedTransfer::Part> parts() const;
    bool supportsResume() const;
    void setBrowserContext(const QUrl &url, const QString &agent, const QJsonArray &cookies);
    void setSpeedLimit(int kibibytes) { m_transfer->setSpeedLimit(kibibytes); }
    int speedLimit() const { return m_transfer->speedLimit(); }
    void setOverwriteApproved(bool approved) { m_overwriteApproved = approved; }
private:
    Scheduler *m_scheduler;
    NetworkManager *m_contextNetwork;
    SegmentedTransfer *m_transfer;
    QFile m_assembly;
    qint64 m_sessionStart = -1;
    int m_generation = 0;
    bool m_overwriteApproved = false;
    void assemble(const QString &path);
    void copyNextBlock(int generation);
    void transferFailed(const QString &message);
};

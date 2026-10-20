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

#include "jobfile.h"
#include <Core/File>
#include <Core/ResourceItem>
#include <Core/Scheduler>
#include <QtCore/QSettings>
#include <QtCore/QTimer>

JobFile::JobFile(QObject *parent, ResourceItem *resource)
    : AbstractJob(parent, resource), m_scheduler(static_cast<Scheduler *>(parent)),
      m_transfer(new SegmentedTransfer(m_scheduler->networkManager(), this))
{
    setMaxConnections(QSettings().value("Downloads/Connections", 8).toInt());
    connect(m_transfer, &SegmentedTransfer::progress, this, [this](qint64 received, qint64 total) {
        if (m_sessionStart < 0) m_sessionStart = received;
        updateInfo(qMax<qint64>(0, received - m_sessionStart), total);
        setBytesReceived(received);
        setBytesTotal(total);
    });
    connect(m_transfer, &SegmentedTransfer::ready, this, &JobFile::assemble);
    connect(m_transfer, &SegmentedTransfer::failed, this, &JobFile::transferFailed);
}

JobFile::~JobFile() { m_transfer->pause(); }

void JobFile::resume()
{
    if (isDownloading()) return;
    beginResume();
    setErrorMessage({});
    m_sessionStart = -1;
    const auto flag = m_file->isOpen() ? File::Open : m_file->open(m_resource, m_overwriteApproved);
    if (flag == File::Skip) { setState(Skipped); finish(); return; }
    if (!checkResume(flag == File::Open)) return;
    tearDownResume();
    m_transfer->start(m_resource->url_TODO(), m_resource->referringPage(),
                      localFullFileName(), maxConnections());
}

void JobFile::pause()
{
    ++m_generation;
    m_assembly.close();
    m_transfer->pause();
    m_file->cancel();
    setState(Paused);
    finish();
}

void JobFile::stop() { if (state() != Completed) pause(); }

void JobFile::rename(const QString &newName)
{
    // Cache identity includes the destination. Avoid losing partial data after a rename.
    if (bytesReceived() > 0 && state() != Completed) return;
    AbstractJob::rename(newName);
}

QList<SegmentedTransfer::Part> JobFile::parts() const { return m_transfer->parts(); }
bool JobFile::supportsResume() const { return m_transfer->supportsResume(); }

void JobFile::assemble(const QString &path)
{
    m_assembly.setFileName(path);
    if (!m_assembly.open(QIODevice::ReadOnly)) { transferFailed(m_assembly.errorString()); return; }
    setState(Endgame);
    const int generation = ++m_generation;
    QTimer::singleShot(0, this, [this, generation] { copyNextBlock(generation); });
}

void JobFile::copyNextBlock(int generation)
{
    if (generation != m_generation || !m_assembly.isOpen()) return;
    const auto data = m_assembly.read(1024 * 1024);
    if (m_assembly.error() != QFileDevice::NoError || !m_file->write(data)) {
        transferFailed(tr("Cannot save the completed file. Check free disk space and folder permissions."));
        return;
    }
    if (!m_assembly.atEnd()) {
        QTimer::singleShot(0, this, [this, generation] { copyNextBlock(generation); });
        return;
    }
    m_assembly.close();
    const bool committed = m_file->commit();
    if (committed) m_transfer->discard();
    else setErrorMessage(tr("Cannot replace the destination file. The partial download is retained."));
    preFinish(committed);
    finish();
}

void JobFile::transferFailed(const QString &message)
{
    ++m_generation;
    m_assembly.close();
    m_file->cancel();
    setErrorMessage(message);
    setState(NetworkError);
    finish();
}

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

#include "downloadprogressdialog.h"
#include <Core/AbstractJob>
#include <Core/JobFile>
#include <Core/Format>
#include <Core/Scheduler>
#include <QtCore/QFileInfo>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

DownloadProgressDialog::DownloadProgressDialog(AbstractJob *job, Scheduler *scheduler, QWidget *parent)
    : QDialog(parent), m_job(job), m_scheduler(scheduler)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName("downloadProgressDialog");
    setWindowFlag(Qt::WindowMinimizeButtonHint);
    setMinimumWidth(480);
    resize(520, 310);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 10);
    layout->setSpacing(7);
    m_name = new QLabel(this);
    m_name->setObjectName("downloadFileName");
    m_name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(m_name);
    auto *source = new QLabel(this);
    source->setObjectName("downloadSource");
    source->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    source->setToolTip(job->sourceUrl().toDisplayString(QUrl::RemovePassword));
    source->setTextFormat(Qt::PlainText);
    source->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(source);
    auto *form = new QFormLayout;
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(4);
    const auto field = [&](const QString &title) {
        auto *label = new QLabel(this);
        label->setTextFormat(Qt::PlainText);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(title, label);
        return label;
    };
    m_status = field(tr("Status"));
    m_status->setWordWrap(true);
    m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_size = field(tr("File size"));
    m_received = field(tr("Downloaded"));
    m_speed = field(tr("Transfer rate"));
    m_remaining = field(tr("Time left"));
    m_resume = field(tr("Resume support"));
    if (auto *file = qobject_cast<JobFile *>(job)) {
        auto *limit = new QSpinBox(this);
        limit->setObjectName("downloadSpeedLimit");
        limit->setRange(0, 1048576);
        limit->setSpecialValueText(tr("Unlimited"));
        limit->setSuffix(tr(" KiB/s"));
        limit->setValue(file->speedLimit());
        limit->setMaximumWidth(140);
        form->addRow(tr("Speed limit"), limit);
        connect(limit, &QSpinBox::valueChanged, file, &JobFile::setSpeedLimit);
    }
    layout->addLayout(form);
    m_progress = new QProgressBar(this);
    m_progress->setFixedHeight(18);
    layout->addWidget(m_progress);
    m_connections = new QTreeWidget(this);
    m_connections->setRootIsDecorated(false);
    m_connections->setHeaderLabels({tr("Connection"), tr("Downloaded"), tr("Progress"), tr("Status")});
    m_connections->header()->setSectionResizeMode(QHeaderView::Stretch);
    m_connections->setFixedHeight(120);
    m_connections->hide();
    layout->addWidget(m_connections);
    auto *footer = new QHBoxLayout;
    auto *details = new QPushButton(tr("Show details"), this);
    details->setCheckable(true);
    details->setObjectName("downloadDetailsButton");
    footer->addWidget(details);
    footer->addStretch();
    auto *buttons = new QDialogButtonBox(this);
    m_pause = buttons->addButton(tr("Pause"), QDialogButtonBox::ActionRole);
    m_pause->setObjectName("primaryButton");
    m_open = buttons->addButton(tr("Open file"), QDialogButtonBox::ActionRole);
    m_folder = buttons->addButton(tr("Open folder"), QDialogButtonBox::ActionRole);
    auto *background = buttons->addButton(tr("Background"), QDialogButtonBox::RejectRole);
    background->setToolTip(tr("Close this window and keep downloading in the main list."));
    footer->addWidget(buttons);
    layout->addLayout(footer);
    connect(details, &QPushButton::toggled, this, [this, details](bool expanded) {
        m_connections->setVisible(expanded);
        details->setText(expanded ? tr("Hide details") : tr("Show details"));
        adjustSize();
    });
    connect(background, &QPushButton::clicked, this, &QDialog::close);
    connect(m_pause, &QPushButton::clicked, this, [this] {
        if (!m_job) return;
        if (m_job->isDownloading()) m_scheduler->pause(m_job);
        else if (m_job->isResumable()) m_scheduler->resume(m_job);
    });
    connect(m_open, &QPushButton::clicked, this, [this] {
        if (m_job && m_job->state() == AbstractJob::Completed) QDesktopServices::openUrl(m_job->localFileUrl());
    });
    connect(m_folder, &QPushButton::clicked, this, [this] {
        if (m_job) QDesktopServices::openUrl(m_job->localDirUrl());
    });
    connect(job, &AbstractJob::changed, this, &DownloadProgressDialog::refresh);
    connect(job, &QObject::destroyed, this, &QDialog::close);
    refresh();
    adjustSize();
}

void DownloadProgressDialog::refresh()
{
    if (!m_job) return;
    auto *job = m_job.data();
    const int progress = job->progress();
    const bool done = job->state() == AbstractJob::Completed;
    m_name->setText(m_name->fontMetrics().elidedText(job->localFileName(), Qt::ElideMiddle, qMax(400, width() - 28)));
    m_name->setToolTip(job->localFileName());
    auto *source = findChild<QLabel *>("downloadSource");
    source->setText(source->fontMetrics().elidedText(job->sourceUrl().toDisplayString(QUrl::RemovePassword), Qt::ElideMiddle, qMax(400, width() - 28)));
    m_name->setTextFormat(Qt::PlainText);
    setWindowTitle((progress >= 0 ? QString::number(progress) + "% - " : QString()) + job->localFileName());
    m_status->setText(job->errorMessage().isEmpty() ? job->stateToString() : job->errorMessage());
    m_size->setText(job->bytesTotal() >= 0 ? Format::fileSizeToString(job->bytesTotal()) : tr("Unknown"));
    m_received->setText(Format::fileSizeToString(job->bytesReceived()));
    m_speed->setText(job->speed() > 0 ? Format::currentSpeedToString(job->speed()) : tr("—"));
    m_remaining->setText(done ? tr("Complete") : job->isDownloading() ? Format::timeToString(job->remainingTime()) : tr("—"));
    m_progress->setRange(0, progress < 0 ? 0 : 100);
    m_progress->setValue(qMax(0, progress));
    m_pause->setText(job->isDownloading() ? tr("Pause") : tr("Resume"));
    m_pause->setEnabled(job->state() != AbstractJob::Endgame && (job->isDownloading() || job->isResumable()));
    m_pause->setVisible(!done);
    m_open->setVisible(done);
    m_folder->setVisible(done);
    m_open->setEnabled(done);
    m_folder->setEnabled(QFileInfo(job->localFilePath()).isDir());
    auto *fileJob = qobject_cast<JobFile *>(job);
    m_resume->setText(done ? tr("Complete") : fileJob && fileJob->supportsResume()
        ? tr("Yes") : tr("Checking / server dependent"));
    const auto parts = fileJob ? fileJob->parts() : QList<SegmentedTransfer::Part>();
    while (m_connections->topLevelItemCount() > parts.size()) delete m_connections->takeTopLevelItem(0);
    for (int i = 0; i < parts.size(); ++i) {
        if (m_connections->topLevelItemCount() <= i) m_connections->addTopLevelItem(new QTreeWidgetItem);
        auto *row = m_connections->topLevelItem(i);
        const auto &part = parts[i];
        const auto length = part.end - part.start + 1;
        const bool complete = length >= 0 && part.received == length;
        row->setText(0, tr("Connection %1").arg(i + 1));
        row->setText(1, Format::fileSizeToString(part.received));
        row->setText(2, length > 0 ? QString::number(100 * part.received / length) + "%" : tr("Unknown"));
        row->setText(3, complete ? tr("Complete") : part.reply ? (part.received > 0 ? tr("Receiving") : tr("Connecting / waiting")) : job->stateToString());
    }
}

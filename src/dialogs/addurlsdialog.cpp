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

#include "addurlsdialog.h"
#include <Core/JobFile>
#include <Core/NetworkManager>
#include <Core/Format>
#include <QtCore/QTimer>
#include <Core/Scheduler>
#include <Core/ResourceItem>
#include <Core/Settings>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QRegularExpression>
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
#include <QtWidgets/QVBoxLayout>

AddUrlsDialog::AddUrlsDialog(const QString &text, Scheduler *scheduler, Settings *settings, QWidget *parent, JobFile *pendingJob)
    : QDialog(parent), m_pendingJob(pendingJob), m_browserForm(pendingJob != nullptr), m_scheduler(scheduler), m_settings(settings)
{
    setObjectName("addDownloadDialog");
    setWindowTitle(tr("Download file info"));
    setMinimumWidth(500);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 10);
    layout->setSpacing(8);
    auto *heading = new QLabel(tr("Download file info"), this);
    heading->setObjectName("dialogHeading");
    layout->addWidget(heading);
    auto *hint = new QLabel(tr("Paste the download link and choose where to save your file."), this);
    hint->setWordWrap(true);
    hint->hide();
    auto *form = new QFormLayout;
    form->setSpacing(7);
    m_url = new QLineEdit(this);
    m_url->setObjectName("downloadUrl");
    m_url->setPlaceholderText("https://example.com/file.zip");
    m_url->setClearButtonEnabled(true);
    m_url->setLayoutDirection(Qt::LeftToRight);
    form->addRow(tr("Download link"), m_url);
    auto *destination = new QHBoxLayout;
    m_saveAs = new QLineEdit(this);
    m_saveAs->setObjectName("downloadSaveAs");
    m_saveAs->setLayoutDirection(Qt::LeftToRight);
    auto *browseButton = new QPushButton(tr("Browse..."), this);
    destination->addWidget(m_saveAs, 1);
    destination->addWidget(browseButton);
    form->addRow(tr("Save as"), destination);
    m_description = new QLineEdit(this);
    m_description->setPlaceholderText(tr("Optional description"));
    form->addRow(tr("Description"), m_description);
    m_connections = new QComboBox(this);
    for (int count : {1, 2, 4, 8, 16}) m_connections->addItem(tr("%1 connection(s)").arg(count), count);
    const int configured = QSettings().value("Downloads/Connections", 8).toInt();
    m_connections->setCurrentIndex(qMax(0, m_connections->findData(configured)));
    form->addRow(tr("Connections"), m_connections);
    layout->addLayout(form);
    m_fileInfo = new QLabel(tr("File size will appear after entering a link."), this);
    m_fileInfo->setWordWrap(true);
    layout->addWidget(m_fileInfo);
    m_metadataTimer = new QTimer(this);
    m_metadataTimer->setSingleShot(true);
    m_metadataTimer->setInterval(500);
    connect(m_metadataTimer, &QTimer::timeout, this, &AddUrlsDialog::inspectLink);
    m_validation = new QLabel(this);
    m_validation->setObjectName("downloadValidation");
    m_validation->setWordWrap(true);
    layout->addWidget(m_validation);
    auto *buttons = new QDialogButtonBox(this);
    m_start = buttons->addButton(tr("Start download"), QDialogButtonBox::AcceptRole);
    m_start->setObjectName("primaryButton");
    m_start->setDefault(true);
    m_start->style()->unpolish(m_start);
    m_start->style()->polish(m_start);
    m_later = buttons->addButton(tr("Download later"), QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &AddUrlsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_later, &QPushButton::clicked, this, &AddUrlsDialog::acceptPaused);
    connect(browseButton, &QPushButton::clicked, this, &AddUrlsDialog::browse);
    connect(m_url, &QLineEdit::textChanged, this, &AddUrlsDialog::updateFields);
    connect(m_saveAs, &QLineEdit::textChanged, this, [this] { m_customPath = true; updateFields(); });
    connect(m_url, &QLineEdit::textChanged, this, [this] {
        if (m_metadataReply) { m_metadataReply->disconnect(this); m_metadataReply->abort(); m_metadataReply->deleteLater(); m_metadataReply.clear(); }
        m_metadataTimer->start();
    });
    m_url->setText(text.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).value(0));
    if (pendingJob) {
        m_customPath = true;
        m_saveAs->setText(QDir::toNativeSeparators(pendingJob->localFullFileName()));
        m_description->setText(pendingJob->resource()->description());
        m_connections->setCurrentIndex(qMax(0, m_connections->findData(pendingJob->maxConnections())));
        m_fileInfo->setText(pendingJob->bytesTotal() > 0
            ? tr("File size: %1").arg(Format::fileSizeToString(pendingJob->bytesTotal())) : tr("Download safely received from your browser."));
        for (auto *button : buttons->buttons()) if (buttons->standardButton(button) == QDialogButtonBox::Cancel)
            button->setText(tr("Keep in queue"));
    }
    updateFields();
    m_url->setFocus();
    m_url->selectAll();
}

QUrl AddUrlsDialog::downloadUrl() const
{
    return QUrl::fromUserInput(m_url->text().trimmed());
}

void AddUrlsDialog::updateFields()
{
    const auto url = downloadUrl();
    const bool valid = url.isValid() && !url.host().isEmpty()
        && (url.scheme() == "https" || url.scheme() == "http");
    if (!m_customPath) {
        const QString directory = QSettings().value("Downloads/Folder",
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).toString();
        QString name = url.fileName();
        name.replace(QRegularExpression(R"([<>:"/\\|?*\x00-\x1f])"), "_");
        if (name.isEmpty()) name = "download";
        const QSignalBlocker blocker(m_saveAs);
        m_saveAs->setText(QDir::toNativeSeparators(QDir(directory).filePath(name)));
    }
    const bool destinationValid = !m_saveAs->text().trimmed().isEmpty();
    m_start->setEnabled(valid && destinationValid);
    m_later->setEnabled(valid && destinationValid);
    m_validation->setText(!valid && !m_url->text().isEmpty()
        ? tr("Enter a complete HTTP or HTTPS download link.") : QString());
}

void AddUrlsDialog::browse()
{
    const auto path = QFileDialog::getSaveFileName(this, tr("Save download as"), m_saveAs->text());
    if (!path.isEmpty()) {
        m_customPath = true;
        m_saveAs->setText(QDir::toNativeSeparators(path));
        updateFields();
    }
}

void AddUrlsDialog::accept() { submit(true); }
void AddUrlsDialog::acceptPaused() { submit(false); }

void AddUrlsDialog::submit(bool started)
{
    if (m_browserForm && !m_pendingJob) { reject(); return; }
    if (!m_start->isEnabled()) return;
    const QFileInfo destination(m_saveAs->text().trimmed());
#ifdef Q_OS_WIN
    const auto name = destination.fileName();
    const bool invalidName = name.contains(QRegularExpression(R"([<>:"/\\|?*\x00-\x1f])"))
        || name.endsWith('.') || name.endsWith(' ') || name.size() > 255
        || QRegularExpression(R"(^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$))",
                              QRegularExpression::CaseInsensitiveOption).match(name).hasMatch();
    if (invalidName) {
        QMessageBox::warning(this, tr("Invalid file name"), tr("Choose a valid Windows file name."));
        return;
    }
#endif
    if (!destination.isAbsolute() || destination.fileName().isEmpty()
        || destination.isDir() || !QDir().mkpath(destination.absolutePath())
        || !QFileInfo(destination.absolutePath()).isWritable()) {
        QMessageBox::warning(this, tr("Choose a save location"), tr("Choose a writable folder and a valid file name."));
        return;
    }
    auto target = destination;
    const auto occupied = [&](const QFileInfo &path) {
        for (auto *existing : m_scheduler->jobs()) {
            if (existing != m_pendingJob && existing->state() != AbstractJob::Completed && existing->state() != AbstractJob::Skipped
                && QFileInfo(existing->localFullFileName()).absoluteFilePath().compare(
                    path.absoluteFilePath(), Qt::CaseInsensitive) == 0) return existing;
        }
        return static_cast<AbstractJob *>(nullptr);
    };
    auto *reserved = occupied(target);
    bool overwrite = false;
    if (destination.exists() || reserved) {
        int suffix = 1;
        do {
            const QString extension = destination.suffix().isEmpty() ? QString() : "." + destination.suffix();
            target = QFileInfo(destination.dir().filePath(destination.completeBaseName()
                + QString(" (%1)").arg(suffix++) + extension));
        } while (target.exists() || occupied(target));
        QMessageBox duplicate(this);
        duplicate.setWindowTitle(tr("File name already used"));
        duplicate.setIcon(QMessageBox::Question);
        duplicate.setText(tr("A file or download named %1 already exists.").arg(destination.fileName()));
        duplicate.setInformativeText(tr("Keep both files with a new name, choose another name, or use the existing download."));
        auto *keep = duplicate.addButton(tr("Keep both: %1").arg(target.fileName()), QMessageBox::AcceptRole);
        auto *rename = duplicate.addButton(tr("Change name..."), QMessageBox::ActionRole);
        auto *replace = !reserved ? duplicate.addButton(tr("Replace existing"), QMessageBox::DestructiveRole) : nullptr;
        auto *show = reserved ? duplicate.addButton(tr("Show existing download"), QMessageBox::ActionRole) : nullptr;
        duplicate.addButton(QMessageBox::Cancel);
        duplicate.setDefaultButton(keep);
        duplicate.exec();
        if (show && duplicate.clickedButton() == show) {
            emit downloadAdded(reserved, true);
            QDialog::accept();
            return;
        }
        if (duplicate.clickedButton() == rename) { m_saveAs->setFocus(); m_saveAs->selectAll(); return; }
        if (replace && duplicate.clickedButton() == replace) { target = destination; overwrite = true; }
        else if (duplicate.clickedButton() != keep) return;
        m_saveAs->setText(QDir::toNativeSeparators(target.absoluteFilePath()));
    }
    auto *resource = m_pendingJob ? m_pendingJob->resource() : new ResourceItem;
    resource->setUrl(downloadUrl().toString(QUrl::FullyEncoded));
    resource->setDestination(target.absolutePath());
    resource->setCustomFileName(target.fileName());
    resource->setDescription(m_description->text().trimmed());
    resource->setMask("*name*");
    if (!m_browserForm && m_settings->isHttpReferringPageEnabled()) resource->setReferringPage(m_settings->httpReferringPage());
    auto *job = m_pendingJob ? m_pendingJob.data() : new JobFile(m_scheduler, resource);
    job->setMaxConnections(m_connections->currentData().toInt());
    job->setOverwriteApproved(overwrite);
    if (m_pendingJob) {
        m_scheduler->activateSnapshot();
        if (started) m_scheduler->resume(job);
    } else m_scheduler->append(QList<AbstractJob *>{job}, started);
    emit downloadAdded(job, started);
    QSettings().setValue("Downloads/Folder", destination.absolutePath());
    QDialog::accept();
}

AddUrlsDialog::~AddUrlsDialog()
{
    if (m_metadataReply) { m_metadataReply->disconnect(this); m_metadataReply->abort(); }
}

void AddUrlsDialog::inspectLink()
{
    if (m_browserForm) return;
    const auto url = downloadUrl();
    if (!url.isValid() || url.host().isEmpty() || (url.scheme() != "http" && url.scheme() != "https")) return;
    m_fileInfo->setText(tr("Checking file information..."));
    auto *reply = m_scheduler->networkManager()->get(url, {}, 0, {}, 0);
    m_metadataReply = reply;
    reply->setParent(this);
    reply->setReadBufferSize(16 * 1024);
    connect(reply, &QNetworkReply::metaDataChanged, this, [this, reply] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200 && status != 206) return;
        qint64 size = -1;
        const auto range = QRegularExpression("/([0-9]+)$").match(QString::fromLatin1(reply->rawHeader("Content-Range")));
        if (range.hasMatch()) size = range.captured(1).toLongLong();
        else if (reply->header(QNetworkRequest::ContentLengthHeader).isValid())
            size = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        const auto disposition = QString::fromLatin1(reply->rawHeader("Content-Disposition"));
        const auto encoded = QRegularExpression("filename\\*=UTF-8''([^;]+)", QRegularExpression::CaseInsensitiveOption).match(disposition);
        const auto plain = QRegularExpression(R"(filename=["']?([^;"']+))", QRegularExpression::CaseInsensitiveOption).match(disposition);
        QString name = encoded.hasMatch() ? QUrl::fromPercentEncoding(encoded.captured(1).toLatin1())
            : plain.hasMatch() ? plain.captured(1).trimmed() : QString();
        name.replace(QRegularExpression(R"([<>:"/\\|?*\x00-\x1f])"), "_");
        while (name.endsWith('.') || name.endsWith(' ')) name.chop(1);
        if (!name.isEmpty() && !m_customPath) {
            const QSignalBlocker blocker(m_saveAs);
            m_saveAs->setText(QDir::toNativeSeparators(QFileInfo(m_saveAs->text()).dir().filePath(name)));
        }
        m_fileInfo->setText(size >= 0 ? tr("File size: %1").arg(Format::fileSizeToString(size))
                                    : tr("The server does not report a file size."));
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
        m_metadataReply.clear();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (m_metadataReply != reply) return;
        m_fileInfo->setText(tr("File information is unavailable. You can still start the download."));
        reply->deleteLater();
        m_metadataReply.clear();
    });
}

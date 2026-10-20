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
#include <Core/Scheduler>
#include <Core/ResourceItem>
#include <Core/Settings>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QRegularExpression>
#include <QtCore/QSignalBlocker>
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

AddUrlsDialog::AddUrlsDialog(const QString &text, Scheduler *scheduler, Settings *settings, QWidget *parent)
    : QDialog(parent), m_scheduler(scheduler), m_settings(settings)
{
    setObjectName("addDownloadDialog");
    setWindowTitle(tr("Add download"));
    setMinimumWidth(600);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 20);
    layout->setSpacing(18);
    auto *heading = new QLabel(tr("Download a file"), this);
    heading->setObjectName("dialogHeading");
    layout->addWidget(heading);
    auto *hint = new QLabel(tr("Paste the download link and choose where to save your file."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *form = new QFormLayout;
    form->setSpacing(12);
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
    layout->addLayout(form);
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
    m_url->setText(text.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).value(0));
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
    if (!m_start->isEnabled()) return;
    const QFileInfo destination(m_saveAs->text().trimmed());
    if (!destination.isAbsolute() || destination.fileName().isEmpty()
        || destination.isDir() || !QDir().mkpath(destination.absolutePath())
        || !QFileInfo(destination.absolutePath()).isWritable()) {
        QMessageBox::warning(this, tr("Choose a save location"), tr("Choose a writable folder and a valid file name."));
        return;
    }
    if (destination.exists()) {
        QMessageBox::information(this, tr("File already exists"), tr("Choose another file name to keep your existing file."));
        return;
    }
    auto *resource = new ResourceItem;
    resource->setUrl(downloadUrl().toString(QUrl::FullyEncoded));
    resource->setDestination(destination.absolutePath());
    resource->setCustomFileName(destination.fileName());
    resource->setMask("*name*");
    if (m_settings->isHttpReferringPageEnabled()) resource->setReferringPage(m_settings->httpReferringPage());
    auto *job = new JobFile(m_scheduler, resource);
    m_scheduler->append(QList<AbstractJob *>{job}, started);
    QSettings().setValue("Downloads/Folder", destination.absolutePath());
    QDialog::accept();
}

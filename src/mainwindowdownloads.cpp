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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "dialogs/downloadprogressdialog.h"
#include <Core/AbstractJob>
#include <Core/Scheduler>
#include <Core/ResourceItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QHBoxLayout>
#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDateTimeEdit>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtGui/QAction>

namespace {
int category(const AbstractJob *job)
{
    const auto extension = QFileInfo(job->localFileName()).suffix().toLower();
    const QList<QStringList> extensions{
        {"zip", "rar", "7z", "tar", "gz", "iso"},
        {"pdf", "doc", "docx", "xls", "xlsx", "txt", "epub", "pptx"},
        {"mp3", "aac", "wav", "flac", "ogg", "m4a"},
        {"mp4", "mkv", "webm", "avi", "mov", "m4v"},
        {"exe", "msi", "msix", "apk", "dmg", "deb"},
        {"png", "jpg", "jpeg", "gif", "webp", "svg", "bmp"}};
    for (int i = 0; i < extensions.size(); ++i) if (extensions[i].contains(extension)) return i + 10;
    return 16;
}
}

void MainWindow::setupDownloadNavigation()
{
    auto *searchBar = new QHBoxLayout;
    auto *label = new QLabel(tr("Downloads"), ui->centralwidget);
    label->setObjectName("dialogHeading");
    m_downloadSearch = new QLineEdit(ui->centralwidget);
    m_downloadSearch->setObjectName("downloadSearch");
    m_downloadSearch->setPlaceholderText(tr("Search file name, address or description"));
    m_downloadSearch->setClearButtonEnabled(true);
    m_downloadSearch->setMaximumWidth(420);
    searchBar->addWidget(label);
    searchBar->addStretch();
    searchBar->addWidget(m_downloadSearch);
    ui->verticalLayout->insertLayout(0, searchBar);
    auto *content = new QSplitter(Qt::Horizontal, ui->centralwidget);
    content->setObjectName("downloadNavigation");
    ui->verticalLayout->removeWidget(m_downloadPages);
    m_downloadCategories = new QTreeWidget(content);
    m_downloadCategories->setObjectName("downloadCategories");
    m_downloadCategories->setHeaderHidden(true);
    m_downloadCategories->setRootIsDecorated(false);
    m_downloadCategories->setIndentation(12);
    m_downloadCategories->setMinimumWidth(175);
    const QList<QPair<QString, int>> items{
        {tr("All downloads"), 0}, {tr("Unfinished"), 1}, {tr("Completed"), 2},
        {tr("Waiting / paused"), 3}, {tr("Archives"), 10}, {tr("Documents"), 11},
        {tr("Music"), 12}, {tr("Video"), 13}, {tr("Programs"), 14},
        {tr("Images"), 15}, {tr("Other files"), 16}};
    for (const auto &entry : items) {
        auto *item = new QTreeWidgetItem(m_downloadCategories, {entry.first});
        item->setData(0, Qt::UserRole, entry.second);
        item->setData(0, Qt::UserRole + 1, entry.first);
        item->setSizeHint(0, QSize(175, 34));
    }
    m_downloadCategories->setCurrentItem(m_downloadCategories->topLevelItem(0));
    content->addWidget(m_downloadPages);
    content->setStretchFactor(0, 0);
    content->setStretchFactor(1, 1);
    content->setSizes({195, 950});
    ui->verticalLayout->addWidget(content, 1);
    connect(m_downloadSearch, &QLineEdit::textChanged, this, &MainWindow::filterDownloads);
    connect(m_downloadCategories, &QTreeWidget::currentItemChanged, this, &MainWindow::filterDownloads);
    auto *stopAll = new QAction(tr("Stop all"), this);
    stopAll->setIcon(ui->actionCancel->icon());
    connect(stopAll, &QAction::triggered, this, &MainWindow::stopAllDownloads);
    auto *start = new QAction(tr("Start queue"), this);
    start->setIcon(ui->actionResume->icon());
    connect(start, &QAction::triggered, this, &MainWindow::startQueue);
    auto *schedule = new QAction(tr("Scheduler"), this);
    schedule->setIcon(QIcon::fromTheme("preference-schedule"));
    connect(schedule, &QAction::triggered, this, &MainWindow::showDownloadSchedule);
    ui->downloadBar->removeAction(ui->actionCancel);
    ui->downloadBar->insertAction(ui->actionRemoveSelected, stopAll);
    ui->downloadBar->addSeparator();
    ui->downloadBar->addAction(start);
    ui->downloadBar->addAction(schedule);
    ui->menuDownload->addSeparator();
    ui->menuDownload->addActions({stopAll, start, schedule, ui->actionRenameFile});
    ui->actionInformation->setText(tr("Download progress"));
    filterDownloads();
}

void MainWindow::filterDownloads()
{
    if (!m_downloadCategories || !m_downloadSearch) return;
    const int selected = m_downloadCategories->currentItem()
        ? m_downloadCategories->currentItem()->data(0, Qt::UserRole).toInt() : 0;
    const auto matches = [](const AbstractJob *job, int key) {
        if (key == 0) return true;
        if (key == 1) return job->state() != AbstractJob::Completed;
        if (key == 2) return job->state() == AbstractJob::Completed;
        if (key == 3) return job->isResumable();
        return category(job) == key;
    };
    const auto jobs = m_scheduler->jobs();
    const auto query = m_downloadSearch->text().trimmed();
    for (int row = 0; row < jobs.size(); ++row) {
        const auto *job = jobs[row];
        const bool search = query.isEmpty() || (job->localFileName() + " "
            + job->sourceUrl().toString() + " " + job->resource()->description()).contains(query, Qt::CaseInsensitive);
        const bool hidden = !search || !matches(job, selected);
        ui->queueView->setRowHidden(row, hidden);
        if (hidden) ui->queueView->selectionModel()->select(ui->queueView->model()->index(row, 0),
            QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
    }
    for (int i = 0; i < m_downloadCategories->topLevelItemCount(); ++i) {
        auto *item = m_downloadCategories->topLevelItem(i);
        int count = 0;
        for (const auto *job : jobs) if (matches(job, item->data(0, Qt::UserRole).toInt())) ++count;
        item->setText(0, item->data(0, Qt::UserRole + 1).toString() + QString("  (%1)").arg(count));
    }
}

void MainWindow::showDownloadProgress(AbstractJob *job)
{
    if (!job) return;
    if (m_progressWindows.value(job)) {
        m_progressWindows[job]->showNormal();
        m_progressWindows[job]->raise();
        m_progressWindows[job]->activateWindow();
        return;
    }
    auto *dialog = new DownloadProgressDialog(job, m_scheduler, this);
    m_progressWindows.insert(job, dialog);
    connect(job, &QObject::destroyed, this, [this, job] { m_progressWindows.remove(job); });
    dialog->show();
}

void MainWindow::startQueue()
{
    for (auto *job : m_scheduler->jobs()) if (job->isResumable()) m_scheduler->resume(job);
}

void MainWindow::stopAllDownloads()
{
    // Include waiting jobs so a free slot cannot start the next download.
    const QSignalBlocker blocked(m_scheduler);
    const auto jobs = m_scheduler->jobs();
    for (auto *job : jobs) if (job->state() == AbstractJob::Idle) m_scheduler->pause(job);
    for (auto *job : jobs) if (job->isDownloading()) m_scheduler->pause(job);
}

void MainWindow::showDownloadSchedule()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Schedule downloads"));
    auto *form = new QFormLayout(&dialog);
    auto *hint = new QLabel(tr("Start waiting downloads at a chosen time. Keep Dragon running."), &dialog);
    hint->setWordWrap(true);
    form->addRow(hint);
    auto *when = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(300), &dialog);
    when->setCalendarPopup(true);
    when->setDisplayFormat("yyyy-MM-dd HH:mm");
    form->addRow(tr("Start queue at"), when);
    auto *status = new QLabel(&dialog);
    status->setWordWrap(true);
    form->addRow(status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const auto delay = QDateTime::currentDateTime().msecsTo(when->dateTime());
        if (delay <= 0 || delay > 7LL * 24 * 60 * 60 * 1000) {
            status->setText(tr("Choose a future time within the next seven days.")); return;
        }
        auto *timer = findChild<QTimer *>("downloadScheduleTimer");
        if (!timer) {
            timer = new QTimer(this);
            timer->setObjectName("downloadScheduleTimer");
            timer->setSingleShot(true);
            connect(timer, &QTimer::timeout, this, &MainWindow::startQueue);
        }
        timer->start(static_cast<int>(delay));
        statusBar()->showMessage(tr("Queue scheduled for %1").arg(when->dateTime().toString("yyyy-MM-dd HH:mm")), 15000);
        dialog.accept();
    });
    dialog.exec();
}

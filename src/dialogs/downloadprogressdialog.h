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
#include <QtCore/QPointer>
#include <QtWidgets/QDialog>
class AbstractJob;
class Scheduler;
class QLabel;
class QPushButton;
class QProgressBar;
class QTreeWidget;

class DownloadProgressDialog : public QDialog
{
    Q_OBJECT
public:
    DownloadProgressDialog(AbstractJob *job, Scheduler *scheduler, QWidget *parent = nullptr);
private:
    QPointer<AbstractJob> m_job;
    Scheduler *m_scheduler;
    QLabel *m_name;
    QLabel *m_status;
    QLabel *m_size;
    QLabel *m_received;
    QLabel *m_speed;
    QLabel *m_remaining;
    QLabel *m_resume;
    QProgressBar *m_progress;
    QTreeWidget *m_connections;
    QPushButton *m_pause;
    QPushButton *m_open;
    QPushButton *m_folder;
    void refresh();
};

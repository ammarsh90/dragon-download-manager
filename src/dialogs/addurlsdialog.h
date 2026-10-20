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

#ifndef DIALOGS_ADD_URLS_DIALOG_H
#define DIALOGS_ADD_URLS_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtCore/QUrl>

class Scheduler;
class Settings;
class QLineEdit;
class QPushButton;
class QLabel;

class AddUrlsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddUrlsDialog(const QString &text, Scheduler *scheduler, Settings *settings, QWidget *parent = nullptr);
    void accept() override;
    void acceptPaused();
private:
    Scheduler *m_scheduler;
    Settings *m_settings;
    QLineEdit *m_url;
    QLineEdit *m_saveAs;
    QLabel *m_validation;
    QPushButton *m_start;
    QPushButton *m_later;
    bool m_customPath = false;
    QUrl downloadUrl() const;
    void updateFields();
    void browse();
    void submit(bool started);
};
#endif

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

#include "queueviewitemdelegate.h"

#include <Constants>
#include <Core/AbstractJob>
#include <Core/MimeDatabase>
#include <Core/QueueModel>
#include <Widgets/CustomStyle>
#include <Widgets/CustomStyleOptionProgressBar>
#include <Widgets/Globals>

#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>

QueueViewItemDelegate::QueueViewItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
    restylizeUi();
}

void QueueViewItemDelegate::restylizeUi()
{
    m_idleIcon = {};
    m_resumeIcon = {};
    m_pauseIcon = {};
    m_stopIcon = {};
    m_completedIcon = {};

    m_idleIcon.addPixmap(QIcon::fromTheme("queue-idle").pixmap(16), QIcon::Normal, QIcon::On);
    m_resumeIcon.addPixmap(QIcon::fromTheme("queue-play").pixmap(16), QIcon::Normal, QIcon::On);
    m_pauseIcon.addPixmap(QIcon::fromTheme("queue-paused").pixmap(16), QIcon::Normal, QIcon::On);
    m_stopIcon.addPixmap(QIcon::fromTheme("queue-stop").pixmap(16), QIcon::Normal, QIcon::On);
    m_completedIcon.addPixmap(QIcon::fromTheme("queue-completed").pixmap(16), QIcon::Normal, QIcon::On);
}

void QueueViewItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem myOption = option;
    initStyleOption(&myOption, index);

    if (myOption.state & QStyle::State_Selected) {
        myOption.font.setBold(true);
    }

    auto palette = qApp->palette();
    myOption.palette.setColor(QPalette::All, QPalette::Window, palette.color(QPalette::Base));
    myOption.palette.setColor(QPalette::All, QPalette::WindowText, palette.color(QPalette::WindowText));
    myOption.palette.setColor(QPalette::All, QPalette::Highlight, palette.color(QPalette::Highlight));
    myOption.palette.setColor(QPalette::All, QPalette::HighlightedText, palette.color(QPalette::HighlightedText));

    if (index.column() == COL_0_FILE_NAME) {
        const QUrl url(myOption.text);
        auto pixmap = MimeDatabase::fileIcon(url, 16);

        myOption.icon.addPixmap(pixmap);
        myOption.decorationAlignment = Qt::AlignHCenter | Qt::AlignVCenter;
        myOption.decorationPosition = QStyleOptionViewItem::Left;
        myOption.features = myOption.features | QStyleOptionViewItem::HasDecoration;

        QStyledItemDelegate::paint(painter, myOption, index);

    } else if (index.column() == COL_2_PROGRESS_BAR) {
        auto progress = index.data(QueueModel::ProgressRole).toInt();
        auto state = static_cast<AbstractJob::State>(
            index.data(QueueModel::StateRole).toInt());

        myOption.text.clear();
        QStyledItemDelegate::paint(painter, myOption, index);
        painter->save();
        painter->setClipRect(myOption.rect);
        const QRect track = myOption.rect.adjusted(8, 7, -8, -7);
        painter->setPen(palette.color(QPalette::Mid));
        painter->setBrush(palette.color(QPalette::AlternateBase));
        painter->drawRoundedRect(track, 3, 3);
        const double fraction = qBound(0.0, double(progress) / double(MAX_PROGRESS), 1.0);
        if (fraction > 0.0) {
            auto fill = track.adjusted(1, 1, -1, -1);
            fill.setWidth(qRound(fill.width() * fraction));
            painter->setPen(Qt::NoPen);
            painter->setBrush(stateColor(state));
            painter->drawRoundedRect(fill, 2, 2);
        }
        painter->setPen(palette.color(QPalette::Text));
        painter->drawText(track, Qt::AlignCenter, progress >= 0 ? QString::number(progress) + "%" : QString());
        painter->restore();
    } else {
        QStyledItemDelegate::paint(painter, myOption, index);
    }
}

QWidget *QueueViewItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option)
    if (!index.isValid() || index.column() != COL_0_FILE_NAME) {
        return nullptr;
    }
    auto editor = new QLineEdit(parent);
    editor->setAutoFillBackground(true);
    editor->setFocusPolicy(Qt::StrongFocus);
    editor->setContentsMargins(16 + 6, 0, 0, 0);
    return editor;
}

void QueueViewItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    auto lineEdit = qobject_cast<QLineEdit *>(editor);
    if (lineEdit) {
        QString text = index.data(Qt::EditRole).toString();
        lineEdit->setText(text);
    }
}

void QueueViewItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex & /*index*/) const
{
    editor->setGeometry(option.rect);
}

QColor QueueViewItemDelegate::stateColor(AbstractJob::State state) const
{
    switch (state) {
    case AbstractJob::Idle:
        return s_darkGrey;

    case AbstractJob::Paused:
        return QColor("#8d9aa7");

    case AbstractJob::Preparing:
    case AbstractJob::Connecting:
    case AbstractJob::DownloadingMetadata:
    case AbstractJob::Downloading:
    case AbstractJob::Endgame:
        return QColor("#4c8dc4");

    case AbstractJob::Completed:
    case AbstractJob::Seeding:
        return QColor("#588b79");

    case AbstractJob::Stopped:
    case AbstractJob::Skipped:
    case AbstractJob::NetworkError:
    case AbstractJob::FileError:
        return s_darkRed;

    default:
        Q_UNREACHABLE();
        break;
    }
    return Qt::black;
}

QIcon QueueViewItemDelegate::stateIcon(AbstractJob::State state) const
{
    switch (state) {
    case AbstractJob::Idle:
        return m_idleIcon;

    case AbstractJob::Paused:
        return m_pauseIcon;

    case AbstractJob::Preparing:
    case AbstractJob::Connecting:
    case AbstractJob::DownloadingMetadata:
    case AbstractJob::Downloading:
    case AbstractJob::Endgame:
        return m_resumeIcon;

    case AbstractJob::Completed:
    case AbstractJob::Seeding:
        return m_completedIcon;

    case AbstractJob::Stopped:
    case AbstractJob::Skipped:
    case AbstractJob::NetworkError:
    case AbstractJob::FileError:
        return m_stopIcon;

    default:
        Q_UNREACHABLE();
        break;
    }
    return {};
}

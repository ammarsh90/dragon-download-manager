/* - ArrowDL - Copyright (C) 2019-present Sebastien Vavassori
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License (LGPL)
 * Version 3, 29 June 2007, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef ABOUT_H
#define ABOUT_H

#include "version.h"

#include <QtCore/QObject>
#include <QtCore/QString>

static QString buildAbout(const QString &paragraphDelimiter)
{
    return QString(QObject::tr("%0 - %1 - version %2 - build %3")
                   .arg(STR_APPLICATION_NAME, STR_COMPILER_WORDSIZE,
                        STR_APPLICATION_VERSION, STR_APPLICATION_BUILD) +
                   paragraphDelimiter +
                   QObject::tr("Copyright (C) %0 %1.")
                   .arg(STR_APPLICATION_DATE, STR_APPLICATION_AUTHOR) +
                   paragraphDelimiter +
                   QObject::tr("Branding and theme by Brami Group.") +
                   paragraphDelimiter +
                   QObject::tr("Based on ArrowDL by Sébastien Vavassori; original project copyrights are retained.") +
                   paragraphDelimiter +
                   QObject::tr("Licensed under the GNU Lesser General Public License, version 3. See LICENSE.md for terms."));
}

inline QString about()
{
    return buildAbout("\n\n");
}

inline QString aboutHtml()
{
    return QString("<h3>" + QObject::tr("About %0").arg(STR_APPLICATION_NAME) + "</h3>" +
                   "<p><img src=\":/resources/logo/icon128.png\" /></p>" +
                   "<p>" + buildAbout("</p><p>") + "</p>" +
                   "<p><a href=\"https://github.com/setvisible/ArrowDL\">" +
                   QObject::tr("Original ArrowDL project") + "</a></p>");
}

#endif // ABOUT_H

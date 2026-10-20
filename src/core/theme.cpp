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

#include "theme.h"

#include <Constants>
#include <Widgets/CustomStyle>

#include <QtCore/QDebug>
#include <QtCore/QString>
#include <QtCore/QObject>
#include <QtGui/QAction>
#include <QtGui/QColor>
#include <QtGui/QPalette>
#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleFactory>


/* Public static constants */
const QLatin1StringView Theme::PlatformStyle("platformStyle");
const QLatin1StringView Theme::IconTheme("iconTheme");
const QLatin1StringView Theme::ColorScheme("colorScheme");

/* Private static constants */
static QString s_themeSearchPath = QString::fromUtf8(":/resources/icons/");

/* Runtime Debug Purpose */
static void _assertNoMissingIconTheme(const QWidget *widget)
{
    auto widgetName = widget->windowTitle().toLatin1().data();
    QList<QAbstractButton*> buttons = widget->findChildren<QAbstractButton*>();
    for (auto *button : buttons) {
        if ( button->objectName() == QLatin1String("qt_menubar_ext_button") ||
             button->objectName() == QLatin1String("qt_toolbar_ext_button")) {
            continue;
        }
        if (!button->icon().isNull() && button->icon().name().isEmpty()) {
            qWarning("Missing icon theme name for QAbstractButton '%s' ('%s') in QWidget '%s'.",
                     button->objectName().toLatin1().data(),
                     button->text().toLatin1().data(),
                     widgetName);
        }
    }
    QList<QAction*> actions = widget->findChildren<QAction*>();
    for (auto *action : actions) {
        if (!action->icon().isNull() && action->icon().name().isEmpty()) {
            qWarning("Missing icon theme name for QAction '%s' ('%s') in QWidget '%s'.",
                     action->objectName().toLatin1().data(),
                     action->text().toLatin1().data(),
                     widgetName);
        }
    }
}

Theme::Theme()
{
}

/******************************************************************************
 ******************************************************************************/
QStringList Theme::availablePlatformStyles()
{
    auto keys = QStyleFactory::keys();
    auto availables = QStringList();
    auto style_fusion = QLatin1String("Fusion");
    for (auto key : keys) {
        if (key.contains(style_fusion, Qt::CaseInsensitive)) {
            availables << key;
            return availables;
        }
    }
    return keys;
}

QString Theme::toPlatformStyle(qsizetype index)
{
    auto keys = Theme::availablePlatformStyles();
    if (index >= 0 && index < keys.count()) {
        return keys.at(index);
    }
    return QLatin1String(""); // Must be an empty string, not a null QString
}

qsizetype Theme::fromPlatformStyle(const QString &platformStyle)
{
    auto index = Theme::availablePlatformStyles().indexOf(platformStyle);
    return index == -1 ? 0 : index;
}

/******************************************************************************
 ******************************************************************************/
QStringList Theme::availableIconThemes()
{
    return {
        QObject::tr("Bootstrap Icons (default)"),
        QObject::tr("FontAwesome Flat Design")
    };
}

QString Theme::toIconTheme(int index)
{
    switch (index) {
    case 0: return THEME_DEFAULT;
    case 1: return THEME_FLAT;
    default: break;
    }
    return QLatin1String(""); // Must be an empty string, not a null QString
}

int Theme::fromIconTheme(const QString &iconTheme)
{
    if (iconTheme.compare(THEME_DEFAULT, Qt::CaseInsensitive) == 0) {
        return 0;
    } else if (iconTheme.compare(THEME_FLAT, Qt::CaseInsensitive) == 0) {
        return 1;
    }
    return 0;
}

/******************************************************************************
 ******************************************************************************/
QStringList Theme::availableColorSchemes()
{
    return {
        QObject::tr("Light"),
        QObject::tr("Dark")
    };
}

QString Theme::toColorScheme(int index)
{
    switch (index) {
    case 0: return SCHEME_LIGHT;
    case 1: return SCHEME_DARK;
    default: break;
    }
    return QLatin1String(""); // Must be an empty string, not a null QString
}

int Theme::fromColorScheme(const QString &colorScheme)
{
    if (colorScheme.compare(SCHEME_LIGHT, Qt::CaseInsensitive) == 0) {
        return 0;
    } else if (colorScheme.compare(SCHEME_DARK, Qt::CaseInsensitive) == 0) {
        return 1;
    }
    return 0;
}

/******************************************************************************
 ******************************************************************************/
/*!
 * Modify palette to dark.
 *
 * Recommandation:
 * Google Material Design Guidelines.
 * https://design.google/library/material-design-dark-theme
 *
 * Inspiration:
 * https://github.com/Jorgen-VikingGod/Qt-Frameless-Window-DarkStyle/blob/master/DarkStyle.cpp
 */
static void polishDark(QPalette &palette)
{
    // --
    palette.setColor(QPalette::WindowText, QColor(232, 238, 247));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(139, 151, 169));
    palette.setColor(QPalette::Button, QColor(27, 40, 59));
    palette.setColor(QPalette::Light, QColor(49, 68, 95));
    palette.setColor(QPalette::Midlight, QColor(37, 53, 75));
    palette.setColor(QPalette::Dark, QColor(9, 16, 27));
    palette.setColor(QPalette::Mid, QColor(19, 32, 51));
    // --
    palette.setColor(QPalette::Text, QColor(232, 238, 247));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(139, 151, 169));
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::ButtonText, QColor(240, 244, 250));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(139, 151, 169));
    palette.setColor(QPalette::Base, QColor(16, 24, 39));
    palette.setColor(QPalette::Window, QColor(17, 27, 42));
    palette.setColor(QPalette::Shadow, QColor(5, 9, 15));
    // --
    palette.setColor(QPalette::Highlight, QColor(218, 166, 54));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(90, 99, 112));
    palette.setColor(QPalette::HighlightedText, QColor(17, 27, 42));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(165, 174, 187));
    // --
    palette.setColor(QPalette::Link, QColor(103, 172, 245));
    palette.setColor(QPalette::LinkVisited, QColor(185, 153, 228));
    // --
    palette.setColor(QPalette::AlternateBase, QColor(23, 34, 54));
    // --
    palette.setColor(QPalette::ToolTipBase, QColor(240, 244, 250));
    palette.setColor(QPalette::ToolTipText, QColor(17, 27, 42));
    // --
    palette.setColor(QPalette::PlaceholderText, QColor(139, 151, 169));
    // --
}

/******************************************************************************
 ******************************************************************************/
void Theme::applyTheme(const QMap<QString, QVariant> &map)
{
    if (!QIcon::themeSearchPaths().contains(s_themeSearchPath)) {
        QIcon::setThemeSearchPaths(QIcon::themeSearchPaths() << s_themeSearchPath);
    }
    if (QIcon::fallbackThemeName().isEmpty()) {
        QIcon::setFallbackThemeName(QLatin1String("default"));
    }

    const QString platformStyle = map.value(Theme::PlatformStyle, QLatin1String()).toString();
    const QString iconTheme = map.value(Theme::IconTheme, QLatin1String()).toString();
    const QString colorScheme = map.value(Theme::ColorScheme, QLatin1String()).toString();

    auto index = Theme::fromIconTheme(iconTheme);
    auto isDarkMode = (Theme::fromColorScheme(colorScheme) == 1);

    if (index == 1)  {
        QIcon::setThemeName(
            isDarkMode
                ? QLatin1String("flat-dark")
                : QLatin1String("flat")
            );
    } else {
        QIcon::setThemeName(
            isDarkMode
                ? QLatin1String("default-dark")
                : QLatin1String("default")
            );
    }
    auto palette = qApp->style()->standardPalette();
    if (isDarkMode) {
        polishDark(palette);
    }
    qApp->setPalette(palette); // emits QEvent::PaletteChange
    qApp->setStyle(new CustomStyle(platformStyle)); // emits QEvent::StyleChange
}

/******************************************************************************
 ******************************************************************************/
/*!
 * QtDesigner doesn't icon themes correctly, this is a workaround
 */
void Theme::setIcons(const QWidget *widget, const QHash<QAbstractButton *, QString> &hash)
{
    QHashIterator<QAbstractButton *, QString> it(hash);
    while (it.hasNext()) {
        it.next();
        auto qbutton = it.key();
        auto name = it.value();
        qbutton->setIcon(QIcon::fromTheme(name));
    }
    _assertNoMissingIconTheme(widget);
}

void Theme::setIcons(const QWidget *widget, const QHash<QAction *, QString> &hash)
{
    QHashIterator<QAction*, QString> it(hash);
    while (it.hasNext()) {
        it.next();
        auto qaction = it.key();
        auto name = it.value();
        qaction->setIcon(QIcon::fromTheme(name));
    }
    _assertNoMissingIconTheme(widget);
}

void Theme::setIcons(const QWidget *widget, const QHash<QLabel *, QString> &hash, int extent)
{
    QHashIterator<QLabel *, QString> it(hash);
    while (it.hasNext()) {
        it.next();
        auto qlabel = it.key();
        auto name = it.value();
        qlabel->setPixmap(QIcon::fromTheme(name).pixmap(extent));
    }
    _assertNoMissingIconTheme(widget);
}

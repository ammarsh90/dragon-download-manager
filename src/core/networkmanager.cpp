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

#include "networkmanager.h"
#include <QtNetwork/QNetworkCookieJar>
#include <QtNetwork/QNetworkCookie>
#include <QtCore/QJsonObject>
#include <QtCore/QDateTime>

#include <Constants>
#include <Core/Settings>

#include <QtCore/QDebug>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkProxy>



NetworkManager::NetworkManager(QObject *parent) : QObject(parent)
  , m_networkAccessManager(new QNetworkAccessManager(this))
{
}

/******************************************************************************
 ******************************************************************************/
Settings *NetworkManager::settings() const
{
    return m_settings;
}

void NetworkManager::setSettings(Settings *settings)
{
    if (m_settings) {
        disconnect(m_settings, SIGNAL(changed()), this, SLOT(onSettingsChanged()));
    }
    m_settings = settings;
    if (m_settings) {
        connect(m_settings, SIGNAL(changed()), this, SLOT(onSettingsChanged()));
        onSettingsChanged();
    }
}

void NetworkManager::onSettingsChanged()
{
    setNetworkSettings(m_settings);
}

/******************************************************************************
 ******************************************************************************/
QStringList NetworkManager::proxyTypeNames()
{
    return QStringList(
    { tr("(none)"),
      tr("SOCKS5"),
      tr("HTTP") });
}

static QNetworkProxy::ProxyType toProxyType(int index)
{
    switch (index) {
    case 0: return QNetworkProxy::NoProxy;
    case 1: return QNetworkProxy::Socks5Proxy;
    case 2: return QNetworkProxy::HttpProxy;
    default:
        break;
    }
    return QNetworkProxy::NoProxy;
}

void NetworkManager::setNetworkSettings(Settings *settings)
{
    Q_ASSERT(m_networkAccessManager);
    if (!settings) {
        return;
    }
    // Proxy options
    QNetworkProxy::ProxyType type = toProxyType(settings->proxyType());
    if (type == QNetworkProxy::NoProxy) {
        QNetworkProxy::setApplicationProxy(type);
        m_networkAccessManager->setProxy(type);
    } else {
        QNetworkProxy proxy;
        proxy.setType(type);
        proxy.setHostName(settings->proxyHostName());
        proxy.setPort(static_cast<quint16>(settings->proxyPort()));
        proxy.setUser(settings->proxyUser());
        proxy.setPassword(settings->proxyPassword());
        QNetworkProxy::setApplicationProxy(proxy);
        m_networkAccessManager->setProxy(proxy);
    }
    // Socket options
    auto timeout_msec = settings->connectionTimeout() * 1000;
    m_networkAccessManager->setTransferTimeout(timeout_msec);
}

/******************************************************************************
 ******************************************************************************/
QNetworkReply* NetworkManager::get(const QUrl &url, const QString &referer, qint64 rangeStart, const QByteArray &validator, qint64 rangeEnd)
{
    Q_ASSERT(m_networkAccessManager);

    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("Accept-Encoding", "identity");
    request.setTransferTimeout(30000);
    if (rangeStart > 0 || rangeEnd >= 0) {
        request.setRawHeader("Range", "bytes=" + QByteArray::number(rangeStart) + "-"
                             + (rangeEnd >= 0 ? QByteArray::number(rangeEnd) : QByteArray()));
        if (!validator.isEmpty()) request.setRawHeader("If-Range", validator);
    }

    // User-Agent
    auto httpUserAgent = m_settings ? m_settings->httpUserAgent() : QLatin1String("");
    request.setHeader(QNetworkRequest::UserAgentHeader, m_browserUserAgent.isEmpty() ? httpUserAgent : m_browserUserAgent);

    // Referer
    if (!referer.isEmpty()) {
        auto rawReferer = referer.toUtf8();
        request.setRawHeader(QByteArray("Referer"), rawReferer);
    }

    // SSL
    request.setSslConfiguration(QSslConfiguration::defaultConfiguration()); // HTTPS
    request.setMaximumRedirectsAllowed(MAX_REDIRECTS_ALLOWED);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    auto reply = m_networkAccessManager->get(request);

    Q_ASSERT(reply);
    connect(reply, SIGNAL(metaDataChanged()), this, SLOT(onMetaDataChanged()));
    connect(reply, SIGNAL(redirected(QUrl)), this, SLOT(onRedirected(QUrl)));

    return reply;
}

/******************************************************************************
 ******************************************************************************/
void NetworkManager::onMetaDataChanged()
{
}

void NetworkManager::onRedirected(const QUrl &/*url*/)
{
}

void NetworkManager::setBrowserContext(const QUrl &url, const QString &userAgent, const QJsonArray &cookies)
{
    if (userAgent.size() <= 1024 && !userAgent.contains('\r') && !userAgent.contains('\n')) m_browserUserAgent = userAgent;
    QList<QNetworkCookie> accepted;
    for (const auto &value : cookies) {
        if (accepted.size() >= 200) break;
        const auto item = value.toObject();
        const QString domain = item["domain"].toString().toLower();
        QString bareDomain = domain;
        if (bareDomain.startsWith('.')) bareDomain.remove(0, 1);
        if (bareDomain.isEmpty() || (url.host().toLower() != bareDomain && !url.host().toLower().endsWith("." + bareDomain))) continue;
        const auto name = item["name"].toString().toUtf8();
        const auto contents = item["value"].toString().toUtf8();
        if (name.isEmpty() || name.contains('\r') || name.contains('\n') || contents.contains('\r') || contents.contains('\n')) continue;
        QNetworkCookie cookie(name, contents);
        if (!item["hostOnly"].toBool()) cookie.setDomain(domain);
        const auto path = item["path"].toString();
        cookie.setPath(path.startsWith('/') ? path : QStringLiteral("/"));
        cookie.setSecure(item["secure"].toBool());
        cookie.setHttpOnly(item["httpOnly"].toBool());
        const double expiration = item["expirationDate"].toDouble();
        if (!item["session"].toBool(true) && expiration > 0 && expiration < 253402300800.0)
            cookie.setExpirationDate(QDateTime::fromSecsSinceEpoch(qint64(expiration), Qt::UTC));
        accepted.append(cookie);
    }
    m_networkAccessManager->cookieJar()->setCookiesFromUrl(accepted, url);
}

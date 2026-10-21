// Dragon Download Manager — Brami Group. Distributed under LGPL v3.
#pragma once
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>
namespace DragonBrowser {
inline QString serverName() {
    const auto user = QStandardPaths::writableLocation(QStandardPaths::HomeLocation).toUtf8();
    return "DragonDownloadManager.Browser." + QString::fromLatin1(QCryptographicHash::hash(user, QCryptographicHash::Sha256).toHex().left(24));
}
constexpr int MaximumMessageSize = 512 * 1024;
}

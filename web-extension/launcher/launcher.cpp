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

// Dragon Download Manager — Brami Group. Distributed under LGPL v3.
#include "../../src/ipc/browserprotocol.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QProcess>
#include <QtCore/QThread>
#include <QtNetwork/QLocalSocket>
#include <iostream>
#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#include <qt_windows.h>
#endif

static void reply(const QJsonObject &message) {
    const auto bytes = QJsonDocument(message).toJson(QJsonDocument::Compact);
    const quint32 size = bytes.size();
    const char prefix[4]{char(size), char(size >> 8), char(size >> 16), char(size >> 24)};
    std::cout.write(prefix, 4);
    std::cout.write(bytes.constData(), bytes.size());
    std::cout.flush();
}
static bool launchDragon(const QString &program, const QStringList &arguments = {}) {
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setWorkingDirectory(QCoreApplication::applicationDirPath());
    process.setStandardInputFile(QProcess::nullDevice());
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_BREAKAWAY_FROM_JOB;
    });
#endif
    return process.startDetached();
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
#ifdef Q_OS_WIN
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    char prefix[4];
    if (!std::cin.read(prefix, 4)) return 1;
    quint32 size = 0;
    for (int i = 0; i < 4; ++i) size |= quint32(static_cast<unsigned char>(prefix[i])) << (8 * i);
    if (!size || size > DragonBrowser::MaximumMessageSize) { reply({{"accepted",false},{"error","Message too large"}}); return 1; }
    QByteArray bytes(size, Qt::Uninitialized);
    if (!std::cin.read(bytes.data(), size)) return 1;
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(bytes, &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) { reply({{"accepted",false},{"error","Invalid message"}}); return 1; }
    const auto action = doc.object().value("action").toString();
#ifdef Q_OS_WIN
    const auto program = QCoreApplication::applicationDirPath() + "/DragonDownloadManager.exe";
#else
    const auto program = QCoreApplication::applicationDirPath() + "/DragonDownloadManager";
#endif
    if (!QFileInfo::exists(program)) { reply({{"accepted",false},{"error","Dragon Download Manager is missing"}}); return 1; }
    if (action == "ping") { reply({{"accepted",true},{"name","Dragon Download Manager"},{"protocol",1}}); return 0; }
    if (action == "open") {
        const bool started = launchDragon(program, {"[MANAGER]"});
        reply({{"accepted", started}}); return started ? 0 : 1;
    }
    if (action != "download" && action != "collect") { reply({{"accepted",false},{"error","Unknown action"}}); return 1; }
    QLocalSocket socket;
    socket.connectToServer(DragonBrowser::serverName());
    if (!socket.waitForConnected(250)) {
        socket.abort();
        if (!launchDragon(program)) {
            reply({{"accepted",false},{"error","Cannot start Dragon"}}); return 1;
        }
        QElapsedTimer timer; timer.start();
        while (timer.elapsed() < 15000) {
            socket.connectToServer(DragonBrowser::serverName());
            if (socket.waitForConnected(250)) break;
            socket.abort(); QThread::msleep(100);
        }
    }
    if (socket.state() != QLocalSocket::ConnectedState) { reply({{"accepted",false},{"error","Dragon did not respond"}}); return 1; }
    bytes.append('\n');
    socket.write(bytes);
    if (socket.bytesToWrite() > 0 && !socket.waitForBytesWritten(5000)) { reply({{"accepted",false},{"error","Cannot send download"}}); return 1; }
    QByteArray response;
    QElapsedTimer timer; timer.start();
    while (timer.elapsed() < 20000 && response.size() <= DragonBrowser::MaximumMessageSize) {
        if (!socket.bytesAvailable()) socket.waitForReadyRead(1000);
        response += socket.readAll();
        if (response.contains('\n')) {
            const auto result = QJsonDocument::fromJson(response.left(response.indexOf('\n')));
            if (result.isObject()) { reply(result.object()); return 0; }
            break;
        }
        if (socket.state() == QLocalSocket::UnconnectedState) break;
    }
    reply({{"accepted",false},{"error","Download handoff was not confirmed"}});
    return 1;
}

// Dragon Download Manager — Brami Group. Distributed under LGPL v3.
#pragma once
#include <QtCore/QObject>
#include <QtCore/QJsonObject>
class QLocalServer;
class Scheduler;
class Settings;
class JobFile;
class BrowserBridge : public QObject {
    Q_OBJECT
public:
    BrowserBridge(Scheduler *scheduler, Settings *settings, QObject *parent = nullptr);
signals:
    void downloadReceived(JobFile *job, bool showDialog);
    void collectionReceived(QString text);
private:
    QLocalServer *m_server;
    Scheduler *m_scheduler;
    Settings *m_settings;
    QJsonObject acceptDownload(const QJsonObject &message);
};

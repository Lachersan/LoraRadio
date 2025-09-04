#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QLocalSocket>
#include "../include/AbstractPlayer.h"


class AbstractPlayer; // forward (assume exists)

class YTPlayer : public AbstractPlayer {
    Q_OBJECT
public:
    explicit YTPlayer(const QString& cookiesFile = QString(), QObject* parent = nullptr);
    ~YTPlayer() override;

    // control API
    void play(const QString& url);
    void start();
    void stop();
    void togglePlayback();
    bool sendQuitAndWait(int waitMs = 1500);
    void gracefulShutdownMpv(int totalWaitMs = 2500);

    void setVolume(int value);
    int volume() const;

    void setMuted(bool muted);
    bool isMuted() const;

    signals:
    void playbackStateChanged(bool playing);
    void volumeChanged(int volume);
    void mutedChanged(bool muted);
    void errorOccurred(const QString& message);

private slots:
    // yt-dlp
    void onYtdlpReadyRead();
    void onYtdlpFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onYtdlpTimeout();

    // mpv process / IPC
    void onMpvProcessReadyRead();
    void onMpvProcessError(QProcess::ProcessError err);
    void onIpcConnected();
    void onIpcDisconnected();
    void onIpcReadyRead();
    void onIpcError(QLocalSocket::LocalSocketError);

private:
    // helpers
    void startMpvProcess();
    void connectIpc();
    void sendIpcCommand(const QJsonArray& cmd); // sends {"command": [...]} over IPC
    void writeLogFile(const QString& name, const QString& contents);
    void tryReconnect();
    // members
    QProcess* mpvProcess = nullptr;
    QProcess* ytdlpProcess = nullptr;
    QTimer* ytdlpTimer = nullptr;
    QTimer* reconnectTimer = nullptr;
    QLocalSocket* ipcSocket = nullptr;
    bool ytdlpTriedManifest = false;
    int ipcRetryAttempts = 0;
    const int ipcMaxRetries = 5;
    qint64 m_mpvPid = 0;

    QByteArray ytdlpStdoutBuffer;
    QString cookiesFile;
    QString pendingNormalizedUrl;
    bool pendingAllowPlaylist = false;

    QString ipcName = QStringLiteral("ytplayer_ipc"); // can be changed
    bool playing = false;
    int currentVolume = 50;
    bool mutedState = false;
    bool m_shuttingDown = false;
    bool m_isRunning = false;
};

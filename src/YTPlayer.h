#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QLocalSocket>
#include "../include/AbstractPlayer.h"
#include <vlc/vlc.h>  // For libVLC types and functions

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
    void gracefulShutdownMpv(int totalWaitMs = 2500);  // Keep for compatibility, but may not be used

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
    void onYtdlpReadyReadError();

    // libVLC events (no slots needed, use static callbacks)

private:
    // libVLC members
    libvlc_instance_t *m_instance = nullptr;
    libvlc_media_player_t *m_player = nullptr;
    libvlc_media_t* m_currentMedia = nullptr;

    // Static libVLC event callbacks
    static void onMediaEndReached(const libvlc_event_t *event, void *user_data);
    static void onMediaError(const libvlc_event_t *event, void *user_data);

    // helpers (MPV-related commented out for VLC replacement)
    // void startMpvProcess();
    // void connectIpc();
    // void sendIpcCommand(const QJsonArray& cmd);
    void writeLogFile(const QString& name, const QString& contents);
    // void tryReconnect();

    // members (MPV-related commented out; keep yt-dlp)
    // QProcess* mpvProcess = nullptr;
    QProcess* ytdlpProcess = nullptr;
    QTimer* ytdlpTimer = nullptr;
    // QTimer* reconnectTimer = nullptr;
    // QLocalSocket* ipcSocket = nullptr;
    bool ytdlpTriedManifest = false;
    // int ipcRetryAttempts = 0;
    // const int ipcMaxRetries = 5;
    // qint64 m_mpvPid = 0;

    QByteArray ytdlpStdoutBuffer;
    QString cookiesFile;
    QString pendingNormalizedUrl;
    bool pendingAllowPlaylist = false;

    // QString ipcName = QStringLiteral("ytplayer_ipc"); // can be changed
    bool playing = false;
    int currentVolume = 50;
    bool mutedState = false;
    // bool m_shuttingDown = false;
    bool m_isRunning = false;
};
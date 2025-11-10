#pragma once

#include <vlc/vlc.h>  // For libVLC types and functions

#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QTimer>

#include "../include/AbstractPlayer.h"

class AbstractPlayer;  // forward (assume exists)

class YTPlayer : public AbstractPlayer
{
    Q_OBJECT
   public:
    explicit YTPlayer(QString cookiesFile = QString(), QObject* parent = nullptr);
    ~YTPlayer() override;

    // control API
    void play(const QString& url);
    void start();
    void stop();
    void togglePlayback();

    bool supportsFeature(const QString& feature) const override;
    void setCookiesFile(const QString& path) override;
    QString cookiesFile() const override;
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
    void onYtdlpReadyReadError() const;

    // libVLC events (no slots needed, use static callbacks)

   private:
    // libVLC members
    libvlc_instance_t* m_instance = nullptr;
    libvlc_media_player_t* m_player = nullptr;
    libvlc_media_t* m_currentMedia = nullptr;

    // Static libVLC event callbacks
    static void onMediaEndReached(const libvlc_event_t* event, void* user_data);
    static void onMediaError(const libvlc_event_t* event, void* user_data);

    void writeLogFile(const QString& name, const QString& contents);

    QProcess* ytdlpProcess = nullptr;
    QTimer* ytdlpTimer = nullptr;
    bool ytdlpTriedManifest = false;

    QByteArray ytdlpStdoutBuffer;
    QString m_cookiesFile;
    QString pendingNormalizedUrl;
    bool pendingAllowPlaylist = false;

    bool playing = false;
    int currentVolume = 50;
    bool mutedState = false;
    bool m_isRunning = false;
};
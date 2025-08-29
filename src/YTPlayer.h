#pragma once
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include <QRegularExpression>
#include "../include/AbstractPlayer.h"
#include <../include/mpv/client.h>

class YTPlayer : public AbstractPlayer
{
    Q_OBJECT
public:
    explicit YTPlayer(const QString& cookiesFile = QString(), QObject* parent = nullptr);
    ~YTPlayer() override;

public slots:
    void play(const QString& url) override;
    void stop() override;
    void togglePlayback() override;
    void setVolume(int value) override;
    int volume() const override;
    void setMuted(bool muted) override;
    bool isMuted() const override;

    signals:
        // Сигналы берутся из AbstractPlayer: playbackStateChanged(bool), volumeChanged(int), errorOccurred(QString), mutedChanged(bool)

    private slots:
    void handleMpvEvents();
    void onYtdlpReadyRead();
    void onYtdlpFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onYtdlpTimeout();

private:
    void processMpvEvents();
    //static void on_mpv_wakeup(void* ctx);

    mpv_handle* mpv = nullptr;
    QString cookiesFile;
    int currentVolume = 50;
    bool playing = false;
    bool mutedState = false;

    // yt-dlp process (асинхронно)
    QProcess* ytdlpProcess = nullptr;
    QByteArray ytdlpStdoutBuffer;
    QTimer* ytdlpTimer = nullptr;

    // временное хранение текущего запроса
    QString pendingNormalizedUrl;
    bool pendingAllowPlaylist = false;
};
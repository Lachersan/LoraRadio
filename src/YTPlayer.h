#pragma once

#include <QAudioFormat>
#include <QAudioSink>
#include <QMediaDevices>
#include <QObject>
#include <QProcess>
#include <QTimer>

#include "../include/AbstractPlayer.h"

class YTPlayer : public AbstractPlayer
{
    Q_OBJECT
   public:
    explicit YTPlayer(QString cookiesFile = QString(), QObject* parent = nullptr);
    ~YTPlayer() override;

    void play(const QString& url) override;
    void stop() override;
    void togglePlayback() override;
    void start();

    void setVolume(int value) override;
    int volume() const override;
    void setMuted(bool muted) override;
    bool isMuted() const override;

    bool supportsFeature(const QString& feature) const override;
    void setCookiesFile(const QString& path) override;
    QString cookiesFile() const override;

   signals:
    void playbackStateChanged(bool playing);
    void volumeChanged(int volume);
    void mutedChanged(bool muted);
    void errorOccurred(const QString& message);

   private slots:
    void onYtdlpReadyRead();
    void onYtdlpFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onYtdlpTimeout();
    void onYtdlpReadyReadError() const;

    void onFfmpegReadyRead();
    void onFfmpegFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onFfmpegError(QProcess::ProcessError error);

   private:
    QProcess* ytdlpProcess = nullptr;
    QTimer* ytdlpTimer = nullptr;
    QByteArray ytdlpStdoutBuffer;
    bool ytdlpTriedManifest = false;

    QProcess* ffmpegProcess = nullptr;
    QAudioSink* m_audioSink = nullptr;
    QIODevice* m_sinkDevice = nullptr;
    QByteArray m_buffer;

    QString m_cookiesFile;
    QString pendingNormalizedUrl;
    bool pendingAllowPlaylist = false;

    bool m_playing = false;
    bool m_stopping = false;
    int m_volume = 50;
    bool m_muted = false;
    bool m_isRunning = false;
};
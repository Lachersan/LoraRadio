#pragma once

#include <QAudioDevice>
#include <QAudioSink>
#include <QPointer>
#include <QProcess>
#include <QTimer>

#include "../include/AbstractPlayer.h"
class YTPlayer : public AbstractPlayer
{
    Q_OBJECT
   public:
    explicit YTPlayer(QString cookiesFile = {}, QObject* parent = nullptr);
    ~YTPlayer() override;

    void play(const QString& url) override;
    void stop() override;
    void togglePlayback() override;
    void setVolume(int value) override;
    [[nodiscard]] int volume() const override;
    void setMuted(bool muted) override;
    [[nodiscard]] bool isMuted() const override;

    void start();
    [[nodiscard]] bool supportsFeature(const QString& feature) const override;
    void setCookiesFile(const QString& path) override;
    [[nodiscard]] QString cookiesFile() const override;

   signals:
    void errorOccurred(const QString& message);
    void playbackStateChanged(bool playing);
    void volumeChanged(int volume);
    void mutedChanged(bool muted);
    void featureChanged(const QString& feature, bool enabled);

   private slots:
    // yt-dlp
    void onYtdlpStarted();
    void onYtdlpReadyRead();
    void onYtdlpReadyReadError();
    void onYtdlpTimeout();
    void onYtdlpFinished(int exitCode, QProcess::ExitStatus status);
    void onYtdlpError(QProcess::ProcessError error);
    // ffmpeg
    void onFfmpegStarted();
    void onFfmpegReadyRead();
    void onFfmpegFinished(int exitCode, QProcess::ExitStatus status);
    void onFfmpegError(QProcess::ProcessError error);

   private:
    void writeLog(const QString& name, const QString& content, bool truncate = false);
    void startFfmpeg(const QString& streamUrl);
    void tryReconnect();
    QString normalizeUrl(const QString& url);
    // Processes
    QProcess* ytdlpProcess = nullptr;
    QProcess* ffmpegProcess = nullptr;
    QTimer* ytdlpTimer = nullptr;
    // Audio
    QAudioSink* m_audioSink = nullptr;
    QPointer<QIODevice> m_sinkDevice;  // QPointer для безопасности
    QByteArray m_buffer;
    // State
    QString m_cookiesFile;
    QString pendingNormalizedUrl;
    bool pendingAllowPlaylist = false;
    QByteArray ytdlpStdoutBuffer;
    bool ytdlpTriedManifest = false;
    int m_volume = 50;
    bool m_muted = false;
    bool m_playing = false;
    bool m_stopping = false;
    bool m_isRunning = false;

    // Reconnect
    int m_reconnectAttempts = 0;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 5;
    static constexpr int RECONNECT_BASE_DELAY_MS = 3000;

    // Buffer limits
    static constexpr qint64 BUFFER_START_THRESHOLD = 48000 * 2 * 2 * 3;  // 576 000 байт
    static constexpr qint64 BUFFER_MAX_SIZE = 48000 * 2 * 2 * 30;        // 5 760 000 байт
};
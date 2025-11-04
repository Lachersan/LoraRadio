#include "YTPlayer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QRegularExpression>
#include <QJsonValue>
#include <QDebug>
#include <QThread>
#include <vlc/vlc.h>

// Helper: write small logs to exe/logs
static void writeLog(const QString &name, const QString &content) {
    QString exeDir = QCoreApplication::applicationDirPath();
    QString logDir = exeDir + QDir::separator() + "logs";
    QDir().mkpath(logDir);
    QFile f(logDir + QDir::separator() + name);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(content.toUtf8());
}

YTPlayer::YTPlayer(const QString& cookiesFile_, QObject* parent)
    : AbstractPlayer(parent)
    , m_cookiesFile(cookiesFile_)
    , m_isRunning(false)
{
    qDebug() << "[YTPlayer] ctor START";

    // Initialize libVLC instance with audio-only options
    const char *vlc_args[] = {
        "--no-video",          // Audio only
        "--intf=dummy",        // No interface
        "--ipv4-timeout=5000",
        "--network-caching=1000",
        "--http-reconnect",
        "--aout=directsound",
        "--plugin-path=./plugins",
        "--lua-intf=luaintf",
        "--verbose=2",         // Detailed logs
        "--file-logging",      // Enable file logging
        "--logfile=logs/vlc_log.txt"  // Log to <exe_dir>/logs/vlc_log.txt (create logs dir if needed)
    };
    m_instance = libvlc_new(sizeof(vlc_args) / sizeof(vlc_args[0]), vlc_args);
    if (!m_instance) {
        qWarning() << "[YTPlayer] Failed to create libVLC instance. LibVLC error:" << libvlc_errmsg();
        emit errorOccurred("Failed to initialize libVLC");
        return;
    }
    qDebug() << "[YTPlayer] libVLC initialized successfully.";

    m_player = libvlc_media_player_new(m_instance);
    if (!m_player) {
        qWarning() << "[YTPlayer] Failed to create libVLC media player";
        emit errorOccurred("Failed to create libVLC player");
        libvlc_release(m_instance);
        m_instance = nullptr;
        return;
    }

    // Optional: Attach event manager for playback events (e.g., end reached, error)
    libvlc_event_manager_t *event_manager = libvlc_media_player_event_manager(m_player);
    libvlc_event_attach(event_manager, libvlc_MediaPlayerEndReached, onMediaEndReached, this);
    libvlc_event_attach(event_manager, libvlc_MediaPlayerEncounteredError, onMediaError, this);

    ytdlpProcess = new QProcess(this);
    ytdlpTimer = new QTimer(this);
    ytdlpTimer->setSingleShot(true);

    // signals for yt-dlp
    connect(ytdlpProcess, &QProcess::readyReadStandardOutput, this, &YTPlayer::onYtdlpReadyRead);
    connect(ytdlpProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &YTPlayer::onYtdlpFinished);
    connect(ytdlpTimer, &QTimer::timeout, this, &YTPlayer::onYtdlpTimeout);

    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        this->stop();
    });
    connect(ytdlpProcess, &QProcess::readyReadStandardError, this, &YTPlayer::onYtdlpReadyReadError);

    // load volume from settings
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    currentVolume = settings.value("volume", 50).toInt();

    // Set initial volume and mute
    libvlc_audio_set_volume(m_player, currentVolume);
    libvlc_audio_set_mute(m_player, mutedState ? true : false);

    m_isRunning = true;  // libVLC is always "running" once initialized

    qDebug() << "[YTPlayer] ctor END";
}

YTPlayer::~YTPlayer()
{
    qDebug() << "[YTPlayer] dtor START";
    stop();

    // Clean up yt-dlp if needed
    if (ytdlpProcess && ytdlpProcess->state() != QProcess::NotRunning) {
        ytdlpProcess->kill();
        ytdlpProcess->waitForFinished(300);
    }

    if (m_currentMedia) {
        libvlc_media_release(m_currentMedia);
        m_currentMedia = nullptr;
    }

    // Release libVLC
    if (m_player) {
        libvlc_media_player_stop(m_player);
        libvlc_media_player_release(m_player);
        m_player = nullptr;
    }
    if (m_instance) {
        libvlc_release(m_instance);
        m_instance = nullptr;
    }

    qDebug() << "[YTPlayer] dtor END";
}

// --- libVLC Event Callbacks ---
void YTPlayer::onMediaEndReached(const libvlc_event_t *event, void *user_data) {
    Q_UNUSED(event);
    YTPlayer *player = static_cast<YTPlayer*>(user_data);
    qDebug() << "[YTPlayer] Media end reached";
    player->playing = false;
    emit player->playbackStateChanged(false);
}

void YTPlayer::onMediaError(const libvlc_event_t *event, void *user_data) {
    Q_UNUSED(event);
    YTPlayer *player = static_cast<YTPlayer*>(user_data);
    qWarning() << "[YTPlayer] Media error encountered";
    player->playing = false;
    emit player->playbackStateChanged(false);
    emit player->errorOccurred("Playback error: Media failed to load");
}

// --------------------- yt-dlp handling ---------------------
void YTPlayer::play(const QString& url)
{
    if (!m_instance || !m_player) {
        emit errorOccurred("libVLC not initialized");
        return;
    }

    if (url.trimmed().isEmpty()) {
        emit errorOccurred("Empty URL provided");
        return;
    }

    stop();

    qDebug() << "[YTPlayer] Play requested for URL:" << url;
    qDebug() << "[YTPlayer] Current working dir:" << QDir::currentPath();
    qDebug() << "[YTPlayer] PATH env:" << qgetenv("PATH");



    // normalize possible 11-char id
    QString normalized = url.trimmed();
    QRegularExpression rx("^[A-Za-z0-9_-]{11}$");
    if (rx.match(normalized).hasMatch())
        normalized = QStringLiteral("https://www.youtube.com/watch?v=%1").arg(normalized);

    pendingNormalizedUrl = normalized;
    pendingAllowPlaylist = normalized.contains("list=") || normalized.contains("playlist");

    // stop any previous yt-dlp
    if (ytdlpProcess->state() != QProcess::NotRunning) {
        ytdlpProcess->kill();
        ytdlpProcess->waitForFinished(200);
    }
    ytdlpStdoutBuffer.clear();
    ytdlpTriedManifest = false;

    // Determine yt-dlp executable
    QFile localYt("thirdparty/libmpv/yt-dlp.exe");  // Keep path as is, or adjust if needed
    QString prog = localYt.exists() ? localYt.fileName() : QStringLiteral("yt-dlp");
    ytdlpProcess->setProgram(prog);

    // Build args: get direct audio URL (prefer m4a)
    QStringList args;
    args << QStringLiteral("-g")
         << QStringLiteral("-f") << QStringLiteral("bestaudio[ext=m4a]/bestaudio")
         << QStringLiteral("--no-check-certificate"); // optional, helps some environments

    if (pendingAllowPlaylist)
        args << QStringLiteral("--yes-playlist");
    else
        args << QStringLiteral("--no-playlist");

    if (!m_cookiesFile.isEmpty())
        args << QStringLiteral("--cookies") << m_cookiesFile;

    args << pendingNormalizedUrl;

    ytdlpProcess->setArguments(args);

    ytdlpProcess->start();
    if (!ytdlpProcess->waitForStarted(3000)) {
        qWarning() << "[YTPlayer] yt-dlp failed to start. Error:" << ytdlpProcess->errorString();
        emit errorOccurred("yt-dlp failed to start: " + ytdlpProcess->errorString());
        return;
    }
    qDebug() << "[YTPlayer] yt-dlp started successfully. PID:" << ytdlpProcess->processId();
    ytdlpTimer->start(30000); // 30s timeout for live/slow responses

}

void YTPlayer::onYtdlpReadyRead()
{
    if (!ytdlpProcess) return;
    ytdlpStdoutBuffer.append(ytdlpProcess->readAllStandardOutput());
}

bool YTPlayer::supportsFeature(const QString& feature) const {
    static const QStringList supported = {
        "youtube", "cookies", "quitAndWait", "playlist"
    };
    return supported.contains(feature);
}

void YTPlayer::setCookiesFile(const QString& path) {
    if (m_cookiesFile != path) {
        m_cookiesFile = path;
        emit featureChanged("cookies", !path.isEmpty());
    }
}

QString YTPlayer::cookiesFile() const {
    return m_cookiesFile;
}


void YTPlayer::onYtdlpReadyReadError() {
    if (!ytdlpProcess) return;
    QString err = QString::fromUtf8(ytdlpProcess->readAllStandardError()).trimmed();
    if (!err.isEmpty()) {
        qWarning() << "[YTPlayer] yt-dlp stderr (real-time):" << err;
        writeLog("yt_err_realtime.txt", err + "\n");  // Append to separate file for real-time
    }
}

void YTPlayer::onYtdlpTimeout()
{
    if (!ytdlpProcess) return;
    if (ytdlpProcess->state() != QProcess::NotRunning) {
        ytdlpProcess->kill();
        ytdlpProcess->waitForFinished(200);
    }
    emit errorOccurred("yt-dlp timed out while resolving stream URL");
}

void YTPlayer::onYtdlpFinished(int exitCode, QProcess::ExitStatus)
{
    ytdlpTimer->stop();

    QString stdoutStr = QString::fromUtf8(ytdlpStdoutBuffer).trimmed();
    QString stderrStr = QString::fromUtf8(ytdlpProcess->readAllStandardError()).trimmed();

    writeLog("yt_out.txt", stdoutStr);
    writeLog("yt_err.txt", stderrStr);

    qDebug() << "[YTPlayer] yt-dlp exitCode =" << exitCode;
    qDebug() << "[YTPlayer] yt-dlp stdout (snippet):" << stdoutStr.left(1024);
    qDebug() << "[YTPlayer] yt-dlp stderr (snippet):" << stderrStr.left(1024);

    bool needFallback = stdoutStr.isEmpty() ||
                        stderrStr.contains("Requested format is not available") ||
                        stderrStr.contains("only manifest");

    if (needFallback && !ytdlpTriedManifest) {
        qDebug() << "[YTPlayer] Trying fallback: request manifest (no -f)";
        ytdlpTriedManifest = true;

        QString prog = ytdlpProcess->program();
        QStringList args;
        args << QStringLiteral("-g")
             << QStringLiteral("--no-check-certificate")
             << pendingNormalizedUrl;

        ytdlpProcess->setArguments(args);
        ytdlpStdoutBuffer.clear();
        ytdlpProcess->start();
        if (!ytdlpProcess->waitForStarted(3000)) {
            qWarning() << "[YTPlayer] yt-dlp failed to start for fallback manifest";
            emit errorOccurred("yt-dlp failed to start (fallback)");
            return;
        }
        ytdlpTimer->start(30000);
        return;
    }

    if (stdoutStr.isEmpty()) {
        qWarning() << "[YTPlayer] yt-dlp returned empty stdout (after fallback). stderr:" << stderrStr;
        emit errorOccurred("yt-dlp returned no URL (even after fallback). See logs.");
        return;
    }

    QStringList lines = stdoutStr.split('\n', Qt::SkipEmptyParts);
    for (QString &s : lines) s = s.trimmed();
    if (lines.isEmpty()) {
        emit errorOccurred("yt-dlp returned unexpected output");
        return;
    }
    QString directUrl = lines.first();
    qDebug() << "[YTPlayer] Resolved URL:" << directUrl.left(400);

    // ИСПРАВЛЕНО: Остановка и освобождение предыдущего media
    libvlc_media_player_stop(m_player);

    // КРИТИЧНО: Освобождаем предыдущий media перед созданием нового
    if (m_currentMedia) {
        libvlc_media_release(m_currentMedia);
        m_currentMedia = nullptr;
        qDebug() << "[YTPlayer] Released previous media";
    }

    // Создаем новый media
    m_currentMedia = libvlc_media_new_location(m_instance, directUrl.toUtf8().constData());
    if (!m_currentMedia) {
        qWarning() << "[YTPlayer] Failed to create libVLC media";
        emit errorOccurred("Failed to create media from URL");
        return;
    }

    // HTTP headers
    QString refererHeader = pendingNormalizedUrl;
    QString userAgent = QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    QString headerList = QString("Referer: %1,User-Agent: %2").arg(refererHeader, userAgent);
    libvlc_media_add_option(m_currentMedia, (":http-header-fields=" + headerList).toUtf8().constData());

    if (!m_cookiesFile.isEmpty()) {
        libvlc_media_add_option(m_currentMedia, (":http-cookies-file=" + m_cookiesFile).toUtf8().constData());
    }

    // Устанавливаем media и воспроизводим
    libvlc_media_player_set_media(m_player, m_currentMedia);
    libvlc_media_player_play(m_player);

    // НЕ ВЫЗЫВАЕМ libvlc_media_release здесь! Храним m_currentMedia для последующего освобождения

    // Устанавливаем громкость и mute
    libvlc_audio_set_volume(m_player, currentVolume);
    libvlc_audio_set_mute(m_player, mutedState ? true : false);

    playing = true;
    emit playbackStateChanged(true);
}

// control methods
void YTPlayer::stop()
{
    if (!m_player) {
        qDebug() << "[YTPlayer] No player, skipping stop";
        return;
    }

    libvlc_media_player_stop(m_player);
    playing = false;
    emit playbackStateChanged(false);
}

void YTPlayer::togglePlayback()
{
    if (!m_player) return;

    if (libvlc_media_player_is_playing(m_player)) {
        libvlc_media_player_pause(m_player);
    } else {
        libvlc_media_player_play(m_player);
    }
    playing = libvlc_media_player_is_playing(m_player);
    emit playbackStateChanged(playing);
}

void YTPlayer::setVolume(int value)
{
    if (currentVolume == value) return;
    currentVolume = value;
    if (m_player) {
        libvlc_audio_set_volume(m_player, currentVolume);
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    settings.setValue("volume", currentVolume);
    emit volumeChanged(currentVolume);
}

int YTPlayer::volume() const { return currentVolume; }

void YTPlayer::setMuted(bool muted)
{
    mutedState = muted;
    if (m_player) {
        libvlc_audio_set_mute(m_player, mutedState);
    }
    emit mutedChanged(mutedState);
}

bool YTPlayer::isMuted() const { return mutedState; }

void YTPlayer::start()
{
    // libVLC is always ready after ctor; nothing to do
    m_isRunning = (m_instance != nullptr && m_player != nullptr);
    if (!m_isRunning) {
        emit errorOccurred("libVLC not ready");
    }
}
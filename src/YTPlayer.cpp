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
    , cookiesFile(cookiesFile_)
    , m_isRunning(false)
{
    qDebug() << "[YTPlayer] ctor START";

    // Initialize libVLC instance with audio-only options
    const char *vlc_args[] = {
        "--no-video",          // Audio only
        "--intf=dummy",        // No interface
        "--ipv4-timeout=5000", // 5s TCP connection timeout (replaces invalid --network-timeout)
        "--network-caching=1000", // 1s network buffering to handle live stream delays
        "--http-reconnect",    // Automatically reconnect if the stream drops
        "--aout=directsound",  // Windows audio output
        "--quiet"
    };
    m_instance = libvlc_new(sizeof(vlc_args) / sizeof(vlc_args[0]), vlc_args);
    if (!m_instance) {
        qWarning() << "[YTPlayer] Failed to create libVLC instance";
        emit errorOccurred("Failed to initialize libVLC");
        return;
    }

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

    if (!cookiesFile.isEmpty())
        args << QStringLiteral("--cookies") << cookiesFile;

    args << pendingNormalizedUrl;

    ytdlpProcess->setArguments(args);
    qDebug() << "[YTPlayer] Starting yt-dlp with args:" << args;
    ytdlpProcess->start();
    if (!ytdlpProcess->waitForStarted(3000)) {
        qWarning() << "[YTPlayer] yt-dlp failed to start";
        emit errorOccurred("yt-dlp failed to start");
        return;
    }
    ytdlpTimer->start(30000); // 30s timeout for live/slow responses
}

void YTPlayer::onYtdlpReadyRead()
{
    if (!ytdlpProcess) return;
    ytdlpStdoutBuffer.append(ytdlpProcess->readAllStandardOutput());
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

    // dump for diagnostics
    writeLog("yt_out.txt", stdoutStr);
    writeLog("yt_err.txt", stderrStr);

    qDebug() << "[YTPlayer] yt-dlp exitCode =" << exitCode;
    qDebug() << "[YTPlayer] yt-dlp stdout (snippet):" << stdoutStr.left(1024);
    qDebug() << "[YTPlayer] yt-dlp stderr (snippet):" << stderrStr.left(1024);

    // Если получил пустой stdout или yt-dlp вернул ошибку о недоступном формате,
    // попробуем fallback: запросить МАНИФЕСТ (без -f).
    bool needFallback = stdoutStr.isEmpty() ||
                        stderrStr.contains("Requested format is not available") ||
                        stderrStr.contains("only manifest");

    if (needFallback && !ytdlpTriedManifest) {
        qDebug() << "[YTPlayer] Trying fallback: request manifest (no -f)";
        ytdlpTriedManifest = true;

        // Перезапускаем yt-dlp без -f чтобы получить манифест (если он есть).
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
        return; // ждём второго завершения
    }

    if (stdoutStr.isEmpty()) {
        qWarning() << "[YTPlayer] yt-dlp returned empty stdout (after fallback). stderr:" << stderrStr;
        emit errorOccurred("yt-dlp returned no URL (even after fallback). See logs.");
        return;
    }

    // first non-empty line is the direct URL or manifest
    QStringList lines = stdoutStr.split('\n', Qt::SkipEmptyParts);
    for (QString &s : lines) s = s.trimmed();
    if (lines.isEmpty()) {
        emit errorOccurred("yt-dlp returned unexpected output");
        return;
    }
    QString directUrl = lines.first();
    qDebug() << "[YTPlayer] Resolved URL:" << directUrl.left(400);

    // Stop current playback
    libvlc_media_player_stop(m_player);

    // Create new media
    libvlc_media_t *media = libvlc_media_new_location(m_instance, directUrl.toUtf8().constData());
    if (!media) {
        qWarning() << "[YTPlayer] Failed to create libVLC media";
        emit errorOccurred("Failed to create media from URL");
        return;
    }

    // Set HTTP headers
    QString refererHeader = pendingNormalizedUrl;
    QString userAgent = QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    QString headerList = QString("Referer: %1,User-Agent: %2").arg(refererHeader, userAgent);
    libvlc_media_add_option(media, (":http-header-fields=" + headerList).toUtf8().constData());

    // Set cookies if provided
    if (!cookiesFile.isEmpty()) {
        libvlc_media_add_option(media, (":http-cookies-file=" + cookiesFile).toUtf8().constData());
    }

    // Set media and play
    libvlc_media_player_set_media(m_player, media);
    libvlc_media_player_play(m_player);
    libvlc_media_release(media);

    // Set volume and mute
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
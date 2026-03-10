#include "YTPlayer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <utility>

static void writeLog(const QString& name, const QString& content)
{
    QString logDir = QCoreApplication::applicationDirPath() + QDir::separator() + "logs";
    QFile f(logDir + QDir::separator() + name);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(content.toUtf8());
}

YTPlayer::YTPlayer(QString cookiesFile_, QObject* parent)
    : AbstractPlayer(parent), m_cookiesFile(std::move(cookiesFile_))
{
    qDebug() << "[YTPlayer] ctor START";

    // --- ffmpeg ---
    ffmpegProcess = new QProcess(this);
    connect(ffmpegProcess, &QProcess::readyReadStandardOutput, this, &YTPlayer::onFfmpegReadyRead);
    connect(ffmpegProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &YTPlayer::onFfmpegFinished);
    connect(ffmpegProcess, &QProcess::errorOccurred, this, &YTPlayer::onFfmpegError);

    // Логируем stderr ffmpeg для диагностики
    connect(ffmpegProcess, &QProcess::readyReadStandardError, this,
            [this]()
            {
                qWarning() << "[YTPlayer] ffmpeg stderr:"
                           << QString::fromUtf8(ffmpegProcess->readAllStandardError()).trimmed();
            });

    // --- QAudioSink ---
    // s16le, 48000 Hz, stereo — должно совпадать с аргументами ffmpeg
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    m_audioSink = new QAudioSink(QMediaDevices::defaultAudioOutput(), format, this);
    // Большой буфер чтобы пережить паузы между HLS сегментами
    m_audioSink->setBufferSize(1152000);  // ~1 сек

    connect(m_audioSink, &QAudioSink::stateChanged, this,
            [](QAudio::State state) { qDebug() << "[YTPlayer] audioSink state:" << state; });

    // --- yt-dlp ---
    ytdlpProcess = new QProcess(this);
    ytdlpTimer = new QTimer(this);
    ytdlpTimer->setSingleShot(true);

    connect(ytdlpProcess, &QProcess::readyReadStandardOutput, this, &YTPlayer::onYtdlpReadyRead);
    connect(ytdlpProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &YTPlayer::onYtdlpFinished);
    connect(ytdlpProcess, &QProcess::readyReadStandardError, this,
            &YTPlayer::onYtdlpReadyReadError);
    connect(ytdlpTimer, &QTimer::timeout, this, &YTPlayer::onYtdlpTimeout);

    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() { stop(); });

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    m_volume = settings.value("volume", 50).toInt();

    m_isRunning = true;
    qDebug() << "[YTPlayer] ctor END";
}

YTPlayer::~YTPlayer()
{
    qDebug() << "[YTPlayer] dtor";
    stop();
}

void YTPlayer::play(const QString& url)
{
    stop();

    QString normalized = url.trimmed();
    QRegularExpression rx("^[A-Za-z0-9_-]{11}$");
    if (rx.match(normalized).hasMatch())
        normalized = QStringLiteral("https://www.youtube.com/watch?v=%1").arg(normalized);

    pendingNormalizedUrl = normalized;
    pendingAllowPlaylist = normalized.contains("list=") || normalized.contains("playlist");

    qDebug() << "[YTPlayer] play:" << pendingNormalizedUrl;

    ytdlpStdoutBuffer.clear();
    ytdlpTriedManifest = false;

    QFile localYt("thirdparty/yt-dlp.exe");
    ytdlpProcess->setProgram(localYt.exists() ? localYt.fileName() : "yt-dlp");

    QStringList args;
    args << "-g" << "-f" << "bestaudio" << "--no-check-certificate";
    args << (pendingAllowPlaylist ? "--yes-playlist" : "--no-playlist");
    if (!m_cookiesFile.isEmpty()) args << "--cookies" << m_cookiesFile;
    args << pendingNormalizedUrl;

    ytdlpProcess->setArguments(args);
    ytdlpProcess->start();

    if (!ytdlpProcess->waitForStarted(3000))
    {
        emit errorOccurred("yt-dlp failed to start: " + ytdlpProcess->errorString());
        return;
    }

    qDebug() << "[YTPlayer] yt-dlp started, PID:" << ytdlpProcess->processId();
    ytdlpTimer->start(30000);
}

void YTPlayer::stop()
{
    if (m_stopping) return;
    m_stopping = true;

    ytdlpTimer->stop();

    if (ytdlpProcess->state() != QProcess::NotRunning)
    {
        ytdlpProcess->kill();
        ytdlpProcess->waitForFinished(200);
    }

    if (m_audioSink->state() != QAudio::StoppedState) m_audioSink->stop();
    m_sinkDevice = nullptr;
    m_buffer.clear();

    if (ffmpegProcess->state() != QProcess::NotRunning)
    {
        ffmpegProcess->kill();
        ffmpegProcess->waitForFinished(300);
    }

    if (m_playing)
    {
        m_playing = false;
        emit playbackStateChanged(false);
    }

    m_stopping = false;
}

void YTPlayer::togglePlayback()
{
    if (!m_playing)
    {
        play(pendingNormalizedUrl);
        return;
    }

    if (m_audioSink->state() == QAudio::ActiveState)
    {
        m_audioSink->suspend();
        m_playing = false;
        emit playbackStateChanged(false);
    }
    else if (m_audioSink->state() == QAudio::SuspendedState)
    {
        m_audioSink->resume();
        m_playing = true;
        emit playbackStateChanged(true);
    }
}

void YTPlayer::setVolume(int value)
{
    if (m_volume == value) return;
    m_volume = value;

    if (!m_muted) m_audioSink->setVolume(value / 100.0f);

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    settings.setValue("volume", m_volume);
    emit volumeChanged(m_volume);
}

int YTPlayer::volume() const
{
    return m_volume;
}

void YTPlayer::setMuted(bool muted)
{
    if (m_muted == muted) return;
    m_muted = muted;
    m_audioSink->setVolume(muted ? 0.0f : m_volume / 100.0f);
    emit mutedChanged(m_muted);
}

bool YTPlayer::isMuted() const
{
    return m_muted;
}

// --- yt-dlp slots ---

void YTPlayer::onYtdlpReadyRead()
{
    ytdlpStdoutBuffer.append(ytdlpProcess->readAllStandardOutput());
}

void YTPlayer::onYtdlpReadyReadError() const
{
    QString err = QString::fromUtf8(ytdlpProcess->readAllStandardError()).trimmed();
    if (!err.isEmpty()) qWarning() << "[YTPlayer] yt-dlp stderr:" << err;
}

void YTPlayer::onYtdlpTimeout()
{
    if (ytdlpProcess->state() != QProcess::NotRunning)
    {
        ytdlpProcess->kill();
        ytdlpProcess->waitForFinished(200);
    }
    emit errorOccurred("yt-dlp timed out");
}

void YTPlayer::onYtdlpFinished(int exitCode, QProcess::ExitStatus)
{
    ytdlpTimer->stop();

    QString stdoutStr = QString::fromUtf8(ytdlpStdoutBuffer).trimmed();
    QString stderrStr = QString::fromUtf8(ytdlpProcess->readAllStandardError()).trimmed();

    writeLog("yt_out.txt", stdoutStr);
    writeLog("yt_err.txt", stderrStr);

    qDebug() << "[YTPlayer] yt-dlp exitCode:" << exitCode;

    bool needFallback = stdoutStr.isEmpty() ||
                        stderrStr.contains("Requested format is not available") ||
                        stderrStr.contains("only manifest");

    if (needFallback && !ytdlpTriedManifest)
    {
        qDebug() << "[YTPlayer] fallback: no -f";
        ytdlpTriedManifest = true;
        ytdlpStdoutBuffer.clear();

        ytdlpProcess->setArguments({"-g", "--no-check-certificate", pendingNormalizedUrl});
        ytdlpProcess->start();

        if (!ytdlpProcess->waitForStarted(3000))
        {
            emit errorOccurred("yt-dlp fallback failed to start");
            return;
        }
        ytdlpTimer->start(30000);
        return;
    }

    if (stdoutStr.isEmpty())
    {
        emit errorOccurred("yt-dlp returned no URL. See logs.");
        return;
    }

    QString directUrl = stdoutStr.split('\n', Qt::SkipEmptyParts).first().trimmed();
    qDebug() << "[YTPlayer] resolved URL:" << directUrl.left(80) << "...";

    // --- Запускаем ffmpeg ---
    // ffmpeg декодирует HLS и пишет сырой PCM в stdout
    QFile localFfmpeg("thirdparty/ffmpeg.exe");
    ffmpegProcess->setProgram(localFfmpeg.exists() ? localFfmpeg.fileName() : "ffmpeg");
    ffmpegProcess->setArguments({"-i", directUrl, "-f", "s16le", "-ar", "48000", "-ac", "2",
                                 "-loglevel", "warning", "pipe:1"});

    ffmpegProcess->start();

    if (!ffmpegProcess->waitForStarted(3000))
    {
        emit errorOccurred("ffmpeg failed to start: " + ffmpegProcess->errorString());
        return;
    }

    qDebug() << "[YTPlayer] ffmpeg started, PID:" << ffmpegProcess->processId();

    m_playing = true;
    emit playbackStateChanged(true);
}

// --- ffmpeg slots ---

void YTPlayer::onFfmpegReadyRead()
{
    m_buffer.append(ffmpegProcess->readAllStandardOutput());

    // Запускаем sink только когда накопили ~1 сек данных
    // чтобы сгладить паузы между HLS сегментами
    // 48000 samples * 2 channels * 2 bytes = 192000 bytes/sec
    if (!m_sinkDevice && m_buffer.size() >= 1152000)
    {
        m_audioSink->setVolume(m_muted ? 0.0f : m_volume / 100.0f);
        m_sinkDevice = m_audioSink->start();  // push режим
        qDebug() << "[YTPlayer] audioSink started, buffer:" << m_buffer.size();
    }

    if (m_sinkDevice && !m_buffer.isEmpty())
    {
        qint64 written = m_sinkDevice->write(m_buffer);
        if (written > 0) m_buffer.remove(0, written);
    }
}

void YTPlayer::onFfmpegFinished(int exitCode, QProcess::ExitStatus)
{
    if (m_stopping) return;

    qWarning() << "[YTPlayer] ffmpeg finished unexpectedly, code:" << exitCode;

    m_sinkDevice = nullptr;
    m_buffer.clear();
    if (m_audioSink->state() != QAudio::StoppedState) m_audioSink->stop();

    if (m_playing)
    {
        m_playing = false;
        emit playbackStateChanged(false);
    }

    // Переподключаемся если есть URL и пользователь сам не остановил
    if (!pendingNormalizedUrl.isEmpty())
    {
        qDebug() << "[YTPlayer] reconnecting in 3 sec...";
        QTimer::singleShot(3000, this,
                           [this]()
                           {
                               if (!m_stopping && !m_playing) play(pendingNormalizedUrl);
                           });
    }
}

void YTPlayer::onFfmpegError(QProcess::ProcessError error)
{
    if (m_stopping) return;
    qWarning() << "[YTPlayer] ffmpeg error:" << error << ffmpegProcess->errorString();
    emit errorOccurred("ffmpeg error: " + ffmpegProcess->errorString());
}

// --- Misc ---

void YTPlayer::start()
{
    m_isRunning = (ffmpegProcess && ytdlpProcess && m_audioSink);
    if (!m_isRunning) emit errorOccurred("Player not ready");
}

bool YTPlayer::supportsFeature(const QString& feature) const
{
    static const QStringList supported = {"youtube", "cookies", "quitAndWait", "playlist"};
    return supported.contains(feature);
}

void YTPlayer::setCookiesFile(const QString& path)
{
    if (m_cookiesFile != path)
    {
        m_cookiesFile = path;
        emit featureChanged("cookies", !path.isEmpty());
    }
}

QString YTPlayer::cookiesFile() const
{
    return m_cookiesFile;
}
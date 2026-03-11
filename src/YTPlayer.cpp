#include "YTPlayer.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMediaDevices>
#include <QRegularExpression>
#include <QSettings>
#include <utility>

// ============================================================================
// Helpers
// ============================================================================

static void writeLogInternal(const QString& logDir, const QString& name, const QString& content,
                             bool truncate)
{
    // Создаём папку logs, если нет
    QDir().mkpath(logDir);
    QString filePath = logDir + QDir::separator() + name;
    QFile f(filePath);
    // Открываем файл: всегда перезаписываем если truncate, иначе добавляем
    QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text;
    if (!truncate) mode |= QIODevice::Append;

    if (f.open(mode))
    {
        if (truncate)
        {
            // Полная перезапись файла
            f.write(content.toUtf8());
        }
        else
        {
            // Добавление в конец + перевод строки для читаемости
            if (!content.isEmpty()) f.write(content.toUtf8() + "\n");
        }
        f.close();
    }
    else
    {
        qWarning() << "[YTPlayer] Failed to write log:" << filePath << "Error:" << f.errorString();
    }
}

void YTPlayer::writeLog(const QString& name, const QString& content, bool truncate)
{
    // Путь: папка с исполняемым файлом + /logs
    QString logDir = QCoreApplication::applicationDirPath() + QDir::separator() + "logs";
    ::writeLogInternal(logDir, name, content, truncate);
}

static void clearOldLogs()
{
    QString logDir = QCoreApplication::applicationDirPath() + QDir::separator() + "logs";
    QDir().mkpath(logDir);

    // Список файлов логов, которые нужно очищать при каждом запуске
    static const QStringList logsToTruncate = {"yt_out.txt", "yt_err.txt",
                                               "app.log",  // <-- добавим общий лог приложения
                                               "ffmpeg.log"};

    for (const QString& fileName : logsToTruncate)
    {
        QString filePath = logDir + QDir::separator() + fileName;
        QFile f(filePath);
        // Просто открываем в режиме записи с усечением — файл очистится
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            f.close();
            qDebug() << "[YTPlayer] Log cleared:" << filePath;
        }
    }
}

QString YTPlayer::normalizeUrl(const QString& url)
{
    QString normalized = url.trimmed();
    // Если передан только видео-ID (11 символов)
    QRegularExpression rx("^[A-Za-z0-9_-]{11}$");
    if (rx.match(normalized).hasMatch())
        normalized = QStringLiteral("https://www.youtube.com/watch?v=%1").arg(normalized);
    return normalized;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

YTPlayer::YTPlayer(QString cookiesFile_, QObject* parent)
    : AbstractPlayer(parent), m_cookiesFile(std::move(cookiesFile_))
{
    qDebug() << "[YTPlayer] ctor START";

    ::clearOldLogs();

    qDebug() << "[YTPlayer] ctor START";

    // --- ffmpeg process ---
    ffmpegProcess = new QProcess(this);
    ffmpegProcess->setProcessChannelMode(QProcess::SeparateChannels);

    connect(ffmpegProcess, &QProcess::started, this, &YTPlayer::onFfmpegStarted);
    connect(ffmpegProcess, &QProcess::readyReadStandardOutput, this, &YTPlayer::onFfmpegReadyRead);
    connect(ffmpegProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &YTPlayer::onFfmpegFinished);
    connect(ffmpegProcess, QOverload<QProcess::ProcessError>::of(&QProcess::errorOccurred), this,
            &YTPlayer::onFfmpegError);
    connect(ffmpegProcess, &QProcess::readyReadStandardError, this,
            [this]()
            {
                QString err = QString::fromUtf8(ffmpegProcess->readAllStandardError()).trimmed();
                if (!err.isEmpty()) qWarning() << "[YTPlayer] ffmpeg stderr:" << err;
            });

    // --- QAudioSink ---
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    auto device = QMediaDevices::defaultAudioOutput();
    if (!device.isFormatSupported(format))
    {
        qWarning() << "[YTPlayer] Format not supported, using preferred...";
        format = device.preferredFormat();
    }

    m_audioSink = new QAudioSink(device, format, this);
    m_audioSink->setBufferSize(48000 * 2 * 2 * 8);  // 2 секунды внутреннего буфера Qt

    connect(m_audioSink, &QAudioSink::stateChanged, this,
            [](QAudio::State state) { qDebug() << "[YTPlayer] audioSink state:" << state; });

    // --- yt-dlp process ---
    ytdlpProcess = new QProcess(this);
    ytdlpProcess->setProcessChannelMode(QProcess::SeparateChannels);

    ytdlpTimer = new QTimer(this);
    ytdlpTimer->setSingleShot(true);

    connect(ytdlpProcess, &QProcess::started, this, &YTPlayer::onYtdlpStarted);
    connect(ytdlpProcess, &QProcess::readyReadStandardOutput, this, &YTPlayer::onYtdlpReadyRead);
    connect(ytdlpProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &YTPlayer::onYtdlpFinished);
    connect(ytdlpProcess, &QProcess::readyReadStandardError, this,
            &YTPlayer::onYtdlpReadyReadError);
    connect(ytdlpProcess, QOverload<QProcess::ProcessError>::of(&QProcess::errorOccurred), this,
            &YTPlayer::onYtdlpError);
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

// ============================================================================
// Public API
// ============================================================================

void YTPlayer::play(const QString& url)
{
    stop();  // Полная остановка перед новым запуском

    pendingNormalizedUrl = normalizeUrl(url);
    pendingAllowPlaylist =
        pendingNormalizedUrl.contains("list=") || pendingNormalizedUrl.contains("playlist");

    qDebug() << "[YTPlayer] play:" << pendingNormalizedUrl;

    ytdlpStdoutBuffer.clear();
    ytdlpTriedManifest = false;
    m_reconnectAttempts = 0;  // Сброс счётчика

    // Поиск yt-dlp
    QFile localYt("thirdparty/yt-dlp.exe");
    ytdlpProcess->setProgram(localYt.exists() ? localYt.fileName() : "yt-dlp");

    QStringList args;
    args << "-g" << "-f" << "bestaudio/best"  // Гибкий выбор формата
         << "--no-check-certificate"
         << "--no-playlist";  // Пока отключим плейлисты для простоты

    if (!m_cookiesFile.isEmpty() && QFileInfo::exists(m_cookiesFile))
        args << "--cookies" << m_cookiesFile;

    args << pendingNormalizedUrl;
    ytdlpProcess->setArguments(args);

    // Асинхронный запуск
    ytdlpProcess->start();
}

void YTPlayer::stop()
{
    if (m_stopping) return;
    m_stopping = true;

    ytdlpTimer->stop();

    // Остановка yt-dlp
    if (ytdlpProcess->state() != QProcess::NotRunning)
    {
        ytdlpProcess->kill();
        if (!ytdlpProcess->waitForFinished(200)) ytdlpProcess->terminate();
    }

    // Остановка аудио
    if (m_audioSink && m_audioSink->state() != QAudio::StoppedState) m_audioSink->stop();
    m_sinkDevice = nullptr;
    m_buffer.clear();

    // Остановка ffmpeg
    if (ffmpegProcess && ffmpegProcess->state() != QProcess::NotRunning)
    {
        ffmpegProcess->kill();
        if (!ffmpegProcess->waitForFinished(300)) ffmpegProcess->terminate();
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

    if (!m_audioSink) return;

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

    if (m_audioSink && !m_muted) m_audioSink->setVolume(value / 100.0f);

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

    if (m_audioSink) m_audioSink->setVolume(muted ? 0.0f : m_volume / 100.0f);

    emit mutedChanged(m_muted);
}

bool YTPlayer::isMuted() const
{
    return m_muted;
}

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

// ============================================================================
// yt-dlp slots
// ============================================================================

void YTPlayer::onYtdlpStarted()
{
    qDebug() << "[YTPlayer] yt-dlp started, PID:" << ytdlpProcess->processId();
    ytdlpTimer->start(30000);  // Таймер стартует только после успешного запуска
}

void YTPlayer::onYtdlpReadyRead()
{
    ytdlpStdoutBuffer.append(ytdlpProcess->readAllStandardOutput());
}

void YTPlayer::onYtdlpReadyReadError()
{
    QString err = QString::fromUtf8(ytdlpProcess->readAllStandardError()).trimmed();
    if (!err.isEmpty()) qWarning() << "[YTPlayer] yt-dlp stderr:" << err;
}

void YTPlayer::onYtdlpTimeout()
{
    qWarning() << "[YTPlayer] yt-dlp timeout";
    if (ytdlpProcess->state() != QProcess::NotRunning)
    {
        ytdlpProcess->kill();
        ytdlpProcess->waitForFinished(200);
    }
    emit errorOccurred("yt-dlp timed out (30s)");
}

void YTPlayer::onYtdlpError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart)
    {
        emit errorOccurred("yt-dlp failed to start: " + ytdlpProcess->errorString());
    }
}

void YTPlayer::onYtdlpFinished(int exitCode, QProcess::ExitStatus status)
{
    ytdlpTimer->stop();

    QString stdoutStr = QString::fromUtf8(ytdlpStdoutBuffer).trimmed();
    QString stderrStr = QString::fromUtf8(ytdlpProcess->readAllStandardError()).trimmed();

    writeLog("yt_out.txt", stdoutStr, true);
    writeLog("yt_err.txt", stderrStr, true);

    qDebug() << "[YTPlayer] yt-dlp exitCode:" << exitCode << "status:" << status;

    // Проверка на необходимость фоллбека
    bool needFallback = stdoutStr.isEmpty() ||
                        stderrStr.contains("Requested format is not available") ||
                        stderrStr.contains("only manifest", Qt::CaseInsensitive);

    if (needFallback && !ytdlpTriedManifest)
    {
        qDebug() << "[YTPlayer] fallback: retrying without -f bestaudio";
        ytdlpTriedManifest = true;
        ytdlpStdoutBuffer.clear();

        QStringList args = {"-g", "--no-check-certificate"};
        if (!m_cookiesFile.isEmpty() && QFileInfo::exists(m_cookiesFile))
            args << "--cookies" << m_cookiesFile;
        args << pendingNormalizedUrl;

        ytdlpProcess->setArguments(args);
        ytdlpProcess->start();  // Асинхронно, сигналы уже подключены
        return;
    }

    if (stdoutStr.isEmpty())
    {
        emit errorOccurred("yt-dlp returned no URL. Check logs/yt_err.txt");
        return;
    }

    // Обработка возможных нескольких URL (плейлист)
    QStringList urls = stdoutStr.split('\n', Qt::SkipEmptyParts);
    QString directUrl = urls.first().trimmed();

    if (urls.size() > 1)
    {
        qWarning() << "[YTPlayer] yt-dlp returned" << urls.size()
                   << "URLs, using first one. Consider --no-playlist flag.";
    }

    qDebug() << "[YTPlayer] resolved URL:" << directUrl.left(100) << "...";

    startFfmpeg(directUrl);
}

// ============================================================================
// ffmpeg helpers & slots
// ============================================================================

void YTPlayer::startFfmpeg(const QString& streamUrl)
{
    QFile localFfmpeg("thirdparty/ffmpeg.exe");
    ffmpegProcess->setProgram(localFfmpeg.exists() ? localFfmpeg.fileName() : "ffmpeg");
    ffmpegProcess->setArguments({
        "-thread_queue_size", "512",               // Очередь для сетевых пакетов
        "-probesize", "1048576",                   // Быстрый старт
        "-analyzeduration", "0", "-i", streamUrl,  // Входной файл

        "-fflags", "+nobuffer",  // Минимальная буферизация
        "-f", "s16le",           // Raw PCM
        "-ar", "48000",          // Sample rate (должен совпадать с QAudioFormat!)
        "-ac", "2",              // Stereo
        "-loglevel", "warning",

        "pipe:1"  // Вывод в stdout
    });

    ffmpegProcess->start();
}

void YTPlayer::onFfmpegStarted()
{
    qDebug() << "[YTPlayer] ffmpeg started, PID:" << ffmpegProcess->processId();

    m_playing = true;
    emit playbackStateChanged(true);
}

void YTPlayer::onFfmpegReadyRead()
{
    // 1. Читаем данные
    QByteArray newData = ffmpegProcess->readAllStandardOutput();
    if (newData.isEmpty()) return;
    m_buffer.append(newData);

    // 2. Trim буфера (удаляем старое)
    if (m_buffer.size() > BUFFER_MAX_SIZE) m_buffer.remove(0, m_buffer.size() - BUFFER_MAX_SIZE);

    // 3. Запуск sink
    if (!m_sinkDevice && m_audioSink && m_buffer.size() >= BUFFER_START_THRESHOLD)
    {
        m_audioSink->setVolume(m_muted ? 0.0f : m_volume / 100.0f);
        m_sinkDevice = m_audioSink->start();
        if (m_sinkDevice)
            qDebug() << "[YTPlayer] ✓ audioSink STARTED, buffer:" << m_buffer.size();
        else
        {
            qCritical() << "[YTPlayer] ✗ audioSink->start() FAILED!";
            emit errorOccurred("Failed to start audio output");
            return;
        }
    }

    // 4. Запись в устройство
    if (m_sinkDevice && !m_buffer.isEmpty())
    {
        // Проверка состояния перед записью
        if (m_audioSink->state() == QAudio::ActiveState ||
            m_audioSink->state() == QAudio::IdleState)
        {
            qint64 written = m_sinkDevice->write(m_buffer);
            if (written > 0)
            {
                m_buffer.remove(0, static_cast<int>(written));
            }
            else if (written == 0)
            {
                // Устройство не готово. Не паникуем, просто ждем следующего вызова слота.
                // Если это происходит постоянно — значит буфер Qt переполнен,
                // и он сам приостановит чтение из pipe ffmpeg.
            }
            else
            {
                qWarning() << "[YTPlayer] write error:" << written;
            }
        }
    }
    static qint64 lastLog = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastLog > 2000)
    {  // Раз в 2 секунды
        qDebug() << "[YTPlayer] buffer:" << m_buffer.size() << "/" << BUFFER_MAX_SIZE << "("
                 << (m_buffer.size() * 1000 / (48000 * 2 * 2)) << "ms)"
                 << "sink:" << (m_audioSink ? m_audioSink->state() : -1);
        lastLog = now;
    }
}

void YTPlayer::onFfmpegFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_stopping) return;

    qWarning() << "[YTPlayer] ffmpeg finished, code:" << exitCode << "status:" << status;

    m_sinkDevice = nullptr;
    m_buffer.clear();

    if (m_audioSink && m_audioSink->state() != QAudio::StoppedState) m_audioSink->stop();

    if (m_playing)
    {
        m_playing = false;
        emit playbackStateChanged(false);
    }

    // Попытка переподключения с ограничением попыток
    tryReconnect();
}

void YTPlayer::onFfmpegError(QProcess::ProcessError error)
{
    if (m_stopping) return;

    qWarning() << "[YTPlayer] ffmpeg error:" << error << ffmpegProcess->errorString();

    emit errorOccurred("ffmpeg error: " + ffmpegProcess->errorString());
    tryReconnect();
}

void YTPlayer::tryReconnect()
{
    if (m_stopping || pendingNormalizedUrl.isEmpty()) return;

    if (m_reconnectAttempts >= MAX_RECONNECT_ATTEMPTS)
    {
        qWarning() << "[YTPlayer] max reconnect attempts reached";
        emit errorOccurred("Connection lost after " + QString::number(MAX_RECONNECT_ATTEMPTS) +
                           " retries");
        return;
    }

    m_reconnectAttempts++;
    // Экспоненциальная задержка: 3s, 6s, 12s, 24s, 30s(max)
    int delay = qMin(RECONNECT_BASE_DELAY_MS * (1 << (m_reconnectAttempts - 1)), 30000);

    qDebug() << "[YTPlayer] scheduling reconnect attempt" << m_reconnectAttempts << "in" << delay
             << "ms";

    QTimer::singleShot(delay, this,
                       [this]()
                       {
                           if (!m_stopping && !m_playing) play(pendingNormalizedUrl);
                       });
}
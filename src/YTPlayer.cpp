#include "YTPlayer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QThread>
#include <utility>

// Helper: write small logs to exe/logs
static void writeLog(const QString& name, const QString& content)
{
    QString exeDir = QCoreApplication::applicationDirPath();
    QString logDir = exeDir + QDir::separator() + "logs";
    QFile f(logDir + QDir::separator() + name);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(content.toUtf8());
}

YTPlayer::YTPlayer(QString cookiesFile_, QObject* parent)
    : AbstractPlayer(parent), m_cookiesFile(std::move(cookiesFile_))
{
    qDebug() << "[YTPlayer] ctor START";

    // ffplay process
    ffplayProcess = new QProcess(this);

    // signals for ffplay
    connect(ffplayProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &YTPlayer::onFfplayFinished);
    connect(ffplayProcess, &QProcess::errorOccurred, this, &YTPlayer::onFfplayError);

    // yt-dlp process
    ytdlpProcess = new QProcess(this);
    ytdlpTimer = new QTimer(this);
    ytdlpTimer->setSingleShot(true);

    // signals for yt-dlp
    connect(ytdlpProcess, &QProcess::readyReadStandardOutput, this, &YTPlayer::onYtdlpReadyRead);
    connect(ytdlpProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &YTPlayer::onYtdlpFinished);
    connect(ytdlpTimer, &QTimer::timeout, this, &YTPlayer::onYtdlpTimeout);

    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() { this->stop(); });
    connect(ytdlpProcess, &QProcess::readyReadStandardError, this,
            &YTPlayer::onYtdlpReadyReadError);

    // load volume from settings
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    currentVolume = settings.value("volume", 50).toInt();

    m_isRunning = true;
    qDebug() << "[YTPlayer] ctor END";
}

YTPlayer::~YTPlayer()
{
    qDebug() << "[YTPlayer] dtor START";
    stop();

    // Clean up yt-dlp if needed
    if (ytdlpProcess && ytdlpProcess->state() != QProcess::NotRunning)
    {
        ytdlpProcess->kill();
        ytdlpProcess->waitForFinished(300);
    }

    if (ffplayProcess && ffplayProcess->state() != QProcess::NotRunning)
    {
        ffplayProcess->kill();
        ffplayProcess->waitForFinished(300);
    }

    qDebug() << "[YTPlayer] dtor END";
}

// --- ffplay handling ---
void YTPlayer::onFfplayFinished(int exitCode, QProcess::ExitStatus)
{
    if (exitCode == 0)
        qDebug() << "[YTPlayer] ffplay finished normally";
    else
        qWarning() << "[YTPlayer] ffplay finished with exit code:" << exitCode;

    if (playing)
    {
        playing = false;
        emit playbackStateChanged(false);
    }
}

void YTPlayer::onFfplayError(QProcess::ProcessError error)
{
    qWarning() << "[YTPlayer] ffplay error:" << error;
    if (playing)
    {
        playing = false;
        emit playbackStateChanged(false);
        emit errorOccurred("ffplay failed to start: " + ffplayProcess->errorString());
    }
}

// --------------------- yt-dlp handling ---------------------
void YTPlayer::play(const QString& url)
{
    stop();  // Stop current playback

    qDebug() << "[YTPlayer] Play requested for URL:" << url;

    // normalize possible 11-char id
    QString normalized = url.trimmed();
    QRegularExpression rx("^[A-Za-z0-9_-]{11}$");
    if (rx.match(normalized).hasMatch())
        normalized = QStringLiteral("https://www.youtube.com/watch?v=%1").arg(normalized);

    pendingNormalizedUrl = normalized;
    pendingAllowPlaylist = normalized.contains("list=") || normalized.contains("playlist");

    // stop any previous yt-dlp
    if (ytdlpProcess->state() != QProcess::NotRunning)
    {
        ytdlpProcess->kill();
        ytdlpProcess->waitForFinished(200);
    }
    ytdlpStdoutBuffer.clear();
    ytdlpTriedManifest = false;

    // Determine yt-dlp executable
    QFile localYt("thirdparty/yt-dlp.exe");
    QString prog = localYt.exists() ? localYt.fileName() : QStringLiteral("yt-dlp");
    ytdlpProcess->setProgram(prog);

    // Build args: get direct audio URL (prefer m4a)
    QStringList args;
    args << QStringLiteral("-g") << QStringLiteral("-f") << QStringLiteral("bestaudio")
         << QStringLiteral("--no-check-certificate");

    if (pendingAllowPlaylist)
        args << QStringLiteral("--yes-playlist");
    else
        args << QStringLiteral("--no-playlist");

    if (!m_cookiesFile.isEmpty()) args << QStringLiteral("--cookies") << m_cookiesFile;

    args << pendingNormalizedUrl;

    ytdlpProcess->setArguments(args);

    ytdlpProcess->start();
    if (!ytdlpProcess->waitForStarted(3000))
    {
        qWarning() << "[YTPlayer] yt-dlp failed to start. Error:" << ytdlpProcess->errorString();
        emit errorOccurred("yt-dlp failed to start: " + ytdlpProcess->errorString());
        return;
    }
    qDebug() << "[YTPlayer] yt-dlp started successfully. PID:" << ytdlpProcess->processId();
    ytdlpTimer->start(30000);  // 30s timeout for live/slow responses
}

void YTPlayer::onYtdlpReadyRead()
{
    if (!ytdlpProcess) return;
    ytdlpStdoutBuffer.append(ytdlpProcess->readAllStandardOutput());
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

void YTPlayer::onYtdlpReadyReadError() const
{
    if (!ytdlpProcess) return;
    QString err = QString::fromUtf8(ytdlpProcess->readAllStandardError()).trimmed();
    if (!err.isEmpty())
    {
        qWarning() << "[YTPlayer] yt-dlp stderr (real-time):" << err;
        writeLog("yt_err_realtime.txt", err + "\n");
    }
}

void YTPlayer::onYtdlpTimeout()
{
    if (!ytdlpProcess) return;
    if (ytdlpProcess->state() != QProcess::NotRunning)
    {
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

    bool needFallback = stdoutStr.isEmpty() ||
                        stderrStr.contains("Requested format is not available") ||
                        stderrStr.contains("only manifest");

    if (needFallback && !ytdlpTriedManifest)
    {
        qDebug() << "[YTPlayer] Trying fallback: request manifest (no -f)";
        ytdlpTriedManifest = true;

        QString prog = ytdlpProcess->program();
        QStringList args;
        args << QStringLiteral("-g") << QStringLiteral("--no-check-certificate")
             << pendingNormalizedUrl;

        ytdlpProcess->setArguments(args);
        ytdlpStdoutBuffer.clear();
        ytdlpProcess->start();
        if (!ytdlpProcess->waitForStarted(3000))
        {
            qWarning() << "[YTPlayer] yt-dlp failed to start for fallback manifest";
            emit errorOccurred("yt-dlp failed to start (fallback)");
            return;
        }
        ytdlpTimer->start(30000);
        return;
    }

    if (stdoutStr.isEmpty())
    {
        qWarning() << "[YTPlayer] yt-dlp returned empty stdout (after "
                      "fallback). stderr:"
                   << stderrStr;
        emit errorOccurred("yt-dlp returned no URL (even after fallback). See logs.");
        return;
    }

    QStringList lines = stdoutStr.split('\n', Qt::SkipEmptyParts);
    for (QString& s : lines) s = s.trimmed();
    if (lines.isEmpty())
    {
        emit errorOccurred("yt-dlp returned unexpected output");
        return;
    }
    QString directUrl = lines.first();
    qDebug() << "[YTPlayer] Resolved URL:" << directUrl.left(400);

    // ffplay args
    QStringList ffplayArgs;
    ffplayArgs << QStringLiteral("-autoexit") << QStringLiteral("-nodisp") << QStringLiteral("-af")
               << QStringLiteral("volume=%1").arg(currentVolume / 1000.0, 0, 'f', 2)
               << QStringLiteral("-loglevel") << QStringLiteral("error") << directUrl;

    // Determine ffplay executable
    QFile localFfplay("thirdparty/ffplay.exe");  // Adjust path if needed
    QString ffplayProg = localFfplay.exists() ? localFfplay.fileName() : QStringLiteral("info");

    ffplayProcess->setProgram(ffplayProg);
    ffplayProcess->setArguments(ffplayArgs);

    ffplayProcess->start();
    if (!ffplayProcess->waitForStarted(3000))
    {
        qWarning() << "[YTPlayer] ffplay failed to start. Error:" << ffplayProcess->errorString();
        emit errorOccurred("ffplay failed to start: " + ffplayProcess->errorString());
        return;
    }

    qDebug() << "[YTPlayer] ffplay started successfully. PID:" << ffplayProcess->processId();

    playing = true;
    emit playbackStateChanged(true);
}

// control methods
void YTPlayer::stop()
{
    if (ffplayProcess && ffplayProcess->state() != QProcess::NotRunning)
    {
        ffplayProcess->kill();
        ffplayProcess->waitForFinished(300);
    }

    if (ytdlpProcess && ytdlpProcess->state() != QProcess::NotRunning)
    {
        ytdlpProcess->kill();
        ytdlpProcess->waitForFinished(200);
    }

    if (playing)
    {
        playing = false;
        emit playbackStateChanged(false);
    }
}

void YTPlayer::togglePlayback()
{
    // ffplay doesn't support pause/resume via CLI, so restart/stop
    if (playing)
    {
        stop();
    }
    else
    {
        play(pendingNormalizedUrl);  // Restart from last URL
    }
}

void YTPlayer::setVolume(int value)
{
    if (currentVolume == value) return;
    currentVolume = value;

    // ffplay doesn't support changing volume at runtime via CLI.
    // You need to restart playback or use a different approach (e.g., SDL
    // mixer). For now, just store the value and use it on next play.

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    settings.setValue("volume", currentVolume);
    emit volumeChanged(currentVolume);
}

int YTPlayer::volume() const
{
    return currentVolume;
}

void YTPlayer::setMuted(bool muted)
{
    mutedState = muted;
    // Mute is not supported in ffplay via CLI.
    // Store the value for future use.
    emit mutedChanged(mutedState);
}

bool YTPlayer::isMuted() const
{
    return mutedState;
}

void YTPlayer::start()
{
    // ffplay is ready after ctor; nothing to do
    m_isRunning = (ffplayProcess != nullptr && ytdlpProcess != nullptr);
    if (!m_isRunning)
    {
        emit errorOccurred("ffplay or yt-dlp not ready");
    }
}
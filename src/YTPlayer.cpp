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
{
    qDebug() << "[YTPlayer] ctor START";
    // create processes/timer
    mpvProcess = new QProcess(this);
    ytdlpProcess = new QProcess(this);
    ytdlpTimer = new QTimer(this);
    ytdlpTimer->setSingleShot(true);

    // signals for yt-dlp
    connect(ytdlpProcess, &QProcess::readyReadStandardOutput, this, &YTPlayer::onYtdlpReadyRead);
    connect(ytdlpProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &YTPlayer::onYtdlpFinished);
    connect(ytdlpTimer, &QTimer::timeout, this, &YTPlayer::onYtdlpTimeout);

    // signals for mpv process
    connect(mpvProcess, &QProcess::readyReadStandardOutput, this, &YTPlayer::onMpvProcessReadyRead);
    connect(mpvProcess, &QProcess::readyReadStandardError, this, &YTPlayer::onMpvProcessReadyRead);
    connect(mpvProcess, QOverload<QProcess::ProcessError>::of(&QProcess::errorOccurred),
            this, &YTPlayer::onMpvProcessError);

    // load volume from settings
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    currentVolume = settings.value("volume", 50).toInt();

    // start mpv.exe
    startMpvProcess();

    qDebug() << "[YTPlayer] ctor END";
}

YTPlayer::~YTPlayer()
{
    qDebug() << "[YTPlayer] dtor START";

    // 1) Попробуем отправить "quit" через существующий сокет/IPC.
    auto tryQuitViaIpc = [&]() -> bool {
        // If we already have a connected ipcSocket — use it.
        if (ipcSocket && ipcSocket->state() == QLocalSocket::ConnectedState) {
            sendIpcCommand(QJsonArray{"quit"});
            // give mpv a moment to exit
            QThread::msleep(200);
            return true;
        }

        // Otherwise try a temporary connection (in case original socket was destroyed or disconnected).
        QLocalSocket temp;
        temp.connectToServer(ipcName);
        if (!temp.waitForConnected(500)) {
            qDebug() << "[YTPlayer] temp IPC connect failed:" << temp.errorString();
            return false;
        }
        // send quit
        QJsonObject root;
        root.insert("command", QJsonArray{"quit"});
        QByteArray out = QJsonDocument(root).toJson(QJsonDocument::Compact) + '\n';
        temp.write(out);
        temp.flush();
        temp.waitForBytesWritten(300);
        temp.disconnectFromServer();
        return true;
    };

    // Try to request mpv to quit gracefully
    bool sentQuit = tryQuitViaIpc();

    // 2) Wait some time for mpvProcess to finish by itself
    if (mpvProcess) {
        if (mpvProcess->state() != QProcess::NotRunning) {
            // If we sent quit, give it a bit longer, otherwise shorter.
            int waitMs = sentQuit ? 1500 : 500;
            mpvProcess->waitForFinished(waitMs);

            if (mpvProcess->state() != QProcess::NotRunning) {
                // Try terminate -> wait -> kill as last resort
                qWarning() << "[YTPlayer] mpv did not exit after quit; trying terminate()";
                mpvProcess->terminate();
                mpvProcess->waitForFinished(800);
            }

            if (mpvProcess->state() != QProcess::NotRunning) {
                qWarning() << "[YTPlayer] mpv still running; killing it";
                mpvProcess->kill();
                mpvProcess->waitForFinished(800);
            }
        }
        delete mpvProcess;
        mpvProcess = nullptr;
    }

    // 3) Clean up IPC socket if still present
    if (ipcSocket) {
        if (ipcSocket->state() == QLocalSocket::ConnectedState)
            ipcSocket->disconnectFromServer();
        ipcSocket->close();
        ipcSocket->deleteLater();
        ipcSocket = nullptr;
    }

    // 4) Kill yt-dlp if needed
    if (ytdlpProcess) {
        if (ytdlpProcess->state() != QProcess::NotRunning) {
            ytdlpProcess->kill();
            ytdlpProcess->waitForFinished(300);
        }
        delete ytdlpProcess;
        ytdlpProcess = nullptr;
    }

    qDebug() << "[YTPlayer] dtor END";
}

// --- start mpv.exe with IPC server ---
void YTPlayer::startMpvProcess()
{
    // locate mpv.exe (fallback to PATH)
    QFile mpvLocal("../thirdparty/libmpv/mpv.exe");
    QString mpvPath = mpvLocal.exists() ? mpvLocal.fileName() : QStringLiteral("mpv");

    QStringList args;
    args << QStringLiteral("--idle") // keep running waiting for commands
         << QStringLiteral("--no-terminal")
         << QStringLiteral("--input-ipc-server=") + ipcName
         << QStringLiteral("--ytdl=no")     // we use yt-dlp externally
         << QStringLiteral("--msg-level=all=warn")
         << QStringLiteral("--no-video")
         << QStringLiteral("--vo=null");

    // start
    mpvProcess->start(mpvPath, args);
    if (!mpvProcess->waitForStarted(3000)) {
        QString err = QString("Failed to start mpv (%1). errno: %2")
                          .arg(mpvPath, QString::number(mpvProcess->error()));
        qWarning() << "[YTPlayer]" << err;
        writeLog("mpv_start_err.txt", err);
        emit errorOccurred(err);
        return;
    }

    // connect IPC
    connectIpc();
}

// --- connect to mpv IPC named pipe via QLocalSocket ---
void YTPlayer::connectIpc()
{
    if (ipcSocket) {
        ipcSocket->abort();
        ipcSocket->deleteLater();
    }
    ipcSocket = new QLocalSocket(this);
    connect(ipcSocket, &QLocalSocket::connected, this, &YTPlayer::onIpcConnected);
    connect(ipcSocket, &QLocalSocket::disconnected, this, &YTPlayer::onIpcDisconnected);
    connect(ipcSocket, &QLocalSocket::readyRead, this, &YTPlayer::onIpcReadyRead);

    ipcRetryAttempts = 0;
    ipcSocket->connectToServer(ipcName);
    // wait asynchronously; if not connected soon - we schedule retries
    QTimer::singleShot(500, this, [this]() {
        if (!ipcSocket) return;
        if (ipcSocket->state() == QLocalSocket::ConnectedState) return;
        ipcRetryAttempts++;
        if (ipcRetryAttempts <= ipcMaxRetries) {
            qDebug() << "[YTPlayer] IPC connect attempt" << ipcRetryAttempts;
            ipcSocket->connectToServer(ipcName);
            QTimer::singleShot(500, this, [this]() { connectIpc(); });
        } else {
            qWarning() << "[YTPlayer] IPC connect failed after retries.";
            writeLog("mpv_ipc_connect_err.txt", ipcSocket->errorString());
        }
    });
}

void YTPlayer::onIpcConnected()
{
    qDebug() << "[YTPlayer] IPC connected";
    // set initial volume
    sendIpcCommand(QJsonArray{"set_property", "volume", currentVolume});
    if (mutedState) sendIpcCommand(QJsonArray{"set_property", "mute", static_cast<int>(1)});
}

void YTPlayer::onIpcDisconnected()
{
    qWarning() << "[YTPlayer] IPC disconnected";
    // try reconnect after brief delay
    QTimer::singleShot(500, this, [this]() { connectIpc(); });
}

void YTPlayer::onIpcReadyRead()
{
    // read responses from mpv (we just log them)
    while (ipcSocket && ipcSocket->bytesAvailable()) {
        QByteArray chunk = ipcSocket->readAll();
        writeLog("mpv_ipc_in.txt", QString::fromUtf8(chunk));
        qDebug() << "[YTPlayer] mpv ipc ->" << QString::fromUtf8(chunk).left(1024);
    }
}

// JSON IPC write helper
void YTPlayer::sendIpcCommand(const QJsonArray& cmd)
{
    if (!ipcSocket || ipcSocket->state() != QLocalSocket::ConnectedState) {
        qWarning() << "[YTPlayer] IPC not connected; command skipped:" << QJsonDocument(cmd).toJson(QJsonDocument::Compact);
        return;
    }
    QJsonObject root;
    root.insert("command", cmd);
    QByteArray out = QJsonDocument(root).toJson(QJsonDocument::Compact) + '\n';
    ipcSocket->write(out);
    ipcSocket->flush();
}

// --- mpv process stdout/stderr logger ---
void YTPlayer::onMpvProcessReadyRead()
{
    if (!mpvProcess) return;
    QByteArray out = mpvProcess->readAllStandardOutput();
    QByteArray err = mpvProcess->readAllStandardError();
    if (!out.isEmpty()) writeLog("mpv_stdout.txt", QString::fromUtf8(out));
    if (!err.isEmpty()) writeLog("mpv_stderr.txt", QString::fromUtf8(err));
    if (!out.isEmpty()) qDebug() << "[YTPlayer][mpv/stdout]" << QString::fromUtf8(out).left(1024);
    if (!err.isEmpty()) qWarning() << "[YTPlayer][mpv/stderr]" << QString::fromUtf8(err).left(1024);
}

void YTPlayer::onMpvProcessError(QProcess::ProcessError err)
{
    qWarning() << "[YTPlayer] mpv process error:" << err;
    writeLog("mpv_proc_err.txt", QString::number(static_cast<int>(err)));
    emit errorOccurred(QStringLiteral("mpv process error: %1").arg(static_cast<int>(err)));
}

// --------------------- yt-dlp handling ---------------------

void YTPlayer::play(const QString& url)
{
    if (!mpvProcess || mpvProcess->state() == QProcess::NotRunning) {
        emit errorOccurred("mpv is not running");
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
    QFile localYt("thirdparty/libmpv/yt-dlp.exe");
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

    // ensure IPC connected
    if (!ipcSocket || ipcSocket->state() != QLocalSocket::ConnectedState) {
        qWarning() << "[YTPlayer] IPC not connected; attempting to reconnect...";
        connectIpc();
        if (ipcSocket && ipcSocket->state() == QLocalSocket::ConnectedState) {
            sendIpcCommand(QJsonArray{"loadfile", directUrl, "replace"});
        } else {
            // wait synchronously a short time for connect
            if (ipcSocket && ipcSocket->waitForConnected(1500)) {
                sendIpcCommand(QJsonArray{"loadfile", directUrl, "replace"});
            } else {
                qWarning() << "[YTPlayer] mpv IPC not connected and we won't spawn detached mpv. Failing playback.";
                emit errorOccurred("mpv IPC not connected; cannot start playback");
            }
        }
    }

    // set optional headers/cookies
    QString refererHeader = pendingNormalizedUrl;
    QString userAgent = QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    QString headerList = QString("Referer: %1,User-Agent: %2").arg(refererHeader, userAgent);
    sendIpcCommand(QJsonArray{"set_property", "http-header-fields", headerList});
    if (!cookiesFile.isEmpty())
        sendIpcCommand(QJsonArray{"set_property", "cookies-file", cookiesFile});

    // load the URL (replace)
    sendIpcCommand(QJsonArray{"loadfile", directUrl, "replace"});

    playing = true;
    emit playbackStateChanged(true);
}

// control methods
void YTPlayer::stop()
{
    if (!ipcSocket || ipcSocket->state() != QLocalSocket::ConnectedState) return;
    sendIpcCommand(QJsonArray{"stop"});
    // clear headers/cookies
    sendIpcCommand(QJsonArray{"set_property", "http-header-fields", ""});
    if (!cookiesFile.isEmpty())
        sendIpcCommand(QJsonArray{"set_property", "cookies", 0});
    playing = false;
    emit playbackStateChanged(false);
}

void YTPlayer::togglePlayback()
{
    if (!ipcSocket || ipcSocket->state() != QLocalSocket::ConnectedState) return;
    sendIpcCommand(QJsonArray{"cycle", "pause"});
    playing = !playing;
    emit playbackStateChanged(playing);
}

void YTPlayer::setVolume(int value)
{
    if (currentVolume == value) return;
    currentVolume = value;
    if (ipcSocket && ipcSocket->state() == QLocalSocket::ConnectedState)
        sendIpcCommand(QJsonArray{"set_property", "volume", currentVolume});

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    settings.setValue("volume", currentVolume);
    emit volumeChanged(currentVolume);
}

int YTPlayer::volume() const { return currentVolume; }

void YTPlayer::setMuted(bool muted)
{
    mutedState = muted;
    if (ipcSocket && ipcSocket->state() == QLocalSocket::ConnectedState)
        sendIpcCommand(QJsonArray{"set_property", "mute", muted ? 1 : 0});
    emit mutedChanged(mutedState);
}

bool YTPlayer::isMuted() const { return mutedState; }

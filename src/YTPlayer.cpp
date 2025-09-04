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
    , m_isRunning(false)
{
    ipcName = QString(R"(\\.\pipe\mpv-ipc-%1)").arg(QCoreApplication::applicationPid());  // Уникальное имя по PID, чтобы избежать конфликтов
    qDebug() << "[YTPlayer] ctor START";
    mpvProcess = new QProcess(this);
    ytdlpProcess = new QProcess(this);
    ytdlpTimer = new QTimer(this);
    ytdlpTimer->setSingleShot(true);
    reconnectTimer = new QTimer(this);
    reconnectTimer->setSingleShot(true);
    connect(reconnectTimer, &QTimer::timeout, this, &YTPlayer::tryReconnect);


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
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        this->gracefulShutdownMpv(2500);
    });

    // load volume from settings
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    currentVolume = settings.value("volume", 50).toInt();

    // start mpv.exe
    startMpvProcess();
    if (mpvProcess->state() == QProcess::Running) {
        m_isRunning = true;
    }

    qDebug() << "[YTPlayer] ctor END";
}

YTPlayer::~YTPlayer()
{
    qDebug() << "[YTPlayer] dtor START";
    stop();

    m_shuttingDown = true;
    gracefulShutdownMpv(5000);  // Увеличьте до 5с для HLS/live, где может быть буфер

    // Clean up IPC socket
    if (ipcSocket) {
        if (ipcSocket->state() == QLocalSocket::ConnectedState)
            ipcSocket->disconnectFromServer();
        ipcSocket->close();
        ipcSocket->deleteLater();
        ipcSocket = nullptr;
    }

    // Kill yt-dlp if needed
    if (ytdlpProcess && ytdlpProcess->state() != QProcess::NotRunning) {
        ytdlpProcess->kill();
        ytdlpProcess->waitForFinished(300);
    }

    if (mpvProcess) {
        delete mpvProcess;  // После shutdown, чтобы избежать утечек
        mpvProcess = nullptr;
    }

    qDebug() << "[YTPlayer] dtor END";
}

// --- start mpv.exe with IPC server ---
void YTPlayer::startMpvProcess()
{
    // locate mpv.exe (fallback to PATH)
    QFile mpvLocal("../thirdparty/libmpv/mpv.exe");
    QString mpvPath = mpvLocal.exists() ? mpvLocal.fileName() : QStringLiteral("mpv");

    if (mpvProcess->state() != QProcess::NotRunning) {
        qDebug() << "[YTPlayer] mpv already running, skipping start";
        return;
    }

    QStringList args;
    args << QStringLiteral("--idle") // keep running waiting for commands
         << QStringLiteral("--no-terminal")
         << QStringLiteral("--input-ipc-server=") + ipcName
         << QStringLiteral("--ytdl=no")     // we use yt-dlp externally
         << QStringLiteral("--msg-level=all=debug")
         << QStringLiteral("--no-video")
         << QStringLiteral("--vo=null")
         << QStringLiteral("--network-timeout=5")
         << QStringLiteral("--demuxer-lavf-o=timeout=5000000");

    // start
    mpvProcess->start(mpvPath, args);
    m_mpvPid = mpvProcess->processId();
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

void YTPlayer::start()
{
    if (mpvProcess->state() == QProcess::Running) {
        qDebug() << "[YTPlayer] mpv process already running, syncing state";
        m_isRunning = true;
        connectIpc();
        return;
    }

    if (m_isRunning) {
        qDebug() << "[YTPlayer] Already running, skipping start";
        return;
    }

    startMpvProcess();
    if (mpvProcess->state() == QProcess::NotRunning) {
        qWarning() << "[YTPlayer] Failed to start mpv process";
        emit errorOccurred("Failed to start mpv");
        return;
    }
    m_isRunning = true;

    // Восстановите состояние (volume, mute)
    setVolume(currentVolume);
    setMuted(mutedState);
    qDebug() << "[YTPlayer] mpv started successfully";
}

// --- connect to mpv IPC named pipe via QLocalSocket ---
void YTPlayer::connectIpc()
{
    if (ipcSocket && ipcSocket->state() == QLocalSocket::ConnectedState) {
        qDebug() << "[YTPlayer] IPC already connected, skipping reconnect";
        return;
    }

    if (ipcSocket) {
        ipcSocket->abort();
        ipcSocket->deleteLater();
    }

    ipcSocket = new QLocalSocket(this);

    connect(ipcSocket, &QLocalSocket::connected, this, &YTPlayer::onIpcConnected);
    connect(ipcSocket, &QLocalSocket::disconnected, this, &YTPlayer::onIpcDisconnected);
    connect(ipcSocket, &QLocalSocket::readyRead, this, &YTPlayer::onIpcReadyRead);
    connect(ipcSocket, &QLocalSocket::errorOccurred, this, &YTPlayer::onIpcError);

    ipcRetryAttempts = 0;
    while (ipcRetryAttempts <= ipcMaxRetries) {
        ipcSocket->connectToServer(ipcName);
        if (ipcSocket->waitForConnected(500)) {
            return; // success
        }
        ipcRetryAttempts++;
        qDebug() << "[YTPlayer] IPC connect attempt" << ipcRetryAttempts << "failed:" << ipcSocket->errorString();
        if (ipcRetryAttempts > ipcMaxRetries) {
            qWarning() << "[YTPlayer] IPC connect failed after retries.";
            writeLog("mpv_ipc_connect_err.txt", ipcSocket->errorString());
            break;
        }
        QThread::msleep(500);
    }
}

void YTPlayer::onIpcError(QLocalSocket::LocalSocketError error) {
    qWarning() << "[YTPlayer] IPC socket error:" << error << ipcSocket->errorString();
}

void YTPlayer::onIpcConnected()
{
    qDebug() << "[YTPlayer] IPC connected";
    reconnectTimer->stop();  // Cancel any pending reconnect to prevent cycles
    ipcRetryAttempts = 0;
    sendIpcCommand(QJsonArray{"observe_property", 1, "idle-active"});
}

void YTPlayer::onIpcDisconnected()
{
    qWarning() << "[YTPlayer] IPC disconnected";
    if (!m_isRunning || m_shuttingDown) {
        qDebug() << "[YTPlayer] Player not running or shutting down; skipping reconnect";
        return;
    }
    if (ipcRetryAttempts < ipcMaxRetries) {
        ipcRetryAttempts++;
        int delay = 500 * ipcRetryAttempts; // backoff: 500, 1000, 1500...
        reconnectTimer->stop();  // Остановка таймера перед новым запуском, чтобы избежать дубликатов
        reconnectTimer->start(delay);
    } else {
        qWarning() << "[YTPlayer] Max reconnect attempts reached; not retrying.";
    }
}

void YTPlayer::tryReconnect() {
    connectIpc();
}

void YTPlayer::onIpcReadyRead()
{
    while (ipcSocket && ipcSocket->bytesAvailable()) {
        QByteArray chunk = ipcSocket->readAll();
        // Убрали writeLog и qDebug для скорости; логируйте только если нужно для дебага
        // Если требуется обработка JSON, добавьте парсинг здесь
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
    m_isRunning = false;  // Синхронизируем флаг при ошибке
    // попытка перезапустить mpv через 500ms
    QTimer::singleShot(500, this, [this]() {
        if (!mpvProcess || mpvProcess->state() == QProcess::NotRunning)
            startMpvProcess();
    });
}

bool YTPlayer::sendQuitAndWait(int waitMs)
{
    // JSON-команда quit
    QJsonObject root;
    root.insert("command", QJsonArray{"quit"});
    QByteArray out = QJsonDocument(root).toJson(QJsonDocument::Compact) + '\n';

    // 1) Если есть подключенный ipcSocket — используем его.
    if (ipcSocket && ipcSocket->state() == QLocalSocket::ConnectedState) {
        qint64 written = ipcSocket->write(out);
        ipcSocket->flush();
        if (written <= 0) {
            qWarning() << "[YTPlayer] sendQuitAndWait: failed to write to existing ipcSocket";
        } else {
            ipcSocket->waitForBytesWritten(300);
            qDebug() << "[YTPlayer] sendQuitAndWait: sent quit via existing ipcSocket";
        }

        if (waitMs > 0 && mpvProcess) {
            mpvProcess->waitForFinished(waitMs);
        }
        return true;
    }

    // 2) Попробовать временное синхронное подключение к IPC (в те же pipe/имя).
    {
        QLocalSocket temp;
        temp.connectToServer(ipcName);
        if (!temp.waitForConnected(500)) {
            qWarning() << "[YTPlayer] sendQuitAndWait: temp IPC connect failed:" << temp.errorString();
            return false;
        }
        qint64 written = temp.write(out);
        temp.flush();
        if (written <= 0) {
            qWarning() << "[YTPlayer] sendQuitAndWait: temp socket write failed";
            temp.disconnectFromServer();
            return false;
        }
        temp.waitForBytesWritten(500);
        temp.disconnectFromServer();
        qDebug() << "[YTPlayer] sendQuitAndWait: sent quit via temp ipcSocket";

        if (waitMs > 0 && mpvProcess) {
            mpvProcess->waitForFinished(waitMs);
        }
        return true;
    }
}

void YTPlayer::gracefulShutdownMpv(int totalWaitMs)
{
    if (!mpvProcess) {
        qDebug() << "[YTPlayer] gracefulShutdownMpv: no mpvProcess";
        return;
    }
    qDebug() << "[YTPlayer] gracefulShutdownMpv: starting with PID" << mpvProcess->processId();

    if (ipcSocket && ipcSocket->state() == QLocalSocket::ConnectedState) {
        qDebug() << "[YTPlayer] gracefulShutdownMpv: sending 'pause' and 'stop' before quit";
        sendIpcCommand(QJsonArray{"set_property", "pause", true});  // Пауза, чтобы прервать буфер
        QThread::msleep(300);
        sendIpcCommand(QJsonArray{"stop"});  // Остановить playback, освободить ресурсы
        QThread::msleep(500);  // Время на обработку HLS-демакса
    } else {
        qWarning() << "[YTPlayer] gracefulShutdownMpv: IPC not connected, skipping pause/stop";
    }

    // 1) Попытка quit через IPC
    bool sent = sendQuitAndWait((totalWaitMs * 40) / 100);
    qDebug() << "[YTPlayer] gracefulShutdownMpv: quit sent?" << sent;

    // 2) Если жив — terminate
    if (mpvProcess->state() != QProcess::NotRunning) {
        qDebug() << "[YTPlayer] gracefulShutdownMpv: still running -> terminate()";
        if (mpvProcess->state() != QProcess::NotRunning) {
            qDebug() << "[YTPlayer] gracefulShutdownMpv: closing write channel (stdin)";
            mpvProcess->closeWriteChannel();  // Закрывает stdin
            QThread::msleep(200);  // Дайте время mpv отреагировать
        }
        mpvProcess->terminate();
        int part = (totalWaitMs * 30) / 100;
        if (part < 300) part = 300;
        mpvProcess->waitForFinished(part);
    }

    // 3) Если ещё жив — kill
    if (mpvProcess->state() != QProcess::NotRunning) {
        qWarning() << "[YTPlayer] gracefulShutdownMpv: terminate failed -> kill()";
        mpvProcess->kill();
        int part = totalWaitMs - ((totalWaitMs * 40) / 100) - ((totalWaitMs * 30) / 100);
        if (part < 500) part = 500;
        mpvProcess->waitForFinished(part);
    }

    if (mpvProcess->state() != QProcess::NotRunning) {
        qWarning() << "[YTPlayer] gracefulShutdownMpv: mpv still alive after kill; forcing taskkill";
        QProcess killer;
        killer.start("taskkill", QStringList() << "/PID" << QString::number(mpvProcess->processId()) << "/F" << "/T");  // /F force, /T kill subtree
        killer.waitForFinished(1000);
    }

    // Итог
    if (mpvProcess->state() == QProcess::NotRunning) {
        qDebug() << "[YTPlayer] gracefulShutdownMpv: mpv exited cleanly";
    } else {
        qWarning() << "[YTPlayer] gracefulShutdownMpv: mpv still reported running after all attempts. Possible Qt bug or mpv hang.";
    }
}

// --------------------- yt-dlp handling ---------------------


void YTPlayer::play(const QString& url)
{
    if (!m_isRunning) {
        qDebug() << "[YTPlayer] mpv not running, starting before play";
        start();
        if (!m_isRunning) {
            emit errorOccurred("Failed to start mpv for playback");
            return;
        }
    }

    if (!mpvProcess || mpvProcess->state() == QProcess::NotRunning) {
        emit errorOccurred("mpv is not running");
        return;
    }
    if (url.trimmed().isEmpty()) {
        emit errorOccurred("Empty URL provided");
        return;
    }

    // set initial volume
    sendIpcCommand(QJsonArray{"set_property", "volume", currentVolume});
    if (mutedState) sendIpcCommand(QJsonArray{"set_property", "mute", static_cast<int>(1)});

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
    if (!m_isRunning) {
        qDebug() << "[YTPlayer] Not running, skipping stop";
        return;
    }

    gracefulShutdownMpv(5000);  // Ваш существующий shutdown
    m_isRunning = false;

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

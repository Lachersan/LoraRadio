
#include "YTPlayer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QFileInfo>
#include <QtGlobal>
#include <QProcess>
#include <QTimer>
#include <QSettings>
#include <QRegularExpression>
#include <QStringList>
#include <cstring>

static void on_mpv_wakeup(void *ctx) {
    YTPlayer *player = static_cast<YTPlayer*>(ctx);
    if (player) {
        QMetaObject::invokeMethod(player, "handleMpvEvents", Qt::QueuedConnection);
    }
}

YTPlayer::YTPlayer(const QString& cookiesFile_, QObject* parent)
    : AbstractPlayer(parent)
    , cookiesFile(cookiesFile_)
{
    qDebug() << "[YTPlayer] ctor START this=" << this << " cookiesFile=" << cookiesFile_;

    mpv = mpv_create();
    if (!mpv) {
        emit errorOccurred("Failed to create mpv handle");
        qDebug() << "[YTPlayer] Failed to create mpv handle";
        return;
    }
    qDebug() << "[YTPlayer] mpv_create succeeded, mpv=" << mpv;

    auto logMpvRc = [&](int rc, const char* when){
        if (rc >= 0) {
            qDebug() << "[YTPlayer] mpv_set_option_string(" << when << ") -> rc =" << rc
                     << " (" << (mpv_error_string ? mpv_error_string(rc) : "no mpv_error_string") << ")";
        } else {
            qWarning() << "[YTPlayer] mpv_set_option_string(" << when << ") -> rc =" << rc
                       << " (" << (mpv_error_string ? mpv_error_string(rc) : "no mpv_error_string") << ")";
        }
    };

    // папка exe + лог
    QString exeDir = QCoreApplication::applicationDirPath();
    qDebug() << "[YTPlayer] exeDir =" << exeDir;
    QString logDir = exeDir + QDir::separator() + "logs";
    bool ok = QDir().mkpath(logDir);
    qDebug() << "[YTPlayer] mkpath(logDir) returned:" << ok << "logDir:" << logDir;

    QString testPath = logDir + QDir::separator() + "test_write.txt";
    QFile testFile(testPath);
    if (testFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        testFile.write("ytplayer log test\n");
        testFile.close();
        qDebug() << "[YTPlayer] Successfully wrote test file to:" << testPath;
    } else {
        qWarning() << "[YTPlayer] Failed to write test file to:" << testPath;
    }

    // helper: set option and handle known error codes gracefully
    auto setOptionSafe = [&](const char* name, const char* val) -> int {
        int rc = mpv_set_option_string(mpv, name, val);
        logMpvRc(rc, name);
        if (rc == MPV_ERROR_OPTION_NOT_FOUND) {
            qWarning() << "[YTPlayer] Option not found in this mpv build:" << name
                       << "- skipping (this is okay)";
            return rc;
        }
        if (rc == MPV_ERROR_OPTION_ERROR) {
            qWarning() << "[YTPlayer] Option parse/set error for" << name
                       << "- value may be unsupported in this build:" << QString::fromUtf8(val);
            return rc;
        }
        return rc;
    };
    qDebug() << "[YTPlayer] setOptionSafe lambda defined";

    // Включаем подробный mpv-лог (в development)
    setOptionSafe("msg-level", "all=debug");
    qDebug() << "[YTPlayer] msg-level set";

    // Отключаем встроенный ytdl — мы запускаем yt-dlp через QProcess
    setOptionSafe("ytdl", "no");
    qDebug() << "[YTPlayer] ytdl set to no";

    // ytdl-path (локальный приоритет) — хранить QByteArray чтобы constData() был валиден
    QFile ytLocal("thirdparty/libmpv/yt-dlp.exe");
    QByteArray ytdlPathBa = ytLocal.exists() ? ytLocal.fileName().toUtf8() : QByteArrayLiteral("yt-dlp");
    int rc_ytdl_path = setOptionSafe("ytdl-path", ytdlPathBa.constData());
    Q_UNUSED(rc_ytdl_path);
    qDebug() << "[YTPlayer] ytdl-path set, rc=" << rc_ytdl_path;

    // формат по умолчанию
    setOptionSafe("ytdl-format", "140");
    qDebug() << "[YTPlayer] ytdl-format set";

    // попытка установить demuxer-lavf-o (может быть не поддержан)
    QByteArray demuxerLavf = QByteArrayLiteral("protocol_whitelist=file,http,https,tls,crypto,dash");
    int rc_demux = setOptionSafe("demuxer-lavf-o", demuxerLavf.constData());
    if (rc_demux == MPV_ERROR_OPTION_ERROR) {
        qWarning() << "[YTPlayer] demuxer-lavf-o rejected: libmpv may be built without lavf/ffmpeg demuxer;"
                   << "fallback: use external yt-dlp to get direct stream URLs.";
    }
    qDebug() << "[YTPlayer] demuxer-lavf-o set, rc=" << rc_demux;

    // остальные опции (без фатального поведения)
    setOptionSafe("cache", "no");
    qDebug() << "[YTPlayer] cache set to no";
    setOptionSafe("vid", "no");
    qDebug() << "[YTPlayer] vid set to no";

    // Добавляем дополнительные опции для теста (как предлагалось ранее)
    setOptionSafe("hwdec", "no");
    qDebug() << "[YTPlayer] hwdec set to no";
    setOptionSafe("vo", "null");
    qDebug() << "[YTPlayer] vo set to null";
    setOptionSafe("ao", "wasapi");
    qDebug() << "[YTPlayer] ao set to wasapi";

    // Инициализация mpv — вызвать ровно один раз и сохранить rc
    qDebug() << "[YTPlayer] Before mpv_initialize";
    int rc_init = mpv_initialize(mpv);
    qDebug() << "[YTPlayer] After mpv_initialize rc=" << rc_init;
    if (rc_init < 0) {
        qWarning() << "[YTPlayer] mpv_initialize failed rc=" << rc_init
                   << " msg=" << (mpv_error_string ? mpv_error_string(rc_init) : "unknown");
        mpv_destroy(mpv);
        mpv = nullptr;
        emit errorOccurred("mpv initialization failed");
        return;
    }

    // wakeup callback
    qDebug() << "[YTPlayer] Before set_wakeup_callback";
    mpv_set_wakeup_callback(mpv, on_mpv_wakeup, this);
    qDebug() << "[YTPlayer] After set_wakeup_callback";

    // загрузим громкость
    qDebug() << "[YTPlayer] Before QSettings";
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    currentVolume = settings.value("volume", 50).toInt();
    qDebug() << "[YTPlayer] After QSettings, currentVolume=" << currentVolume;

    double vol = static_cast<double>(currentVolume);
    qDebug() << "[YTPlayer] Before mpv_set_property volume";
    if (mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &vol) < 0) {
        qWarning() << "[YTPlayer] Failed to set volume property on mpv";
    }
    qDebug() << "[YTPlayer] After mpv_set_property volume";
    emit volumeChanged(currentVolume);
    qDebug() << "[YTPlayer] After emit volumeChanged";

    // Подготовка yt-dlp процесса (мы полагаемся на него, если libmpv не поддерживает ytdl_hook)
    qDebug() << "[YTPlayer] Before creating ytdlpProcess";
    ytdlpProcess = new QProcess(this);
    if (ytLocal.exists()) {
        ytdlpProcess->setProgram(ytLocal.fileName());
    } else {
        ytdlpProcess->setProgram(QStringLiteral("yt-dlp"));
    }
    qDebug() << "[YTPlayer] After creating ytdlpProcess, program=" << ytdlpProcess->program();

    qDebug() << "[YTPlayer] Before connecting ytdlpProcess signals";
    connect(ytdlpProcess, &QProcess::readyReadStandardOutput, this, &YTPlayer::onYtdlpReadyRead);
    connect(ytdlpProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &YTPlayer::onYtdlpFinished);
    qDebug() << "[YTPlayer] After connecting ytdlpProcess signals";

    qDebug() << "[YTPlayer] Before creating ytdlpTimer";
    ytdlpTimer = new QTimer(this);
    ytdlpTimer->setSingleShot(true);
    qDebug() << "[YTPlayer] After creating ytdlpTimer";

    qDebug() << "[YTPlayer] Before connecting ytdlpTimer";
    connect(ytdlpTimer, &QTimer::timeout, this, &YTPlayer::onYtdlpTimeout);
    qDebug() << "[YTPlayer] After connecting ytdlpTimer";

    qDebug() << "[YTPlayer] yt-dlp program set to:" << ytdlpProcess->program();
    qDebug() << "[YTPlayer] ctor END this=" << this;
}

YTPlayer::~YTPlayer() {
    qDebug() << "[YTPlayer] dtor START this=" << this;
    if (mpv) {
        // отключаем callback прежде чем разрушать mpv
        mpv_set_wakeup_callback(mpv, nullptr, nullptr);
        mpv_terminate_destroy(mpv);
        mpv = nullptr;
    }

    if (ytdlpProcess) {
        if (ytdlpProcess->state() != QProcess::NotRunning) {
            ytdlpProcess->kill();
            ytdlpProcess->waitForFinished(1000);
        }
        delete ytdlpProcess;
        ytdlpProcess = nullptr;
    }
}

void YTPlayer::play(const QString& url)
{
    if (!mpv) {
        emit errorOccurred("mpv handle is null");
        return;
    }
    if (url.isEmpty()) {
        emit errorOccurred("Empty URL provided");
        return;
    }

    // нормализация: если передан 11-символьный ID -> формируем youtube URL
    QString normalized = url.trimmed();
    QRegularExpression rx("^[A-Za-z0-9_-]{11}$");
    if (rx.match(normalized).hasMatch()) {
        normalized = QStringLiteral("https://www.youtube.com/watch?v=%1").arg(normalized);
    }

    // простая эвристика плейлиста
    bool looksLikePlaylist = normalized.contains("list=") || normalized.contains("playlist");

    // сохраняем текущий запрос
    pendingNormalizedUrl = normalized;
    pendingAllowPlaylist = looksLikePlaylist;

    // Если предыдущий процесс ещё работает — убиваем его
    if (ytdlpProcess->state() != QProcess::NotRunning) {
        qWarning() << "[YTPlayer] previous yt-dlp running, killing...";
        ytdlpProcess->kill();
        ytdlpProcess->waitForFinished(200);
    }

    ytdlpStdoutBuffer.clear();

    // Проверим исполняемый путь yt-dlp: если задан абсолютный путь — проверим существование,
    // иначе считаем, что "yt-dlp" может быть в PATH и попытаемся запустить.
    QString prog = ytdlpProcess->program();
    bool programIsPath = prog.contains('/') || prog.contains('\\');
    if (programIsPath) {
        QFileInfo yf(prog);
        if (!yf.exists() || !yf.isExecutable()) {
            qWarning() << "[YTPlayer] yt-dlp executable not found or not executable:" << prog
                       << " — falling back to mpv ytdl integration.";
            // временный fallback: включим mpv ytdl и попытаемся загрузить исходный URL
            mpv_set_option_string(mpv, "ytdl", "yes");
            QByteArray ba = pendingNormalizedUrl.toUtf8();
            const char* cmd[] = {"loadfile", ba.constData(), nullptr};
            int ret = mpv_command(mpv, cmd);
            mpv_set_option_string(mpv, "ytdl", "no");
            if (ret < 0) {
                emit errorOccurred("mpv fallback failed: " + pendingNormalizedUrl);
            } else {
                playing = true;
                emit playbackStateChanged(true);
            }
            return;
        }
    }

    // Формируем аргументы yt-dlp для резолва прямых URL (HLS-friendly)
    QStringList args;
    args << "-g" << "-f" << "bestaudio/best"
         << "--hls-prefer-ffmpeg" << "--hls-use-mpegts";

    if (looksLikePlaylist)
        args << "--yes-playlist";
    else
        args << "--no-playlist";

    if (!cookiesFile.isEmpty())
        args << "--cookies" << cookiesFile;

    // --- ВАЖНО: добавляем целевой URL ПОСЛЕ всех опций ---
    args << normalized;

    ytdlpProcess->setArguments(args);
    qDebug() << "[YTPlayer] Starting yt-dlp with args:" << args << "for" << normalized;
    ytdlpProcess->start();

    if (!ytdlpProcess->waitForStarted(2000)) {
        qWarning() << "[YTPlayer] yt-dlp failed to start. program:" << ytdlpProcess->program();
        emit errorOccurred("yt-dlp failed to start");
        return;
    }

    // более длинный таймаут для live (30s)
    ytdlpTimer->start(30000);
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
        qWarning() << "[YTPlayer] yt-dlp timed out, killing";
        ytdlpProcess->kill();
        ytdlpProcess->waitForFinished(200);
    }
    emit errorOccurred("yt-dlp timed out while resolving stream URL");
}

void YTPlayer::onYtdlpFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)
    ytdlpTimer->stop();

    QString stdoutStr = QString::fromUtf8(ytdlpStdoutBuffer).trimmed();
    QString stderrStr = QString::fromUtf8(ytdlpProcess->readAllStandardError()).trimmed();

    // Diagnostic dump of yt-dlp output to app logs (exe folder logs)
    QString projDir = QCoreApplication::applicationDirPath();
    QString logDir  = projDir + QDir::separator() + "logs";
    QDir().mkpath(logDir);

    QString ytOutPath = logDir + QDir::separator() + "yt_out.txt";
    QString ytErrPath = logDir + QDir::separator() + "yt_err.txt";

    QFile outFile(ytOutPath);
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        outFile.write(stdoutStr.toUtf8());
        outFile.close();
        qDebug() << "[YTPlayer] Wrote yt-dlp stdout to" << ytOutPath;
    } else {
        qWarning() << "[YTPlayer] Failed to write yt-dlp stdout to" << ytOutPath;
    }

    QFile errFile(ytErrPath);
    if (errFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errFile.write(stderrStr.toUtf8());
        errFile.close();
        qDebug() << "[YTPlayer] Wrote yt-dlp stderr to" << ytErrPath;
    } else {
        qWarning() << "[YTPlayer] Failed to write yt-dlp stderr to" << ytErrPath;
    }

    qDebug() << "[YTPlayer] yt-dlp exitCode =" << exitCode;
    qDebug() << "[YTPlayer] yt-dlp stdout (snippet):" << stdoutStr.left(1024);
    qDebug() << "[YTPlayer] yt-dlp stderr (snippet):" << stderrStr.left(1024);

    if (stdoutStr.isEmpty()) {
        qWarning() << "[YTPlayer] yt-dlp returned empty stdout. stderr:" << stderrStr;
        // fallback: временно дать mpv использовать свою интеграцию ytdl
        mpv_set_option_string(mpv, "ytdl", "yes");
        QByteArray ba = pendingNormalizedUrl.toUtf8();
        const char* cmd[] = {"loadfile", ba.constData(), nullptr};
        int ret = mpv_command(mpv, cmd);
        mpv_set_option_string(mpv, "ytdl", "no");
        if (ret < 0) {
            emit errorOccurred(QString("Fallback mpv failed (ret=%1). yt-dlp stderr: %2").arg(ret).arg(stderrStr));
        } else {
            playing = true;
            emit playbackStateChanged(true);
        }
        return;
    }

    // Разбираем строки — yt-dlp -g возвращает 1+ URL-ов (по одной строке на поток)
    QStringList lines = stdoutStr.split('\n', Qt::SkipEmptyParts);
    for (QString &s : lines) s = s.trimmed();

    // Проверка — первая строка должна начинаться с http(s)
    if (lines.isEmpty() || !(lines.first().startsWith("http://") || lines.first().startsWith("https://"))) {
        qWarning() << "[YTPlayer] yt-dlp returned unexpected output (not an http URL). stdout snippet:" << stdoutStr.left(1024);
        qWarning() << "[YTPlayer] yt-dlp stderr snippet:" << stderrStr.left(1024);
        emit errorOccurred("yt-dlp returned unexpected output instead of stream URL. See logs.");
        return;
    }

    // Установим заголовки (Referer/User-Agent) и cookies в mpv перед загрузкой манифеста/сегментов
    QString refererHeader = pendingNormalizedUrl;
    QString userAgent = QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36");
    QString headerList = QString("Referer: %1,User-Agent: %2").arg(refererHeader, userAgent);
    QByteArray headerBa = headerList.toUtf8();
    mpv_set_option_string(mpv, "http-header-fields", headerBa.constData());

    if (!cookiesFile.isEmpty()) {
        QByteArray cookieBa = cookiesFile.toUtf8();
        mpv_set_option_string(mpv, "cookies-file", cookieBa.constData());
        mpv_set_option_string(mpv, "cookies", "yes");
    }

    // Загрузка первого URL (replace)
    bool loadFailed = false;
    {
        QByteArray ba = lines.first().toUtf8();
        const char* cmd[] = {"loadfile", ba.constData(), nullptr};
        int ret = mpv_command(mpv, cmd);
        if (ret < 0) {
            qWarning() << "[YTPlayer] mpv failed to load first resolved stream (ret=" << ret << "). URL:" << lines.first();
            loadFailed = true;
        }
    }

    // Для остальной части плейлиста — append-play
    if (!loadFailed && lines.size() > 1) {
        for (int i = 1; i < lines.size(); ++i) {
            QByteArray ba = lines.at(i).toUtf8();
            const char* cmd[] = {"loadfile", ba.constData(), "append-play", nullptr};
            int ret = mpv_command(mpv, cmd);
            if (ret < 0) {
                qWarning() << "[YTPlayer] failed to append playlist item" << i << "ret=" << ret << "URL:" << lines.at(i);
            }
        }
    }

    // Сбросим временные заголовки/куки (чтобы не влияли на другие запросы)
    mpv_set_option_string(mpv, "http-header-fields", "");
    if (!cookiesFile.isEmpty()) {
        mpv_set_option_string(mpv, "cookies", "no");
    }

    if (loadFailed) {
        emit errorOccurred("mpv failed to load resolved stream URL. See logs.");
        return;
    }

    playing = true;
    emit playbackStateChanged(true);
}

void YTPlayer::stop()
{
    if (!mpv) return;
    const char* cmd[] = {"stop", nullptr};
    mpv_command(mpv, cmd);

    // Сбросим временные заголовки и cookie-настройки, чтобы не влияли на последующие запросы
    mpv_set_option_string(mpv, "http-header-fields", "");
    if (!cookiesFile.isEmpty()) {
        mpv_set_option_string(mpv, "cookies", "no");
    }
    qDebug() << "[YTPlayer] stop() called — headers/cookies cleared.";

    playing = false;
    emit playbackStateChanged(false);
}

void YTPlayer::togglePlayback()
{
    if (!mpv) return;
    const char* cmd[] = {"cycle", "pause", nullptr};
    mpv_command(mpv, cmd);
    playing = !playing;
    emit playbackStateChanged(playing);
}

void YTPlayer::setVolume(int value)
{
    if (!mpv) return;
    if (currentVolume == value) return;

    currentVolume = value;
    double vol = value;
    mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &vol);

    // Сохраняем сразу в QSettings
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    settings.setValue("volume", currentVolume);

    emit volumeChanged(value);
}

int YTPlayer::volume() const
{
    return currentVolume;
}

void YTPlayer::setMuted(bool muted)
{
    if (!mpv) return;
    int flag = muted ? 1 : 0;
    mpv_set_property(mpv, "mute", MPV_FORMAT_FLAG, &flag);
    mutedState = muted;
    emit mutedChanged(mutedState);
}

bool YTPlayer::isMuted() const
{
    return mutedState;
}

void YTPlayer::handleMpvEvents()
{
    if (!mpv) return;

    while (true) {
        mpv_event* ev = mpv_wait_event(mpv, 0);
        if (!ev) break;
        if (ev->event_id == MPV_EVENT_NONE) break;

        switch (ev->event_id) {
        case MPV_EVENT_END_FILE: {
            mpv_event_end_file* end_file = static_cast<mpv_event_end_file*>(ev->data);
            if (end_file->reason == MPV_END_FILE_REASON_EOF) {
                qDebug() << "[YTPlayer] End of file reached (EOF)";
                playing = false;
                emit playbackStateChanged(false);
            } else if (end_file->reason == MPV_END_FILE_REASON_ERROR) {
                int err = end_file->error;
                const char* errStr = mpv_error_string ? mpv_error_string(err) : "unknown";
                qWarning() << "[YTPlayer] Error playing media, reason:" << err << "error string:" << errStr;
                playing = false;
                emit playbackStateChanged(false);
                emit errorOccurred(QString("Failed to play media: mpv error %1").arg(QString::fromUtf8(errStr)));
            } else {
                qDebug() << "[YTPlayer] Playback stopped, reason:" << end_file->reason;
                playing = false;
                emit playbackStateChanged(false);
            }


            break;
        }

        case MPV_EVENT_SHUTDOWN:
            qWarning() << "[YTPlayer] mpv shutdown";
            playing = false;
            emit errorOccurred("mpv shutdown");
            break;

        case MPV_EVENT_LOG_MESSAGE: {
            // В некоторых сборках libmpv поля prefix/level/text — const char*
            mpv_event_log_message* msg = static_cast<mpv_event_log_message*>(ev->data);
            const char* prefixC = msg->prefix ? msg->prefix : "";
            const char* levelC  = msg->level  ? msg->level  : "";
            const char* textC   = msg->text   ? msg->text   : "";

            QString prefix = QString::fromUtf8(prefixC);
            QString text   = QString::fromUtf8(textC);
            QString logMsg = QString("[%1] %2: %3").arg(prefix, QString::fromUtf8(levelC), text).trimmed();

            // сравниваем строковые уровни логов: fatal/error/warn/info/...
            if (std::strcmp(levelC, "fatal") == 0 || std::strcmp(levelC, "error") == 0) {
                qWarning() << "[YTPlayer] mpv error log:" << logMsg;
                emit errorOccurred(QString("mpv error: %1").arg(logMsg));
            } else if (std::strcmp(levelC, "warn") == 0) {
                qWarning() << "[YTPlayer] mpv warn log:" << logMsg;
            } else {
                qDebug() << "[YTPlayer] mpv log:" << logMsg;
            }
            break;
        }

        default:
            qDebug() << "[YTPlayer] Unhandled mpv event:" << ev->event_id;
            break;
        } // switch
    } // while
}

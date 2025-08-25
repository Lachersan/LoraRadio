#include "YTPlayer.h"
#include <QDebug>
#include <QSettings>
#include <QTimer>
#include <QRegularExpression>

static void on_mpv_wakeup(void* ctx) {
    auto self = static_cast<YTPlayer*>(ctx);
    self->processMpvEvents();
}

YTPlayer::YTPlayer(const QString& cookiesFile_, QObject* parent)
    : AbstractPlayer(parent)
    , cookiesFile(cookiesFile_)
{
    mpv = mpv_create();
    if (!mpv) {
        emit errorOccurred("Failed to create mpv handle");
        return;
    }
    mpv_set_option_string(mpv, "ytdl-path", "thirdparty/libmpv/yt-dlp.exe");
    mpv_set_option_string(mpv, "ytdl", "yes");
    mpv_set_option_string(mpv, "ytdl-format", "bestaudio/best"); // best в fallback, если bestaudio не работает
    mpv_set_option_string(mpv, "cache", "no"); // Отключаем кэш для live-стримов
    mpv_set_option_string(mpv, "vid", "no"); // Отключаем видео для audio-only
    mpv_set_option_string(mpv, "demuxer-lavf-o", "protocol_whitelist=file,http,https,tls,crypto,dash"); // Поддержка DASH

    // Объединяем ytdl-raw-options
    QString rawOpts = "yes-playlist=,format-sort=+res:720"; // Для live-стримов, сортировка по качеству
    if (!cookiesFile.isEmpty()) {
        qDebug() << "[YTPlayer] Using cookies file:" << cookiesFile;
        rawOpts += QString(",cookies=%1").arg(cookiesFile);
    }
    mpv_set_option_string(mpv, "ytdl-raw-options", rawOpts.toUtf8().constData());

    // Включаем подробные логи mpv для отладки
    mpv_set_option_string(mpv, "msg-level", "all=debug");

    if (mpv_initialize(mpv) < 0) {
        emit errorOccurred("mpv initialization failed");
        return;
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    currentVolume = settings.value("volume", 50).toInt();
    qDebug() << "[YTPlayer] Loaded volume from QSettings:" << currentVolume;
    double volume = currentVolume;
    mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &volume);
    emit volumeChanged(currentVolume);

    mpv_set_wakeup_callback(mpv, on_mpv_wakeup, this);
}

YTPlayer::~YTPlayer() {
    if (mpv) {
        mpv_terminate_destroy(mpv);
        mpv = nullptr;
    }
}

void YTPlayer::play(const QString& url)
{
    if (!mpv) {
        qWarning() << "[YTPlayer] mpv handle is null";
        emit errorOccurred("mpv handle is null");
        return;
    }

    if (url.isEmpty()) {
        qWarning() << "[YTPlayer] Empty URL provided";
        emit errorOccurred("Empty URL provided");
        return;
    }

    QString normalized = url.trimmed();
    QRegularExpression rx("^[A-Za-z0-9_-]{11}$");
    if (rx.match(normalized).hasMatch()) {
        normalized = QStringLiteral("https://www.youtube.com/watch?v=%1").arg(normalized);
    }

    qDebug() << "[YTPlayer] Attempting to play URL:" << normalized;

    // Пробуем сначала bestaudio, затем fallback на best
    QByteArray ba = normalized.toUtf8();
    const char* cmd[] = {"loadfile", ba.constData(), nullptr};
    int ret = mpv_command(mpv, cmd);
    qDebug() << "[YTPlayer] mpv_command returned:" << ret;

    if (ret < 0) {
        qWarning() << "[YTPlayer] mpv_command failed with code:" << ret << "for URL:" << normalized;
        // Fallback: пробуем формат "best" вместо "bestaudio"
        mpv_set_option_string(mpv, "ytdl-format", "best");
        ret = mpv_command(mpv, cmd);
        qDebug() << "[YTPlayer] Fallback mpv_command returned:" << ret;
        if (ret < 0) {
            emit errorOccurred(QString("mpv failed to load URL: %1 (ret=%2)").arg(normalized, QString::number(ret)));
            return;
        }
        // Восстанавливаем bestaudio после fallback
        mpv_set_option_string(mpv, "ytdl-format", "bestaudio/best");
    }

    playing = true;
    emit playbackStateChanged(true);
}

void YTPlayer::stop()
{
    if (!mpv) return;

    const char* cmd[] = {"stop", nullptr};
    mpv_command(mpv, cmd);

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

    if (currentVolume != value) {
        qDebug() << "[YTPlayer] setVolume called with value:" << value;
        currentVolume = value;
        double volume = value;
        mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &volume);
        qDebug() << "[YTPlayer] Volume set to:" << currentVolume;
        emit volumeChanged(value);
    }
}

int YTPlayer::volume() const
{
    return currentVolume;
}

void YTPlayer::setMuted(bool muted)
{
    if (!mpv) return;

    mutedState = muted;
    mpv_set_property(mpv, "mute", MPV_FORMAT_FLAG, &mutedState);
    emit mutedChanged(mutedState);
}

bool YTPlayer::isMuted() const
{
    return mutedState;
}

void YTPlayer::processMpvEvents()
{
    if (!mpv) return;

    while (true) {
        mpv_event* ev = mpv_wait_event(mpv, 0);
        if (ev->event_id == MPV_EVENT_NONE)
            break;

        switch (ev->event_id) {
        case MPV_EVENT_END_FILE: {
            mpv_event_end_file* end_file = static_cast<mpv_event_end_file*>(ev->data);
            if (end_file->reason == MPV_END_FILE_REASON_EOF) {
                qDebug() << "[YTPlayer] End of file reached, possibly next item in playlist";
                playing = true;
                emit playbackStateChanged(true);
            } else if (end_file->reason == MPV_END_FILE_REASON_ERROR) {
                qWarning() << "[YTPlayer] Error playing media, reason:" << end_file->error
                           << "error string:" << mpv_error_string(end_file->error);
                playing = false;
                emit playbackStateChanged(false);
                emit errorOccurred("Failed to play media: mpv error " + QString(mpv_error_string(end_file->error)));
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
            mpv_event_log_message* msg = static_cast<mpv_event_log_message*>(ev->data);
            QString logMsg = QString("[%1] %2: %3").arg(msg->prefix, msg->level, msg->text).trimmed();
            if (strcmp(msg->level, "error") == 0 || strcmp(msg->level, "fatal") == 0) {
                qWarning() << "[YTPlayer] mpv error log:" << logMsg;
                emit errorOccurred(QString("mpv error: %1").arg(logMsg));
            } else if (strcmp(msg->level, "debug") == 0 || strcmp(msg->level, "info") == 0) {
                qDebug() << "[YTPlayer] mpv log:" << logMsg;
            }
            break;
        }
        default:
            qDebug() << "[YTPlayer] Unhandled mpv event:" << ev->event_id;
            break;
        }
    }
}
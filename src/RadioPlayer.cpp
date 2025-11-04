#include "RadioPlayer.h"
#include "StationManager.h"
#include <QUrl>
#include <QDebug>

RadioPlayer::RadioPlayer(StationManager* stations, QObject* parent)
    : AbstractPlayer(parent)
    , m_stations(stations)
    , m_player(new QMediaPlayer(this))
    , m_audio(new QAudioOutput(this))
{
    {
            QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                               "MyApp", "LoraRadio");
            m_currentVolume = settings.value("volume", 50).toInt();
          }
    m_player->setAudioOutput(m_audio);
    m_audio->setVolume(m_currentVolume / 100.0);
    emit volumeChanged(m_currentVolume);

    connect(m_stations, &StationManager::stationsChanged,
            this, &RadioPlayer::emitStationList);


    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &RadioPlayer::onMediaStatusChanged);

    connect(m_player, &QMediaPlayer::errorOccurred,
            this, &RadioPlayer::onErrorOccurred);

    emitStationList();
}

RadioPlayer::~RadioPlayer() {
    m_player->stop();
}

void RadioPlayer::emitStationList() {
    QStringList names;
    for (const auto& st : m_stations->stations())
        names << st.name;
    emit stationsChanged(names);
}

void RadioPlayer::play(const QString& url) {
    m_player->stop();
    m_player->setSource(QUrl(url));
    m_player->play();
    emit playbackStateChanged(true);
}

void RadioPlayer::stop() {
    m_player->stop();
    emit playbackStateChanged(false);
}

void RadioPlayer::togglePlayback() {
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
        emit playbackStateChanged(false);
    } else if (m_currentIndex >= 0) {
        m_player->play();
        emit playbackStateChanged(true);
    }
}

void RadioPlayer::setVolume(int value)
{
    if (m_currentVolume != value) {
        m_currentVolume = value;
        m_audio->setVolume(value / 100.0);
        qDebug() << "[RadioPlayer] Volume set to:" << m_currentVolume;
        emit volumeChanged(value);
    }
}

int RadioPlayer::volume() const {
    return m_currentVolume;
}

void RadioPlayer::setMuted(bool muted) {
    m_audio->setMuted(muted);
    emit mutedChanged(muted);
}

bool RadioPlayer::isMuted() const {
    return m_audio->isMuted();
}


void RadioPlayer::onMediaStatusChanged(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::StalledMedia ||
        status == QMediaPlayer::EndOfMedia   ||
        status == QMediaPlayer::InvalidMedia)
    {
        if (status == QMediaPlayer::EndOfMedia)
            emit playbackStateChanged(false);
    }
}

void RadioPlayer::onErrorOccurred(QMediaPlayer::Error, const QString& errorString) {
    qWarning() << "Stream error:" << errorString;
    emit errorOccurred(errorString);
}

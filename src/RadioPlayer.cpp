#include "RadioPlayer.h"
#include "AutoStartRegistry.h"
#include <QUrl>

RadioPlayer::RadioPlayer(StationManager *stations, QObject *parent)
    : QObject(parent),
      m_stations(stations),
      m_player(new QMediaPlayer(this)),
      m_audio(new QAudioOutput(this)),
      m_settings("MyApp", "LoraRadio")
{
    m_player->setAudioOutput(m_audio);

    m_currentVolume = m_settings.value("audio/volume", 50).toInt();
    m_audio->setVolume(m_currentVolume / 100.0);

    connect(m_stations, &StationManager::stationsChanged,
            this, &RadioPlayer::emitStationList);

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &RadioPlayer::reconnectStation);

    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this,      &RadioPlayer::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::errorOccurred,
            this,      &RadioPlayer::onErrorOccurred);



    emitStationList();
}

void RadioPlayer::emitStationList()
{
    QStringList names;
    for (auto &st : m_stations->stations())
        names << st.name;

    qDebug() << "RadioPlayer emits stationsChanged:" << names;
    emit stationsChanged(names);
}

QStringList RadioPlayer::stationNames() const {
    QStringList names;
    for (auto &st : m_stations->stations())
        names << st.name;
    return names;
}


void RadioPlayer::selectStation(int index)
{
    if (index < 0 || index >= m_stations->stations().size()) return;
    m_currentIndex = index;
    const Station &st = m_stations->stations().at(index);
    m_player->stop();
    m_player->setSource(QUrl(st.url));
    m_player->play();
}

void RadioPlayer::addStation(const Station &st)
{
    qDebug() << "Adding station" << st.name;

    m_stations->addStation(st);
    m_stations->save();
    emitStationList();

}

void RadioPlayer::removeStation(int index)
{
    if (index < 0) return;
    m_stations->removeStation(index);
    m_stations->save();
}

void RadioPlayer::updateStation(int index, const Station &st)
{
    if (index < 0) return;
    m_stations->updateStation(index, st);
    m_stations->save();
}

void RadioPlayer::togglePlayback()
{
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
    } else if (m_currentIndex >= 0) {
        m_player->play();
    }
}

bool RadioPlayer::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}


void RadioPlayer::reconnectStation()
{
    if (m_currentIndex < 0)
        return;
    selectStation(m_currentIndex);
}

void RadioPlayer::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::StalledMedia
     || status == QMediaPlayer::EndOfMedia
     || status == QMediaPlayer::InvalidMedia)
    {
        scheduleReconnect();
    }
}

void RadioPlayer::onErrorOccurred(QMediaPlayer::Error error,
                                  const QString &errorString)
{
    Q_UNUSED(error);
    qWarning() << "Stream error:" << errorString;
    scheduleReconnect();
}

void RadioPlayer::scheduleReconnect()
{
    if (m_reconnectTimer.isActive())
        return;

    m_reconnectTimer.start(2000);
}


void RadioPlayer::changeVolume(int value)
{
    m_currentVolume = value;
    m_settings.setValue("audio/volume", value);
    m_audio->setVolume(value / 100.0);
    emit volumeChanged(value);
}

void RadioPlayer::setMuted(bool muted)
{
    m_audio->setMuted(muted);
}

bool RadioPlayer::isMuted() const
{
    return m_audio->isMuted();
}

void RadioPlayer::toggleAutostart(bool enabled)
{
    m_settings.setValue("autostart/enabled", enabled);
    AutoStartRegistry::setEnabled(enabled);
}
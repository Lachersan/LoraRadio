#include "SwitchPlayer.h"

#include <QRegularExpression>

#include "RadioPlayer.h"
#include "YTPlayer.h"

SwitchPlayer::SwitchPlayer(RadioPlayer* radio, YTPlayer* yt, QObject* parent)
    : AbstractPlayer(parent), m_radio(radio), m_yt(yt)
{
    if (m_radio && m_radio->parent() == nullptr) m_radio->setParent(this);
    if (m_yt && m_yt->parent() == nullptr) m_yt->setParent(this);

    if (m_radio)
    {
        connect(m_radio, &AbstractPlayer::playbackStateChanged, this,
                &SwitchPlayer::onChildPlaybackStateChanged);
        connect(m_radio, &AbstractPlayer::volumeChanged, this,
                &SwitchPlayer::onChildVolumeChanged);
        connect(m_radio, &AbstractPlayer::mutedChanged, this,
                &SwitchPlayer::onChildMutedChanged);
        connect(m_radio, &AbstractPlayer::errorOccurred, this,
                &SwitchPlayer::onChildError);
    }
    if (m_yt)
    {
        connect(m_yt, &AbstractPlayer::playbackStateChanged, this,
                &SwitchPlayer::onChildPlaybackStateChanged);
        connect(m_yt, &AbstractPlayer::volumeChanged, this,
                &SwitchPlayer::onChildVolumeChanged);
        connect(m_yt, &AbstractPlayer::mutedChanged, this,
                &SwitchPlayer::onChildMutedChanged);
        connect(m_yt, &AbstractPlayer::errorOccurred, this,
                &SwitchPlayer::onChildError);
    }
}

SwitchPlayer::~SwitchPlayer() = default;

bool SwitchPlayer::looksLikeYouTube(const QString& url)
{
    if (url.contains("youtube.com") || url.contains("youtu.be")) return true;
    QRegularExpression rx("^[A-Za-z0-9_-]{11}$");
    return rx.match(url).hasMatch();
}

void SwitchPlayer::play(const QString& url)
{
    if (looksLikeYouTube(url))
    {
        if (m_radio) m_radio->stop();
        if (m_yt)
        {
            m_currentSource = Source::YouTube;
            m_yt->play(url);
            return;
        }
        else
        {
            emit errorOccurred("YouTube player not available");
        }
    }
    else
    {
        if (m_yt) m_yt->stop();
        if (m_radio)
        {
            m_currentSource = Source::Radio;
            m_radio->play(url);
        }
        else
        {
            emit errorOccurred("Radio player not available");
        }
    }
}

void SwitchPlayer::stop()
{
    if (m_radio) m_radio->stop();
    if (m_yt) m_yt->stop();
    m_currentSource = Source::None;
}

void SwitchPlayer::togglePlayback()
{
    if (m_currentSource == Source::YouTube && m_yt)
        m_yt->togglePlayback();
    else if (m_currentSource == Source::Radio && m_radio)
        m_radio->togglePlayback();
    else if (m_radio)
        m_radio->togglePlayback();  // fallback
}

void SwitchPlayer::setVolume(int value)
{
    if (m_radio) m_radio->setVolume(value);
    if (m_yt) m_yt->setVolume(value);
    emit volumeChanged(value);
}

int SwitchPlayer::volume() const
{
    if (m_radio) return m_radio->volume();
    if (m_yt) return m_yt->volume();
    return 0;
}

void SwitchPlayer::setMuted(bool muted)
{
    if (m_radio) m_radio->setMuted(muted);
    if (m_yt) m_yt->setMuted(muted);
    emit mutedChanged(muted);
}

bool SwitchPlayer::isMuted() const
{
    if (m_radio) return m_radio->isMuted();
    if (m_yt) return m_yt->isMuted();
    return false;
}

void SwitchPlayer::onChildPlaybackStateChanged(bool playing)
{
    emit playbackStateChanged(playing);
}
void SwitchPlayer::onChildVolumeChanged(int v)
{
    emit volumeChanged(v);
}
void SwitchPlayer::onChildMutedChanged(bool m)
{
    emit mutedChanged(m);
}
void SwitchPlayer::onChildError(const QString& err)
{
    emit errorOccurred(err);
}

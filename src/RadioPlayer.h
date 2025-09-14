
#include "../include/AbstractPlayer.h"
#include "AutoStartRegistry.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSettings>
#include <QTimer>

class StationManager;

class RadioPlayer : public AbstractPlayer {
    Q_OBJECT
public:
    explicit RadioPlayer(StationManager* stations, QObject* parent = nullptr);
    ~RadioPlayer() override;

    void play(const QString& url) override;
    void stop() override;
    void togglePlayback() override;

    void setVolume(int value) override;
    int volume() const override;
    void setMuted(bool muted) override;
    bool isMuted() const override;

    signals:
        void stationsChanged(const QStringList& names);

private slots:
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onErrorOccurred(QMediaPlayer::Error error,
                         const QString& errorString);
    void reconnectStation();

private:
    void emitStationList();
    void scheduleReconnect();

    StationManager* m_stations;
    QMediaPlayer*   m_player;
    QAudioOutput*   m_audio;
    QTimer          m_reconnectTimer;
    int             m_currentVolume;
    int             m_currentIndex{-1};
};

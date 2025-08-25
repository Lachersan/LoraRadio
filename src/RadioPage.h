#pragma once

#include <QWidget>
#include <QPointer>
#include <QListWidget>
#include "stationmanager.h"
#include "../iclude/AbstractPlayer.h"

class QListWidget;
class QSlider;
class QSpinBox;
class IconButton;

class RadioPage : public QWidget {
    Q_OBJECT
public:
    explicit RadioPage(StationManager* stations,
                       AbstractPlayer* player,
                       QWidget* parent = nullptr);

    int currentStationIndex() const { return m_listWidget->currentRow(); }
    signals:
    void requestAdd();
    void requestRemove(int index);
    void requestUpdate(int index);
    void playStation(int index);
    void reconnectRequested();
    void togglePlayback();
    void prev();
    void next();
    void reconnect();
    void volumeChanged(int value);
    void muteToggled(bool nowMuted);

public slots:
    void setStations(const QStringList& names);
    void setPlaybackState(bool isPlaying);
    void setVolume(int value);
    void setMuted(bool muted);
    void onVolumeChanged(int value);
    void stopPlayback();

private:
    StationManager*    m_stations;
    AbstractPlayer*    m_player;
    QListWidget*       m_listWidget;
    QSlider*           m_volumeSlider;
    QSpinBox*          m_volumeSpin;
    QPointer<IconButton>        m_btnAdd;
    QPointer<IconButton>        m_btnRemove;
    QPointer<IconButton>        m_btnUpdate;
    QPointer<IconButton>        m_btnPrev;
    QPointer<IconButton>        m_btnPlay;
    QPointer<IconButton>        m_btnNext;
    QPointer<IconButton>        m_btnMute;
    QPointer<IconButton>        m_btnReconnect;

    void setupUi();
    void setupConnections();
    bool m_isPlaying = false;
};



#pragma once

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSettings>
#include "StationManager.h"

class RadioPlayer : public QObject {
    Q_OBJECT

public:
    explicit RadioPlayer(StationManager *stations, QObject *parent = nullptr);
    QStringList stationNames() const;


public slots:
    void selectStation(int index);
    void addStation(const Station &st);
    void removeStation(int index);
    void updateStation(int index, const Station &st);
    void reconnectStation();
    void changeVolume(int value);
    void toggleAutostart(bool enabled);

    signals:
    void stationsChanged(const QStringList &names);
    void volumeChanged(int value);
    void errorOccurred(const QString &message);

private:
    StationManager    *m_stations;
    QMediaPlayer      *m_player;
    QAudioOutput      *m_audio;
    QSettings          m_settings;
    int                m_currentVolume = 50;
    int m_currentIndex = -1;


    void emitStationList();
};
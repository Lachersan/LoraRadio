#pragma once

#include <QWidget>
#include <QSpinBox>


class QListWidget;
class QPushButton;
class QSlider;
class QSpinBox;
class StationManager;

class QuickControlPopup : public QWidget {
    Q_OBJECT
public:
    explicit QuickControlPopup(StationManager *stations, QWidget *parent = nullptr);
public slots:
    void setCurrentStation(int index);
    void setVolume(int value);
    void updateStations();

    signals:
    void stationSelected(int index);
    void reconnectRequested();
    void volumeChanged(int value);

private:
    StationManager *m_stations;
    QListWidget *listWidget;
    QPushButton *btnReconnect;
    QSlider     *volumeSlider;
    QSpinBox    *volumeSpin;
};
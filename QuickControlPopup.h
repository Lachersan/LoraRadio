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
    void setVolume(int value);

    signals:
    void stationSelected(int index);
    void reconnectRequested();
    void volumeChanged(int value);

private:
    QListWidget *listWidget;
    QPushButton *btnReconnect;
    QSlider     *volumeSlider;
    QSpinBox    *volumeSpin;
};
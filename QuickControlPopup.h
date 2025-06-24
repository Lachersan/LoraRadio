#pragma once

#include <QWidget>
class QListWidget;
class QPushButton;
class QSlider;
class StationManager;

class QuickControlPopup : public QWidget {
    Q_OBJECT
public:
    explicit QuickControlPopup(StationManager *stations, QWidget *parent = nullptr);

    signals:
        void stationSelected(int index);
    void reconnectRequested();
    void volumeChanged(int value);

private:
    QListWidget *listWidget;
    QPushButton *btnReconnect;
    QSlider     *volumeSlider;
};
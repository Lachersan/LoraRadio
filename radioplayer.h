#ifndef RADIOPLAYER_H
#define RADIOPLAYER_H

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSystemTrayIcon>
#include "trayiconmanager.h"
#include <QSlider>
#include <QSpinBox>
#include <QHBoxLayout>

class RadioPlayer : public QWidget {
    Q_OBJECT
public:
    explicit RadioPlayer(QWidget *parent = nullptr);
protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QMediaPlayer *player;
    TrayIconManager *m_trayManager;
    QAudioOutput *audioOutput;
    QSystemTrayIcon *trayIcon;
    QSlider   *volumeSlider;
    QSpinBox  *volumeSpin;
    QHBoxLayout *controlLayout;

private slots:
    void onVolumeChanged(int value);
};



#endif // RADIOPLAYER_H
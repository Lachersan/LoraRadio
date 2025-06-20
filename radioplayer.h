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
#include <QSettings>
#include <QApplication>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QDebug>

class RadioPlayer : public QWidget {
    Q_OBJECT
public:
    explicit RadioPlayer(QWidget *parent = nullptr);
    void setStreamUrl(const QString &url);
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
    QSettings settings;

private slots:
    void onVolumeChanged(int value);
};



#endif // RADIOPLAYER_H
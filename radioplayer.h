#ifndef RADIOPLAYER_H
#define RADIOPLAYER_H

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSystemTrayIcon>
#include "trayiconmanager.h"

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
};

#endif // RADIOPLAYER_H
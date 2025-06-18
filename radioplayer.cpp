#include "radioplayer.h"
#include <QApplication>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QDebug>

RadioPlayer::RadioPlayer(QWidget *parent) : QWidget(parent) {
    audioOutput = new QAudioOutput(this);
    player = new QMediaPlayer(this);
    player->setAudioOutput(audioOutput);

    player->setSource(QUrl("https://stream.laut.fm/chillradioluka"));
    audioOutput->setVolume(0.3);
    player->play();

    connect(player, &QMediaPlayer::errorOccurred, this, [](QMediaPlayer::Error error){
        qDebug() << "Player error:" << error;
    });

    m_trayManager = new TrayIconManager(this);
    connect(m_trayManager, &TrayIconManager::showRequested, this, [&](){
        this->showNormal();
        this->raise();
        this->activateWindow();
    });
    connect(m_trayManager, &TrayIconManager::quitRequested, qApp, &QApplication::quit);

    this->hide();
}

void RadioPlayer::closeEvent(QCloseEvent *event) {
    this->hide();
    event->ignore();
}
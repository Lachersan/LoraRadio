#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QTranslator>
#include <QActionGroup>
#include <QStackedWidget>
#include <QToolButton>
#include "stationmanager.h"
#include "../include/AbstractPlayer.h"
#include "QuickControlPopup.h"
#include "SwitchPlayer.h"
#include "YTPlayer.h"  // Добавьте, если не включён через SwitchPlayer

class QListWidget;
class QSlider;
class QSpinBox;
class IconButton;
class QuickControlPopup;
class RadioPage;
class YouTubePage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(StationManager* stations,
                        AbstractPlayer* player,
                        QWidget* parent = nullptr);

    void onYouTubePlayRequested(const QString &url);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void playStation(int index);
    void onPlaybackStateChanged(bool isPlaying);
    void onVolumeChanged(int value);
    void onVolumeMuteClicked();
    void onAddClicked();
    void onRemoveClicked(int idx);
    void onUpdateClicked(int idx);
    void onPrevClicked();
    void onPlayClicked();
    void onNextClicked();
    void onReconnectClicked();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void switchLanguage(QAction* action);
    void onModeChanged(int newIndex);
    void onRadioPlayRequested(int localIdx);
    void onPlayerVolumeChanged(int value);

private:
    void setupUi();
    void setupTray();
    void setupConnections();
    int m_lastModeIndex = 0;
    bool m_isInitializing = true;
    int m_lastMode = 0;// 0 - Radio, 1 - YouTube
    int m_currentGlobalIdx = -1;

    QTabBar            *modeTabBar;
    QStackedWidget     *modeStack;
    RadioPage          *radioPage;
    YouTubePage        *ytPage;


    StationManager     *m_stations;
    AbstractPlayer     *m_player;
    QListWidget        *m_listWidget;
    QSlider            *m_volumeSlider;
    QSpinBox           *m_volumeSpin;
    IconButton         *m_volumeMute;
    IconButton         *m_btnPlay;
    IconButton         *m_btnNext;
    IconButton         *m_btnPrev;
    IconButton         *m_btnAdd;
    IconButton         *m_btnRemove;
    IconButton         *m_btnUpdate;
    IconButton         *m_btnReconnect;
    IconButton         *m_btnClose;
    IconButton         *m_btnMinimize;
    QMenu              *m_langMenu;
    QActionGroup       *m_langGroup;
    QToolButton        *m_btnLang;
    QuickControlPopup *m_quickPopup;
    QSystemTrayIcon    *m_trayIcon;
    QAction            *m_autostartAction;
    QPoint             m_dragPosition;
    QTranslator        m_translator;
    YTPlayer           *ytPlayer = nullptr;  // Новый член для доступа к YTPlayer
};
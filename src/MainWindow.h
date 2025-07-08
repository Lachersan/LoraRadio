#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QTranslator>
#include <QActionGroup>
#include <QToolButton>
#include "stationmanager.h"
#include "AbstractPlayer.h"
#include "QuickControlPopup.h"

class QListWidget;
class QSlider;
class QSpinBox;
class IconButton;
class QuickControlPopup;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(StationManager* stations,
                        AbstractPlayer* player,
                        QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onStationsChanged();
    void playStation(int index);
    void onPlaybackStateChanged(bool isPlaying);
    void onVolumeChanged(int value);
    void onVolumeMuteClicked();
    void onAddClicked();
    void onRemoveClicked();
    void onUpdateClicked();
    void onPrevClicked();
    void onPlayClicked();
    void onNextClicked();
    void onReconnectClicked();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void switchLanguage(QAction* action);

private:
    void setupUi();
    void setupTray();
    void setupConnections();

    QTabBar           *modeTabBar;
    StationManager*    m_stations;
    AbstractPlayer*    m_radio;
    QListWidget*       m_listWidget;
    QSlider*           m_volumeSlider;
    QSpinBox*          m_volumeSpin;
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
};

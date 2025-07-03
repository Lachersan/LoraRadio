#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>

#include "fluent_icons.h"
#include "StationManager.h"
#include "RadioPlayer.h"
#include "QuickControlPopup.h"


class StationManager;
class RadioPlayer;
class QListWidget;
class QSlider;
class QSpinBox;
class IconButton;
class QuickControlPopup;
class QSystemTrayIcon;
class QAction;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(StationManager *stations, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onStationsChanged();
    void onVolumeChanged(int value);
    void onAddClicked();
    void onRemoveClicked();
    void onUpdateClicked();
    void onReconnectClicked();
    void onTrayActivated(QSystemTrayIcon::ActivationReason);

private:
    void setupUi();
    void setupTray();
    void setupConnections();

    StationManager   *m_stations;
    RadioPlayer      *m_radio;
    QListWidget      *m_listWidget;
    QSlider          *m_volumeSlider;
    QSpinBox         *m_volumeSpin;
    IconButton       *m_btnAdd;
    IconButton       *m_btnRemove;
    IconButton       *m_btnUpdate;
    IconButton       *m_btnReconnect;
    QuickControlPopup* m_quickPopup;
    QSystemTrayIcon  *m_trayIcon;
    QAction          *m_autostartAction;

};
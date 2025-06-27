#ifndef RADIOPLAYER_H
#define RADIOPLAYER_H

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSystemTrayIcon>
#include <QSettings>
#include <QSpinBox>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "stationmanager.h"
#include "QuickControlPopup.h"
#include "IconButton.h"





QT_BEGIN_NAMESPACE
class QSlider;
class QSpinBox;
class QListWidget;
class QLineEdit;
class QPushButton;
class QMenu;
class QAction;
class QHBoxLayout;
class QFormLayout;
QT_END_NAMESPACE

class RadioPlayer : public QWidget {
    Q_OBJECT

public:
    explicit RadioPlayer(StationManager *stations, QWidget *parent = nullptr);
    void setStreamUrl(const QString &url);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onStationSelected(int row);
    void onAddStation();
    void onRemoveStation();
    void onUpdateStation();
    void onVolumeChanged(int value);
    void onReconnectStation();
    void showQuickPopup();
    void onAutostartToggled(bool enabled);




private:
    // — методы для “подчистки” конструктора
    void setupUi();
    void setupTrayIcon();
    void setupConnections();
    void refreshStationList();
    int currentVolume = 3;


    // — поля
    QMediaPlayer   *player;
    QAudioOutput   *audioOutput;
    QSystemTrayIcon* trayIcon;
    QSettings       settings;

    StationManager *m_stations;
    QuickControlPopup *quickPopup;


    // GUI-блок “громкость”
    QSlider   *volumeSlider;
    QSpinBox  *volumeSpin;
    QHBoxLayout *volumeLayout;


    // GUI-блок “станции”
    QListWidget *listWidget;
    QLineEdit   *editName;
    QLineEdit   *editUrl;
    IconButton *btnAdd;
    IconButton *btnRemove;
    IconButton *btnUpdate;
    IconButton *btnReconnect;



};
#endif // RADIOPLAYER_H
#pragma once

#include <QWidget>
#include <QPointer>
#include "stationmanager.h"
#include "../include/AbstractPlayer.h"

class QLineEdit;
class QListWidget;
class QSlider;
class QSpinBox;
class IconButton;

class AbstractPlayer;
class IconButton;

class YouTubePage : public QWidget
{
    Q_OBJECT
public:
    explicit YouTubePage(AbstractPlayer* player, QWidget* parent = nullptr);
    void setStations(const QVector<Station>& stations);

    signals:
    void searchRequested(const QString& query);
    void playRequested(const QString &url);

    // Навигация / управление плейлистом
    void prevRequested();
    void nextRequested();
    void reconnectRequested();

    // Обработчики CRUD для элементов списка (аналог RadioPage)
    void requestAdd();
    void requestRemove(int index);
    void requestUpdate(int index);

    // Громкость / mute / состояние воспроизведения
    void volumeChanged(int value);

public slots:
    void onVolumeChanged(int value);
    void setVolume(int value);
    void setPlaybackState(bool isPlaying);
    void setMuted(bool muted);
    void stopPlayback();

private:
    void setupUi();
    void setupConnections();
    bool m_isPlaying = false;

    // UI
    QLineEdit*   m_urlEdit = nullptr;
    QListWidget* m_resultList = nullptr;

    IconButton*  m_btnSearch = nullptr;
    IconButton*  m_btnAdd = nullptr;
    IconButton*  m_btnRemove = nullptr;
    IconButton*  m_btnUpdate = nullptr;

    IconButton*  m_btnPrev = nullptr;
    IconButton*  m_btnPlay = nullptr;
    IconButton*  m_btnNext = nullptr;
    IconButton*  m_btnReconnect = nullptr;
    IconButton*  m_btnMute = nullptr;

    QSlider*     m_volumeSlider = nullptr;
    QSpinBox*    m_volumeSpin = nullptr;

    QPointer<AbstractPlayer> m_player; // nullable pointer to player
};


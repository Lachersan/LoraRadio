#pragma once
#include "../iclude/AbstractPlayer.h"
#include <../include/client.h>

/*
  YTPlayer: thin wrapper over libmpv for audio-only YouTube playback.
  - processMpvEvents() помечен как слот, чтобы можно было безопасно вызывать
    его из mpv wakeup-callback через QMetaObject::invokeMethod(..., Qt::QueuedConnection).
  - Копирование запрещено (QObjects не копировать).
*/

class YTPlayer : public AbstractPlayer
{
    Q_OBJECT
public:
    explicit YTPlayer(const QString& cookiesFile_ = {}, QObject* parent = nullptr);
    ~YTPlayer() override;

    // Основные API (переопределены из AbstractPlayer)
    void play(const QString& url) override;
    void stop() override;
    void togglePlayback() override;
    void setVolume(int value) override;
    int volume() const override;
    void setMuted(bool muted) override;
    bool isMuted() const override;

public slots:
    // Должен выполняться в main (GUI) thread — вызывается через Qt::QueuedConnection
    void processMpvEvents();

private:
    mpv_handle* mpv = nullptr;     // libmpv handle (инициализируется в .cpp)
    QString cookiesFile;           // путь к cookies (если нужен для авторизации)
    bool playing = false;          // внутреннее состояние воспроизведения
    int currentVolume = 5;        // 0..100
    bool mutedState = false;       // флаг mute

    Q_DISABLE_COPY(YTPlayer)      // запрет копирования (QObject-совместимо)
};
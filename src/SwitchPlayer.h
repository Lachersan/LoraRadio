#pragma once
#include "../include/AbstractPlayer.h"

class RadioPlayer;
class YTPlayer;

// SwitchPlayer — прокси, который решает, куда посылать play()/stop().
// Наследует AbstractPlayer, поэтому MainWindow ничего не меняет.
class SwitchPlayer : public AbstractPlayer {
    Q_OBJECT
public:
    explicit SwitchPlayer(RadioPlayer* radio, YTPlayer* yt, QObject* parent = nullptr);
    ~SwitchPlayer() override;
    YTPlayer* getYTPlayer() const { return m_yt; }

public slots:
    void play(const QString& url) override;
    void stop() override;
    void togglePlayback() override;
    void setVolume(int value) override;
    int  volume() const override;
    void setMuted(bool muted) override;
    bool isMuted() const override;

private slots:
    void onChildPlaybackStateChanged(bool playing);
    void onChildVolumeChanged(int v);
    void onChildMutedChanged(bool m);
    void onChildError(const QString& err);

private:
    bool looksLikeYouTube(const QString& url) const;

    RadioPlayer* m_radio = nullptr;
    YTPlayer*    m_yt = nullptr;
    enum class Source { None, Radio, YouTube } m_currentSource = Source::None;
};

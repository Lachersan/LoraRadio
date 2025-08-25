#pragma once

#include <QObject>
#include <QString>

class AbstractPlayer : public QObject {
    Q_OBJECT
public:
    explicit AbstractPlayer(QObject* parent = nullptr)
        : QObject(parent) {}
    ~AbstractPlayer() override {}

    virtual void play(const QString& url) = 0;
    virtual void stop() = 0;
    virtual void togglePlayback() = 0;

    virtual void setVolume(int value) = 0;
    virtual int volume() const = 0;
    virtual void setMuted(bool muted) = 0;
    virtual bool isMuted() const = 0;

    signals:
        void playbackStateChanged(bool isPlaying);
    void volumeChanged(int value);
    void mutedChanged(bool muted);
    void errorOccurred(const QString& errorString);
};

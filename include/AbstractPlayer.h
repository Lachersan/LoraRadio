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

    virtual bool supportsFeature(const QString& feature) const { return false; }
    virtual void setCookiesFile(const QString& path) { Q_UNUSED(path); }
    virtual QString cookiesFile() const { return QString(); }
    virtual bool sendQuitAndWait(int waitMs = 1500) { Q_UNUSED(waitMs); return false; }

    signals:
    void playbackStateChanged(bool isPlaying);
    void volumeChanged(int value);
    void mutedChanged(bool muted);
    void errorOccurred(const QString& errorString);
    void featureChanged(const QString& feature, bool enabled);
};

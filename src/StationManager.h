#pragma once

#include <QCryptographicHash>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QVector>

struct Station
{
    QString name;
    QString url;
    QString type;  // "radio" или "youtube"
    int volume;
};

Q_DECLARE_METATYPE(Station)

class StationManager : public QObject
{
    Q_OBJECT
   public:
    explicit StationManager(const QString& jsonPath, QObject* parent = nullptr);

    const QVector<Station>& stations() const
    {
        return m_stations;
    }
    QVector<Station> stationsForType(const QString& type) const;

    static int lastStationIndex(const QString& type);
    void setLastStationIndex(int index, const QString& type);

   public slots:
    bool load();
    bool save() const;
    void addStation(const Station& st);
    void removeStation(int index);
    void updateStation(int index, const Station& st);
    static void saveStationVolume(const Station& st);
   signals:
    void stationsChanged();
    void stationAdded(int index);
    void stationRemoved(int index);
    void stationUpdated(int index);
    void lastStationIndexChanged(int index);

   private:
    QString m_jsonPath;
    QVector<Station> m_stations;
};

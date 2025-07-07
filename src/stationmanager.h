#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QSettings>

struct Station {
    QString name;
    QString url;
};

Q_DECLARE_METATYPE(Station)

class StationManager : public QObject {
    Q_OBJECT
public:
    explicit StationManager(const QString &jsonPath, QObject *parent = nullptr);

    const QVector<Station>& stations() const { return m_stations; }

    int  lastStationIndex() const;
    void setLastStationIndex(int index);

public slots:
    bool load();
    bool save() const;
    void addStation(const Station &st);
    void removeStation(int index);
    void updateStation(int index, const Station &st);

    signals:
    void stationsChanged();
    void stationAdded(int index);
    void stationRemoved(int index);
    void stationUpdated(int index);
    void lastStationIndexChanged(int index);


private:
    QString m_jsonPath;
    QVector<Station> m_stations;
    QSettings   m_settings;
};

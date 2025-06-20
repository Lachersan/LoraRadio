#pragma once

#include <QObject>
#include <QVector>
#include <QString>

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

public slots:
    bool load();                                 // загрузить из файла
    bool save() const;                           // сохранить в файл
    void addStation(const Station &st);          // добавить
    void removeStation(int index);               // удалить по индексу
    void updateStation(int index, const Station &st); // изменить

    signals:
        void stationsChanged();                      // общий сигнал
    void stationAdded(int index);
    void stationRemoved(int index);
    void stationUpdated(int index);

private:
    QString m_jsonPath;
    QVector<Station> m_stations;
};

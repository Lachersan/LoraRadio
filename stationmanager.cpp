#include "stationmanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>


StationManager::StationManager(const QString &jsonPath, QObject *parent)
    : QObject(parent)
    , m_jsonPath("F:/Project/untitled/stations.json"),
    m_settings("MyApp", "LoraRadio")    // организация/приложение

{
    if (load()) {
        // после загрузки JSON читаем из QSettings сохранённый индекс
        int idx = m_settings.value("player/lastIndex", -1).toInt();
        if (idx >= 0 && idx < m_stations.size()) {
            // сигнал, чтобы UI мог восстановить плейбек
            emit lastStationIndexChanged(idx);
        }
    }
}


bool StationManager::load() {
    QFile f(m_jsonPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isArray()) return false;

    m_stations.clear();
    for (auto v : doc.array()) {
        auto o = v.toObject();
        Station st;
        st.name = o.value("name").toString();
        st.url  = o.value("url").toString();
        if (!st.name.isEmpty() && !st.url.isEmpty())
            m_stations.append(st);
    }
    emit stationsChanged();
    return true;
}

bool StationManager::save() const {
    QJsonArray arr;
    for (auto &st : m_stations) {
        QJsonObject o;
        o.insert("name", st.name);
        o.insert("url", st.url);
        arr.append(o);
    }
    QJsonDocument doc(arr);
    QFile f(m_jsonPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

int StationManager::lastStationIndex() const
{
    return m_settings.value("player/lastIndex", -1).toInt();
}

void StationManager::setLastStationIndex(int index)
{
    if (index < 0 || index >= m_stations.size())
        index = -1;

    if (lastStationIndex() == index)
        return; // без изменения

    m_settings.setValue("player/lastIndex", index);
    emit lastStationIndexChanged(index);
}


void StationManager::addStation(const Station &st) {
    m_stations.append(st);
    int idx = m_stations.size() - 1;
    emit stationAdded(idx);
    emit stationsChanged();
}

void StationManager::removeStation(int index) {
    if (index < 0 || index >= m_stations.size()) return;
    m_stations.remove(index);
    emit stationRemoved(index);
    emit stationsChanged();
}

void StationManager::updateStation(int index, const Station &st) {
    if (index < 0 || index >= m_stations.size()) return;
    m_stations[index] = st;
    emit stationUpdated(index);
    emit stationsChanged();
}

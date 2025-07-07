#include "stationmanager.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QCoreApplication>
#include <QDir>

static QString defaultStationsPath()
{
    QString appDir = QCoreApplication::applicationDirPath();
    return QDir(appDir).filePath("stations.json");
}


StationManager::StationManager(const QString &jsonPath, QObject *parent)
    : QObject(parent)
    , m_jsonPath(jsonPath.isEmpty()
                  ? defaultStationsPath()
                  : jsonPath)
    , m_settings("MyApp", "LoraRadio")
{
    if (!QFile::exists(m_jsonPath)) {
        QDir().mkpath(QFileInfo(m_jsonPath).path());

        QFile res(":/stations_default.json");
        if (res.open(QIODevice::ReadOnly)) {
            QFile out(m_jsonPath);
            if (out.open(QIODevice::WriteOnly)) {
                out.write(res.readAll());
                out.close();
            }
            res.close();
        }
    }

    if (load()) {
        int idx = m_settings.value("player/lastIndex", -1).toInt();
        if (idx >= 0 && idx < m_stations.size())
            emit lastStationIndexChanged(idx);
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

bool StationManager::save() const
{
    QJsonArray arr;
    for (auto &st : m_stations) {
        QJsonObject o;
        o.insert("name", st.name);
        o.insert("url",  st.url);
        arr.append(o);
    }
    QJsonDocument doc(arr);

    QFile f(m_jsonPath);
    QFileInfo info(f);
    QDir().mkpath(info.path());

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    f.write(doc.toJson(QJsonDocument::Indented));
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
        return;

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

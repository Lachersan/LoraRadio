#include "StationManager.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCoreApplication>
#include <QSettings>

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

    load();
}

bool StationManager::load()
{
    QFile f(m_jsonPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isArray())
        return false;

    m_stations.clear();
    for (auto v : doc.array()) {
        auto o = v.toObject();
        Station st;
        st.name = o.value("name").toString();
        st.url  = o.value("url").toString();
        st.type = o.value("type").toString("radio"); // по умолчанию radio
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
        o.insert("type", st.type);
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

QVector<Station> StationManager::stationsForType(const QString& type) const
{
    QVector<Station> result;
    for (const auto &st : m_stations)
        if (st.type == type)
            result.append(st);
    return result;
}

int StationManager::lastStationIndex(const QString& type) const
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "MyApp", "LoraRadio");
    return settings.value(QString("player/%1/lastIndex").arg(type), -1).toInt();
}

void StationManager::setLastStationIndex(int index, const QString& type)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "MyApp", "LoraRadio");
    int prev = settings.value(QString("player/%1/lastIndex").arg(type), -1).toInt();
    if (prev == index)
        return;

    settings.setValue(QString("player/%1/lastIndex").arg(type), index);
    settings.sync();
    emit lastStationIndexChanged(index);
}

void StationManager::addStation(const Station &st)
{
    m_stations.append(st);
    int idx = m_stations.size() - 1;
    emit stationAdded(idx);
    emit stationsChanged();
}

void StationManager::removeStation(int index)
{
    if (index < 0 || index >= m_stations.size()) return;
    m_stations.remove(index);
    emit stationRemoved(index);
    emit stationsChanged();
}

void StationManager::updateStation(int index, const Station &st)
{
    if (index < 0 || index >= m_stations.size()) return;
    m_stations[index] = st;
    emit stationUpdated(index);
    emit stationsChanged();
}

#include "StationManager.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>

static QString defaultStationsPath()
{
    // Принудительно используем стандартную директорию настроек
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configPath.isEmpty()) {
        // Fallback для старых версий Qt или проблем с системой
        configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/MyApp";
    }

    // Создаем директорию если её нет
    QDir().mkpath(configPath);

    return QDir(configPath).filePath("stations.json");
}

static QString hashedUrl(const QString& url) {
    return QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Md5).toHex();
}

StationManager::StationManager(const QString &jsonPath, QObject *parent)
    : QObject(parent)
    , m_jsonPath(jsonPath.isEmpty()
                 ? defaultStationsPath()
                 : jsonPath)
{
    if (!QFile::exists(m_jsonPath)) {
        // Создаем директорию если её нет
        QDir().mkpath(QFileInfo(m_jsonPath).path());

        // Копируем дефолтный файл из ресурсов
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
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    for (auto v : doc.array()) {
        auto o = v.toObject();
        Station st;
        st.name = o.value("name").toString();
        st.url  = o.value("url").toString();
        st.type = o.value("type").toString("radio"); // по умолчанию radio
        if (!st.name.isEmpty() && !st.url.isEmpty()) {
            QString key = QString("volumes/%1/%2").arg(st.type).arg(hashedUrl(st.url));
            st.volume = settings.value(key, 50).toInt();
            m_stations.append(st);
        }
    }
    emit stationsChanged();
    return true;
}

void StationManager::saveStationVolume(const Station& st) {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    QString key = QString("volumes/%1/%2").arg(st.type).arg(hashedUrl(st.url));
    settings.setValue(key, st.volume);
    settings.sync();
    qDebug() << "[StationManager] Saved volume for" << st.url << ":" << st.volume;
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
    Station newSt = st;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    QString key = QString("volumes/%1/%2").arg(st.type).arg(hashedUrl(st.url));
    newSt.volume = settings.value(key, 50).toInt();
    m_stations.append(newSt);
    emit stationAdded(m_stations.size() - 1);
    emit stationsChanged();
}

void StationManager::removeStation(int index)
{
    qDebug() << "[StationManager] removeStation called with index:" << index;
    qDebug() << "[StationManager] Total stations before removal:" << m_stations.size();

    if (index < 0 || index >= m_stations.size()) {
        qWarning() << "[StationManager] Invalid index:" << index;
        return;
    }

    qDebug() << "[StationManager] Removing station:" << m_stations.at(index).name;
    m_stations.remove(index);
    qDebug() << "[StationManager] Total stations after removal:" << m_stations.size();

    emit stationRemoved(index);
    emit stationsChanged();
}

void StationManager::updateStation(int index, const Station &st)
{
    if (index < 0 || index >= m_stations.size()) return;

    const Station &old = m_stations.at(index);
    Station updated = st;

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "MyApp", "LoraRadio");
    if (old.url != st.url || old.type != st.type) {
        // Если URL или type изменились, загружаем volume для нового ключа (default 50)
        QString newKey = QString("volumes/%1/%2").arg(st.type).arg(hashedUrl(st.url));
        updated.volume = settings.value(newKey, 50).toInt();
    }

    m_stations[index] = updated;

    bool structuralChange = (old.name != st.name) || (old.url != st.url) || (old.type != st.type);
    if (structuralChange) {
        emit stationUpdated(index);
        emit stationsChanged();
    }
}
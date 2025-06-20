#include "AutoStartRegistry.h"
#include <QApplication>
#include "radioplayer.h"
#include "stationmanager.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    StationManager stations(qApp->applicationDirPath() + "/stations.json");
    RadioPlayer radio;
    QObject::connect(&stations, &StationManager::stationUpdated,
        [&](int idx){
            const auto &st = stations.stations().at(idx);
            radio.setStreamUrl(st.url);
        });
    // при старте поставим первую
    if (!stations.stations().isEmpty())
        radio.setStreamUrl(stations.stations().first().url);
    setAutoStartRegistry(true);
    return app.exec();
}
#include "AutoStartRegistry.h"
#include <QApplication>
#include "radioplayer.h"
#include "stationmanager.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    StationManager stations(qApp->applicationDirPath() + "/stations.json");
    RadioPlayer radio(&stations);

    radio.show();
    setAutoStartRegistry(false);
    return app.exec();
}
#include "AutoStartRegistry.h"
#include <QApplication>
#include "radioplayer.h"
#include "stationmanager.h"
#include "IconProvider.h"


int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    A();
    StationManager stations(qApp->applicationDirPath() + "/stations.json");
    RadioPlayer radio(&stations);

    radio.show();
    setAutoStartRegistry(false);
    return app.exec();
}
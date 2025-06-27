#include "AutoStartRegistry.h"
#include <QApplication>
#include "radioplayer.h"
#include "stationmanager.h"
#include "IconProvider.h"


int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyleSheet(R"(
      IconButton {
        background-color: transparent;
        border: none;
      }

    )");

    A();
    StationManager stations(qApp->applicationDirPath() + "/stations.json");
    RadioPlayer radio(&stations);

    radio.show();
    AutoStartRegistry::setEnabled(false);
    return app.exec();
}
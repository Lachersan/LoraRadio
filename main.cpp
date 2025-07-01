#include <QApplication>
#include "MainWindow.h"
#include "StationManager.h"
#include "FontLoader.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    loadFluentIconsFont();

    StationManager *stations = new StationManager("stations.json");
    MainWindow w(stations);
    w.show();

    return app.exec();
}
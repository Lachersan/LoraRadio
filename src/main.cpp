#include <QApplication>
#include <QFile>
#include "MainWindow.h"
#include "stationmanager.h"
#include "../iclude/FontLoader.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    loadFluentIconsFont();

    QFile f(":/styles/darkstyle.qss");
    if (f.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream ts(&f);
        app.setStyleSheet(ts.readAll());
    } else {
        qWarning("Cannot load qdarkstyle qss");
    }
    StationManager *stations = new StationManager("stations.json");
    MainWindow w(stations);
    w.show();

    return app.exec();
}
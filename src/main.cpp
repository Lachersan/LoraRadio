#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>
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

    QSettings settings("MyApp", "LoraRadio");
    QString lang = settings.value("language", "en").toString();

    QTranslator qtTrans;
    bool qtLoaded = qtTrans.load(
        QString("qt_%1").arg(lang),
        QLibraryInfo::path(QLibraryInfo::TranslationsPath)
    );
    qDebug() << "Qt translation loaded:" << qtLoaded;
    if (qtLoaded) {
        app.installTranslator(&qtTrans);
    }

    QTranslator appTrans;
    bool appLoaded = appTrans.load(
        QString(":/translations/translations/loraradio_%1.qm").arg(lang)
    );
    qDebug() << "App translation loaded:" << appLoaded;
    if (appLoaded) {
        app.installTranslator(&appTrans);
    }


    StationManager *stations = new StationManager("stations.json");
    MainWindow w(stations);
    w.show();

    return app.exec();
}
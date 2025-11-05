#include <QApplication>
#include <QLibraryInfo>
#include <QDir>
#include <QTextStream>
#include "MainWindow.h"
#include "StationManager.h"
#include "RadioPlayer.h"
#include "YTPlayer.h"
#include "SwitchPlayer.h"
#include "../include/FontLoader.h"
#include <QSettings>
#include <csignal>
#include <QMessageLogger>
#include <QFile>
#include <QDateTime>
#include <QStandardPaths>

// Custom message handler
void myMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QDir::separator() + "logs";
    QDir().mkpath(logDir);
    QFile logFile(logDir + QDir::separator() + "app_log.txt");
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        out << ts << " [" << type << "] " << msg << " (" << context.file << ":" << context.line << ")\n";
        logFile.close();
    }
    // Still output to console for debugging
    if (type == QtFatalMsg) abort();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qInstallMessageHandler(myMessageHandler);

    QCoreApplication::setOrganizationName("MyApp");
    QCoreApplication::setApplicationName("LoraRadio");

    QDir::setCurrent(QCoreApplication::applicationDirPath());

    loadFluentIconsFont();

    QFile f(":/styles/darkstyle.qss");
    if (f.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream ts(&f);
        app.setStyleSheet(ts.readAll());
    } else {
        qWarning("Cannot load qdarkstyle qss");
    }

    QString lang;
    {
        QSettings settings(
            QSettings::IniFormat,
            QSettings::UserScope,
            "MyApp",
            "LoraRadio"
        );
        lang = settings.value("language", "en").toString();
    }

    QTranslator qtTrans;
    bool qtLoaded = qtTrans.load(
        QString("qt_%1").arg(lang),
        QLibraryInfo::path(QLibraryInfo::TranslationsPath)
    );
    qDebug() << "Qt translation loaded:" << qtLoaded;
    if (qtLoaded)
        app.installTranslator(&qtTrans);

    QTranslator appTrans;
    bool appLoaded = appTrans.load(
        QString(":/translations/translations/loraradio_%1.qm").arg(lang)
    );
    qDebug() << "App translation loaded:" << appLoaded;
    if (appLoaded)
        app.installTranslator(&appTrans);

    StationManager* stations = new StationManager("");
    RadioPlayer* radio = new RadioPlayer(stations);
    YTPlayer* ytplayer = new YTPlayer(QStringLiteral(""), nullptr);

    SwitchPlayer* player = new SwitchPlayer(radio, ytplayer, nullptr);
    qDebug() << "[main] Created SwitchPlayer at" << player;

    MainWindow w(stations, player);

    int result = app.exec();

    {
        QSettings settings(
            QSettings::IniFormat,
            QSettings::UserScope,
            "MyApp",
            "LoraRadio"
        );
        settings.setValue("lastExitCode", result);
        settings.sync();
    }

    return result;
}

#include "AutoStartRegistry.h"
#include <QSettings>
#include <QCoreApplication>
#include <QDir>

bool AutoStartRegistry::isEnabled()
{
#ifdef Q_OS_WIN
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                  QSettings::NativeFormat);

    const QString appName = QCoreApplication::applicationName();
    return reg.contains(appName);
#else
    return false;
#endif
}

void AutoStartRegistry::setEnabled(bool enable)
{
#ifdef Q_OS_WIN
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                  QSettings::NativeFormat);

    const QString appName = QCoreApplication::applicationName();
    const QString appPath = QDir::toNativeSeparators(
        QCoreApplication::applicationFilePath()
    );

    if (enable) {
        reg.setValue(appName, "\"" + appPath + "\"");
    } else {
        reg.remove(appName);
    }
#endif
}

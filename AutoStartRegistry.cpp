#include "AutoStartRegistry.h"
#include <QDir>
#include <QCoreApplication>
#include <QSettings>

void setAutoStartRegistry(bool enable) {
    // Ключ для автозапуска текущего пользователя
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                  QSettings::NativeFormat);

    const QString appName = QCoreApplication::applicationName();
    const QString appPath = QDir::toNativeSeparators(
        QCoreApplication::applicationFilePath()
    );

    if (enable) {
        // Включаем автозапуск
        reg.setValue(appName, "\"" + appPath + "\"");
    } else {
        // Отключаем автозапуск
        reg.remove(appName);
    }
}
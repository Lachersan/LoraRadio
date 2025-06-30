#pragma once
#include <QFontDatabase>
#include <QDebug>

inline QString loadFluentIconsFont()
{
    // статическая переменная, чтобы шрифт не пытался грузиться заново
    static QString fluentFamily;

    if (!fluentFamily.isEmpty())
        return fluentFamily;

    // Попытка загрузить шрифт из ресурса
    const char* path = ":/fonts/FluentSystemIcons-Filled.ttf";
    qDebug() << "Trying to load font from" << path;
    int id = QFontDatabase::addApplicationFont(path);
    qDebug() << " addApplicationFont returned ID =" << id;
    if (id < 0) {
        qWarning() << "  → addApplicationFont failed!";
        return QString{};
    }

    QStringList fams = QFontDatabase::applicationFontFamilies(id);
    qDebug() << " Loaded families =" << fams;
    if (fams.isEmpty()) {
        qWarning() << "  → No families found in font ID";
        return QString{};
    }

    fluentFamily = fams.first();
    qDebug() << " Using fluentFamily =" << fluentFamily;
    return fluentFamily;
}

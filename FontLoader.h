#pragma once
#include <QFontDatabase>
#include <QDebug>

inline QString loadFluentIconsFont()
{
    static QString fluentFamily;

    if (!fluentFamily.isEmpty())
        return fluentFamily;

    // Попытка загрузить шрифт из ресурса
    const char* path = ":/fonts/FluentSystemIcons-Filled.ttf";
    int id = QFontDatabase::addApplicationFont(path);

    QStringList fams = QFontDatabase::applicationFontFamilies(id);

    fluentFamily = fams.first();
    return fluentFamily;
}

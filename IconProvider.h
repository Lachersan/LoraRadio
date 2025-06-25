#pragma once
#include <QApplication>
#include <qtawesome.h>

inline fa::QtAwesome& A() {
    static fa::QtAwesome aw{ qApp };
    static bool inited = false;
    if (!inited) {
        aw.initFontAwesome();  // Solid-иконки
        inited = true;
    }
    return aw;
}

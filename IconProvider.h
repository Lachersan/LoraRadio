#pragma once
#include <QtAwesome.h>

inline fa::QtAwesome& A() {
    static fa::QtAwesome inst(qApp);
    static bool inited = [&]() {
        inst.initFontAwesome();      // подгрузка Font Awesome Free
        // inst.initMaterialDesign(); // при желании других наборов
        return true;
    }();
    return inst;
}

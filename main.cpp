#include <QApplication>
#include "radioplayer.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    RadioPlayer radio;
    return app.exec();
}
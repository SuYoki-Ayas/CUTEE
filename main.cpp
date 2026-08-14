#include "CUTEE.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    CUTEE window;
    window.show();
    return app.exec();
}

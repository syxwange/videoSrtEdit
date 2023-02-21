#include "widget/MainWnd.h"
#include <QApplication>
#include <QFontDatabase>

#pragma comment(lib, "user32.lib")

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFontDatabase::addApplicationFont("../res/fontawesome-webfont.ttf");
    
    MainWnd w;
    w.show();
    return a.exec();
}
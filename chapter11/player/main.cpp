#include "mainwindow.h"

#include <QApplication>
// 引入SDL要增加下面的声明#undef main，否则编译会报错“undefined reference to qMain(int, char**)”
#undef main

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}

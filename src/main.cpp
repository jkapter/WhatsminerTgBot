#include "mainwindow.h"

#include <QApplication>
#include <QDir>

#include "logger.h"

int main(int argc, char *argv[])
{
    Logger log(QDir::currentPath(), QString("log.txt"));
    Logger::SetMaxSize(200000);

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}

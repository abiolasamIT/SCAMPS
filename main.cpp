#include "gui/MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName("SCAPMS");

    scapms::gui::MainWindow window;
    window.show();
    return application.exec();
}

#include <gst/gst.h>

#include <QApplication>
#include <QFont>
#include <QStyleFactory>

#include "xdaq_camera_control.h"


int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    QApplication::setApplicationVersion("0.0.3");
    QApplication::setApplicationName("Thor Vision-" + QApplication::applicationVersion() + "-beta");

    QApplication app(argc, argv);

    // TODO: the style has to be set to something other than "windows11"
    // in order to change QComboBox item text color
    // app.setStyle(QStyleFactory::create("windows11"));
    // app.setStyle(QStyleFactory::create("windowsvista"));
    app.setStyle(QStyleFactory::create("Windows"));
    // app.setStyle(QStyleFactory::create("Fusion"));
    // qDebug() << QStyleFactory::keys();

    auto main_window = new XDAQCameraControl();
    main_window->show();

    return app.exec();
}
#include <gst/gst.h>

#include <QApplication>
#include <QFont>
#include <QStyleFactory>

#include "../libxvc.h"
#include "portpool.h"
#include "xdaq_camera_control.h"

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    QApplication app(argc, argv);
    QFont font("Open Sans", 9);
    app.setFont(font);

    xvc::port_pool = std::make_unique<PortPool>(9000, 9010);

    // TODO: the style has to be set to something other than "windows11"
    // in order to change QComboBox item text color
    // app.setStyle(QStyleFactory::create("windows11"));
    // app.setStyle(QStyleFactory::create("windowsvista"));
    // app.setStyle(QStyleFactory::create("Windows"));
    app.setStyle(QStyleFactory::create("Fusion"));
    // qDebug() << QStyleFactory::keys();

    auto main_window = new XDAQCameraControl();
    main_window->show();

    return app.exec();
}
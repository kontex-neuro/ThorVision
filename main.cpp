#include <gst/gst.h>

#include <QApplication>
#include <memory>

#include "../libxvc.h"
#include "portpool.h"
#include "xdaq_camera_control.h"



int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);
    QApplication app(argc, argv);

    xvc::port_pool = std::make_unique<PortPool>(9000, 9010);

    XDAQCameraControl *main_window = new XDAQCameraControl();
    main_window->show();

    return app.exec();
}

#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>
#include <QtWidgets/qboxlayout.h>
// #include <_types/_uint64_t.h>
#include <fmt/core.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/gstbin.h>
#include <gst/gstcaps.h>
#include <gst/gstelement.h>
#include <gst/gstelementfactory.h>
#include <gst/gstobject.h>
#include <gst/gstpipeline.h>
#include <gst/gstutils.h>
#include <gst/video/videooverlay.h>
#include <qstackedwidget.h>

#include <QApplication>
#include <iostream>

#include "libxvc.h"
#include "mainwindow.h"
#include "portpool.h"

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);
    QApplication app(argc, argv);

    xvc::port_pool = new PortPool(9000, 9010);

#ifdef __APPLE__
    app.setStyle(QStyleFactory::create("Fusion"));
#endif

    // used to trasnfer 256 bits XDAQ data
    qRegisterMetaType<std::array<uint64_t, 4>>("std::array<uint64_t, 4>");

    MainWindow *main_window = new MainWindow();

    main_window->show();

    return app.exec();
}
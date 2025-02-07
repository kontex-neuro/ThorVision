#include "app.h"

#include <gst/gst.h>
#include <spdlog/spdlog.h>

#include <QDateTime>
#include <QFont>
#include <QPointer>
#include <QStandardPaths>
#include <QStyleFactory>
#include <filesystem>

#include "xdaq_camera_control.h"
#include "xdaqmetadata/logger.h"


namespace fs = std::filesystem;


App::App(int &argc, char **argv) : QApplication(argc, argv)
{
    gst_init(&argc, &argv);
    setApplicationVersion("0.0.3");
    setApplicationName("Thor Vision-" + applicationVersion() + "-beta");

    setStyle(QStyleFactory::create("windowsvista"));
    // qDebug() << QStyleFactory::keys();

    auto config_dir = QStandardPaths::standardLocations(QStandardPaths::AppConfigLocation).at(1);
    auto dir_path = fs::path(config_dir.toStdString());
    if (!fs::exists(dir_path)) {
        fs::create_directory(dir_path);
    }
    auto log_path = dir_path / "trace.log";
    if (fs::exists(log_path)) {
        fs::remove(log_path);
    }
    auto logger = logs::setup_logger(log_path.generic_string());

    spdlog::info(
        "Logging start at: {}",
        QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss").toStdString()
    );
    spdlog::info("Thor Vision GUI v{}", applicationVersion().toStdString());

    spdlog::info("Creating XDAQCameraControl.");
    auto main_window = new XDAQCameraControl();
    main_window->show();
}


bool App::notify(QObject *receiver, QEvent *e)
{
    try {
        if (e->type() == QEvent::UpdateRequest) {
            const auto weak = QPointer<QObject>(receiver);
            if (!weak) {
                return true;
            }
        }
        return QApplication::notify(receiver, e);
    } catch (const std::exception &_e) {
        spdlog::error(
            "Error {} sending event {} to object {} {}",
            _e.what(),
            typeid(*e).name(),
            qPrintable(receiver->objectName()),
            typeid(*receiver).name()
        );
    }
    return false;
}
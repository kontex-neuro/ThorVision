#include "app.h"

#include <gst/gst.h>
#include <spdlog/spdlog.h>

#include <QDir>
#include <QFont>
#include <QPointer>
#include <QStandardPaths>
#include <QStyleFactory>
#include <filesystem>

#include "xdaq_camera_control.h"
#include "xdaqmetadata/logger.h"

// TODO: include these 2 headers only to get VERSION
#include "xdaqmetadata/xdaqmetadata.h"
#include "xdaqvc/xvc.h"

namespace fs = std::filesystem;

App::App(int &argc, char **argv) : QApplication(argc, argv)
{
#ifdef __APPLE__
    auto app_path = QCoreApplication::applicationDirPath();
    auto plugin_dir = QDir::cleanPath(app_path + "/../PlugIns/gstreamer");

    setenv("GST_PLUGIN_PATH", plugin_dir.toUtf8(), 1);
    spdlog::info("GST_PLUGIN_PATH = {}", getenv("GST_PLUGIN_PATH"));
#endif

    if (!gst_is_initialized()) {
        gst_init(&argc, &argv);
    }

#if defined(_WIN32)
    setStyle(QStyleFactory::create("windowsvista"));
#elif defined(__APPLE__)
    setStyle(QStyleFactory::create("Fusion"));
#endif

#if defined(_WIN32)
    auto appdata_dir = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).at(1);
#elif defined(__APPLE__)
    auto appdata_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif

    auto dir_path = fs::path(appdata_dir.toStdString());
    if (!fs::exists(dir_path)) {
        spdlog::info("Create directory = {}", dir_path.generic_string());
        fs::create_directory(dir_path);
    }
    auto log_path = dir_path / "trace.log";
    if (fs::exists(log_path)) {
        spdlog::info("Log file: '{}' already exists. Removing it.", log_path.generic_string());
        fs::remove(log_path);
    }

    setApplicationVersion("0.1.4");
    setApplicationName(QString("ThorVision-%1-beta").arg(applicationVersion()));

    auto logger = logs::setup_logger(log_path.generic_string());

    spdlog::info(
        "ThorVision app v{}, libxvc v{}, libxdaqmetadata v{}",
        applicationVersion().toStdString(),
        LIBXVC_API_VER,
        xdaqmetadata_version()
    );

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
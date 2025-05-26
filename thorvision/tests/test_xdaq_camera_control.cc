#include "test_xdaq_camera_control.h"

#include <QtCore/qobject.h>
#include <fmt/core.h>
#include <gst/gst.h>

#include "../src/xdaq_camera_control.h"
#include "xdaqvc/camera.h"


int run_test(App &app, const int camera_count, const int width, const int height, const int fps)
{
    auto main_window = new XDAQCameraControl();

    std::function<void()> start_test = [&]() {
        for (auto i = 0; i < camera_count; ++i) {
            auto camera = std::make_unique<Camera>(i, fmt::format("{}", i));
            auto cap = Camera::Cap{"video/x-raw", "RGB", width, height, fps, 1};
            camera->set_test(true);
            camera->add_cap(cap);

            main_window->add_camera(camera.release());
        }
    };

    start_test();
    main_window->show();

    return app.exec();
}

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);
    auto app = App(argc, argv);

    if (argc != 4) {
        fmt::println("Usage: {} <camera_count> <resolution> <fps>", argv[0]);
        return 1;
    }

    auto ok = false;
    auto camera_count = QString(argv[1]).toInt(&ok);
    if (!ok || camera_count < 1) {
        fmt::println("Invalid camera count: {}", argv[1]);
        return 1;
    }

    auto resolution = QString(argv[2]);
    auto parts = resolution.split('x');
    if (parts.size() != 2) {
        return 1;
    }

    auto ok1 = false, ok2 = false;
    auto width = 640, height = 480;
    width = parts[0].toInt(&ok1);
    height = parts[1].toInt(&ok2);
    if (!ok1 || !ok2) {
        fmt::println("Invalid resolution '{}', defaulting to 640x480", resolution.toStdString());
        width = 640;
        height = 480;
    }

    auto fps = QString(argv[3]).toInt(&ok);
    if (!ok || fps < 1) {
        fmt::println("Invalid FPS: {}", argv[3]);
        return 1;
    }
    
    return run_test(app, camera_count, width, height, fps);
}
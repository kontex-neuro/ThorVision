#include "test_main.h"

#include <fmt/core.h>
#include <gst/gst.h>

#include <cstdlib>

#include "../src/stream_window.h"
#include "xdaqvc/camera.h"


int run_test(App &app, int camera_count, const int width, const int height)
{
    std::vector<StreamWindow *> stream_windows;

    std::function<void()> start_test = [&]() {
        for (auto win : stream_windows) {
            win->deleteLater();
        }
        stream_windows.clear();

        spdlog::info("Starting test with resolution {}x{}", width, height);

        for (int i = 0; i < camera_count; ++i) {
            auto camera = std::make_unique<Camera>(i, fmt::format("{}", i));
            auto cap = Camera::Cap{"video/x-raw", "RGB", width, height, 30, 1};
            camera->set_test(true);
            camera->set_current_cap(fmt::format(
                "{},format={},width={},height={},framerate={}/{}",
                cap.media_type,
                cap.format,
                cap.width,
                cap.height,
                cap.fps_n,
                cap.fps_d
            ));

            auto stream_window = new StreamWindow(camera.release());
            stream_window->setFloating(true);
            stream_window->show();
            stream_window->play();

            stream_windows.push_back(stream_window);
        }
    };

    start_test();
    return app.exec();
}

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);
    auto app = App(argc, argv);

    if (argc != 3) {
        fmt::println("Usage: {} <camera_count> <resolution>", argv[0]);
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

    return run_test(app, camera_count, width, height);
}
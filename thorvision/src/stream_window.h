#pragma once

#include <gst/gstelement.h>

#include <QCloseEvent>
#include <QDockWidget>
#include <QImage>
#include <QLabel>
#include <filesystem>
#include <future>
#include <thread>

#include "xdaqmetadata/metadata_handler.h"
#include "xdaqvc/camera.h"

namespace fs = std::filesystem;

class StreamWindow : public QDockWidget
{
    Q_OBJECT

public:
    explicit StreamWindow(Camera *camera, bool view_enabled = true, QWidget *parent = nullptr);
    ~StreamWindow();

    Camera *_camera;
    std::unique_ptr<GstElement, decltype(&gst_object_unref)> _pipeline;
    std::vector<std::pair<std::thread, std::future<void>>> _parsing_threads;

    enum class Record { KeepNo, Start, Keep, Stop };
    Record _status;
    std::unique_ptr<MetadataHandler> _handler;

    void play();
    void stop();
    void set_image(const QImage &image);
    void set_metadata(const XDAQFrameData &metadata);

    // TODO: UGLY HACK.
    void start_h265_recording(
        fs::path &filepath, bool continuous, int max_size_time, int max_files
    );

private:
    QImage _image;
    XDAQFrameData _metadata;

    std::atomic_bool _bus_thread_running;
    std::jthread _bus_thread;
    void poll_bus_messages();
    void cleanupParsingThreads();

    std::atomic_bool _fps_thread_running;
    std::jthread _fps_thread;

public slots:
    void update_title(const QString &title);

protected:
    void closeEvent(QCloseEvent *e) override;
    void paintEvent(QPaintEvent *) override;

signals:
    void window_close();
};
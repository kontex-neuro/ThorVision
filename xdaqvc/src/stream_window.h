#pragma once

#include <gst/gstelement.h>

#include <QCloseEvent>
#include <QDockWidget>
#include <QImage>
#include <QLabel>
#include <QPropertyAnimation>
#include <filesystem>

#include "xdaqmetadata/metadata_handler.h"
#include "xdaqvc/camera.h"


namespace fs = std::filesystem;


class StreamWindow : public QDockWidget
{
    Q_OBJECT

public:
    explicit StreamWindow(Camera *camera, QWidget *parent = nullptr);
    ~StreamWindow();

    Camera *_camera;
    std::unique_ptr<GstElement, decltype(&gst_object_unref)> _pipeline;
    std::string _saved_video_path;

    enum class Record { KeepNo, Start, Keep, Stop };
    Record _status;
    std::unique_ptr<MetadataHandler> _handler;

    void play();
    void set_image(const QImage &image);
    void set_metadata(const XDAQFrameData &metadata);

    // TODO: UGLY HACK.
    void start_h265_recording(
        fs::path &filepath, bool continuous, int max_size_time, int max_files
    );

private:
    bool _pause;
    QImage _image;
    XDAQFrameData _metadata;
    QLabel *_icon;
    QPropertyAnimation *_fade;

    std::atomic<bool> bus_thread_running;
    std::thread bus_thread;
    void poll_bus_messages();

protected:
    void closeEvent(QCloseEvent *e) override;
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;

signals:
    void window_close();
};
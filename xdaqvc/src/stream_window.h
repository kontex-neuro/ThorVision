#pragma once

#include <gst/gstelement.h>

#include <QCloseEvent>
#include <QDockWidget>
#include <QImage>
#include <QLabel>
#include <QPropertyAnimation>
#include <filesystem>

#include "xdaqvc/camera.h"
#include "xdaqmetadata/h265_metadata_handler.h"


namespace fs = std::filesystem;


class StreamWindow : public QDockWidget
{
    Q_OBJECT

public:
    StreamWindow(Camera *_camera, QWidget *parent = nullptr);
    ~StreamWindow();

    Camera *camera;
    GstElement *pipeline;
    GstClockTime frame_time;
    std::string saved_video_path;

    enum class Record { KeepNo, Start, Keep, Stop };
    Record status;
    bool recording;
    std::unique_ptr<H265MetadataHandler> handler;

    void play();
    void set_image(const QImage &_image);
    void set_metadata(const XDAQFrameData &_metadata);
    void set_fps(const double fps);
    // HACK
    void start_h265_recording(fs::path &filepath);

protected:
    void closeEvent(QCloseEvent *e) override;
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;

private:
    bool pause;
    QImage image;
    XDAQFrameData metadata;
    double fps_text;
    QLabel *icon;
    QPropertyAnimation *fade;
};
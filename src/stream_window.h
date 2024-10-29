#pragma once

#include <gst/app/gstappsink.h>
#include <gst/gstelement.h>

#include <QCloseEvent>
#include <QDockWidget>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QWidget>
#include <fstream>
#include <memory>

#include "../src/camera.h"
#include "safedeque.h"


class StreamWindow : public QDockWidget
{
    Q_OBJECT

public:
    StreamWindow(Camera *_camera, QWidget *parent = nullptr);
    ~StreamWindow();

    std::unique_ptr<SafeDeque::SafeDeque> safe_deque;
    std::unique_ptr<SafeDeque::SafeDeque> safe_deque_filesink;
    uint64_t first_iframe_timestamp;
    bool start_save_bin;
    std::string saved_video_path;
    std::ofstream filestream;
    Camera *camera;
    GstElement *pipeline;
    bool recording;

    int parse_counter;
    int sample_counter;
    int record_counter;
    int record_parse_counter;
    int record_parse_save_counter;

    void play();
    // void set_image(const QImage& _image);
    void set_image(unsigned char *_image, const int width, const int height);
    void set_metadata(const XDAQFrameData &_metadata);
    void set_fps(const double fps);

    void open_filestream();
    void close_filestream();

protected:
    void closeEvent(QCloseEvent *e) override;
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;

private:
    bool pause;
    int probe_id;
    QImage image;
    XDAQFrameData metadata;
    double fps_text;
    QLabel *icon;


signals:
    // void frame_ready(const QImage &image, const XDAQFrameData& metadata);
    void frame_ready(const QImage &image);
private slots:
    // void display_frame(const QImage &image, const XDAQFrameData& metadata);
    void display_frame(const QImage &image);
};
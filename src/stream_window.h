#pragma once

#include <gst/gstelement.h>

#include <QCloseEvent>
#include <QDockWidget>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QWidget>
#include <fstream>

#include "camera.h"
#include "safedeque.h"


class StreamWindow : public QDockWidget
{
    Q_OBJECT

public:
    StreamWindow(Camera *camera, QWidget *parent = nullptr);
    ~StreamWindow() = default;

    SafeDeque::SafeDeque *safe_deque;
    std::ofstream *filestream;

    bool playing;
    bool recording;
    Camera *camera;

    void play();
    void pause();

    QImage image;
    XDAQFrameData metadata;
    GstElement *pipeline;

    // void set_image(const QImage& _image);
    void set_image(unsigned char *_image, const int width, const int height);
    void set_metadata(const XDAQFrameData &_metadata);

protected:
    void closeEvent(QCloseEvent *e) override;
    void paintEvent(QPaintEvent *) override;

private:
    QLabel *image_label;
    QLabel *timestamp_label;


    // int camera_id;
    // int port;
    // std::string camera_capability;

signals:
    // void frame_ready(const QImage &image, const XDAQFrameData& metadata);
    void frame_ready(const QImage &image);

private slots:
    // void stream_handler();

    // void stop_stream();
    // void record_stream();

    // void display_frame(const QImage &image, const XDAQFrameData& metadata);
    void display_frame(const QImage &image);
};
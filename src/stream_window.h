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

#include "camera.h"
#include "safedeque.h"


class StreamWindow : public QDockWidget
{
    Q_OBJECT

public:
    // StreamWindow(Camera *camera, QWidget *parent = nullptr);
    StreamWindow(Camera *_camera, QWidget *parent = nullptr);
    ~StreamWindow();

    std::unique_ptr<SafeDeque::SafeDeque> safe_deque;
    std::unique_ptr<std::ofstream> filestream;
    Camera *camera;
    GstElement *pipeline;
    // std::unique_ptr<GstElement, decltype(&gst_object_unref)> pipeline;
    XDAQFrameData metadata;

    void play();
    // void pause();

    QImage image;
    bool pause;
    GstAppSinkCallbacks callbacks;

    // void set_image(const QImage& _image);
    void set_image(unsigned char *_image, const int width, const int height);
    void set_metadata(const XDAQFrameData &_metadata);

protected:
    void closeEvent(QCloseEvent *e) override;
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;

private:
    int probe_id;
    signals:
    // void frame_ready(const QImage &image, const XDAQFrameData& metadata);
    void frame_ready(const QImage &image);
private slots:
    // void display_frame(const QImage &image, const XDAQFrameData& metadata);
    void display_frame(const QImage &image);
};
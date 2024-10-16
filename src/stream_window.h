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
    std::unique_ptr<std::ofstream> filestream;
    Camera *camera;
    GstElement *pipeline;
    GstClockTime frame_time;

    void play();
    void set_image(const QImage &_image);
    void set_metadata(const XDAQFrameData &_metadata);
    void set_fps(const double fps);

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
signals:
    // void frame_ready(const QImage &image, const XDAQFrameData& metadata);
    void frame_ready(const QImage &image);
private slots:
    // void display_frame(const QImage &image, const XDAQFrameData& metadata);
    void display_frame(const QImage &image);
};
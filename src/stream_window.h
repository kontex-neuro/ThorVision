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
#include "include/safedeque.h"
#include "include/xdaqmetadata.h"

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

    enum class Record { KeepNo, Start, Keep, Stop };
    Record status;
    bool recording;

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
};
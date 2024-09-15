#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qobjectdefs.h>
#include <QtGui/qevent.h>
#include <_types/_uint64_t.h>
#include <gst/app/gstappsink.h>
#include <gst/gstpad.h>

#include <QMediaPlayer>
#include <QVideoFrame>
#include <QtWidgets>
#include <array>

#include "mainwindow.h"
#include "portpool.h"
#include "safedeque.h"

class StreamWidget : public QDockWidget
{
    Q_OBJECT
signals:
    void frame_ready(const QImage &image, const std::array<uint64_t, 4> timestamp);

public slots:
    void stream_handler();

    void start_stream();
    void stop_stream();
    void record_stream();

    void select_dir();
    void select_camera_setting(QString setting);
    void parse_video();
    void display_frame(const QImage &image, const std::array<uint64_t, 4> timestamp);

public:
    StreamWidget(MainWindow *parent = nullptr);
    ~StreamWidget() = default;
    SafeDeque::SafeDeque *safe_deque;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QLabel *image_label;
    QLabel *timestamp_label;
    GstElement *pipeline;
    QLineEdit *save_path;

    QPushButton *stream_button;
    QPushButton *record_button;

    bool streaming;
    bool recording;

    int camera_id;
    int port;
};
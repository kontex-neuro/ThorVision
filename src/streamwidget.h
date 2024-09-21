#pragma once

// #include <array>

#include <gst/gstelement.h>
#include <QDockWidget>
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QImage>
#include <QCloseEvent>
#include <fstream>
#include <memory>

// #include "../libxvc.h"
#include "camera.h"
#include "safedeque.h"
// #include "portpool.h"
// #include "camera.h"


class StreamWidget : public QDockWidget
{
    Q_OBJECT

public:
    StreamWidget(QWidget *parent = nullptr);
    ~StreamWidget() = default;


    SafeDeque::SafeDeque *safe_deque;
    std::ofstream* filestream;

    void set_use_camera(bool use_camera);

    bool playing;
    bool recording;
    Camera camera;
    bool use_camera;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QLabel *image_label;
    QLabel *timestamp_label;
    QLineEdit *save_path;

    QPushButton *stream_button;
    QPushButton *record_button;
    GstElement *pipeline;

    // int camera_id;
    // int port;
    // std::string camera_capability;

signals:
    void frame_ready(const QImage &image, const std::array<uint64_t, 4> timestamp);

public slots:
    void stream_handler();

    void start_stream();
    void stop_stream();
    void record_stream();

    void select_dir();
    void select_camera_capability(QString capability);
    void open_video(QString filepath);
    void display_frame(const QImage &image, const std::array<uint64_t, 4> timestamp);
};
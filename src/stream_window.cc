#include "stream_window.h"

#include <fmt/core.h>
#include <glib.h>
#include <gst/app/gstappsink.h>
#include <gst/gstelement.h>
#include <gst/gstobject.h>
#include <gst/gstpad.h>
#include <gst/gstpipeline.h>
#include <gst/gstsample.h>
#include <gst/gststructure.h>
#include <gst/video/video-info.h>
#include <qnamespace.h>
#include <spdlog/spdlog.h>

#include <QDateTime>
#include <QDir>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QSettings>
#include <QString>
#include <memory>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>

#include "../libxvc.h"

using nlohmann::json;
using namespace std::chrono_literals;

static void set_state(GstElement *element, GstState state)
{
    GstStateChangeReturn ret = gst_element_set_state(element, state);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_error(
            "Failed to change the element %s state to: %s",
            GST_ELEMENT_NAME(element),
            gst_element_state_get_name(state)
        );
        gst_element_set_state(element, GST_STATE_NULL);
        gst_object_unref(element);
    }
}

static GstFlowReturn callback(GstAppSink *sink, void *user_data)
{
    std::unique_ptr<GstSample, decltype(&gst_sample_unref)> sample(
        gst_app_sink_pull_sample(sink), gst_sample_unref
    );
    if (sample.get()) {
        GstBuffer *buffer = gst_sample_get_buffer(sample.get());
        GstMapInfo info;  // contains the actual image
        if (gst_buffer_map(buffer, &info, GST_MAP_READ)) {
            std::unique_ptr<GstVideoInfo, decltype(&gst_video_info_free)> video_info(
                gst_video_info_new(), gst_video_info_free
            );
            if (!gst_video_info_from_caps(video_info.get(), gst_sample_get_caps(sample.get()))) {
                g_warning("Failed to parse video info");
                return GST_FLOW_ERROR;
            }
            GstCaps *caps = gst_sample_get_caps(sample.get());
            GstStructure *structure = gst_caps_get_structure(caps, 0);
            const int width = g_value_get_int(gst_structure_get_value(structure, "width"));
            const int height = g_value_get_int(gst_structure_get_value(structure, "height"));
            // const std::string format =
            //     g_value_get_string(gst_structure_get_value(structure, "format"));

            // Get the pixel value of the center pixel
            // int stride = video_info->finfo->bits / 8;
            // unsigned int pixel_offset = video_info->width / 2 * stride +
            //                             video_info->width * video_info->height / 2 * stride;

            // this is only one pixel
            // when dealing with formats like BGRx
            // pixel_data will consist out of
            // pixel_offset   => B
            // pixel_offset+1 => G
            // pixel_offset+2 => R
            // pixel_offset+3 => x

            auto stream_window = (StreamWindow *) user_data;

            unsigned char *image_data = info.data;
            QImage image = QImage(image_data, width, height, QImage::Format::Format_RGB888);
            stream_window->set_image(image);

            GstClockTime current_time = GST_BUFFER_PTS(buffer);
            if (stream_window->frame_time != GST_CLOCK_TIME_NONE) {
                GstClockTime diff = current_time - stream_window->frame_time;
                double fps = GST_SECOND / (double) diff;
                stream_window->set_fps(fps);
            }
            stream_window->frame_time = current_time;

            auto metadata = stream_window->safe_deque->check_pts_pop_timestamp(buffer->pts);
            stream_window->set_metadata(metadata.value_or(XDAQFrameData{0, 0, 0, 0, 0, 0}));

            // QVideoFrame frame = QVideoFrame(snapShot);
            // Create a QVideoFrame from the buffer
            // QVideoFrame::PixelFormat format = QVideoFrame::Format_RGB32;
            // QVideoFrame frame((uchar *) info.data, QSize(width, height));
            // QVideoFrame frame(snapShot);

            // pixel_data = info.data[pixel_offset];
            gst_buffer_unmap(buffer, &info);
        }
    }
    return GST_FLOW_OK;
}

StreamWindow::StreamWindow(Camera *_camera, QWidget *parent)
    : QDockWidget(parent), pipeline(nullptr), frame_time(GST_CLOCK_TIME_NONE), pause(false)
{
    camera = _camera;

    safe_deque = std::make_unique<SafeDeque::SafeDeque>();
    filestream = std::make_unique<std::ofstream>();

    setFixedSize(480, 360);
    setFeatures(features() & ~QDockWidget::DockWidgetClosable);
    // QSizePolicy sp = sizePolicy();
    // sp.setHorizontalPolicy(QSizePolicy::Preferred);
    // sp.setVerticalPolicy(QSizePolicy::Preferred);
    // sp.setHeightForWidth(true);
    // setSizePolicy(sp);

    // icon = new QLabel(this);
    // icon->setAlignment(Qt::AlignCenter);
    // QPropertyAnimation *fadeAnimation = new QPropertyAnimation(icon, "opacity");
    // fadeAnimation->setDuration(2000);  // 2 seconds fade-outg
    // fadeAnimation->setStartValue(1.0);
    // fadeAnimation->setEndValue(0.0);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    pipeline = gst_pipeline_new("video-capture");
    if (!pipeline) {
        g_error("Pipeline could be created");
        return;
    }

    std::string ip = "192.168.177.100";
    std::string uri = fmt::format("{}:{}", ip, camera->get_port());
    if (camera->get_current_cap().find("image/jpeg") != std::string::npos) {
        xvc::setup_jpeg_srt_stream(GST_PIPELINE(pipeline), uri);
    } else if (camera->get_id() == -1) {
        xvc::mock_high_frame_rate(GST_PIPELINE(pipeline), uri);
    } else {
        xvc::setup_h265_srt_stream(GST_PIPELINE(pipeline), uri);

        GstElement *parser = gst_bin_get_by_name(GST_BIN(pipeline), "parser");
        std::unique_ptr<GstPad, decltype(&gst_object_unref)> sink_pad(
            gst_element_get_static_pad(parser, "sink"), gst_object_unref
        );
        gst_pad_add_probe(
            sink_pad.get(), GST_PAD_PROBE_TYPE_BUFFER, parse_h265_metadata, safe_deque.get(), NULL
        );
    }

    GstAppSinkCallbacks callbacks = {NULL};
    callbacks.new_sample = callback;
    GstElement *appsink = gst_bin_get_by_name(GST_BIN(pipeline), "appsink");
    gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, this, NULL);

    auto save_path = QSettings("KonteX", "VC").value("save_path", QDir::currentPath()).toString();
    auto dir_name =
        QSettings("KonteX", "VC")
            .value("dir_name", QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"))
            .toString();
    auto file_path = save_path + "/" + dir_name + "/";
    QSettings("KonteX", "VC").setValue("file_path", file_path);
    fmt::println("save_path = {}, dir_name = {}", save_path.toStdString(), dir_name.toStdString());

    if (QSettings("KonteX", "VC").value("additional_metadata", false).toBool()) {
        filestream->open(
            file_path.toStdString() + camera->get_name() + ".bin",
            std::ios::binary | std::ios::app | std::ios::out
        );
    }
}

StreamWindow::~StreamWindow()
{
    camera->stop();
    safe_deque->clear();
    xvc::port_pool->release_port(camera->get_port());

    set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

void StreamWindow::closeEvent(QCloseEvent *e) { e->accept(); }

void StreamWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    // painter.setRenderHint(QPainter::Antialiasing);
    // painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QRect(0, 0, width(), height()), image, image.rect());
    if (metadata.ttl_in >= 1) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(QColor(181, 157, 99)));
        QPointF DI(width() - 16, 16);
        painter.setOpacity(0.5);
        painter.drawEllipse(DI, 10, 10);

        QString ttl_in = QString::number(metadata.ttl_in);
        painter.setOpacity(1);
        painter.setPen(QPen(Qt::black));
        QRectF text(DI.x() - 8, DI.y() - 8, 15, 15);
        painter.drawText(text, Qt::AlignCenter, ttl_in);
        painter.setPen(QPen(Qt::white));
    }
    painter.drawText(
        QRect(10, height() - 60, width() / 2, height() - 60),
        QString::fromStdString(fmt::format("XDAQ Time {:08x}", metadata.fpga_timestamp))
    );
    painter.drawText(
        QRect(10, height() - 30, width() / 2, height() - 30),
        QString::fromStdString(fmt::format("Ephys Time {:04x}", metadata.rhythm_timestamp))
    );
    painter.drawText(
        QRect(width() - 130, height() - 30, width(), height() - 30),
        QString::fromStdString(fmt::format("DO word {:04x}", metadata.ttl_out))
    );
    painter.drawText(
        QRect(width() - 130, height() - 60, width(), height() - 30),
        QString::fromStdString(fmt::format("FPS {:.2f}", fps_text))
    );
    // if (pause) {
    //     painter.drawPixmap(
    //         width() / 2, height() / 2, style()->standardPixmap(QStyle::SP_MediaPause)
    //     );
    // } else {
    //     painter.drawPixmap(
    //         width() / 2, height() / 2, style()->standardPixmap(QStyle::SP_MediaPlay)
    //     );
    // }
}

void StreamWindow::mousePressEvent(QMouseEvent *e) { pause = !pause; }

void StreamWindow::set_image(const QImage &_image)
{
    if (!pause) {
        image = _image;
        update();
    }
}

void StreamWindow::set_metadata(const XDAQFrameData &_metadata)
{
    if (!pause) {
        metadata = _metadata;
        update();
    }
}

void StreamWindow::set_fps(const double _fps)
{
    if (!pause) {
        fps_text = _fps;
        update();
    }
}

void StreamWindow::play()
{
    camera->start();
    if (pipeline) {
        set_state(pipeline, GST_STATE_PLAYING);
    }
}

// void StreamWindow::display_frame(const QImage &image, const XDAQFrameData &metadata)
void StreamWindow::display_frame(const QImage &image)
{
    // image_label->setPixmap(QPixmap::fromImage(image).scaled(
    //     image_label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation
    // ));
    // image_label->setPixmap(QPixmap::fromImage(image));
    // timestamp_label->setText(QString::fromStdString(fmt::format(
    //     "DAQ Time {:08x} Ephys Time {:08x} DIN word {:08x} DO word {:08x}",
    //     metadata.fpga_timestamp,
    //     metadata.rhythm_timestamp,
    //     metadata.ttl_in,
    //     metadata.ttl_out
    // )));
    // fmt::println("timestamp[0] = {:x}", timestamp[0]);
    // fmt::println("timestamp[1] = {:x}", timestamp[1]);
    // fmt::println("timestamp[2] = {:x}", timestamp[2]);
    // fmt::println("timestamp[3] = {:x}", timestamp[3]);
}
#include "stream_window.h"

#include <fmt/core.h>
#include <glib.h>
#include <gst/app/gstappsink.h>
#include <gst/codecparsers/gsth265parser.h>
#include <gst/gstelement.h>
#include <gst/gstobject.h>
#include <gst/gstpad.h>
#include <gst/gstpipeline.h>
#include <gst/gststructure.h>
#include <gst/video/video-info.h>
#include <qfiledialog.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qpainter.h>

#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QObject>
#include <QOpenGLWidget>
#include <QPixmap>
#include <QSettings>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <array>
#include <fstream>
#include <ios>
#include <memory>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <utility>

#include "../libxvc.h"
#include "safedeque.h"


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
        // std::string msg =
        //     std::string("Failed to change the element state to: ") + std::to_string(state);
        gst_element_set_state(element, GST_STATE_NULL);
        gst_object_unref(element);
        // throw std::runtime_error(msg);
    }
}

static GstPadProbeReturn extract_timestamp_from_SEI(
    GstPad *pad, GstPadProbeInfo *info, gpointer user_data
)
{
    static bool extract_metadata = QSettings("KonteX", "VC").value("extract_metadata").toBool();

    auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buffer) return GST_PAD_PROBE_OK;

    std::unique_ptr<GstH265Parser, decltype(&gst_h265_parser_free)> nalu_parser(
        gst_h265_parser_new(), gst_h265_parser_free
    );
    GstMapInfo map_info;
    GstH265NalUnit nalu;
    for (unsigned int i = 0; i < gst_buffer_n_memory(buffer); ++i) {
        std::unique_ptr<GstMemory, decltype(&gst_memory_unref)> mem_in_buffer(
            gst_buffer_get_memory(buffer, i), gst_memory_unref
        );
        if (!gst_memory_map(mem_in_buffer.get(), &map_info, GST_MAP_READ)) {
            continue;
        }
        GstH265ParserResult parse_result = gst_h265_parser_identify_nalu_unchecked(
            nalu_parser.get(), map_info.data, 0, map_info.size, &nalu
        );
        if (parse_result == GST_H265_PARSER_OK || parse_result == GST_H265_PARSER_NO_NAL_END ||
            parse_result == GST_H265_PARSER_NO_NAL) {
            // g_print("type: %d\n", nalu.type);

            if (nalu.type == GST_H265_NAL_PREFIX_SEI || nalu.type == GST_H265_NAL_SUFFIX_SEI) {
                GArray *array = g_array_new(false, false, sizeof(GstH265SEIMessage));
                auto result = gst_h265_parser_parse_sei(nalu_parser.get(), &nalu, &array);
                fmt::println("gst_h265_parser_parse_sei = {}", (int) result);
                GstH265SEIMessage sei_msg = g_array_index(array, GstH265SEIMessage, 0);
                GstH265RegisteredUserData register_user_data = sei_msg.payload.registered_user_data;

                XDAQFrameData metadata;
                std::memcpy(&metadata, &register_user_data.data, register_user_data.size);

                uint64_t pts = buffer->pts;
                auto stream_window = (StreamWindow *) user_data;
                stream_window->safe_deque->push(pts, metadata);

                if (extract_metadata) {
                    stream_window->filestream->write(
                        (const char *) &metadata.fpga_timestamp, sizeof(metadata.fpga_timestamp)
                    );
                }
                g_array_free(array, true);
            }
        }
        gst_memory_unmap(mem_in_buffer.get(), &map_info);
    }
    return GST_PAD_PROBE_OK;
}

static GstFlowReturn callback(GstAppSink *sink, void *user_data)
{
    GstSample *sample = gst_app_sink_pull_sample(sink);
    // g_signal_emit_by_name(sink, "pull-sample", &sample, NULL);

    if (sample) {
        // static guint framecount = 0;
        // int pixel_data = -1;

        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo info;  // contains the actual image
        if (gst_buffer_map(buffer, &info, GST_MAP_READ)) {
            GstVideoInfo *video_info = gst_video_info_new();
            if (!gst_video_info_from_caps(video_info, gst_sample_get_caps(sample))) {
                // Could not parse video info (should not happen)
                g_warning("Failed to parse video info");
                return GST_FLOW_ERROR;
            }

            // pointer to the image data
            // unsigned char* data = info.data;

            GstCaps *caps = gst_sample_get_caps(sample);
            GstStructure *structure = gst_caps_get_structure(caps, 0);
            const int width = g_value_get_int(gst_structure_get_value(structure, "width"));
            const int height = g_value_get_int(gst_structure_get_value(structure, "height"));
            const std::string format =
                g_value_get_string(gst_structure_get_value(structure, "format"));

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
            stream_window->set_image(info.data, width, height);

            // QImage *image =
            //     new QImage((const uchar *) info.data, width, height,
            //     QImage::Format::Format_RGB888);

            auto metadata = stream_window->safe_deque->check_pts_pop_timestamp(buffer->pts);
            stream_window->set_metadata(metadata.value_or(XDAQFrameData_default));

            // QVideoFrame frame = QVideoFrame(snapShot);
            // Create a QVideoFrame from the buffer
            // QVideoFrame::PixelFormat format = QVideoFrame::Format_RGB32;
            // QVideoFrame frame((uchar *) info.data, QSize(width, height));
            // QVideoFrame frame(snapShot);

            // pixel_data = info.data[pixel_offset];
            gst_buffer_unmap(buffer, &info);
            gst_video_info_free(video_info);
        }

        // GstClockTime timestamp = GST_BUFFER_PTS(buffer);

        // g_print(
        //     "Captured frame %d, Pixel Value=%03d Timestamp=%" GST_TIME_FORMAT "            \r",
        //     framecount,
        //     pixel_data,
        //     GST_TIME_ARGS(timestamp)
        // );
        // framecount++;

        gst_sample_unref(sample);
    }
    return GST_FLOW_OK;
}

StreamWindow::StreamWindow(Camera *_camera, QWidget *parent)
    : QDockWidget(parent), pipeline(nullptr)
{
    camera = _camera;
    setFixedSize(480, 360);

    // setAllowedAreas(Qt::LeftDockWidgetArea);
    // QSizePolicy sp = sizePolicy();
    // sp.setHorizontalPolicy(QSizePolicy::Preferred);
    // sp.setVerticalPolicy(QSizePolicy::Preferred);
    // sp.setHeightForWidth(true);
    // setSizePolicy(sp);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    pipeline = gst_pipeline_new(NULL);
    if (!pipeline) {
        g_error("Pipeline could be created");
        return;
    }

    std::string ip = "192.168.177.100";
    std::string uri = fmt::format("{}:{}", ip, camera->get_port());
    xvc::open_video_stream(GST_PIPELINE(pipeline), uri);

    GstElement *parser = gst_bin_get_by_name(GST_BIN(pipeline), "parser");
    GstPad *parser_pad = gst_element_get_static_pad(parser, "sink");
    gst_pad_add_probe(
        parser_pad, GST_PAD_PROBE_TYPE_BUFFER, extract_timestamp_from_SEI, this, NULL
    );

    GstAppSinkCallbacks callbacks = {NULL};
    callbacks.new_sample = callback;
    GstElement *appsink = gst_bin_get_by_name(GST_BIN(pipeline), "appsink");
    gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, this, NULL);

    safe_deque = new SafeDeque::SafeDeque();
    filestream = new std::ofstream();
    auto save_path = QSettings("KonteX", "VC").value("save_path", ".").toString();
    auto dir_name = QSettings("KonteX", "VC").value("dir_name", ".").toString();
    auto dir_path = save_path + dir_name;
    fmt::println("save_path = {}, dir_name = {}", save_path.toStdString(), dir_name.toStdString());

    // QString formatted_time = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    // QString time_zone = QDateTime::currentDateTime().timeZoneAbbreviation();

    // int utc_offset = QDateTime::currentDateTime().offsetFromUtc() / 3600;
    // QString utc_offset_str = QString("%1%2").arg(utc_offset >= 0 ? "+" : "").arg(utc_offset);
    QDir dir;
    if (!dir.exists(dir_path)) {
        if (!dir.mkdir(dir_path)) {
            qDebug() << "Failed to create directory: " << dir_path;
            return;
        }
    }
    filestream->open(dir_path.toStdString(), std::ios::binary);
}

void StreamWindow::closeEvent(QCloseEvent *e)
{
    // if (playing) {
    //     this->camera.stop();
    //     if (pipeline) {
    //         set_state(pipeline, GST_STATE_NULL);
    //         gst_object_unref(pipeline);
    //     }
    // }
    // this->camera.change_status(Camera::Status::Idle);

    fmt::println("port = {}", camera->get_port());
    xvc::port_pool->release_port(camera->get_port());
    xvc::port_pool->print_available_ports();

    camera->stop();
    safe_deque->clear();
    delete safe_deque;
    safe_deque = nullptr;

    e->accept();
}

void StreamWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(rect(), image, image.rect());
    painter.drawText(
        QRect(10, height() - 60, width() / 2, height() - 60),
        QString::fromStdString(fmt::format("XDAQ Time {:08x}", metadata.fpga_timestamp))
    );
    painter.drawText(
        QRect(10, height() - 30, width() / 2, height() - 30),
        QString::fromStdString(fmt::format("Ephys Time {:04x}", metadata.rhythm_timestamp))
    );
    painter.drawText(
        QRect(width() - 130, height() - 60, width(), height() - 60),
        QString::fromStdString(fmt::format("DIN word {:04x}", metadata.ttl_in))
    );
    painter.drawText(
        QRect(width() - 130, height() - 30, width(), height() - 30),
        QString::fromStdString(fmt::format("DO word {:04x}", metadata.ttl_out))
    );

    // painter.drawText(
    //     rect(),
    //     QString::fromStdString(fmt::format(
    //         "Ephys Time {:08x} DIN word {:08x} DO word {:08x}",
    //         metadata.rhythm_timestamp,
    //         metadata.ttl_in,
    //         metadata.ttl_out
    //     ))
    // );
    // painter.draw
    // timestamp_label->setText(QString::fromStdString(fmt::format(
    //     "DAQ Time {:08x} Ephys Time {:08x} DIN word {:08x} DO word {:08x}",
    //     _metadata.fpga_timestamp,
    //     _metadata.rhythm_timestamp,
    //     _metadata.ttl_in,
    //     _metadata.ttl_out
    // )));
}

void StreamWindow::mousePressEvent(QMouseEvent *e)
{
    safe_deque->clear();
    if (pipeline) {
        set_state(pipeline, GST_STATE_PAUSED);
    }
}

// void StreamWindow::set_image(const QImage &_image)
void StreamWindow::set_image(unsigned char *image_bits, const int width, const int height)
{
    QImage _image =
        QImage((const uchar *) image_bits, width, height, QImage::Format::Format_RGB888);
    image = _image;
    update();
}

void StreamWindow::set_metadata(const XDAQFrameData &_metadata)
{
    metadata = _metadata;
    update();
}

void StreamWindow::play()
{
    camera->start();
    if (pipeline) {
        set_state(pipeline, GST_STATE_PLAYING);
    }
}

void StreamWindow::pause()
{
    // if (playing && recording) {
    //     camera->change_status(Camera::Status::Occupied);
    //     camera->stop();
    // }
    // camera->stop();

    safe_deque->clear();
    if (pipeline) {
        set_state(pipeline, GST_STATE_PAUSED);
        gst_object_unref(pipeline);
    }
}

// void StreamWindow::record_stream()
// {
//     if (recording) {
//         qInfo("stop recording");

//         xvc::stop_recording(GST_PIPELINE(pipeline));
//         stop_stream();
//         filestream->close();

//     } else {
//         qInfo("start recording");

//         if (!playing) {
//             start_stream();
//         }

//         record_button->setDisabled(true);
//         stream_button->setText("Stop");
//         playing = true;

//         QSettings settings;
//         QString filepath = settings.value("record_directory", "").toString();

//         QString time_zone = QDateTime::currentDateTime().timeZoneAbbreviation();
//         QString formatted_time = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");

//         int utc_offset = QDateTime::currentDateTime().offsetFromUtc() / 3600;
//         QString utc_offset_str = QString("%1%2").arg(utc_offset >= 0 ? "+" : "").arg(utc_offset);

//         fmt::println(
//             "utc_offset = {}, time_zone = {}, filepath = {}",
//             utc_offset_str.toStdString(),
//             time_zone.toStdString(),
//             formatted_time.toStdString()
//         );

//         filepath += formatted_time;

//         filestream->open(filepath.toStdString() + ".bin", std::ios::binary | std::ios::app);
//         if (!filestream->is_open()) {
//             fmt::println("Failed to open file {}", filepath.toStdString());
//             return;
//         }

//         xvc::start_recording(GST_PIPELINE(pipeline), filepath.toStdString());
//     }
//     recording = !recording;
// }

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
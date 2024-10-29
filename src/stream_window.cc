#include "stream_window.h"

#include <fmt/core.h>
#include <glib.h>
#include <gst/app/gstappsink.h>
#include <gst/gstbuffer.h>
#define GST_USE_UNSTABLE_API
#include <gst/codecparsers/gsth265parser.h>
#include <gst/gstelement.h>
#include <gst/gstobject.h>
#include <gst/gstpad.h>
#include <gst/gstpipeline.h>
#include <gst/gstsample.h>
#include <gst/gststructure.h>
#include <gst/video/video-info.h>
#include <qnamespace.h>
#include <spdlog/spdlog.h>

#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QObject>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>

#include "../libxvc.h"
#include "safedeque.h"
#include "xdaq_camera_control.h"



using nlohmann::json;
using namespace std::chrono_literals;

#pragma pack(push, 1)
struct PtsMetadata {
    uint64_t pts;
    XDAQFrameData metadata;
};
#pragma pack(pop)

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

static GstPadProbeReturn extract_metadata(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    static bool additional_metadata =
        QSettings("KonteX", "VC").value("additional_metadata").toBool();
    // static bool recording = false;  // prevent record during recording
    auto stream_window = (StreamWindow *) user_data;
    stream_window->sample_counter++;
    if (stream_window->recording) stream_window->record_counter++;
    if (stream_window->camera->get_id() == -1) {
        return GST_PAD_PROBE_OK;
    }

    auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buffer) return GST_PAD_PROBE_OK;

    for (int j = 0; j < gst_buffer_n_memory(buffer); ++j) {  // henry
        GstMemory *mem2 = gst_buffer_get_memory(buffer, j);
        GstMapInfo mem_map;
        gst_memory_map(mem2, &mem_map, GST_MAP_READ);
        printf("\nbuffer index: %d, map_size: %lu\n", j, mem_map.size);
        for (size_t i = 0; i < std::clamp<int>(mem_map.size, 0, 64); i++) {
            printf(" %02x", mem_map.data[i]);
            if ((i + 1) % 16 == 0) {
                printf(" ");
            }
        }
        printf("\n");
        gst_memory_unmap(mem2, &mem_map);
        gst_memory_unref(mem2);
    }

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
            gst_memory_unmap(mem_in_buffer.get(), &map_info);
            continue;
        }
        GstH265ParserResult parse_result = gst_h265_parser_identify_nalu_unchecked(
            nalu_parser.get(), map_info.data, 0, map_info.size, &nalu
        );
        printf("type: %d, size: %u\n", nalu.type, nalu.size);  // henry
        if (parse_result == GST_H265_PARSER_OK || parse_result == GST_H265_PARSER_NO_NAL_END ||
            parse_result == GST_H265_PARSER_NO_NAL) {
            // g_print("type: %d\n", nalu.type);

            if (nalu.type == GST_H265_NAL_SLICE_CRA_NUT || nalu.type == GST_H265_NAL_VPS ||
                nalu.type == GST_H265_NAL_SPS || nalu.type == GST_H265_NAL_PPS ||
                nalu.type == GST_H265_NAL_PREFIX_SEI || nalu.type == GST_H265_NAL_SUFFIX_SEI) {
                stream_window->parse_counter++;

                bool start_saving_bin = false;
                if (!stream_window->start_save_bin && (nalu.type == GST_H265_NAL_VPS || nalu.type == GST_H265_NAL_SPS ||
                    nalu.type == GST_H265_NAL_PPS)) {
                        start_saving_bin = true;
                }

                if (nalu.type == GST_H265_NAL_SLICE_CRA_NUT) {
                    GstMemory *mem2 = gst_buffer_get_memory(buffer, 0);
                    GstMapInfo mem_map;
                    gst_memory_map(mem2, &mem_map, GST_MAP_READ);
                    for (size_t i = 0; i < 64; i++) {
                        printf(" %02x", mem_map.data[nalu.size + i]);
                        if ((i + 1) % 16 == 0) {
                            printf(" ");
                        }
                    }
                    printf("\n");

                    char a[] = {0x00, 0x00, 0x01, 0x4e};

                    // auto it = std::search(
                    // mem_map.data, mem_map.data+nalu.size,
                    // std::begin(a), std::end(a));

                    // if (it == mem_map.data+nalu.size)
                    // {
                    //     printf("subrange not found\n");
                    //     // not found
                    // }
                    // else
                    // {
                    //     printf("subrange found at std::distance(std::begin(Buffer), it): %lu\n",
                    //     std::distance(mem_map.data, it));
                    //     // subrange found at std::distance(std::begin(Buffer), it)
                    // }

                    std::string needle("\x00\x00\x01\x4e", 4);
                    // std::string needle("\x01\x2a\x01\xaf");
                    printf("needle: %s\n, len: %lu\n", needle.c_str(), needle.length());
                    std::string haystack(
                        mem_map.data,
                        mem_map.data + nalu.size
                    );  // or "+ sizeof Buffer"

                    std::size_t n = haystack.find(needle);

                    if (n == std::string::npos) {
                        printf("subrange not found\n");
                        // not found
                    } else {
                        printf("subrange found at std::distance(std::begin(Buffer), it): %lu\n", n);
                        // position is n
                    }
                    gst_memory_unmap(mem2, &mem_map);
                    gst_memory_unref(mem2);
                }
                // parsing sei by adding offset of vps size
                if (nalu.type == GST_H265_NAL_VPS) {
                    size_t vps_size = 76;  // Extract size accordingly, found by trial and error

                    gst_h265_parser_identify_nalu_unchecked(
                        nalu_parser.get(), map_info.data, vps_size, map_info.size - vps_size, &nalu
                    );
                }


                GArray *array = g_array_new(false, false, sizeof(GstH265SEIMessage));
                // typedef enum {
                //     GST_H265_PARSER_OK,
                //     GST_H265_PARSER_BROKEN_DATA,
                //     GST_H265_PARSER_BROKEN_LINK,
                //     GST_H265_PARSER_ERROR,
                //     GST_H265_PARSER_NO_NAL,
                //     GST_H265_PARSER_NO_NAL_END
                // } GstH265ParserResult;
                // FIXME: gst_h265_parser_parse_sei output 3 = GST_H265_PARSER_ERROR
                gst_h265_parser_parse_sei(nalu_parser.get(), &nalu, &array);
                GstH265SEIMessage sei_msg = g_array_index(array, GstH265SEIMessage, 0);
                GstH265RegisteredUserData register_user_data = sei_msg.payload.registered_user_data;

                PtsMetadata pts_metadata;
                // PtsMetadata pts_metadata2;

                std::memcpy(&pts_metadata, register_user_data.data, register_user_data.size);
                // std::memcpy(&pts_metadata2, register_user_data.data, register_user_data.size);



                XDAQFrameData metadata = pts_metadata.metadata;
                // std::memcpy(&metadata, &(pts_metadata.metadata), sizeof(pts_metadata.metadata));

                fmt::print(
                    "pts: {}, fpga_timestamp: {}\n",
                    pts_metadata.pts,
                    pts_metadata.metadata.fpga_timestamp
                );
                fmt::print(
                    "pts2: {}, fpga_timestamp2: {}\n",
                    pts_metadata.pts,
                    pts_metadata.metadata.fpga_timestamp
                );

                // XDAQFrameData metadata;
                // std::memcpy(&metadata, register_user_data.data, sizeof(XDAQFrameData));

                if (!stream_window->recording && metadata.ttl_in) {
                    stream_window->recording = true;

                    QSettings settings("KonteX", "VC");
                    settings.beginGroup(stream_window->camera->get_name());
                    auto trigger_on = settings.value("trigger_on", false).toBool();
                    settings.endGroup();

                    if (trigger_on) {
                        settings.beginGroup(stream_window->camera->get_name());
                        auto save_path =
                            settings.value("save_path", QDir::currentPath()).toString();
                        QString dir_name;
                        if (settings.value("dir_date", true).toBool()) {
                            dir_name = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
                        } else {
                            dir_name = settings.value("dir_name").toString();
                        }
                        int trigger_duration = settings.value("trigger_duration", 10).toInt();
                        settings.endGroup();
                        fmt::println("trigger_duration = {}", trigger_duration);

                        QDir dir(save_path);
                        if (!dir.exists(dir_name)) {
                            if (!dir.mkdir(dir_name)) {
                                qDebug()
                                    << "Failed to create directory: " << save_path + "/" + dir_name;
                            }
                        }

                        QMetaObject::invokeMethod(
                            stream_window,
                            [stream_window, save_path, dir_name, trigger_duration]() {
                                spdlog::info("Start recording");
                                std::string filepath = save_path.toStdString() + "/" +
                                                       dir_name.toStdString() + "/" +
                                                       stream_window->camera->get_name();
                                xvc::start_recording(
                                    GST_PIPELINE(stream_window->pipeline), filepath
                                );
                                // HACK
                                XDAQCameraControl *xdaq_camera_control =
                                    qobject_cast<XDAQCameraControl *>(
                                        stream_window->parentWidget()->parentWidget()
                                    );
                                xdaq_camera_control->cameras_list->setDisabled(true);
                                xdaq_camera_control->record_settings->setDisabled(true);
                                xdaq_camera_control->record_button->setDisabled(true);
                                xdaq_camera_control->record_button->setText(
                                    QString::fromStdString("STOP")
                                );
                                xdaq_camera_control->record_time->setText(
                                    QString::fromStdString("00:00:00")
                                );
                                xdaq_camera_control->timer->start(1000);
                                xdaq_camera_control->elapsed_time = 0;

                                QTimer::singleShot(
                                    trigger_duration,
                                    [xdaq_camera_control, stream_window]() {
                                        spdlog::info("Stop recording");
                                        xvc::stop_recording(GST_PIPELINE(stream_window->pipeline));
                                        stream_window->recording = false;

                                        xdaq_camera_control->timer->stop();
                                        xdaq_camera_control->cameras_list->setDisabled(false);
                                        xdaq_camera_control->record_settings->setDisabled(false);
                                        xdaq_camera_control->record_button->setDisabled(false);
                                        xdaq_camera_control->record_button->setText(
                                            QString::fromStdString("REC")
                                        );
                                    }
                                );
                            }
                        );
                    } else {
                        stream_window->recording = false;
                    }
                }
                // uint64_t pts = buffer->pts;
                fmt::print("push1\n");
                stream_window->safe_deque->push(pts_metadata.pts, pts_metadata.metadata);
                // fmt::print("push2\n");
                stream_window->safe_deque_filesink->push(pts_metadata.pts, pts_metadata.metadata);
                // fmt::print("push done\n");

                buffer = gst_buffer_make_writable(buffer);

                GST_BUFFER_PTS(buffer) = pts_metadata.pts;
                fmt::print("before print pts\n");
                fmt::print("pts: {}\n", pts_metadata.pts);

                if (start_saving_bin && additional_metadata && stream_window->recording) {
                    stream_window->start_save_bin = true;
                    stream_window->first_iframe_timestamp = pts_metadata.pts;
                    // stream_window->record_parse_counter++;
                    // spdlog::info("write data");
                    // stream_window->filestream.write(
                    //     (const char *) &pts_metadata, sizeof(pts_metadata)
                    // );
                }
                fmt::print("here1\n");
                g_array_free(array, true);
                fmt::print("here2\n");


                // parse the rest nalu

                size_t sei_size = 51;  // Extract size accordingly, found by trial and error

                gst_h265_parser_identify_nalu_unchecked(
                    nalu_parser.get(), map_info.data, sei_size, map_info.size - sei_size, &nalu
                );
                fmt::print("&&&&&&&");
                fmt::print("nalu type: {}", nalu.type);
                fmt::print("&&&&&&&");
            }
        }
        fmt::print("here3\n");

        gst_memory_unmap(mem_in_buffer.get(), &map_info);
        fmt::print("here\4n");

        fmt::print("parse_counter: {}\n", stream_window->parse_counter);
        fmt::print("sample_counter: {}\n", stream_window->sample_counter);
        fmt::print("record_counter: {}\n", stream_window->record_counter);
        fmt::print("record_parse_counter: {}\n", stream_window->record_parse_counter);
        fmt::print("record_parse_save_counter: {}\n", stream_window->record_parse_save_counter);

        // fmt::print("counter: {}\n", counter); //henry
    }
    return GST_PAD_PROBE_OK;
}

GstFlowReturn callback(GstAppSink *sink, void *user_data)
{
    static GstClockTime last_time = GST_CLOCK_TIME_NONE;
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

            // pointer to the image data
            // unsigned char* data = info.data;

            GstCaps *caps = gst_sample_get_caps(sample.get());
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

            GstClockTime current_time = GST_BUFFER_PTS(buffer);
            if (last_time != GST_CLOCK_TIME_NONE) {
                GstClockTime diff = current_time - last_time;
                double fps = GST_SECOND / (double) diff;
                stream_window->set_fps(fps);
            }
            last_time = current_time;

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
        }
    }
    return GST_FLOW_OK;
}

StreamWindow::StreamWindow(Camera *_camera, QWidget *parent)
    : QDockWidget(parent), pipeline(nullptr), pause(false), recording(false)
{
    camera = _camera;
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
            gst_element_get_static_pad(parser, "src"), gst_object_unref
        );
        probe_id = gst_pad_add_probe(
            sink_pad.get(), GST_PAD_PROBE_TYPE_BUFFER, extract_metadata, this, NULL
        );
    }

    safe_deque = std::make_unique<SafeDeque::SafeDeque>();
    safe_deque_filesink = std::make_unique<SafeDeque::SafeDeque>();

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
}

StreamWindow::~StreamWindow()
{
    GstElement *parser = gst_bin_get_by_name(GST_BIN(pipeline), "parser");
    GstPad *parser_pad = gst_element_get_static_pad(parser, "sink");
    gst_pad_remove_probe(parser_pad, probe_id);

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
    painter.drawText(
        QRect(10, height() - 60, width() / 2, height() - 60),
        QString::fromStdString(fmt::format("XDAQ Time {:08x}", metadata.fpga_timestamp))
    );
    painter.drawText(
        QRect(10, height() - 30, width() / 2, height() - 30),
        QString::fromStdString(fmt::format("Ephys Time {:04x}", metadata.rhythm_timestamp))
    );
    if (metadata.ttl_in) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(Qt::yellow));
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

// void StreamWindow::set_image(const QImage &_image)
void StreamWindow::set_image(unsigned char *image_bits, const int width, const int height)
{
    if (!pause) {
        QImage _image =
            QImage((const uchar *) image_bits, width, height, QImage::Format::Format_RGB888);
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
        parse_counter = 0;
        sample_counter = 0;
        record_counter = 0;
        record_parse_counter = 0;
    }
}

void StreamWindow::open_filestream()
{
    spdlog::info("open_filestream");
    QString save_path = QSettings("KonteX", "VC").value("save_path").toString();
    QString dir_name = QSettings("KonteX", "VC").value("dir_name").toString();
    spdlog::info("file_path = {}", save_path.toStdString());
    spdlog::info("camera->get_name() = {}", camera->get_name());
    spdlog::info("dir_name() = {}", dir_name.toStdString());
    filestream.open(
        fmt::format(
            "{}/{}/{}.bin", save_path.toStdString(), dir_name.toStdString(), camera->get_name()
        ),
        std::ios::binary | std::ios::app | std::ios::out
    );
    record_counter = 0;
    record_parse_counter = 0;
}

void StreamWindow::close_filestream()
{
    spdlog::info("close_filestream");
    filestream.close();
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
#include "libxvc.h"

#include <fmt/core.h>
#include <glib-object.h>
#include <glib.h>
#include <glibconfig.h>
#define GST_USE_UNSTABLE_API
#include <gst/codecparsers/gsth265parser.h>
#include <gst/gst.h>
#include <gst/gstbin.h>
#include <gst/gstbuffer.h>
#include <gst/gstcaps.h>
#include <gst/gstelement.h>
#include <gst/gstelementfactory.h>
#include <gst/gstevent.h>
#include <gst/gstinfo.h>
#include <gst/gstmeta.h>
#include <gst/gstobject.h>
#include <gst/gstpad.h>
#include <gst/gstparse.h>
#include <gst/gstpipeline.h>
#include <gst/gststructure.h>
#include <gst/gstutils.h>
#include <gst/video/video-info.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string>

using namespace std::chrono_literals;

GstElement *create_element(const gchar *factoryname, const gchar *name)
{
    GstElement *element = gst_element_factory_make(factoryname, name);
    if (!element) {
        g_error("Element %s could not be created.", factoryname);
    }
    return element;
}

namespace xvc
{

// GstElement *open_video_stream(GstPipeline *pipeline, Camera *camera)
void setup_h265_srt_stream(GstPipeline *pipeline, const int port)
{
    g_info("setup_h265_srt_stream");

    GstElement *src = create_element("udpsrc", "src");
    GstElement *cf_src = create_element("capsfilter", "cf_src");
    GstElement *depay = create_element("rtph265depay", "depay");
    GstElement *parser = create_element("h265parse", "parser");
    GstElement *cf_parser = create_element("capsfilter", "cf_parser");
    GstElement *tee = create_element("tee", "t");
    GstElement *queue_display = create_element("queue", "queue_display");
#ifdef _WIN32
    // GstElement *decoder = create_element("d3d11h265dec", "dec");
    GstElement *decoder = create_element("d3d11h265device1dec", "dec");
#elif __APPLE__
    GstElement *decoder = create_element("vtdec", "dec");
#else
    GstElement *decoder = create_element("avdec_h265", "dec");
#endif
    GstElement *cf_dec = create_element("capsfilter", "cf_dec");
    GstElement *conv = create_element("videoconvert", "conv");
    GstElement *cf_conv = create_element("capsfilter", "cf_conv");
    GstElement *appsink = create_element("appsink", "appsink");

    // clang-format off
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_src_caps(
        gst_caps_new_simple(
        "application/x-rtp",
        "encoding-name", G_TYPE_STRING, "H265", 
        NULL),
        gst_caps_unref
    );
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_parser_caps(
        gst_caps_new_simple(
        "video/x-h265",
        "stream-format", G_TYPE_STRING, "byte-stream", 
        "alignment", G_TYPE_STRING, "au", 
        nullptr),
        gst_caps_unref
    );
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_dec_caps(
        gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "NV12", 
        nullptr),
        gst_caps_unref
    );
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_conv_caps(
        gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGB", 
        nullptr),
        gst_caps_unref
    );
    // clang-format on

    g_object_set(G_OBJECT(src), "port", port, nullptr);
    g_object_set(G_OBJECT(cf_src), "caps", cf_src_caps.get(), nullptr);
    g_object_set(G_OBJECT(cf_parser), "caps", cf_parser_caps.get(), nullptr);
    g_object_set(G_OBJECT(cf_dec), "caps", cf_dec_caps.get(), nullptr);
    g_object_set(G_OBJECT(cf_conv), "caps", cf_conv_caps.get(), nullptr);

    gst_bin_add_many(
        GST_BIN(pipeline),
        src,
        cf_src,
        depay,
        parser,
        cf_parser,
        tee,
        queue_display,
        decoder,
        cf_dec,
        conv,
        cf_conv,
        appsink,
        nullptr
    );

    if (!gst_element_link_many(src, cf_src, depay, parser, cf_parser, tee, nullptr) ||
        !gst_element_link_many(
            tee, queue_display, decoder, cf_dec, conv, cf_conv, appsink, nullptr
        )) {
        g_error("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return;
    }

    std::unique_ptr<GstPad, decltype(&gst_object_unref)> sink_pad(
        gst_element_get_static_pad(parser, "sink"), gst_object_unref
    );
    gst_pad_add_probe(
        sink_pad.get(),
        GST_PAD_PROBE_TYPE_BUFFER,
        [](GstPad *pad, GstPadProbeInfo *info, gpointer user_data) -> GstPadProbeReturn {
            auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
            if (!buffer) return GST_PAD_PROBE_OK;
            GstMapInfo map_info;
            for (unsigned int i = 0; i < gst_buffer_n_memory(buffer); ++i) {
                std::unique_ptr<GstMemory, decltype(&gst_memory_unref)> mem_in_buffer(
                    gst_buffer_get_memory(buffer, i), gst_memory_unref
                );
                if (gst_memory_map(mem_in_buffer.get(), &map_info, GST_MAP_READ)) {
                    std::unique_ptr<GstH265Parser, decltype(&gst_h265_parser_free)> nalu_parser(
                        gst_h265_parser_new(), gst_h265_parser_free
                    );
                    GstH265NalUnit nalu;
                    GstH265ParserResult parse_result = gst_h265_parser_identify_nalu_unchecked(
                        nalu_parser.get(), map_info.data, 0, map_info.size, &nalu
                    );
                    if (parse_result == GST_H265_PARSER_OK) {
                        if (nalu.type == GST_H265_NAL_VPS) {
                            if (last_i_frame_buffer) {
                                gst_buffer_unref(last_i_frame_buffer);
                                last_i_frame_buffer = nullptr;
                            }
                            for (auto b : last_frame_buffer) {
                                gst_buffer_unref(b);
                            }
                            last_frame_buffer.clear();

                            last_i_frame_buffer = gst_buffer_ref(buffer);
                            // spdlog::info(
                            //     "I-Frame detected last_i_frame_buffer.pts = {}",
                            //     last_i_frame_buffer->pts
                            // );
                        } else {
                            last_frame_buffer.push_back(gst_buffer_ref(buffer));
                            // spdlog::info(
                            //     "SEI-Frame detected last_frame_buffer.pts = {}",
                            //     last_frame_buffer.back()->pts
                            // );
                        }
                    }
                    gst_memory_unmap(mem_in_buffer.get(), &map_info);
                }
            }
            return GST_PAD_PROBE_OK;
        },
        NULL,
        NULL
    );

    // tell appsink to notify us when it receives an image
    // g_object_set(G_OBJECT(sink), "emit-signals", TRUE, NULL);

    // tell appsink what function to call when it notifies us
    // g_signal_connect(sink, "new-sample", G_CALLBACK(callback), NULL);
}

void setup_jpeg_srt_stream(GstPipeline *pipeline, const std::string &uri)
{
    g_info("setup_jpeg_srt_stream");

    GstElement *src = create_element("srtclientsrc", "src");
    GstElement *tee = create_element("tee", "t");
    GstElement *parser = create_element("jpegparse", "parser");
    GstElement *queue_display = create_element("queue", "queue_display");
#ifdef _WIN32
    GstElement *dec = create_element("qsvjpegdec", "dec");
#else
    GstElement *dec = create_element("jpegdec", "dec");
#endif
    GstElement *conv = create_element("videoconvert", "conv");
    GstElement *cf_conv = create_element("capsfilter", "cf_conv");

    GstElement *appsink = create_element("appsink", "appsink");

    // clang-format off
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_conv_caps(
        gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGB", 
        nullptr),
        gst_caps_unref
    );
    // clang-format on

    g_object_set(G_OBJECT(src), "uri", fmt::format("srt://{}", uri).c_str(), nullptr);
    g_object_set(G_OBJECT(cf_conv), "caps", cf_conv_caps.get(), nullptr);

    gst_bin_add_many(
        GST_BIN(pipeline), src, tee, parser, queue_display, dec, conv, cf_conv, appsink, nullptr
    );

    if (!gst_element_link_many(src, tee, nullptr) ||
        !gst_element_link_many(tee, parser, queue_display, dec, conv, cf_conv, appsink, nullptr)) {
        g_error("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return;
    }
}

void start_h265_recording(GstPipeline *pipeline, std::string &filepath)
{
    spdlog::info("start_h265_recording");

    GstElement *tee = gst_bin_get_by_name(GST_BIN(pipeline), "t");
    GstPad *src_pad = gst_element_request_pad_simple(tee, "src_1");

    GstElement *queue_record = create_element("queue", "queue_record");
    GstElement *parser = create_element("h265parse", "record_parser");
    GstElement *cf_parser = create_element("capsfilter", "cf_record_parser");
    GstElement *filesink = create_element("splitmuxsink", "filesink");

    filepath += "-%02d.mkv";
    spdlog::info("filepath = {}", filepath);

    // clang-format off
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_parser_caps(
        gst_caps_new_simple(
        "video/x-h265",
        "stream-format", G_TYPE_STRING, "hvc1", 
        "alignment", G_TYPE_STRING, "au", 
        nullptr),
        gst_caps_unref
    );
    // clang-format on

    g_object_set(G_OBJECT(cf_parser), "caps", cf_parser_caps.get(), nullptr);
    g_object_set(G_OBJECT(filesink), "location", filepath.c_str(), nullptr);
    g_object_set(G_OBJECT(filesink), "max-size-time", 0, nullptr);  // max-size-time=0 -> continuous
    g_object_set(G_OBJECT(filesink), "max-files", 10, nullptr);
    g_object_set(G_OBJECT(filesink), "muxer-factory", "matroskamux", nullptr);

    gst_bin_add_many(GST_BIN(pipeline), queue_record, parser, cf_parser, filesink, nullptr);

    if (!gst_element_link_many(queue_record, parser, cf_parser, filesink, nullptr)) {
        g_error("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return;
    }

    gst_element_sync_state_with_parent(queue_record);
    gst_element_sync_state_with_parent(parser);
    gst_element_sync_state_with_parent(cf_parser);
    gst_element_sync_state_with_parent(filesink);

    std::unique_ptr<GstPad, decltype(&gst_object_unref)> sink_pad(
        gst_element_get_static_pad(queue_record, "sink"), gst_object_unref
    );

    GstPadLinkReturn ret = gst_pad_link(src_pad, sink_pad.get());
    if (GST_PAD_LINK_FAILED(ret)) {
        g_error("Failed to link tee src pad to queue sink pad: %d", ret);
    }
    spdlog::info("Pushing I-frame with PTS: {}", GST_BUFFER_PTS(last_i_frame_buffer));
    gst_pad_push(src_pad, gst_buffer_ref(last_i_frame_buffer));
    for (auto buffer : last_frame_buffer) {
        // spdlog::info("Pushing frame with PTS: {}", GST_BUFFER_PTS(buffer));
        gst_pad_push(src_pad, gst_buffer_ref(buffer));
    }

    GST_DEBUG_BIN_TO_DOT_FILE(
        GST_BIN(pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "video-capture-after-link"
    );
}

void stop_h265_recording(GstPipeline *pipeline)
{
    g_info("stop_h265_recording");
    GstElement *tee = gst_bin_get_by_name(GST_BIN(pipeline), "t");
    GstPad *src_pad = gst_element_get_static_pad(tee, "src_1");
    gst_pad_add_probe(
        src_pad,
        GST_PAD_PROBE_TYPE_IDLE,
        [](GstPad *src_pad, GstPadProbeInfo *info, gpointer user_data) -> GstPadProbeReturn {
            g_info("Unlinking...");
            GstPipeline *pipeline = GST_PIPELINE(user_data);
            GstElement *tee = gst_bin_get_by_name(GST_BIN(pipeline), "t");
            std::unique_ptr<GstElement, decltype(&gst_object_unref)> queue_record(
                gst_bin_get_by_name(GST_BIN(pipeline), "queue_record"), gst_object_unref
            );
            std::unique_ptr<GstElement, decltype(&gst_object_unref)> parser(
                gst_bin_get_by_name(GST_BIN(pipeline), "record_parser"), gst_object_unref
            );
            std::unique_ptr<GstElement, decltype(&gst_object_unref)> cf_parser(
                gst_bin_get_by_name(GST_BIN(pipeline), "cf_record_parser"), gst_object_unref
            );
            std::unique_ptr<GstElement, decltype(&gst_object_unref)> filesink(
                gst_bin_get_by_name(GST_BIN(pipeline), "filesink"), gst_object_unref
            );
            std::unique_ptr<GstPad, decltype(&gst_object_unref)> sink_pad(
                gst_element_get_static_pad(queue_record.get(), "sink"), gst_object_unref
            );
            gst_pad_unlink(src_pad, sink_pad.get());
            gst_pad_send_event(sink_pad.get(), gst_event_new_eos());

            gst_bin_remove(GST_BIN(pipeline), queue_record.get());
            gst_bin_remove(GST_BIN(pipeline), parser.get());
            gst_bin_remove(GST_BIN(pipeline), cf_parser.get());
            gst_bin_remove(GST_BIN(pipeline), filesink.get());

            gst_element_set_state(queue_record.get(), GST_STATE_NULL);
            gst_element_set_state(cf_parser.get(), GST_STATE_NULL);
            gst_element_set_state(parser.get(), GST_STATE_NULL);
            gst_element_set_state(filesink.get(), GST_STATE_NULL);

            gst_element_release_request_pad(tee, src_pad);
            gst_object_unref(src_pad);
            g_info("Unlinked");

            return GST_PAD_PROBE_REMOVE;
        },
        pipeline,
        NULL
    );
}

void start_jpeg_recording(GstPipeline *pipeline, std::string &filepath)
{
    spdlog::info("start_jpeg_recording");

    GstElement *tee = gst_bin_get_by_name(GST_BIN(pipeline), "t");
    GstPad *src_pad = gst_element_request_pad_simple(tee, "src_1");

    GstElement *queue_record = create_element("queue", "queue_record");
    GstElement *parser = create_element("jpegparse", "record_parser");
    GstElement *filesink = create_element("splitmuxsink", "filesink");

    filepath += "-%02d.mkv";

    g_object_set(G_OBJECT(filesink), "location", filepath.c_str(), nullptr);
    g_object_set(G_OBJECT(filesink), "max-size-time", 0, nullptr);  // max-size-time=0 -> continuous
    g_object_set(G_OBJECT(filesink), "max-files", 10, nullptr);
    g_object_set(G_OBJECT(filesink), "muxer-factory", "matroskamux", nullptr);

    gst_bin_add_many(GST_BIN(pipeline), queue_record, parser, filesink, nullptr);

    if (!gst_element_link_many(queue_record, parser, filesink, nullptr)) {
        g_error("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return;
    }

    gst_element_sync_state_with_parent(queue_record);
    gst_element_sync_state_with_parent(parser);
    gst_element_sync_state_with_parent(filesink);

    std::unique_ptr<GstPad, decltype(&gst_object_unref)> sink_pad(
        gst_element_get_static_pad(queue_record, "sink"), gst_object_unref
    );

    GstPadLinkReturn ret = gst_pad_link(src_pad, sink_pad.get());
    if (GST_PAD_LINK_FAILED(ret)) {
        g_error("Failed to link tee src pad to queue sink pad: %d", ret);
    }
    GST_DEBUG_BIN_TO_DOT_FILE(
        GST_BIN(pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "video-capture-after-link"
    );
}

void stop_jpeg_recording(GstPipeline *pipeline)
{
    g_info("stop_jpeg_recording");
    GstElement *tee = gst_bin_get_by_name(GST_BIN(pipeline), "t");
    GstPad *src_pad = gst_element_get_static_pad(tee, "src_1");
    gst_pad_add_probe(
        src_pad,
        GST_PAD_PROBE_TYPE_IDLE,
        [](GstPad *src_pad, GstPadProbeInfo *info, gpointer user_data) -> GstPadProbeReturn {
            g_info("unlink");
            GstPipeline *pipeline = GST_PIPELINE(user_data);
            GstElement *tee = gst_bin_get_by_name(GST_BIN(pipeline), "t");
            std::unique_ptr<GstElement, decltype(&gst_object_unref)> queue_record(
                gst_bin_get_by_name(GST_BIN(pipeline), "queue_record"), gst_object_unref
            );
            std::unique_ptr<GstElement, decltype(&gst_object_unref)> parser(
                gst_bin_get_by_name(GST_BIN(pipeline), "record_parser"), gst_object_unref
            );
            std::unique_ptr<GstElement, decltype(&gst_object_unref)> filesink(
                gst_bin_get_by_name(GST_BIN(pipeline), "filesink"), gst_object_unref
            );
            std::unique_ptr<GstPad, decltype(&gst_object_unref)> sink_pad(
                gst_element_get_static_pad(queue_record.get(), "sink"), gst_object_unref
            );
            gst_pad_send_event(sink_pad.get(), gst_event_new_eos());

            gst_pad_unlink(src_pad, sink_pad.get());

            gst_bin_remove(GST_BIN(pipeline), queue_record.get());
            gst_bin_remove(GST_BIN(pipeline), parser.get());
            gst_bin_remove(GST_BIN(pipeline), filesink.get());

            gst_element_set_state(queue_record.get(), GST_STATE_NULL);
            gst_element_set_state(parser.get(), GST_STATE_NULL);
            gst_element_set_state(filesink.get(), GST_STATE_NULL);

            gst_element_release_request_pad(tee, src_pad);
            gst_object_unref(src_pad);

            return GST_PAD_PROBE_REMOVE;
        },
        pipeline,
        NULL
    );
}

void mock_high_frame_rate(GstPipeline *pipeline, const std::string &uri)
{
    g_info("mock_high_frame_rate");

    GstElement *src = create_element("srtsrc", "src");
    // GstElement *tee = create_element("tee", "t");
    GstElement *parser = create_element("jpegparse", "parser");
    GstElement *dec = create_element("jpegdec", "dec");
    GstElement *conv = create_element("videoconvert", "conv");
    GstElement *cf_conv = create_element("capsfilter", "cf_conv");
    GstElement *queue = create_element("queue", "queue");
    GstElement *appsink = create_element("appsink", "appsink");

    // clang-format off
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_conv_caps(
        gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGB", 
        nullptr),
        gst_caps_unref
    );
    // clang-format on

    g_object_set(G_OBJECT(src), "uri", fmt::format("srt://{}", uri).c_str(), nullptr);
    g_object_set(G_OBJECT(cf_conv), "caps", cf_conv_caps.get(), nullptr);

    gst_bin_add_many(
        GST_BIN(pipeline),
        src,
        parser,
        // tee,
        dec,
        conv,
        cf_conv,
        queue,
        appsink,
        nullptr
    );

    if (!gst_element_link_many(src, parser, dec, conv, cf_conv, queue, appsink, nullptr)) {
        g_error("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return;
    }
}

}  // namespace xvc
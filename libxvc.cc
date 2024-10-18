#include "libxvc.h"

#include <fmt/core.h>
#include <glib-object.h>
#include <glib.h>
#include <glibconfig.h>
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

static GstElement *create_element(const gchar *factoryname, const gchar *name)
{
    GstElement *element = gst_element_factory_make(factoryname, name);
    if (!element) {
        g_error("Element %s could not be created.", factoryname);
    }
    return element;
}

static void link_element(GstElement *src, GstElement *dest)
{
    if (!gst_element_link(src, dest)) {
        g_error(
            "Element %s could not be linked to %s.",
            gst_element_get_name(src),
            gst_element_get_name(dest)
        );
    }
}

static void pad_added_handler(GstElement *src, GstPad *pad, gpointer data)
{
    GstElement *demuxer = (GstElement *) data;
    std::unique_ptr<GstPad, decltype(&gst_object_unref)> sink_pad(
        gst_element_get_static_pad(demuxer, "sink"), gst_object_unref
    );

    g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(pad), GST_ELEMENT_NAME(src));

    if (gst_pad_is_linked(sink_pad.get())) {
        g_warning("pad was linked");
    }

    // fmt::println("Dynamic pad created, linking demuxer/decoder");
    // GstCaps *pad_caps = gst_pad_get_current_caps(pad);
    // GstStructure *pad_struct = gst_caps_get_structure(pad_caps, 0);
    // const gchar* pad_type = gst_structure_get_name(pad_struct);
    // if (!g_str_has_prefix(pad_type, "video/x-raw")) {
    //     g_print("It has type '%s' which is not raw audio. Ignoring.\n", pad_type);
    // }

    GstPadLinkReturn ret = gst_pad_link(pad, sink_pad.get());
    if (ret != GST_PAD_LINK_OK) {
        g_error("Failed to link demuxer pad to queue pad.");
    }
}

namespace xvc
{

// GstElement *open_video_stream(GstPipeline *pipeline, Camera *camera)
void setup_h265_srt_stream(GstPipeline *pipeline, const std::string &uri)
{
    g_info("setup_h265_srt_stream");

    GstElement *src = create_element("srtsrc", "src");
    GstElement *tee = create_element("tee", "t");
    GstElement *queue_display = create_element("queue", "queue_display");
    GstElement *parser = create_element("h265parse", "parser");
    GstElement *cf_parser = create_element("capsfilter", "cf_parser");
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
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_parser_caps(
        gst_caps_new_simple(
        "video/x-h265",
        "stream-format", G_TYPE_STRING, "byte-stream", 
        "alignment", G_TYPE_STRING, "au", 
        NULL),
        gst_caps_unref
    );
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_dec_caps(
        gst_caps_new_simple(
        "video/x-raw(memory:D3D11Memory)",
        "format", G_TYPE_STRING, "NV12", 
        NULL),
        gst_caps_unref
    );
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_conv_caps(
        gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGB", 
        NULL),
        gst_caps_unref
    );
    // clang-format on

    g_object_set(G_OBJECT(src), "uri", fmt::format("srt://{}", uri).c_str(), NULL);
    g_object_set(G_OBJECT(cf_parser), "caps", cf_parser_caps.get(), NULL);
    g_object_set(G_OBJECT(cf_dec), "caps", cf_dec_caps.get(), NULL);
    g_object_set(G_OBJECT(cf_conv), "caps", cf_conv_caps.get(), NULL);

    // gboolean keep_listening = true;
    // gboolean wait_for_connection = false;
    // gboolean auto_reconnect = false;

    // g_object_set(G_OBJECT(src), "mode", 2, NULL);
    // g_object_set(G_OBJECT(src), "wait-for-connection", wait_for_connection, NULL);
    // g_object_set(G_OBJECT(src), "keep-listening", keep_listening, NULL);
    // g_object_set(G_OBJECT(src), "auto-reconnect", auto_reconnect, NULL);

    gst_bin_add_many(
        GST_BIN(pipeline),
        src,
        tee,
        queue_display,
        parser,
        cf_parser,
        decoder,
        cf_dec,
        conv,
        cf_conv,
        appsink,
        NULL
    );

    link_element(src, tee);
    link_element(tee, queue_display);
    link_element(queue_display, parser);
    link_element(parser, cf_parser);
    link_element(cf_parser, decoder);
    link_element(decoder, cf_dec);
    link_element(cf_dec, conv);
    link_element(conv, cf_conv);
    link_element(cf_conv, appsink);

    // tell appsink to notify us when it receives an image
    // g_object_set(G_OBJECT(sink), "emit-signals", TRUE, NULL);

    // tell appsink what function to call when it notifies us
    // g_signal_connect(sink, "new-sample", G_CALLBACK(callback), NULL);
}

void setup_jpeg_srt_stream(GstPipeline *pipeline, const std::string &uri)
{
    g_info("setup_jpeg_srt_stream");

    GstElement *src = create_element("srtsrc", "src");
    GstElement *tee = create_element("tee", "t");
    GstElement *parser = create_element("jpegparse", "parser");
#ifdef _WIN32
    GstElement *dec = create_element("qsvjpegdec", "dec");
    // clang-format off
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_dec_caps(
        gst_caps_new_simple(
        "video/x-raw(memory:D3D11Memory)",
        "format", G_TYPE_STRING, "BGRA",
        NULL),
        gst_caps_unref
    );
    // clang-format on
#else
    GstElement *dec = create_element("jpegdec", "dec");
    // clang-format off
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_dec_caps(
        gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGB", 
        NULL),
        gst_caps_unref
    );
    // clang-format on
#endif
    GstElement *cf_dec = create_element("capsfilter", "cf_dec");
    GstElement *conv = create_element("videoconvert", "conv");
    GstElement *cf_conv = create_element("capsfilter", "cf_conv");
    GstElement *queue_display = create_element("queue", "queue_display");
    GstElement *appsink = create_element("appsink", "appsink");

    // clang-format off
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_conv_caps(
        gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGB", 
        NULL),
        gst_caps_unref
    );
    // clang-format on

    g_object_set(G_OBJECT(src), "uri", fmt::format("srt://{}", uri).c_str(), NULL);
    g_object_set(G_OBJECT(cf_dec), "caps", cf_dec_caps.get(), NULL);
    g_object_set(G_OBJECT(cf_conv), "caps", cf_conv_caps.get(), NULL);

    gst_bin_add_many(
        GST_BIN(pipeline),
        src,
        tee,
        parser,
        dec,
        cf_dec,
        conv,
        cf_conv,
        queue_display,
        appsink,
        NULL
    );

    link_element(src, tee);
    link_element(tee, parser);
    link_element(parser, dec);
    link_element(dec, cf_dec);
    link_element(cf_dec, conv);
    link_element(conv, cf_conv);
    link_element(cf_conv, queue_display);
    link_element(queue_display, appsink);
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

    // clang-format off
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_parser_caps(
        gst_caps_new_simple(
        "video/x-h265",
        "stream-format", G_TYPE_STRING, "hvc1", 
        "alignment", G_TYPE_STRING, "au", 
        NULL),
        gst_caps_unref
    );
    // clang-format on

    g_object_set(G_OBJECT(parser), "config-interval", true, NULL);
    g_object_set(G_OBJECT(cf_parser), "caps", cf_parser_caps.get(), NULL);
    g_object_set(G_OBJECT(filesink), "location", filepath.c_str(), NULL);
    g_object_set(G_OBJECT(filesink), "max-size-time", 0, NULL);  // continuous
    // g_object_set(G_OBJECT(filesink), "max-files", 10, NULL);
    g_object_set(G_OBJECT(filesink), "muxer-factory", "matroskamux", NULL);

    gst_bin_add_many(GST_BIN(pipeline), queue_record, parser, cf_parser, filesink, NULL);

    link_element(queue_record, parser);
    link_element(parser, cf_parser);
    link_element(cf_parser, filesink);

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
            g_info("unlink");
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
            gst_pad_send_event(sink_pad.get(), gst_event_new_eos());

            gst_pad_unlink(src_pad, sink_pad.get());

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

    g_object_set(G_OBJECT(filesink), "location", filepath.c_str(), NULL);
    g_object_set(G_OBJECT(filesink), "max-size-time", 0, NULL);  // continuous
    // g_object_set(G_OBJECT(filesink), "max-files", 10, NULL);
    g_object_set(G_OBJECT(filesink), "muxer-factory", "matroskamux", NULL);

    gst_bin_add_many(GST_BIN(pipeline), queue_record, parser, filesink, NULL);

    link_element(queue_record, parser);
    link_element(parser, filesink);

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

void open_video(GstPipeline *pipeline, const std::string &filepath)
{
    g_info("open_video");

    GstElement *src = create_element("filesrc", "src");
    GstElement *demuxer = create_element("matroskademux", "demuxer");
    GstElement *queue = create_element("queue", "queue");
    GstElement *parser = create_element("h265parse", "parser");
    GstElement *cf_parser = create_element("capsfilter", "cf_parser");
#ifdef _WIN32
    // GstElement *decoder = create_element("d3d11h265dec", "dec");
    GstElement *decoder = create_element("d3d11h265device1dec", "dec");
#elif __APPLE__
    GstElement *decoder = create_element("vtdec", "dec");
#else
    GstElement *decoder = create_element("avdec_h265", "dec");
#endif
    GstElement *conv = create_element("videoconvert", "conv");
    GstElement *cf_conv = create_element("capsfilter", "cf_conv");
    GstElement *appsink = create_element("appsink", "appsink");

    // clang-format off
    GstCaps *cf_parser_caps = gst_caps_new_simple(
        "video/x-h265",
        "stream-format", G_TYPE_STRING, "byte-stream", 
        "alignment", G_TYPE_STRING, "au", 
        NULL
    );
    GstCaps *cf_conv_caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGB", 
        NULL
    );
    // clang-format on

    g_object_set(G_OBJECT(src), "location", filepath.c_str(), NULL);
    g_object_set(G_OBJECT(cf_parser), "caps", cf_parser_caps, NULL);
    g_object_set(G_OBJECT(cf_conv), "caps", cf_conv_caps, NULL);
    gst_bin_add_many(
        GST_BIN(pipeline),
        src,
        demuxer,
        queue,
        parser,
        cf_parser,
        decoder,
        conv,
        cf_conv,
        appsink,
        NULL
    );

    link_element(src, demuxer);
    link_element(queue, parser);
    link_element(parser, cf_parser);
    link_element(cf_parser, decoder);
    link_element(decoder, conv);
    link_element(conv, cf_conv);
    link_element(cf_conv, appsink);

    g_signal_connect(demuxer, "pad-added", G_CALLBACK(pad_added_handler), queue);
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
        NULL),
        gst_caps_unref
    );
    // clang-format on

    g_object_set(G_OBJECT(src), "uri", fmt::format("srt://{}", uri).c_str(), NULL);
    g_object_set(G_OBJECT(cf_conv), "caps", cf_conv_caps.get(), NULL);

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
        NULL
    );

    if (!gst_element_link_many(src, parser, dec, conv, cf_conv, queue, appsink, NULL)) {
        g_error("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return;
    }
}

}  // namespace xvc
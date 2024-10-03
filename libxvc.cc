#include "libxvc.h"

#include <cpr/cpr.h>
#include <cpr/timeout.h>
#include <fmt/core.h>
#include <glib.h>
#include <glibconfig.h>
#include <gst/app/gstappsink.h>
#include <gst/codecparsers/gsth265parser.h>
// #define GST_USE_UNSTABLE_API
#include <gst/gst.h>
#include <gst/gstbin.h>
#include <gst/gstbuffer.h>
#include <gst/gstcaps.h>
#include <gst/gstdevice.h>
#include <gst/gstdevicemonitor.h>
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

#include <cstddef>
#include <cstdlib>
// #include <memory>
#include <memory>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>

using nlohmann::json;
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
void open_video_stream(GstPipeline *pipeline, std::string ip)
{
    g_info("open_video_stream");

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

    std::string uri = fmt::format("srt://{}", ip);

    // clang-format off
    GstCaps *cf_parser_caps = gst_caps_new_simple(
        "video/x-h265",
        "stream-format", G_TYPE_STRING, "byte-stream", 
        "alignment", G_TYPE_STRING, "au", 
        NULL
    );
    GstCaps *cf_dec_caps = gst_caps_new_simple(
        "video/x-raw(memory:D3D11Memory)",
        "format", G_TYPE_STRING, "NV12", 
        NULL
    );
    GstCaps *cf_conv_caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGB", 
        NULL
    );
    // clang-format on

    // g_object_set(G_OBJECT(cf_src), "caps", cf_src_caps, NULL);
    g_object_set(G_OBJECT(src), "uri", uri.c_str(), NULL);
    g_object_set(G_OBJECT(cf_parser), "caps", cf_parser_caps, NULL);
    g_object_set(G_OBJECT(cf_dec), "caps", cf_dec_caps, NULL);
    g_object_set(G_OBJECT(cf_conv), "caps", cf_conv_caps, NULL);

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
    // link_element(parser, decoder);
    // link_element(decoder, conv);
    link_element(decoder, cf_dec);
    link_element(cf_dec, conv);
    link_element(conv, cf_conv);
    link_element(cf_conv, appsink);

    // tell appsink to notify us when it receives an image
    // g_object_set(G_OBJECT(sink), "emit-signals", TRUE, NULL);

    // tell appsink what function to call when it notifies us
    // g_signal_connect(sink, "new-sample", G_CALLBACK(callback), NULL);
}

static GstPadProbeReturn unlink(GstPad *src_pad, GstPadProbeInfo *info, gpointer user_data)
{
    g_info("unlink");
    GstPipeline *pipeline = GST_PIPELINE(user_data);
    GstElement *tee = gst_bin_get_by_name(GST_BIN(pipeline), "t");
    std::unique_ptr<GstElement, decltype(&gst_object_unref)> queue_record(
        gst_bin_get_by_name(GST_BIN(pipeline), "queue_record"), gst_object_unref
    );
    // GstElement *queue_record = gst_bin_get_by_name(GST_BIN(pipeline), "queue_record");
    GstElement *parser = gst_bin_get_by_name(GST_BIN(pipeline), "record_parser");
    // GstElement *muxer = gst_bin_get_by_name(GST_BIN(pipeline), "muxer");
    GstElement *cf_parser = gst_bin_get_by_name(GST_BIN(pipeline), "cf_record_parser");
    GstElement *filesink = gst_bin_get_by_name(GST_BIN(pipeline), "filesink");

    std::unique_ptr<GstPad, decltype(&gst_object_unref)> sink_pad(
        gst_element_get_static_pad(queue_record.get(), "sink"), gst_object_unref
    );
    gst_pad_send_event(sink_pad.get(), gst_event_new_eos());

    gst_pad_unlink(src_pad, sink_pad.get());

    gst_bin_remove(GST_BIN(pipeline), queue_record.get());
    gst_bin_remove(GST_BIN(pipeline), parser);
    // gst_bin_remove(GST_BIN(pipeline), muxer);
    gst_bin_remove(GST_BIN(pipeline), cf_parser);
    gst_bin_remove(GST_BIN(pipeline), filesink);

    gst_element_set_state(queue_record.get(), GST_STATE_NULL);
    // gst_element_set_state(muxer, GST_STATE_NULL);
    gst_element_set_state(cf_parser, GST_STATE_NULL);
    gst_element_set_state(parser, GST_STATE_NULL);
    gst_element_set_state(filesink, GST_STATE_NULL);

    // gst_object_unref(queue_record);
    gst_object_unref(parser);
    // gst_object_unref(muxer);
    gst_object_unref(cf_parser);
    gst_object_unref(filesink);

    gst_element_release_request_pad(tee, src_pad);
    gst_object_unref(src_pad);

    return GST_PAD_PROBE_REMOVE;
}

void start_recording(GstPipeline *pipeline, std::string filepath)
{
    g_warning("start_recording");

    GstElement *tee = gst_bin_get_by_name(GST_BIN(pipeline), "t");
    GstPad *src_pad = gst_element_request_pad_simple(tee, "src_1");

    GstElement *queue_record = create_element("queue", "queue_record");
    GstElement *parser = create_element("h265parse", "record_parser");
    GstElement *cf_parser = create_element("capsfilter", "cf_record_parser");
    // GstElement *muxer = create_element("matroskamux", "muxer");
    GstElement *filesink = create_element("splitmuxsink", "filesink");

    filepath += ".mkv";

    // clang-format off
    GstCaps *cf_parser_caps = gst_caps_new_simple(
        "video/x-h265",
        "stream-format", G_TYPE_STRING, "hvc1", 
        "alignment", G_TYPE_STRING, "au", 
        NULL
    );
    // clang-format on

    const unsigned int SEC = 1'000'000'000;
    g_object_set(G_OBJECT(filesink), "location", filepath.c_str(), NULL);
    g_object_set(G_OBJECT(filesink), "max-size-time", 0, NULL);  // in ns
    g_object_set(G_OBJECT(filesink), "muxer-factory", "matroskamux", NULL);
    g_object_set(G_OBJECT(parser), "config-interval", true, NULL);
    g_object_set(G_OBJECT(cf_parser), "caps", cf_parser_caps, NULL);

    // gst_bin_add_many(GST_BIN(pipeline), queue_record, parser, cf_parser, muxer, filesink, NULL);
    gst_bin_add_many(GST_BIN(pipeline), queue_record, parser, cf_parser, filesink, NULL);

    link_element(queue_record, parser);
    link_element(parser, cf_parser);
    // link_element(cf_parser, muxer);
    // link_element(muxer, filesink);
    link_element(cf_parser, filesink);

    gst_element_sync_state_with_parent(queue_record);
    gst_element_sync_state_with_parent(parser);
    gst_element_sync_state_with_parent(cf_parser);
    // gst_element_sync_state_with_parent(muxer);
    gst_element_sync_state_with_parent(filesink);

    std::unique_ptr<GstPad, decltype(&gst_object_unref)> sink_pad(
        gst_element_get_static_pad(queue_record, "sink"), gst_object_unref
    );

    GstPadLinkReturn ret = gst_pad_link(src_pad, sink_pad.get());
    if (GST_PAD_LINK_FAILED(ret)) {
        g_error("Failed to link tee src pad to queue sink pad: %d", ret);
    } else {
        g_info("Linked succeeded");
        // GST_DEBUG_BIN_TO_DOT_FILE(
        //     GST_BIN(pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "video-capture-after-link"
        // );
    }
}


void stop_recording(GstPipeline *pipeline)
{
    g_info("stop_recording");
    GstElement *tee = gst_bin_get_by_name(GST_BIN(pipeline), "t");
    GstPad *src_pad = gst_element_get_static_pad(tee, "src_1");
    gst_pad_add_probe(src_pad, GST_PAD_PROBE_TYPE_IDLE, unlink, pipeline, NULL);
}

void open_video(GstPipeline *pipeline, std::string filename)
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

    // g_object_set(G_OBJECT(cf), "caps", cf_caps, NULL);
    g_object_set(G_OBJECT(src), "location", filename.c_str(), NULL);
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

}  // namespace xvc
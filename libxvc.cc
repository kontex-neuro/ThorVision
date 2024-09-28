#include "libxvc.h"

#include <cpr/cpr.h>
#include <cpr/timeout.h>
#include <fmt/core.h>
#include <glibconfig.h>
#include <gst/app/gstappsink.h>
#include <gst/codecparsers/gsth265parser.h>
#include <gst/gst.h>
#include <gst/gstbin.h>
#include <gst/gstbuffer.h>
#include <gst/gstcaps.h>
#include <gst/gstdevice.h>
#include <gst/gstdevicemonitor.h>
#include <gst/gstelement.h>
#include <gst/gstelementfactory.h>
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
#include <fstream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
// #include <vector>

using nlohmann::json;
using namespace std::chrono_literals;

static GstElement *create_element(const gchar *factoryname, const gchar *name)
{
    GstElement *element = gst_element_factory_make(factoryname, name);
    if (!element) {
        gst_object_unref(element);
        g_error("Element %s could not be created.", factoryname);
    }
    return element;
}

static void link_element(GstElement *src, GstElement *dest)
{
    if (!gst_element_link(src, dest)) {
        gst_object_unref(src);
        gst_object_unref(dest);
        g_error(
            "Element %s could not be linked to %s.",
            gst_element_get_name(src),
            gst_element_get_name(dest)
        );
    }
}

static void pad_added_handler(GstElement *src, GstPad *pad, gpointer data)
{
    GstElement *queue = (GstElement *) data;
    GstPad *sink_pad = gst_element_get_static_pad(queue, "sink");

    g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(pad), GST_ELEMENT_NAME(src));

    if (gst_pad_is_linked(sink_pad)) {
        g_warning("pad was linked");
        g_object_unref(sink_pad);
        return;
    }

    // fmt::println("Dynamic pad created, linking demuxer/decoder");
    // GstCaps *pad_caps = gst_pad_get_current_caps(pad);
    // GstStructure *pad_struct = gst_caps_get_structure(pad_caps, 0);
    // const gchar* pad_type = gst_structure_get_name(pad_struct);
    // if (!g_str_has_prefix(pad_type, "video/x-raw")) {
    //     g_print("It has type '%s' which is not raw audio. Ignoring.\n", pad_type);
    // }

    GstPadLinkReturn ret = gst_pad_link(pad, sink_pad);
    if (ret != GST_PAD_LINK_OK) {
        gst_object_unref(sink_pad);
        g_error("Failed to link demuxer pad to queue pad.");
        return;
    }

    gst_object_unref(sink_pad);
}

namespace xvc
{

// std::vector<xvc::Camera> xvc::list_cameras(std::string server_ip)
// std::string list_cameras(std::string url)
// {
//     g_info("list_cameras");

//     auto response = cpr::Get(cpr::Url{url}, cpr::Timeout{1s});
//     // assert(response.elapsed <= 1);
//     if (response.status_code == 200) {
//         return json::parse(response.text).dump(2);
//     }
//     return "";
// }

/*
  This function will be called in a separate thread when our appsink
  says there is data for us. user_data has to be defined
  when calling g_signal_connect. It can be used to pass objects etc.
  from your other function to the callback.

  user_data can point to additional data for your usage
            marked as unused to prevent compiler warnings
*/
// static GstFlowReturn callback(GstAppSink *sink, void *user_data __attribute__((unused)))
// {
//     GstSample *sample = NULL;
//     g_signal_emit_by_name(sink, "pull-sample", &sample, NULL);
//     // sample = gst_app_sink_pull_sample(sink);

//     if (sample) {
//         static guint framecount = 0;
//         int pixel_data = -1;

//         GstBuffer *buffer = gst_sample_get_buffer(sample);
//         GstMapInfo info;  // contains the actual image
//         if (gst_buffer_map(buffer, &info, GST_MAP_READ)) {
//             GstVideoInfo *video_info = gst_video_info_new();
//             if (!gst_video_info_from_caps(video_info, gst_sample_get_caps(sample))) {
//                 // Could not parse video info (should not happen)
//                 g_warning("Failed to parse video info");
//                 return GST_FLOW_ERROR;
//             }

//             // pointer to the image data
//             // unsigned char* data = info.data;
//             // fmt::println("data = {}", *data);

//             // Get the pixel value of the center pixel
//             int stride = video_info->finfo->bits / 8;
//             unsigned int pixel_offset = video_info->width / 2 * stride +
//                                         video_info->width * video_info->height / 2 * stride;

//             // this is only one pixel
//             // when dealing with formats like BGRx
//             // pixel_data will consist out of
//             // pixel_offset   => B
//             // pixel_offset+1 => G
//             // pixel_offset+2 => R
//             // pixel_offset+3 => x

//             pixel_data = info.data[pixel_offset];

//             gst_buffer_unmap(buffer, &info);
//             gst_video_info_free(video_info);
//         }

//         GstClockTime timestamp = GST_BUFFER_PTS(buffer);

//         g_print(
//             "Captured frame %d, Pixel Value=%03d Timestamp=%" GST_TIME_FORMAT "            \r",
//             framecount,
//             pixel_data,
//             GST_TIME_ARGS(timestamp)
//         );
//         framecount++;

//         // delete our reference so that gstreamer can handle the sample
//         gst_sample_unref(sample);
//     }
//     return GST_FLOW_OK;
// }

// GstElement *open_video_stream(GstPipeline *pipeline, Camera *camera)
void open_video_stream(GstPipeline *pipeline, std::string ip)
{
    g_info("open_video_stream");

    GstElement *src = create_element("srtsrc", "src");
    GstElement *tee = create_element("tee", "tee");

    // GstElement *src = create_element("videotestsrc", "src");
    // GstElement *cf_src = create_element("capsfilter", "cf");

    GstElement *queue_display = create_element("queue", "queue_display");
    GstElement *parser = create_element("h265parse", "parser");
    GstElement *cf_parser = create_element("capsfilter", "cf_parser");
#ifdef _WIN32
    GstElement *decoder = create_element("d3d11h265dec", "dec");
#elif __APPLE__
    GstElement *decoder = create_element("vtdec", "dec");
#else
    GstElement *decoder = create_element("avdec_h265", "dec");
#endif
    // GstElement *cf_dec = create_element("capsfilter", "cf_dec");
    GstElement *conv = create_element("videoconvert", "conv");
    GstElement *cf_conv = create_element("capsfilter", "cf_conv");
    GstElement *appsink = create_element("appsink", "appsink");

    std::string uri = fmt::format("srt://{}", ip);
    fmt::println("uri = {}", uri);

    // clang-format off
    // GstCaps *cf_src_caps = gst_caps_new_simple(
    //     "video/x-raw",
    //     "format", G_TYPE_STRING, "UYVY", 
    //     "framerate", GST_TYPE_FRACTION, 15, 1,
    //     "width", G_TYPE_INT, 960,
    //     "height", G_TYPE_INT, 540, 
    //     NULL
    // );
    GstCaps *cf_parser_caps = gst_caps_new_simple(
        "video/x-h265",
        "stream-format", G_TYPE_STRING, "byte-stream", 
        "alignment", G_TYPE_STRING, "au", 
        NULL
    );
    // GstCaps *cf_dec_caps = gst_caps_new_simple(
    //     "video/x-raw(memory:D3D11Memory)",
    //     "format", G_TYPE_STRING, "NV12", 
    //     NULL
    // );
    GstCaps *cf_conv_caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGB", 
        NULL
    );
    // clang-format on

    // g_object_set(G_OBJECT(cf_src), "caps", cf_src_caps, NULL);

    g_object_set(G_OBJECT(src), "uri", uri.c_str(), NULL);
    g_object_set(G_OBJECT(cf_parser), "caps", cf_parser_caps, NULL);
    // g_object_set(G_OBJECT(cf_dec), "caps", cf_dec_caps, NULL);
    g_object_set(G_OBJECT(cf_conv), "caps", cf_conv_caps, NULL);

    // gboolean keep_listening = true;
    // gboolean wait_for_connection = false;
    // gboolean auto_reconnect = false;

    // g_object_set(G_OBJECT(src), "mode", 2, NULL);
    // g_object_set(G_OBJECT(src), "wait-for-connection", wait_for_connection, NULL);
    // g_object_set(G_OBJECT(src), "keep-listening", keep_listening, NULL);
    // g_object_set(G_OBJECT(src), "auto-reconnect", auto_reconnect, NULL);

    // gst_bin_add_many(GST_BIN(pipeline), src, capsfilter, queue_display, conv, sink, NULL);
    gst_bin_add_many(
        GST_BIN(pipeline),
        src,
        tee,
        queue_display,
        parser,
        cf_parser,
        decoder,
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
    link_element(decoder, conv);
    link_element(conv, cf_conv);
    link_element(cf_conv, appsink);

    // tell appsink to notify us when it receives an image
    // g_object_set(G_OBJECT(sink), "emit-signals", TRUE, NULL);

    // tell appsink what function to call when it notifies us
    // g_signal_connect(sink, "new-sample", G_CALLBACK(callback), NULL);
}

static GstPadProbeReturn unlink(GstPad *src_pad, GstPadProbeInfo *info, gpointer user_data)
{
    g_warning("unlink");

    GstElement *pipeline = (GstElement *) user_data;

    GstElement *queue_record = gst_bin_get_by_name(GST_BIN(pipeline), "queue_record");
    GstElement *muxer = gst_bin_get_by_name(GST_BIN(pipeline), "muxer");
    GstElement *tee = gst_bin_get_by_name(GST_BIN(pipeline), "tee");

    GstPad *sink_pad = gst_element_get_static_pad(queue_record, "sink");
    gst_pad_unlink(src_pad, sink_pad);
    gst_object_unref(sink_pad);

    gst_element_send_event(muxer, gst_event_new_eos());

    gst_element_release_request_pad(tee, src_pad);
    gst_object_unref(src_pad);

    return GST_PAD_PROBE_REMOVE;
}

void start_recording(GstPipeline *pipeline, std::string filename)
{
    g_warning("start_recording");

    GstElement *tee = gst_bin_get_by_name(GST_BIN(pipeline), "tee");
    // GstPadTemplate *templ =
    //     gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(tee), "src_1");
    // GstPad *srcpad = gst_element_request_pad(tee, templ, NULL, NULL);
    GstPad *src_pad = gst_element_request_pad_simple(tee, "src_1");
    GstElement *queue_record = create_element("queue", "queue_record");
    GstElement *parser = create_element("h265parse", NULL);
    GstElement *cf_parser = create_element("capsfilter", NULL);
    GstElement *muxer = create_element("matroskamux", "muxer");
    GstElement *filesink = create_element("filesink", "filesink");

    filename += ".mkv";

    // clang-format off
    GstCaps *cf_parser_caps = gst_caps_new_simple(
        "video/x-h265",
        "stream-format", G_TYPE_STRING, "hvc1", 
        "alignment", G_TYPE_STRING, "au", 
        NULL
    );
    // clang-format on

    g_object_set(G_OBJECT(filesink), "location", filename.c_str(), NULL);
    g_object_set(G_OBJECT(cf_parser), "caps", cf_parser_caps, NULL);

    gst_bin_add_many(GST_BIN(pipeline), queue_record, parser, cf_parser, muxer, filesink, NULL);

    link_element(queue_record, parser);
    link_element(parser, cf_parser);
    link_element(cf_parser, muxer);
    link_element(muxer, filesink);

    gst_element_sync_state_with_parent(queue_record);
    gst_element_sync_state_with_parent(parser);
    gst_element_sync_state_with_parent(cf_parser);
    gst_element_sync_state_with_parent(muxer);
    gst_element_sync_state_with_parent(filesink);

    GstPad *sink_pad = gst_element_get_static_pad(queue_record, "sink");
    g_warning("link tee src pad to queue sink pad");
    gst_pad_link(src_pad, sink_pad);

    gst_object_unref(sink_pad);
}

void stop_recording(GstPipeline *pipeline)
{
    g_warning("stop_recording");
    GstElement *tee = gst_bin_get_by_name(GST_BIN(pipeline), "tee");
    GstPad *teepad = gst_element_get_static_pad(tee, "src_1");
    gst_pad_add_probe(teepad, GST_PAD_PROBE_TYPE_IDLE, unlink, pipeline, NULL);
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
    GstElement *decoder = create_element("d3d11h265dec", "dec");
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
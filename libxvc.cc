#include "libxvc.h"

#include <fmt/core.h>
#include <glib-object.h>
#include <glib.h>
#include <glibconfig.h>
#include <gst/app/gstappsink.h>

#include "src/portpool.h"

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

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include "key_value_store.h"
#include "src/safedeque.h"


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


std::string getSubstringAfterNthColon(const std::string &str, int n)
{
    size_t pos = 0;
    int colonCount = 0;

    // Loop to find the nth colon
    while (colonCount < n && pos != std::string::npos) {
        pos = str.find(':', pos);
        if (pos != std::string::npos) {
            colonCount++;
            pos++;  // Move past the colon
        }
    }

    // If the nth colon is found, return the substring after it
    if (colonCount == n && pos != std::string::npos) {
        return str.substr(pos);
    }

    // Return an empty string if the nth colon is not found
    return "";
}

// GstElement *open_video_stream(GstPipeline *pipeline, Camera *camera)
void setup_h265_srt_stream(GstPipeline *pipeline, const std::string &uri)
{
    g_info("setup_h265_srt_stream");

    GstElement *src = create_element("udpsrc", "src");
    GstElement *cf_rtp = create_element("capsfilter", "cf_rtp");
    // GstElement *src = create_element("srtsrc", "src");
    GstElement *rtph265depay = create_element("rtph265depay", "rtph265depay");
    GstElement *parser = create_element("h265parse", "parser");
    GstElement *cf_parser = create_element("capsfilter", "cf_parser");

    GstElement *tee = create_element("tee", "t");
    GstElement *queue_display = create_element("queue", "queue_display");
    // GstElement *cf_parser = create_element("capsfilter", "cf_parser");
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
    printf("here1\n");
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_rtp_caps(
        gst_caps_new_simple(
        "application/x-rtp",
        "encoding-name", G_TYPE_STRING, "H265", 
        NULL),
        gst_caps_unref
    );
    printf("here2\n");
    GstCaps* cf_parser_caps = gst_caps_new_simple(
        "video/x-h265",
        "stream-format", G_TYPE_STRING, "byte-stream", 
        "alignment", G_TYPE_STRING, "au", 
        NULL
    );
    printf("here3\n");

    // std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_dec_caps(
    //     gst_caps_new_simple(
    //     "video/x-raw(memory:D3D11Memory)",
    //     "format", G_TYPE_STRING, "NV12", 
    //     NULL),
    //     gst_caps_unref
    // );
    printf("here4\n");
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_conv_caps(
        gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGB", 
        NULL),
        gst_caps_unref
    );
    printf("here5\n");
    // clang-format on

    // g_object_set(G_OBJECT(src), "uri", fmt::format("srt://{}", uri).c_str(), NULL);
    fmt::print("uri: {}", uri);
    auto port = getSubstringAfterNthColon(uri, 1);
    fmt::print("port: {}\n", port);
    g_object_set(G_OBJECT(src), "port", std::atoi(port.c_str()), NULL);
    g_object_set(G_OBJECT(cf_rtp), "caps", cf_rtp_caps.get(), NULL);
    g_object_set(G_OBJECT(cf_parser), "caps", cf_parser_caps, NULL);
    // g_object_set(G_OBJECT(cf_dec), "caps", cf_dec_caps.get(), NULL);
    g_object_set(G_OBJECT(cf_conv), "caps", cf_conv_caps.get(), NULL);
    g_object_set(G_OBJECT(queue_display), "max-size-bytes", 0, NULL);
    g_object_set(G_OBJECT(src), "blocksize", 20000, NULL);

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
        cf_rtp,
        rtph265depay,
        parser,
        cf_parser,
        tee,
        queue_display,
        // cf_parser,
        decoder,
        cf_dec,
        conv,
        cf_conv,
        appsink,
        NULL
    );

    link_element(src, cf_rtp);
    link_element(cf_rtp, rtph265depay);
    link_element(rtph265depay, parser);
    // link_element(src, tee);
    link_element(parser, cf_parser);
    link_element(cf_parser, tee);
    link_element(tee, queue_display);
    link_element(queue_display, decoder);

    // link_element(parser, cf_parser);
    // link_element(cf_parser, decoder);
    // if(!gst_element_link_filtered(parser, tee, cf_parser_caps))
    // {
    //     g_printerr("Linking with cf_parser_caps failed.\n");
    // }
    // gst_caps_unref(cf_parser_caps);
    link_element(decoder, conv);
    // link_element(decoder, cf_dec);
    // link_element(cf_dec, conv);
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
#else
    GstElement *dec = create_element("jpegdec", "dec");
#endif
    GstElement *cf_dec = create_element("capsfilter", "cf_dec");
    GstElement *conv = create_element("videoconvert", "conv");
    GstElement *cf_conv = create_element("capsfilter", "cf_conv");
    GstElement *queue_display = create_element("queue", "queue_display");
    GstElement *appsink = create_element("appsink", "appsink");

    // clang-format off
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_dec_caps(
        gst_caps_new_simple(
        "video/x-raw(memory:D3D11Memory)",
        "format", G_TYPE_STRING, "BGRA", 
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

void start_recording(GstPipeline *pipeline, std::string &filepath)
{
    g_info("start_recording");

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

void stop_recording(GstPipeline *pipeline)
{
    g_info("stop_recording");
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

#pragma pack(push, 1)
struct PtsMetadata {
    uint64_t pts;
    XDAQFrameData metadata;
};
#pragma pack(pop)

void parse_video_save_binary(std::string &video_filepath)
{
    // 使用提供的视频文件名
    // auto pos = video_filepath.find("mkv");
    std::string bin_file_name = video_filepath;
    bin_file_name.replace(bin_file_name.end() - 3, bin_file_name.end(), "bin");

    // std::string video_file_name =
    //     std::filesystem::path(video_filepath.c_str()).stem().string();  // 获取不带扩展名的文件名
    // std::string bin_file_name = video_file_name + ".bin";  // 生成 .bin 文件名
    // KeyValueStore bin_store("C:/Users/kontex/Desktop/project/xvc/build/Release/test.bin");  // 使用生成的文件名
    // bin_store.openFile();
    // // char abc[32] = "12345";
    // KeyValuePair kv;
    // kv.key = 123;
    // kv.value[0] = 'A';
    // bin_store.appendKeyValuePair(kv);
    // bin_store.closeFile();

    struct UserData
    {
        int parsed_frame_count = 0;
        int parse_pts_count = 0;
        KeyValueStore bin_store;

        UserData(std::string filename): bin_store(filename) {}
    };

    std::cout << "bin_file_name: " << bin_file_name << std::endl;

    UserData user_data(bin_file_name);
    user_data.bin_store.openFile();

    // Define the pipeline string
    std::string pipeline_str = " filesrc location=\"" + video_filepath +
                               "\" "
                               " ! matroskademux "
                               " ! h265parse name=h265parse "
                               " ! video/x-h265, stream-format=byte-stream, alignment=au "
                               " ! fakesink ";
    printf("pipeline_str: %s\n", pipeline_str.c_str());

    GError *error = NULL;
    GstElement *pipeline = gst_parse_launch(pipeline_str.c_str(), &error);

    if (!pipeline) {
        std::cerr << "Failed to create pipeline: " << error->message << std::endl;
        g_clear_error(&error);
        return;
    }

    // parse pts : h265parse src
    std::unique_ptr<GstElement, decltype(&gst_object_unref)> h265parse{
        gst_bin_get_by_name(GST_BIN(pipeline), "h265parse"), gst_object_unref
    };
    if (h265parse.get() == nullptr) {
        std::cerr << "Failed to get h265parse element" << std::endl;
        return;
    } else {
        std::unique_ptr<GstPad, decltype(&gst_object_unref)> pad{
            gst_element_get_static_pad(h265parse.get(), "src"), gst_object_unref
        };
        if (pad.get() != nullptr) {
            gst_pad_add_probe(
                pad.get(),
                GST_PAD_PROBE_TYPE_BUFFER,
                [](GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
                    UserData *data = (UserData*)user_data;
                    data->parse_pts_count++;
                    printf(
                        "-----------------------------parse_pts------------------------------------"
                        "--\n"
                    );
                    auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
                    // std::cout << "buffer: " << buffer << std::endl;
                    //  buffer = gst_buffer_make_writable(buffer);

                    if (buffer == NULL) {
                        printf("************************empty buffer*****************************\n"
                        );
                        return GST_PAD_PROBE_OK;
                    }
                    GstMapInfo map_info;
                    // if(!gst_buffer_map(buffer, &map_info, GST_MAP_READ))
                    // {
                    //     printf("gst_buffer_map failed!\n");
                    // }


                    std::cout << "buffer_n_memory: " << gst_buffer_n_memory(buffer) << std::endl;
                    std::cout << "total memory_size: " << gst_buffer_get_size(buffer) << std::endl;
                    std::cout << "buffer pts: " << GST_BUFFER_PTS(buffer) << std::endl;

                    // std::cout << "buffer: " << buffer << std::endl;
                    // std::cout << "buffer offset diff: " << (buffer->offset_end - buffer->offset)
                    // << std::endl; for (int j = 0; j < gst_buffer_n_memory(buffer); ++j) {
                    //     GstMemory *mem = gst_buffer_get_memory(buffer, j);
                    //     GstMapInfo mem_map;
                    //     gst_memory_map(mem, &mem_map, GST_MAP_READ);
                    //     for (size_t i = 0; i < std::clamp<int>(mem_map.size, 0, 64); i++) {
                    //         printf(" %02x", mem_map.data[i]);
                    //         if ((i + 1) % 16 == 0) {
                    //             printf(" ");
                    //         }
                    //     }
                    //     printf("\n");
                    // }
                    // printf("\n");

                    // 创建一个nalu解析器
                    GstH265Parser *nalu_parser = gst_h265_parser_new();
                    // GstMapInfo map_info;

                    GstH265NalUnit nalu;
                    for (int k = 0; k < gst_buffer_n_memory(buffer); ++k) {
                        GstMemory *mem_in_buffer = gst_buffer_get_memory(buffer, k);
                        // GstMapInfo mem_map;
                        gst_memory_map(mem_in_buffer, &map_info, GST_MAP_READ);


                        // GstH265ParserResult parse_result =
                        // gst_h265_parser_identify_nalu(nalu_parser, map_info.data, 0,
                        // map_info.size, &nalu);
                        GstH265ParserResult parse_result = gst_h265_parser_identify_nalu_unchecked(
                            nalu_parser, map_info.data, 0, map_info.size, &nalu
                        );


                        // GstH265Parser *nalu_parser = gst_h265_parser_new();
                        // GstH265NalUnit nalu;
                        // GstH265ParserResult parse_result = gst_h265_parser_parse_nal(nalu_parser,
                        // &nalu); printf("type: %d, layer-id: %d, id2: %d\n", nalu.type,
                        // nalu.layer_id, nalu.temporal_id_plus1);
                        //  GstH265ParserResult parse_result =
                        //  gst_h265_parser_identify_nalu(nalu_parser, map_info.data, 0,
                        //  map_info.size, &nalu);


                        printf("parse_result: %d\n", parse_result);
                        for (int j = 0; j < gst_buffer_n_memory(buffer); ++j) {
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
                            gst_memory_unmap(mem2, &mem_map);
                            gst_memory_unref(mem2);
                        }
                        printf("\n");
                        if (parse_result == GST_H265_PARSER_OK ||
                            parse_result == GST_H265_PARSER_NO_NAL_END ||
                            parse_result == GST_H265_PARSER_NO_NAL)  // GST_H265_PARSER_NO_NAL_END)
                        {
                            printf("type: %d, size: %u\n", nalu.type, nalu.size);
                            if (nalu.type == GST_H265_NAL_SLICE_CRA_NUT ||
                                nalu.type == GST_H265_NAL_VPS || nalu.type == GST_H265_NAL_SPS ||
                                nalu.type == GST_H265_NAL_PPS ||
                                nalu.type == GST_H265_NAL_PREFIX_SEI ||
                                nalu.type ==
                                    GST_H265_NAL_SUFFIX_SEI)  // 在I帧或P帧前面插SEI,也可以在其他位置插入，根据nalu.type判断
                            {
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
                                    //     printf("subrange found at
                                    //     std::distance(std::begin(Buffer), it): %lu\n",
                                    //     std::distance(mem_map.data, it));
                                    //     // subrange found at std::distance(std::begin(Buffer),
                                    //     it)
                                    // }

                                    std::string needle("\x00\x00\x01\x4e", 4);
                                    // std::string needle("\x01\x2a\x01\xaf");
                                    printf(
                                        "needle: %s\n, len: %lu\n", needle.c_str(), needle.length()
                                    );
                                    std::string haystack(
                                        mem_map.data,
                                        mem_map.data + nalu.size
                                    );  // or "+ sizeof Buffer"

                                    std::size_t n = haystack.find(needle);

                                    if (n == std::string::npos) {
                                        printf("subrange not found\n");
                                        // not found
                                    } else {
                                        printf(
                                            "subrange found at std::distance(std::begin(Buffer), "
                                            "it): "
                                            "%lu\n",
                                            n
                                        );
                                        // position is n
                                    }
                                    gst_memory_unmap(mem2, &mem_map);
                                    gst_memory_unref(mem2);
                                }
                                // parsing sei by adding offset of vps size
                                if (nalu.type == GST_H265_NAL_VPS) {
                                    size_t vps_size =
                                        76;  // Extract size accordingly, found by trial and error

                                    gst_h265_parser_identify_nalu_unchecked(
                                        nalu_parser,
                                        map_info.data,
                                        vps_size,
                                        map_info.size - vps_size,
                                        &nalu
                                    );
                                }
                                GArray *array =
                                    g_array_new(false, false, sizeof(GstH265SEIMessage));
                                printf("before parse SEI\n");
                                gst_h265_parser_parse_sei(nalu_parser, &nalu, &array);
                                GstH265SEIMessage sei_msg =
                                    g_array_index(array, GstH265SEIMessage, 0);
                                GstH265RegisteredUserData register_user_data =
                                    sei_msg.payload.registered_user_data;
                                std::string str(
                                    (const char *) register_user_data.data, register_user_data.size
                                );

                                PtsMetadata pts_metadata;
                                std::memcpy(
                                    &pts_metadata, register_user_data.data, register_user_data.size
                                );
                                fmt::print("pts in pts_metadata: {:x}\n", pts_metadata.pts);
                                fmt::print("register_user_data.size: {}", register_user_data.size);
                                // std::cout << "\nSEI: " << std::hex << (uint8_t*)str.c_str() << "\n\n";
                                
                                g_array_free(array, true);

                                std::cout << "stop1";

                                XDAQFrameData metadata;
                                std::memcpy(
                                    &metadata,
                                    &(pts_metadata.metadata),
                                    sizeof(pts_metadata.metadata)
                                );
                                std::cout << "stop2";

                                data->bin_store.appendKeyValuePair(KeyValuePair(
                                    pts_metadata.pts, (const char *) &(pts_metadata.metadata)
                                ));
                                std::cout << "stop3";
                                data->parsed_frame_count++;
                                // std::string delimiter = ".";
                                // std::string token = video_name.substr(0,
                                // video_name.find(delimiter));

                                // std::string csv_file_name = token + ".csv";
                                // write data to file
                                // if(wrtie_file_with_data(csv_file_name, str + "\n"))
                                //     std::cout << "-----------------write timestamp_output.csv
                                //     success!" << std::endl;
                                // else
                                //     std::cout << "-----------------wirte timestamp_output.csv
                                //     fail!"
                                //     << std::endl;


                                // std::cout << "buffer_n_memory: " << gst_buffer_n_memory(buffer)
                                // << std::endl; std::cout << "total memory_size: " <<
                                // gst_buffer_get_size(buffer) << std::endl;

                                // std::cout << "buffer offset : " << (buffer->offset) << std::endl;
                                // std::cout << "buffer_end offset : " << (buffer->offset_end) <<
                                // std::endl; std::cout << "buffer offset diff: " <<
                                // (buffer->offset_end
                                // - buffer->offset) << std::endl; for (int j = 0; j <
                                // gst_buffer_n_memory(buffer); ++j) {
                                //     GstMemory *mem = gst_buffer_get_memory(buffer, j);
                                //     GstMapInfo mem_map;
                                //     gst_memory_map(mem, &mem_map, GST_MAP_READ);
                                //     for (size_t i = 0; i < std::clamp<int>(mem_map.size, 0, 64);
                                //     i++)
                                //     {
                                //         printf(" %02x", mem_map.data[i]);
                                //         if ((i + 1) % 16 == 0) {
                                //             printf(" ");
                                //         }
                                //     }
                                //     printf("\n");
                                // }
                                // printf("\n");
                                printf("OOOOOOOOOOOOOOOOOOOOO SEI OOOOOOOOOOOOOOOOOOOOOOOOOO\n");
                            }
                        }
                    }
                    // 释放nalu解析器
                    gst_h265_parser_free(nalu_parser);


                    return GST_PAD_PROBE_OK;
                },
                &user_data,
                NULL
            );
        }
    }

    // Start playing the pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // Event loop to keep the pipeline running
    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg;
    bool terminate = false;

    while (!terminate) {
        // Wait for a message for up to 100 milliseconds
        msg = gst_bus_timed_pop_filtered(
            bus, 100 * GST_MSECOND, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)
        );

        // Handle errors and EOS messages
        if (msg != NULL) {
            GError *err;
            gchar *debug_info;

            switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg, &err, &debug_info);
                std::cerr << "Error from element " << GST_OBJECT_NAME(msg->src) << ": "
                          << err->message << std::endl;
                g_clear_error(&err);
                g_free(debug_info);
                terminate = true;
                break;
            case GST_MESSAGE_EOS:
                std::cout << "End-Of-Stream reached." << std::endl;
                terminate = true;
                break;
            default: break;
            }
            gst_message_unref(msg);
        }
    }

    // Clean up and shutdown the pipeline
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    user_data.bin_store.closeFile();
    std::cout << "parsed_frame_count: " << user_data.parsed_frame_count << std::endl;
    std::cout << "parse_pts_count: " << user_data.parse_pts_count << std::endl;
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
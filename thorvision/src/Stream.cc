#include "Stream.h"

#include <gst/gstelement.h>
#include <gst/gstobject.h>
#include <gst/gstpad.h>
#include <gst/gstutils.h>
#include <spdlog/spdlog.h>

#include <QQuickWindow>
#include <QRunnable>
#include <cassert>

struct StartPipeline : public QRunnable {
    GstPipeline *_pipeline;

    StartPipeline(GstPipeline *p) { _pipeline = (GstPipeline *) gst_object_ref(p); }
    ~StartPipeline()
    {
        if (_pipeline) gst_object_unref(_pipeline);
    }

    void run()
    {
        if (_pipeline) {
            gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_PLAYING);
        }
    }
};

constexpr std::string pipeline_desc(std::string_view uri, [[maybe_unused]] Stream::Codec codec)
{
#ifdef _WIN32
    return std::format(
        "srtclientsrc name=src uri=srt://{} keep-listening=true latency=125 ! "
        "jpegparse name=parser ! "
        "tee name=t ! "
        "queue name=queue_dec leaky=2 ! "
        "jpegdec name=dec ! "
        "d3d11upload name=upload ! "
        "d3d11convert name=conv ! video/x-raw(memory:D3D11Memory), format=(string)RGB ! "
        "queue name=queue_sink leaky=2 ! "
        "fpsdisplaysink name=sink sync=false text-overlay=false",
        uri
    );
#elif __APPLE__
    return std::format(
        "srtclientsrc name=src uri=srt://{} keep-listening=true latency=125 ! "
        "jpegparse name=parser ! "
        "tee name=t ! "
        "queue name=queue_dec leaky=2 ! "
        "vtdec name=dec ! video/x-raw, format=(string)NV12 ! "
        "glupload name=upload ! video/x-raw(memory:GLMemory) ! "
        "glcolorconvert name=conv ! video/x-raw(memory:GLMemory), format=(string)RGB ! "
        "queue name=queue_sink leaky=2 ! "
        "fpsdisplaysink name=sink sync=false text-overlay=false",
        uri
    );
#endif
}

gboolean bus_handler(GstBus *, GstMessage *msg, gpointer user_data)
{
    auto stream = static_cast<Stream *>(user_data);
    if (!stream) {
        spdlog::error("Invalid 'Stream' cast in 'bus_handler' callback");
        return G_SOURCE_REMOVE;
    }

    GError *err = nullptr;
    gchar *debug = nullptr;

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ASYNC_DONE: {
        spdlog::debug("ASYNC_DONE");
        stream->set_streaming(true);
        break;
    }
    case GST_MESSAGE_ERROR: {
        stream->set_streaming(false);

        gst_message_parse_error(msg, &err, &debug);
        spdlog::error("ERROR from element {}: {}", GST_OBJECT_NAME(msg->src), err->message);
        spdlog::debug("Debugging info: {}", (debug) ? debug : "None");
        g_clear_error(&err);
        g_free(debug);

        gst_element_set_state(GST_ELEMENT(stream->pipeline()), GST_STATE_READY);
        break;
    }
    case GST_MESSAGE_WARNING: {
        gst_message_parse_warning(msg, &err, &debug);
        spdlog::warn("Warning from element {}: {}", GST_OBJECT_NAME(msg->src), err->message);
        spdlog::debug("Debugging info: {}", (debug) ? debug : "None");
        g_clear_error(&err);
        g_free(debug);
        break;
    }
    case GST_MESSAGE_EOS: {
        // TODO: error handling
        spdlog::debug("End-Of-Stream reached.");
        stream->set_streaming(false);
        gst_element_set_state(GST_ELEMENT(stream->pipeline()), GST_STATE_READY);
        break;
    }
    case GST_MESSAGE_ELEMENT: {
        auto const structure = gst_message_get_structure(msg);
        if (!gst_structure_has_name(structure, "GstBinForwarded")) {
            break;
        }
        GstMessage *forward_msg = nullptr;

        gst_structure_get(structure, "message", GST_TYPE_MESSAGE, &forward_msg, nullptr);
        if (GST_MESSAGE_TYPE(forward_msg) == GST_MESSAGE_EOS) {
            auto element_name = GST_OBJECT_NAME(GST_MESSAGE_SRC(forward_msg));
            spdlog::debug("EOS from element {}", element_name);

            if (std::format("{}", element_name) != "filesink") {
                spdlog::debug("Not filesink EOS, ignore");
                gst_message_unref(forward_msg);
                break;
            }

            auto pipeline = GST_BIN(stream->pipeline());
            auto tee = gst_bin_get_by_name(pipeline, "t");
            auto queue = gst_bin_get_by_name(pipeline, "queue_record");
            auto parser = gst_bin_get_by_name(pipeline, "record_parser");
            auto filesink = gst_bin_get_by_name(pipeline, "filesink");

            auto queue_sinkpad = gst_element_get_static_pad(queue, "sink");
            auto tee_srcpad = gst_element_get_static_pad(tee, "src_1");

            gst_element_set_state(queue, GST_STATE_NULL);
            gst_element_set_state(parser, GST_STATE_NULL);
            gst_element_set_state(filesink, GST_STATE_NULL);

            gst_bin_remove_many(pipeline, queue, parser, filesink, nullptr);

            gst_element_release_request_pad(tee, tee_srcpad);

            gst_object_unref(tee_srcpad);
            gst_object_unref(tee);
            gst_object_unref(queue_sinkpad);
            gst_object_unref(queue);
            gst_object_unref(parser);
            gst_object_unref(filesink);

            spdlog::debug("Unlinked");
        }
        gst_message_unref(forward_msg);
    } break;
    default: {
        break;
    }
    }
    return G_SOURCE_CONTINUE;
};

GstPadProbeReturn extract_metadata(GstPad *, GstPadProbeInfo *info, gpointer user_data)
{
    auto stream = static_cast<Stream *>(user_data);
    if (!stream) return GST_PAD_PROBE_OK;
    auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buffer) return GST_PAD_PROBE_DROP;

    if (auto xdaqmetadata =
            stream->metadata_handler()._safe_queue.dequeue(GST_BUFFER_PTS(buffer))) {
        QMetaObject::invokeMethod(stream, [stream = QPointer<Stream>(stream), xdaqmetadata]() {
            if (!stream) return;
            emit stream->metadata_received(xdaqmetadata.value());
        });
    }
    return GST_PAD_PROBE_OK;
}

Stream::Stream(QQuickItem *video_item, int index, int port, QObject *parent)
    : QObject(parent),
      _pipeline(nullptr),
      _video_item(video_item),
      _index(index),
      _port(port),
      _streaming(false),
      _parser_pad_id(GST_PAD_PROBE_TYPE_INVALID),
      _dec_pad_id(GST_PAD_PROBE_TYPE_INVALID)
{
    init_pipeline(pipeline_desc(std::format("{}:{}", "192.168.177.100", _port), Codec::MJPEG));

    // TODO UGLY: This callback prevent others windows from going blank when camera unplugged.
    connect(
        _video_item,
        &QQuickItem::windowChanged,
        [stream = QPointer<Stream>(this)](QQuickWindow *window) {
            if (!window) {
                if (!stream || !stream->_pipeline) {
                    spdlog::warn("Stream destroyed before window change handling");
                    return;
                }

                spdlog::info("_video_item lost window, set pipeline to NULL state");

                auto pipeline = stream->_pipeline;
                gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
                gst_object_unref(pipeline);
                stream->_pipeline = nullptr;
            }
        }
    );
}

Stream::~Stream() { reset(); }

bool Stream::init_pipeline(std::string_view pipeline_desc)
{
    spdlog::info("Initializing stream pipeline with desc: {}", pipeline_desc);

    GError *err = nullptr;
    _pipeline = GST_PIPELINE(gst_parse_launch(pipeline_desc.data(), &err));
    if (!_pipeline) {
        spdlog::error("Failed to parse pipeline: {}", err ? err->message : "unknown");
        g_clear_error(&err);
        return false;
    }

#ifdef _WIN32
    auto sink = gst_element_factory_make("qml6d3d11sink", "sink");
    if (!sink) {
        spdlog::error("Failed to create 'qml6d3d11sink' element");
        gst_object_unref(_pipeline);
        return false;
    }
#elif __APPLE__
    auto sink = gst_element_factory_make("qml6glsink", "sink");
    if (!sink) {
        spdlog::error("Failed to create 'qml6glsink' element");
        gst_object_unref(_pipeline);
        return false;
    }
#endif

    auto fpsdisplaysink = gst_bin_get_by_name(GST_BIN(_pipeline), "sink");
    if (!fpsdisplaysink) {
        spdlog::error("Failed to get 'sink' element from pipeline");
        gst_object_unref(_pipeline);
        return false;
    }

    g_object_set(_pipeline, "message-forward", true, nullptr);
    g_object_set(sink, "sync", false, "widget", _video_item.data(), nullptr);
    g_object_set(fpsdisplaysink, "video-sink", sink, nullptr);

    if (auto window = _video_item->window()) {
        window->scheduleRenderJob(
            new StartPipeline(_pipeline), QQuickWindow::BeforeSynchronizingStage
        );
    }

    // // TODO: what this callback does is when camera unplugged, it will not cause other camera's
    // // windows to go blank.
    // _connection = connect(
    //     _video_item,
    //     &QQuickItem::windowChanged,
    //     [stream = QPointer<Stream>(this)](QQuickWindow *window) {
    //         if (!window) {
    //             if (!stream || !stream->_pipeline) {
    //                 spdlog::warn("Stream destroyed before window change handling");
    //                 return;
    //             }

    //             spdlog::info("_video_item lost window, set pipeline to NULL state");

    //             auto pipeline = stream->_pipeline;
    //             gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    //             gst_object_unref(pipeline);
    //             stream->_pipeline = nullptr;
    //         }
    //     }
    // );

    auto parser = gst_bin_get_by_name(GST_BIN(_pipeline), "parser");
    auto parser_srcpad = gst_element_get_static_pad(parser, "src");
    _parser_pad_id = gst_pad_add_probe(
        parser_srcpad, GST_PAD_PROBE_TYPE_BUFFER, parse_jpeg_metadata, &_metadata_handler, nullptr
    );
    gst_object_unref(parser_srcpad);
    gst_object_unref(parser);

    auto dec = gst_bin_get_by_name(GST_BIN(_pipeline), "dec");
    auto dec_sinkpad = gst_element_get_static_pad(dec, "src");
    _dec_pad_id =
        gst_pad_add_probe(dec_sinkpad, GST_PAD_PROBE_TYPE_BUFFER, extract_metadata, this, nullptr);
    gst_object_unref(dec_sinkpad);
    gst_object_unref(dec);

    auto bus = gst_pipeline_get_bus(_pipeline);
    gst_bus_add_watch(bus, bus_handler, this);
    gst_object_unref(bus);

    return _pipeline != nullptr;
}

void Stream::reset()
{
    set_streaming(false);

    if (!_pipeline) {
        spdlog::error("Pipeline is null, cannot reset stream");
        return;
    }

    gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_NULL);
    GstState state;
    if (gst_element_get_state(GST_ELEMENT(_pipeline), &state, nullptr, GST_CLOCK_TIME_NONE) ==
        GST_STATE_CHANGE_FAILURE) {
        spdlog::error("Failed to set pipeline to NULL state");
        return;
    }

    auto parser = gst_bin_get_by_name(GST_BIN(_pipeline), "parser");
    auto parser_srcpad = gst_element_get_static_pad(parser, "src");
    if (_parser_pad_id != GST_PAD_PROBE_TYPE_INVALID) {
        gst_pad_remove_probe(parser_srcpad, _parser_pad_id);
        _parser_pad_id = GST_PAD_PROBE_TYPE_INVALID;
    }
    gst_object_unref(parser_srcpad);
    gst_object_unref(parser);

    auto dec = gst_bin_get_by_name(GST_BIN(_pipeline), "dec");
    auto dec_sinkpad = gst_element_get_static_pad(dec, "src");
    if (_dec_pad_id != GST_PAD_PROBE_TYPE_INVALID) {
        gst_pad_remove_probe(dec_sinkpad, _dec_pad_id);
        _dec_pad_id = GST_PAD_PROBE_TYPE_INVALID;
    }
    gst_object_unref(dec_sinkpad);
    gst_object_unref(dec);

    if (auto sink = gst_bin_get_by_name(GST_BIN(_pipeline), "sink")) {
        g_object_set(sink, "video-sink", nullptr, nullptr);
        gst_object_unref(sink);
    }

    if (auto bus = gst_pipeline_get_bus(_pipeline)) {
        gst_bus_remove_watch(bus);
        gst_object_unref(bus);
    }

    gst_object_unref(_pipeline);
}

bool Stream::start([[maybe_unused]] Codec codec)
{
    if (!_pipeline) {
        spdlog::error("Pipeline is null, cannot start stream");
        return false;
    }

    if (_streaming) {
        reset();
        init_pipeline(pipeline_desc(std::format("{}:{}", "192.168.177.100", _port), codec));
    }
    return true;
}

bool Stream::stop()
{
    if (!_pipeline) {
        spdlog::error("Pipeline is null, cannot stop stream");
        return false;
    }

    if (gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE) {
        spdlog::error("Failed to set pipeline to NULL state");
        return false;
    }
    return true;
}
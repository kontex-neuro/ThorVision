#pragma once

#include <QQuickItem>
#include <QQuickWindow>
#include <QRunnable>
#include <cassert>

#include "xdaqmetadata/metadata_handler.h"

struct StartPipeline : public QRunnable {
    GstPipeline *_pipeline;

    explicit StartPipeline(GstPipeline *p) : _pipeline(p) { setAutoDelete(true); }
    ~StartPipeline() = default;

    void run() override
    {
        if (this->_pipeline) {
            gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_PLAYING);
        }
    }
};

struct Stream : public QObject {
    Q_OBJECT

signals:
    void metadata_received(const XDAQFrameData &metadata);
    void status_changed(bool streaming);

public:
    GstPipeline *_pipeline;
    QQuickItem *_video_item;
    int _index;
    int _port;

    std::atomic_bool _streaming;

    std::optional<GstClockTime> _base_time;
    std::unique_ptr<MetadataHandler> _metadata_handler;

    // TODO: media_type
    static std::string pipeline(std::string_view uri, [[maybe_unused]] std::string_view media_type)
    {
#ifdef _WIN32
        return fmt::format(
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
        // auto dec = gst_element_factory_make("qsvjpegdec", "dec");
        // auto dec = gst_element_factory_make("nvjpegdec", "dec");
        // auto dec = gst_element_factory_make("decodebin", "dec");
#elif __APPLE__
        return fmt::format(
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

    static gboolean bus_handler([[maybe_unused]] GstBus *bus, GstMessage *msg, gpointer user_data)
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
            spdlog::info("ASYNC_DONE");
            stream->set_streaming(true);
            break;
        }
        case GST_MESSAGE_ERROR: {
            stream->set_streaming(false);
            gst_message_parse_error(msg, &err, &debug);
            spdlog::error("ERROR from element {}: {}", GST_OBJECT_NAME(msg->src), err->message);
            spdlog::warn("Debugging info: {}", (debug) ? debug : "None");
            g_clear_error(&err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_WARNING: {
            gst_message_parse_warning(msg, &err, &debug);
            spdlog::warn("Warning from element {}: {}", GST_OBJECT_NAME(msg->src), err->message);
            spdlog::warn("Debugging info: {}", (debug) ? debug : "None");
            g_clear_error(&err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_EOS: {
            // TODO: error handling
            spdlog::info("End-Of-Stream reached.");
            stream->set_streaming(false);
            break;
        }
        case GST_MESSAGE_ELEMENT: {
            auto const structure = gst_message_get_structure(msg);
            if (gst_structure_has_name(structure, "GstBinForwarded")) {
                GstMessage *forward_msg = nullptr;

                gst_structure_get(structure, "message", GST_TYPE_MESSAGE, &forward_msg, nullptr);
                if (GST_MESSAGE_TYPE(forward_msg) == GST_MESSAGE_EOS) {
                    auto element_name = GST_OBJECT_NAME(GST_MESSAGE_SRC(forward_msg));
                    spdlog::info("EOS from element {}", element_name);

                    if (fmt::format("{}", element_name) != "filesink") {
                        spdlog::info("Not filesink EOS, ignore");
                        gst_message_unref(forward_msg);
                        break;
                    }

                    auto pipeline = GST_BIN(stream->_pipeline);
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
            }
            break;
        }
        default: {
            spdlog::debug("Unexpected message type: {}", GST_MESSAGE_TYPE_NAME(msg));
            break;
        }
        }
        return G_SOURCE_CONTINUE;
    };

    Stream(
        std::string_view pipeline_desc, QQuickItem *video_item, int index, int port,
        QObject *parent = nullptr
    )
        : QObject(parent),
          _pipeline(nullptr),
          _video_item(video_item),
          _index(index),
          _port(port),
          _streaming(false)
    {
        _metadata_handler = std::make_unique<MetadataHandler>();
        init_pipeline(pipeline_desc);
    }
    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &) = delete;
    // Stream(Stream &&) = delete;
    // Stream &operator=(Stream &&) = delete;
    Stream(Stream &&stream) noexcept
        : _pipeline(stream._pipeline),
          _video_item(stream._video_item),
          _index(stream._index),
          _port(stream._port),
          _streaming(stream._streaming.load()),
          _base_time(stream._base_time),
          _metadata_handler(std::move(stream._metadata_handler))
    {
    }
    // Stream &operator=(Stream &&stream) noexcept
    // {
    //     if (this == &stream) return *this;
    //     _pipeline = std::exchange(stream._pipeline, nullptr);
    //     _streaming = false;
    //     return *this;
    // }
    Stream &operator=(Stream &&stream) noexcept = delete;

    void init_pipeline(std::string_view pipeline_desc)
    {
        spdlog::debug("Stream::init_pipeline({})", pipeline_desc.data());

        GError *err = nullptr;
        _pipeline = GST_PIPELINE(gst_parse_launch(pipeline_desc.data(), &err));

        if (!_pipeline) {
            spdlog::error("Pipeline parse error: {}", err ? err->message : "unknown");
            g_clear_error(&err);
            return;
        }

#ifdef _WIN32
        auto sink = gst_element_factory_make("qml6d3d11sink", "sink");
        if (!sink) {
            spdlog::error("Failed to create 'qml6d3d11sink' element");
            gst_object_unref(_pipeline);
        }
#elif __APPLE__
        auto sink = gst_element_factory_make("qml6glsink", "sink");
        if (!sink) {
            spdlog::error("Failed to create 'qml6glsink' element");
            gst_object_unref(_pipeline);
        }
#endif

        auto fpsdisplaysink = gst_bin_get_by_name(GST_BIN(_pipeline), "sink");
        g_object_set(_pipeline, "message-forward", true, nullptr);
        g_object_set(sink, "sync", false, "widget", _video_item, nullptr);
        g_object_set(fpsdisplaysink, "video-sink", sink, nullptr);

        auto window = _video_item->window();
        assert(window != nullptr && "Stream::init_pipeline() - window is null");

        window->scheduleRenderJob(
            new StartPipeline(_pipeline), QQuickWindow::BeforeSynchronizingStage
        );

        auto parser = gst_bin_get_by_name(GST_BIN(_pipeline), "parser");
        auto parser_srcpad = gst_element_get_static_pad(parser, "src");
        gst_pad_add_probe(
            parser_srcpad,
            GST_PAD_PROBE_TYPE_BUFFER,
            parse_jpeg_metadata,
            _metadata_handler.get(),
            nullptr
        );
        gst_object_unref(fpsdisplaysink);
        gst_object_unref(parser_srcpad);
        gst_object_unref(parser);

        auto dec = gst_bin_get_by_name(GST_BIN(_pipeline), "dec");
        auto dec_sinkpad = gst_element_get_static_pad(dec, "src");
        gst_pad_add_probe(dec_sinkpad, GST_PAD_PROBE_TYPE_BUFFER, extract_metadata, this, nullptr);
        gst_object_unref(dec_sinkpad);
        gst_object_unref(dec);

        auto bus = gst_pipeline_get_bus(_pipeline);
        gst_bus_add_watch(bus, bus_handler, this);
        gst_object_unref(bus);
    }

    static GstPadProbeReturn extract_metadata(GstPad *, GstPadProbeInfo *info, gpointer user_data)
    {
        auto stream = static_cast<Stream *>(user_data);

        auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
        if (!buffer) return GST_PAD_PROBE_DROP;

        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) return GST_PAD_PROBE_DROP;
        gst_buffer_unmap(buffer, &map);

        if (auto xdaqmetadata = stream->_metadata_handler->safe_deque.check_pts_pop_timestamp(
                GST_BUFFER_PTS(buffer)
            )) {
            emit stream->metadata_received(xdaqmetadata.value_or(XDAQFrameData{0, 0, 0, 0, 0, 0}));
        }

        return GST_PAD_PROBE_OK;
    }

    void reset()
    {
        spdlog::info("Stream::reset()");
        set_streaming(false);

        auto bus = gst_pipeline_get_bus(_pipeline);
        gst_bus_remove_watch(bus);
        gst_object_unref(bus);

        if (_pipeline) {
            gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_NULL);
            gst_object_unref(_pipeline);
        }
    }

    void start(std::string_view media_type)
    {
        if (!_pipeline) {
            spdlog::error("Pipeline is null, cannot start stream");
            return;
        }
        spdlog::info("Stream::start()");

        if (_streaming) {
            spdlog::warn("Stream is already started, resetting...");
            reset();
            init_pipeline(pipeline(fmt::format("{}:{}", "192.168.177.100", _port), media_type));
        }
    }

    void stop()
    {
        if (!_pipeline) {
            spdlog::error("Pipeline is null, cannot start stream");
            return;
        }
        spdlog::info("Stream::stop()");

        if (gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_NULL) ==
            GST_STATE_CHANGE_FAILURE) {
            spdlog::error("Failed to set pipeline to NULL state");
        }
    }

    ~Stream()
    {
        spdlog::info("~Stream()");
        // set_streaming(false);

        // if (_pipeline) {
        //     gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_NULL);
        //     gst_object_unref(_pipeline);
        // }
        reset();
    }

    void set_streaming(bool now)
    {
        if (_streaming != now) {
            _streaming = now;
            emit status_changed(_streaming);
        }
    }
};
#include "GstVideoSink.h"

#include <spdlog/spdlog.h>

#include "xdaqvc/xvc.h"

GstVideoSink::GstVideoSink(QObject *parent)
    : QObject(parent),
      _pipeline(nullptr),
      _provider(nullptr),
      _bus(nullptr, gst_object_unref),
      _bus_thread_running(true)
{
    if (!gst_is_initialized()) {
        gst_init(nullptr, nullptr);
    }
    _metadata_handler = new MetadataHandler();
}

GstVideoSink::GstVideoSink(Camera *camera, QObject *parent)
    : QObject(parent),
      _pipeline(nullptr),
      _provider(nullptr),
      _camera(camera),
      _bus(nullptr, gst_object_unref),
      _bus_thread_running(true)
{
    if (!gst_is_initialized()) {
        gst_init(nullptr, nullptr);
    }
    _metadata_handler = new MetadataHandler();
}

GstVideoSink::~GstVideoSink()
{
    stop_pipeline();
    delete _metadata_handler;
}

void GstVideoSink::start_pipeline()
{
    stop_pipeline();

    _pipeline = gst_pipeline_new(nullptr);

    _bus = {gst_element_get_bus(_pipeline), gst_object_unref};

    _bus_thread_running = true;
    _bus_thread = std::jthread([this]() {
        while (_bus_thread_running) {
            std::unique_ptr<GstMessage, decltype(&gst_message_unref)> msg(
                gst_bus_timed_pop_filtered(
                    _bus.get(),
                    100 * GST_MSECOND,
                    static_cast<GstMessageType>(
                        GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_WARNING
                    )
                ),
                gst_message_unref
            );

            if (!msg) continue;
            GError *err;
            gchar *debug_info;

            switch (GST_MESSAGE_TYPE(msg.get())) {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg.get(), &err, &debug_info);
                spdlog::error("Error from element {}:", GST_OBJECT_NAME(msg->src), err->message);
                g_clear_error(&err);
                g_free(debug_info);
                _bus_thread_running = false;
                break;
            case GST_MESSAGE_WARNING:
                gst_message_parse_warning(msg.get(), &err, &debug_info);
                spdlog::warn("Warning from element {}:", GST_OBJECT_NAME(msg->src), err->message);
                g_clear_error(&err);
                g_free(debug_info);
                break;
            case GST_MESSAGE_EOS:
                spdlog::info("End-Of-Stream reached.");
                _bus_thread_running = false;
                break;
            default:
                spdlog::trace("Unexpected message type: {}", GST_MESSAGE_TYPE_NAME(msg.get()));
                break;
            }
        }
    });

    auto uri = fmt::format("{}:{}", "192.168.177.100", _camera->port());

    if (_camera->stream_codec() == Camera::Codec::MJPEG) {
        xvc::setup_jpeg_srt_stream(GST_PIPELINE(_pipeline), uri);

        auto parser = gst_bin_get_by_name(GST_BIN(_pipeline), "parser");
        std::unique_ptr<GstPad, decltype(&gst_object_unref)> src_pad(
            gst_element_get_static_pad(parser, "src"), gst_object_unref
        );
        gst_pad_add_probe(
            src_pad.get(),
            GST_PAD_PROBE_TYPE_BUFFER,
            parse_jpeg_metadata,
            _metadata_handler,
            nullptr
        );
    }

    GstAppSinkCallbacks callbacks = {
        nullptr, nullptr, on_new_sample_static, nullptr, nullptr, {nullptr}
    };
    auto appsink = gst_bin_get_by_name(GST_BIN(_pipeline), "appsink");
    gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, this, nullptr);

    gst_element_set_state(_pipeline, GST_STATE_PLAYING);

    // const char *pipelineStr =
    //     "videotestsrc is-live=false pattern=18 ! "
    //     "video/x-raw,format=RGB,width=320,height=240 ! fpsdisplaysink "
    //     "video-sink=appsink name=mysink text-overlay=true";

    // GError *error = nullptr;
    // pipeline = gst_parse_launch(pipelineStr, &error);
    // if (error) {
    //     qWarning() << "Failed to create pipeline:" << error->message;
    //     g_error_free(error);
    //     return;
    // }

    // GstElement *appsink = gst_bin_get_by_name_recurse_up(GST_BIN(pipeline), "mysink");
    // // GstElement *appsink = gst_bin_get_by_name(GST_BIN(pipeline), "mysink");
    // // gst_app_sink_set_emit_signals((GstAppSink *) appsink, true);
    // // gst_app_sink_set_max_buffers((GstAppSink *) appsink, 1);
    // // gst_app_sink_set_drop((GstAppSink *) appsink, true);

    // GstAppSinkCallbacks callbacks = {nullptr, nullptr, onNewSampleStatic};
    // gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, this, nullptr);

    // gst_element_set_state(pipeline, GST_STATE_PLAYING);
    // g_object_unref(appsink);
}

void GstVideoSink::stop_pipeline()
{
    _bus_thread_running = false;
    if (_bus_thread.joinable()) {
        _bus_thread.join();
    }

    if (_pipeline) {
        gst_element_set_state(_pipeline, GST_STATE_NULL);
        gst_object_unref(_pipeline);
        _pipeline = nullptr;
    }
}

GstFlowReturn GstVideoSink::on_new_sample(GstAppSink *sink)
{
    std::unique_ptr<GstSample, decltype(&gst_sample_unref)> sample(
        gst_app_sink_pull_sample(sink), gst_sample_unref
    );
    if (!sample) return GST_FLOW_OK;
    if (!_provider) {
        spdlog::warn("No image provider, skip");
        return GST_FLOW_OK;
    }

    auto caps = gst_sample_get_caps(sample.get());
    auto s = gst_caps_get_structure(caps, 0);

    auto width = 0, height = 0;
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);

    auto buffer = gst_sample_get_buffer(sample.get());
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        spdlog::error("Failed to read buffer");
        return GST_FLOW_ERROR;
    }
    gst_buffer_unmap(buffer, &map);

    auto frame_data = static_cast<const unsigned char *>(map.data);
    QImage image(frame_data, width, height, QImage::Format_RGB888);

    // static auto count = 0;
    auto opt_xdaqmetadata = _metadata_handler->safe_deque.check_pts_pop_timestamp(buffer->pts);
    auto xdaqmetadata = opt_xdaqmetadata.value_or(XDAQFrameData{0, 0, 0, 0, 0, 0});

    // spdlog::info("id: {}, set Image. {}", _camera->id(), count++);
    // spdlog::info(
    //     "fpga_timestamp: {}, rhythm_timestamp: {}, reserved: {}, spi_perf_counter: "
    //     "{}, ttl_in: {}, ttl_out: {}",
    //     xdaqmetadata.fpga_timestamp,
    //     xdaqmetadata.rhythm_timestamp,
    //     xdaqmetadata.reserved,
    //     xdaqmetadata.spi_perf_counter,
    //     xdaqmetadata.ttl_in,
    //     xdaqmetadata.ttl_out
    // );

    // _provider->setImage(QString::number(_camera->id()), image);

    _provider->setImage(QString::number(_camera->id()), image, xdaqmetadata);

    return GST_FLOW_OK;
}
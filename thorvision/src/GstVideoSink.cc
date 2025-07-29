#include "GstVideoSink.h"

#include <spdlog/spdlog.h>

#include "xdaqvc/xvc.h"

GstVideoSink::GstVideoSink(QObject *parent) : QObject(parent), pipeline(nullptr), _provider(nullptr)
{
    gst_init(nullptr, nullptr);
    // startPipeline();
    _metadata_handler = new MetadataHandler();
}

GstVideoSink::GstVideoSink(Camera *camera, QObject *parent)
    : QObject(parent), pipeline(nullptr), _provider(nullptr), _camera(camera)
{
    gst_init(nullptr, nullptr);
    // startPipeline();
    _metadata_handler = new MetadataHandler();
}

GstVideoSink::~GstVideoSink()
{
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }
    _camera->stop();
    delete _metadata_handler;
}

void GstVideoSink::setImageProvider(ImageProvider *provider) { _provider = provider; }

void GstVideoSink::startPipeline()
{
    pipeline = gst_pipeline_new(nullptr);

    // GstElement *source = gst_element_factory_make("videotestsrc", nullptr);
    // GstElement *convert = gst_element_factory_make("videoconvert", nullptr);
    // GstElement *capsfilter = gst_element_factory_make("capsfilter", nullptr);
    // GstElement *fpssink = gst_element_factory_make("fpsdisplaysink", nullptr);
    // GstElement *appsink = gst_element_factory_make("appsink", nullptr);

    // GstCaps *caps = gst_caps_new_simple(
    //     "video/x-raw",
    //     "format",
    //     G_TYPE_STRING,
    //     "RGB",
    //     "width",
    //     G_TYPE_INT,
    //     1920,
    //     "height",
    //     G_TYPE_INT,
    //     1080,
    //     nullptr
    // );
    // g_object_set(capsfilter, "caps", caps, nullptr);
    // gst_caps_unref(caps);

    // g_object_set(source, "pattern", 18, nullptr);
    // g_object_set(source, "is-live", true, nullptr);
    // g_object_set(appsink, "emit-signals", true, nullptr);
    // g_object_set(fpssink, "sync", false, nullptr);
    // g_object_set(fpssink, "video-sink", appsink, nullptr);
    // g_object_set(fpssink, "text-overlay", true, nullptr);

    // gst_bin_add_many(GST_BIN(pipeline), source, convert, capsfilter, fpssink, nullptr);
    // gst_element_link_many(source, convert, capsfilter, fpssink, nullptr);

    auto uri = fmt::format("{}:{}", "192.168.177.100", _camera->port());

    if (_camera->stream_codec() == Camera::Codec::M_JPEG) {
        xvc::setup_jpeg_srt_stream(GST_PIPELINE(pipeline), uri);

        auto parser = gst_bin_get_by_name(GST_BIN(pipeline), "parser");
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
        nullptr, nullptr, onNewSampleStatic, nullptr, nullptr, {nullptr}
    };
    auto appsink = gst_bin_get_by_name(GST_BIN(pipeline), "appsink");
    gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, this, nullptr);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

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

GstFlowReturn GstVideoSink::onNewSample(GstAppSink *sink)
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

GstFlowReturn GstVideoSink::onNewSampleStatic(GstAppSink *sink, gpointer user_data)
{
    return static_cast<GstVideoSink *>(user_data)->onNewSample(sink);
}
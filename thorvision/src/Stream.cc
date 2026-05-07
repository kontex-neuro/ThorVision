#include "Stream.h"

#include <gst/gstelement.h>
#include <gst/gstobject.h>
#include <gst/gstpad.h>
#include <gst/gstutils.h>
#include <spdlog/spdlog.h>

#include <QQuickWindow>
#include <QRunnable>
#include <cassert>
#include <cstdint>
#include <filesystem>

namespace fs = std::filesystem;

struct FileTracker {
    std::string base_filepath;
    std::vector<fs::path> file_paths;
    int max_files;
};

static gchararray generate_filename(GstElement *, guint, gpointer udata)
{
    auto tracker = static_cast<FileTracker *>(udata);
    if (!tracker) {
        spdlog::error("FileTracker is null");
        return nullptr;
    }

    const auto &now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    const auto &timestamp = std::format("{:%Y-%m-%d_%H-%M-%S}", now);
    const auto &file_path = std::format("{}-{}.mkv", tracker->base_filepath, timestamp);

    tracker->file_paths.emplace_back(file_path);

    if (tracker->file_paths.size() > static_cast<size_t>(tracker->max_files)) {
        auto _file_path = tracker->file_paths.front();
        fs::remove(_file_path);
        spdlog::debug("Remove file: {}", _file_path.generic_string());

        _file_path.replace_extension(".bin");
        fs::remove(_file_path);
        spdlog::debug("Remove file: {}", _file_path.generic_string());

        tracker->file_paths.erase(tracker->file_paths.begin());
    }

    return g_strdup(file_path.c_str());
}

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

constexpr std::string pipeline_desc(std::string_view uri, std::string_view media_type)
{
    if (media_type == "video/x-h265") {
#ifdef _WIN32
        // Windows H.265 pipeline using d3d11h265dec
        return std::format(
            "srtclientsrc name=src uri=srt://{} keep-listening=true latency=125 ! "
            "h265parse name=parser ! video/x-h265,stream-format=byte-stream,alignment=au ! "
            "tee name=t ! "
            "queue name=queue_dec leaky=2 ! "
            "d3d11h265dec name=dec ! "
            "d3d11convert name=conv ! video/x-raw(memory:D3D11Memory), format=(string)RGB ! "
            "videorate max-rate=30 ! "
            "queue name=queue_sink leaky=2 ! "
            "fpsdisplaysink name=sink sync=false text-overlay=false "
            "t. ! queue name=queue_record max-size-time=10000000000 max-size-buffers=10 "
            "max-size-bytes=0 ! "
            "fakesink name=record_sink async=false sync=false",
            uri
        );
#elif __APPLE__
        return std::format(
            "srtclientsrc name=src uri=srt://{} keep-listening=true latency=125 ! "
            "h265parse name=parser ! video/x-h265, stream-format=byte-stream, alignment=au ! "
            "tee name=t ! "
            "queue name=queue_dec leaky=2 ! "
            "h265parse ! video/x-h265, stream-format=hvc1, alignment=au ! "
            "vtdec name=dec ! video/x-raw, format=(string)NV12 ! "
            "glupload name=upload ! video/x-raw(memory:GLMemory) ! "
            "glcolorconvert name=conv ! video/x-raw(memory:GLMemory), format=(string)RGB ! "
            "queue name=queue_sink leaky=2 ! "
            "fpsdisplaysink name=sink sync=false text-overlay=false "
            "t. ! queue name=queue_record max-size-time=10000000000 max-size-buffers=10 "
            "max-size-bytes=0 ! "
            "fakesink name=record_sink async=false sync=false ",
            uri
        );
#endif
    } else if (media_type == "image/jpeg") {
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
    } else {
        spdlog::error("Unsupported media type: {}", media_type);
        return "";
    }
}

static gboolean bus_handler(GstBus *, GstMessage *msg, gpointer user_data)
{
    auto stream = static_cast<Stream *>(user_data);
    if (!stream) return G_SOURCE_REMOVE;

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

        gst_element_set_state(GST_ELEMENT(stream->_pipeline), GST_STATE_READY);
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
        gst_element_set_state(GST_ELEMENT(stream->_pipeline), GST_STATE_READY);
        break;
    }
    case GST_MESSAGE_ELEMENT: {
        auto const structure = gst_message_get_structure(msg);
        if (!gst_structure_has_name(structure, "GstBinForwarded")) break;
        GstMessage *forward_msg = nullptr;

        gst_structure_get(structure, "message", GST_TYPE_MESSAGE, &forward_msg, nullptr);
        if (GST_MESSAGE_TYPE(forward_msg) == GST_MESSAGE_EOS) {
            auto element_name = GST_OBJECT_NAME(GST_MESSAGE_SRC(forward_msg));
            spdlog::debug("EOS from element {}", element_name);
            std::string name_str(element_name);
            auto pipeline = GST_BIN(stream->_pipeline);

            // H.265 recording path
            if (name_str.find("splitmux") != std::string::npos) {
                if (auto splitmux = gst_bin_get_by_name(pipeline, "splitmux")) {
                    spdlog::info("H.265 recording finalized and splitmux removed");
                    gst_element_set_state(splitmux, GST_STATE_NULL);
                    gst_bin_remove(pipeline, splitmux);
                    gst_object_unref(splitmux);
                }
            }
            // MJPEG recording path
            else if (name_str == "filesink") {
                auto tee = gst_bin_get_by_name(pipeline, "t");
                auto queue = gst_bin_get_by_name(pipeline, "queue_record");
                auto parser = gst_bin_get_by_name(pipeline, "record_parser");
                auto filesink = gst_bin_get_by_name(pipeline, "filesink");

                if (!tee || !queue || !parser || !filesink) {
                    spdlog::warn("Recording elements not found during EOS handling");
                    if (tee) gst_object_unref(tee);
                    if (queue) gst_object_unref(queue);
                    if (parser) gst_object_unref(parser);
                    if (filesink) gst_object_unref(filesink);
                    break;
                }

                auto queue_sinkpad = gst_element_get_static_pad(queue, "sink");
                auto tee_srcpad = gst_element_get_static_pad(tee, "src_1");

                if (!queue_sinkpad || !tee_srcpad) {
                    if (queue_sinkpad) gst_object_unref(queue_sinkpad);
                    if (tee_srcpad) gst_object_unref(tee_srcpad);
                    break;
                }

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

                spdlog::debug("MJPEG recording unlinked");
            }
        }
        gst_message_unref(forward_msg);
    } break;
    default: {
        break;
    }
    }
    return G_SOURCE_CONTINUE;
};

static GstPadProbeReturn extract_metadata(GstPad *, GstPadProbeInfo *info, gpointer user_data)
{
    auto stream = static_cast<Stream *>(user_data);
    if (!stream) return GST_PAD_PROBE_REMOVE;
    auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buffer) return GST_PAD_PROBE_DROP;

    if (!GST_BUFFER_PTS_IS_VALID(buffer)) return GST_PAD_PROBE_DROP;

    if (auto xdaqmetadata = stream->_safe_queue.dequeue(GST_BUFFER_PTS(buffer))) {
        QMetaObject::invokeMethod(
            stream,
            [stream = QPointer<Stream>(stream), xdaqmetadata]() {
                if (!stream) return;
                emit stream->metadata_received(xdaqmetadata.value());
            },
            Qt::QueuedConnection
        );
    }
    return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn buffer_collector_probe(GstPad *, GstPadProbeInfo *info, gpointer user_data)
{
    auto stream = static_cast<Stream *>(user_data);
    if (!stream) return GST_PAD_PROBE_REMOVE;
    auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buffer) return GST_PAD_PROBE_OK;

    constexpr auto PRE_RECORD_DURATION = 1 * GST_SECOND;
    static thread_local auto last_log_time = std::chrono::steady_clock::now();
    static thread_local uint64_t total_refs = 0;
    static thread_local uint64_t total_unrefs = 0;
    auto now = std::chrono::steady_clock::now();

    // Only collect buffers when NOT recording
    if (!stream->_recording.load()) {
        std::lock_guard<std::mutex> lock(stream->_pre_record_mutex);

        // Check if this is a keyframe (IDR/CRA frame)
        // DELTA_UNIT flag is set for P/B frames, NOT set for keyframes
        auto is_keyframe = !GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);

        // Keep a reference instead of deep-copying payload; much cheaper on streaming thread.
        stream->_pre_record_buffer.push_back(gst_buffer_ref(buffer));
        total_refs++;

        // Track the index of the most recent keyframe
        if (is_keyframe) {
            stream->_last_keyframe_index = stream->_pre_record_buffer.size() - 1;
            spdlog::trace(
                "Buffer collector: keyframe detected at index {}", stream->_last_keyframe_index
            );
        }

        spdlog::trace(
            "Buffer collector: pushed buffer (keyframe={}), buffer size = {}",
            is_keyframe,
            stream->_pre_record_buffer.size()
        );

        // Remove old buffers beyond PRE_RECORD_DURATION
        while (stream->_pre_record_buffer.size() > 1) {
            auto oldest = stream->_pre_record_buffer.front();
            auto newest = stream->_pre_record_buffer.back();

            // Check if we have valid PTS values
            if (GST_BUFFER_PTS_IS_VALID(oldest) && GST_BUFFER_PTS_IS_VALID(newest)) {
                if (GST_BUFFER_PTS(newest) - GST_BUFFER_PTS(oldest) > PRE_RECORD_DURATION) {
                    gst_buffer_unref(oldest);
                    total_unrefs++;
                    stream->_pre_record_buffer.pop_front();
                    // Adjust keyframe index since we removed from front
                    if (stream->_last_keyframe_index > 0) {
                        stream->_last_keyframe_index--;
                    }
                    spdlog::trace(
                        "Buffer collector: removed old buffer (by PTS), buffer size = {}",
                        stream->_pre_record_buffer.size()
                    );
                } else {
                    break;
                }
            } else {
                // Fallback: limit by count (assume ~30fps, 10 seconds = ~300 frames)
                if (stream->_pre_record_buffer.size() > 300) {
                    gst_buffer_unref(oldest);
                    total_unrefs++;
                    stream->_pre_record_buffer.pop_front();
                    // Adjust keyframe index since we removed from front
                    if (stream->_last_keyframe_index > 0) {
                        stream->_last_keyframe_index--;
                    }
                    spdlog::debug(
                        "Buffer collector: removed old buffer (by count), buffer size = {}",
                        stream->_pre_record_buffer.size()
                    );
                } else {
                    break;
                }
            }
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - last_log_time);
        if (elapsed.count() >= 1) {
            last_log_time = now;
            spdlog::info(
                "Buffer stats: refs={}, unrefs={}, delta={}, buffer_count={}",
                total_refs,
                total_unrefs,
                total_refs - total_unrefs,
                stream->_pre_record_buffer.size()
            );
        }
    }

    return GST_PAD_PROBE_OK;
}

Stream::Stream(QQuickItem *video_item, int index, int port, QObject *parent)
    : QObject(parent),
      _pipeline(nullptr),
      _recording(false),
      _video_item(video_item),
      _index(index),
      _port(port),
      _streaming(false)
{
    init_pipeline(pipeline_desc(std::format("{}:{}", "192.168.177.100", _port), "image/jpeg"));

    // TODO: This callback prevent others windows from going blank when camera unplugged.
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
                stream->reset();
            }
        }
    );
}

bool Stream::init_pipeline(std::string_view pipeline_desc)
{
    spdlog::info("Initializing stream pipeline with desc: {}", pipeline_desc);

    auto fail = [this]() {
        if (_pipeline) {
            gst_object_unref(_pipeline);
            _pipeline = nullptr;
        }
        return false;
    };

    GError *err = nullptr;
    _pipeline = GST_PIPELINE(gst_parse_launch(pipeline_desc.data(), &err));
    if (!_pipeline) {
        spdlog::error("Failed to parse pipeline: {}", err ? err->message : "unknown");
        g_clear_error(&err);
        return false;
    }

#ifdef _WIN32
    auto sink = gst_element_factory_make("qml6d3d11sink", "sink");
#elif __APPLE__
    auto sink = gst_element_factory_make("qml6glsink", "sink");
#endif
    if (!sink) {
        spdlog::error("Failed to create 'sink' element");
        return fail();
    }

    g_object_set(sink, "sync", false, "widget", _video_item.data(), nullptr);
    g_object_set(_pipeline, "message-forward", true, nullptr);

    auto fpsdisplaysink = gst_bin_get_by_name(GST_BIN(_pipeline), "sink");
    if (!fpsdisplaysink) {
        spdlog::error("Failed to get 'sink' element from pipeline");
        return fail();
    }

    g_object_set(fpsdisplaysink, "video-sink", sink, nullptr);
    gst_object_unref(sink);
    gst_object_unref(fpsdisplaysink);

    auto parser = gst_bin_get_by_name(GST_BIN(_pipeline), "parser");
    if (!parser) {
        spdlog::error("Failed to get 'parser' element from pipeline");
        return fail();
    }

    auto parser_srcpad = gst_element_get_static_pad(parser, "src");
    if (!parser_srcpad) {
        spdlog::error("Failed to get 'src' pad from 'parser' element");
        gst_object_unref(parser);
        return fail();
    }

    gchar *factory_name = nullptr;
    if (auto factory = gst_element_get_factory(parser)) {
        factory_name = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
    }

    if (!factory_name) {
        spdlog::error("Failed to get factory name from 'parser' element");
        gst_object_unref(parser_srcpad);
        gst_object_unref(parser);
        return fail();
    }

    if (std::string(factory_name) == "jpegparse") {
        gst_pad_add_probe(
            parser_srcpad, GST_PAD_PROBE_TYPE_BUFFER, parse_jpeg_metadata, &_safe_queue, nullptr
        );
    } else if (std::string(factory_name) == "h265parse") {
        gst_pad_add_probe(
            parser_srcpad, GST_PAD_PROBE_TYPE_BUFFER, parse_h265_metadata, &_safe_queue, nullptr
        );

        auto queue = gst_bin_get_by_name(GST_BIN(_pipeline), "queue_record");
        if (!queue) {
            spdlog::error("Failed to get 'queue_record' element from pipeline");
            return fail();
        }

        auto queue_srcpad = gst_element_get_static_pad(queue, "src");
        if (!queue_srcpad) {
            spdlog::error("Failed to get 'src' pad from 'queue_record' element");
            gst_object_unref(queue);
            return fail();
        }

        gst_pad_add_probe(
            queue_srcpad, GST_PAD_PROBE_TYPE_BUFFER, buffer_collector_probe, this, nullptr
        );
        spdlog::info("Buffer collector probe installed for pre-recording");

        gst_object_unref(queue);
        gst_object_unref(queue_srcpad);
    }
    gst_object_unref(parser_srcpad);
    gst_object_unref(parser);

    auto dec = gst_bin_get_by_name(GST_BIN(_pipeline), "dec");
    if (!dec) {
        spdlog::error("Failed to get 'dec' element from pipeline");
        return fail();
    }

    auto dec_sinkpad = gst_element_get_static_pad(dec, "src");
    if (!dec_sinkpad) {
        spdlog::error("Failed to get 'src' pad from 'dec' element");
        gst_object_unref(dec);
        return fail();
    }
    gst_pad_add_probe(dec_sinkpad, GST_PAD_PROBE_TYPE_BUFFER, extract_metadata, this, nullptr);
    gst_object_unref(dec_sinkpad);
    gst_object_unref(dec);

    auto bus = gst_pipeline_get_bus(_pipeline);
    if (!bus) {
        spdlog::error("Failed to get bus from pipeline");
        return fail();
    }
    gst_bus_add_watch(bus, bus_handler, this);
    gst_object_unref(bus);

    return _pipeline != nullptr;
}

void Stream::reset()
{
    if (!_pipeline) {
        spdlog::error("Pipeline is null, cannot reset stream");
        return;
    }
    spdlog::info("Stream::reset()");
    set_streaming(false);

    gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_NULL);

    {
        std::lock_guard<std::mutex> lock(_pre_record_mutex);
        const auto count = _pre_record_buffer.size();
        auto unreffed = 0;
        for (auto buffer : _pre_record_buffer) {
            if (buffer && GST_IS_BUFFER(buffer)) {
                if (GST_MINI_OBJECT_REFCOUNT_VALUE(buffer) >= 1) {
                    gst_buffer_unref(buffer);
                    unreffed++;
                } else {
                    spdlog::warn(
                        "clear_pre_record_buffer: buffer has invalid refcount {}",
                        GST_MINI_OBJECT_REFCOUNT_VALUE(buffer)
                    );
                }
            }
        }
        _pre_record_buffer.clear();
        _last_keyframe_index = 0;
        spdlog::debug("Pre-record buffer cleared, had {} frames, unreffed {}", count, unreffed);
    }

    if (auto fpsdisplaysink = gst_bin_get_by_name(GST_BIN(_pipeline), "sink")) {
        GstElement *video_sink = nullptr;
        g_object_get(fpsdisplaysink, "video-sink", &video_sink, nullptr);
        if (video_sink) {
            g_object_set(video_sink, "widget", nullptr, nullptr);
            spdlog::info("Detached QML widget");
        }
        g_object_set(fpsdisplaysink, "video-sink", nullptr, nullptr);
        gst_object_unref(fpsdisplaysink);
    }

    if (auto bus = gst_pipeline_get_bus(_pipeline)) {
        gst_bus_remove_watch(bus);
        gst_object_unref(bus);
    }

    gst_object_unref(_pipeline);
    _pipeline = nullptr;
}

bool Stream::start(std::string_view media_type)
{
    if (!_pipeline) {
        spdlog::error("Pipeline is null, cannot start stream");
        return false;
    }

    if (_streaming) {
        reset();
        init_pipeline(pipeline_desc(std::format("{}:{}", "192.168.177.100", _port), media_type));
    }

    if (auto window = _video_item->window()) {
        window->scheduleRenderJob(
            new StartPipeline(_pipeline), QQuickWindow::BeforeSynchronizingStage
        );
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

bool Stream::start_h265_recording(const xvc::RecordConfig &config)
{
    if (!_pipeline) {
        spdlog::error("Pipeline is null");
        return false;
    }

    const auto &pipeline = GST_BIN(_pipeline);
    const auto &location = config._path.generic_string();
    const auto &split = config._split;
    const auto &duration = config._max_size_time.count() * GST_SECOND;

    auto queue_record = gst_bin_get_by_name(pipeline, "queue_record");
    auto fakesink = gst_bin_get_by_name(pipeline, "record_sink");

    if (!queue_record || !fakesink) {
        spdlog::error("Failed to get 'queue_record' or 'record_sink'");
        if (queue_record) gst_object_unref(queue_record);
        if (fakesink) gst_object_unref(fakesink);
        return false;
    }

    auto record_parser = gst_element_factory_make("h265parse", "record_parser");
    auto cf_parser = gst_element_factory_make("capsfilter", "cf_record_parser");
    auto muxer = gst_element_factory_make("matroskamux", "muxer");
    auto splitmux = gst_element_factory_make("splitmuxsink", "splitmux");

    if (!record_parser || !cf_parser || !muxer || !splitmux) {
        spdlog::error("Failed to create H.265 recording elements");
        gst_object_unref(queue_record);
        gst_object_unref(fakesink);
        if (record_parser) gst_object_unref(record_parser);
        if (cf_parser) gst_object_unref(cf_parser);
        if (muxer) gst_object_unref(muxer);
        if (splitmux) gst_object_unref(splitmux);
        return false;
    }

    // clang-format off
    auto caps = gst_caps_new_simple(
        "video/x-h265",
        "stream-format", G_TYPE_STRING, "hvc1",        
        "alignment", G_TYPE_STRING, "au",
        nullptr
    );
    // clang-format on
    g_object_set(cf_parser, "caps", caps, nullptr);
    gst_caps_unref(caps);

    g_object_set(muxer, "timecodescale", 1, "offset-to-zero", TRUE, nullptr);

    auto tracker = new FileTracker(location, {}, INT_MAX);
    g_signal_connect_data(
        splitmux,
        "format-location",
        G_CALLBACK(generate_filename),
        tracker,
        [](gpointer data, GClosure *) {
            delete static_cast<FileTracker *>(data);
            spdlog::debug("FileTracker deleted");
        },
        G_CONNECT_DEFAULT
    );

    // clang-format off
    g_object_set(
        splitmux,
        "max-size-time", split ? duration : 0,
        "async-finalize", false,
        "muxer", muxer,
        nullptr
    );
    // clang-format on

    gst_bin_add_many(pipeline, record_parser, cf_parser, splitmux, nullptr);

    auto queue_srcpad = gst_element_get_static_pad(queue_record, "src");
    if (!queue_srcpad) {
        spdlog::error("Failed to get src pad of 'queue_record'");
        return false;
    }

    gst_pad_add_probe(
        queue_srcpad,
        GST_PAD_PROBE_TYPE_IDLE,
        [](GstPad *, GstPadProbeInfo *, gpointer user_data) -> GstPadProbeReturn {
            auto stream = static_cast<Stream *>(user_data);
            if (!stream) return GST_PAD_PROBE_REMOVE;
            auto pipeline = GST_BIN(stream->_pipeline);
            if (!pipeline) return GST_PAD_PROBE_REMOVE;

            auto queue_record = gst_bin_get_by_name(pipeline, "queue_record");
            auto fakesink = gst_bin_get_by_name(pipeline, "record_sink");
            auto record_parser = gst_bin_get_by_name(pipeline, "record_parser");
            auto cf_parser = gst_bin_get_by_name(pipeline, "cf_record_parser");
            auto splitmux = gst_bin_get_by_name(pipeline, "splitmux");

            if (!queue_record || !fakesink || !record_parser || !cf_parser || !splitmux) {
                spdlog::error("start_h265_recording idle probe: elements not found");
                if (queue_record) gst_object_unref(queue_record);
                if (fakesink) gst_object_unref(fakesink);
                if (record_parser) gst_object_unref(record_parser);
                if (cf_parser) gst_object_unref(cf_parser);
                if (splitmux) gst_object_unref(splitmux);
                return GST_PAD_PROBE_REMOVE;
            }

            auto queue_srcpad = gst_element_get_static_pad(queue_record, "src");
            auto fakesink_sinkpad = gst_element_get_static_pad(fakesink, "sink");

            gst_pad_unlink(queue_srcpad, fakesink_sinkpad);

            gst_element_set_state(fakesink, GST_STATE_NULL);
            gst_bin_remove(pipeline, fakesink);

            if (!gst_element_link_many(queue_record, record_parser, cf_parser, splitmux, nullptr)) {
                spdlog::error("Failed to link H.265 recording elements");
                return GST_PAD_PROBE_REMOVE;
            }

            gst_element_sync_state_with_parent(record_parser);
            gst_element_sync_state_with_parent(cf_parser);
            gst_element_sync_state_with_parent(splitmux);

            // Stop further pre-record collection before we begin flushing cached data.
            stream->_recording = true;

            // Flush pre-recorded buffer to record_parser
            {
                std::lock_guard<std::mutex> lock(stream->_pre_record_mutex);

                size_t start_index = 0;
                for (size_t i = 0; i < stream->_pre_record_buffer.size(); ++i) {
                    auto buf = stream->_pre_record_buffer[i];
                    if (buf && !GST_BUFFER_FLAG_IS_SET(buf, GST_BUFFER_FLAG_DELTA_UNIT)) {
                        start_index = i;
                        break;
                    }
                }

                auto frames_to_flush = stream->_pre_record_buffer.size() - start_index;
                spdlog::info(
                    "Flushing {} pre-recorded frames to record_parser (starting from keyframe "
                    "at index "
                    "{}, skipping {} frames)",
                    frames_to_flush,
                    start_index,
                    start_index
                );

                auto record_parser_sinkpad = gst_element_get_static_pad(record_parser, "sink");
                if (!record_parser_sinkpad) {
                    spdlog::error("Failed to get sink pad of 'record_parser'");
                    return GST_PAD_PROBE_REMOVE;
                }

                // 1. Forward sticky STREAM_START from queue
                if (auto stream_start_event =
                        gst_pad_get_sticky_event(queue_srcpad, GST_EVENT_STREAM_START, 0)) {
                    gst_pad_send_event(record_parser_sinkpad, stream_start_event);
                } else {
                    const auto fallback_stream_id = std::format(
                        "pre-record-{}-{}", stream->_index, reinterpret_cast<std::uintptr_t>(stream)
                    );
                    gst_pad_send_event(
                        record_parser_sinkpad,
                        gst_event_new_stream_start(fallback_stream_id.c_str())
                    );
                }

                // 2. Forward sticky CAPS from queue
                if (auto caps_event = gst_pad_get_sticky_event(queue_srcpad, GST_EVENT_CAPS, 0)) {
                    gst_pad_send_event(record_parser_sinkpad, caps_event);
                } else {
                    // clang-format off
                            auto input_caps = gst_caps_new_simple(
                                "video/x-h265",
                                "stream-format", G_TYPE_STRING, "byte-stream",
                                "alignment", G_TYPE_STRING, "au",
                                nullptr
                            );
                    // clang-format on

                    gst_pad_send_event(record_parser_sinkpad, gst_event_new_caps(input_caps));
                    gst_caps_unref(input_caps);
                }

                // 3. Forward sticky SEGMENT from queue (This is the magic fix!)
                if (auto seg_event = gst_pad_get_sticky_event(queue_srcpad, GST_EVENT_SEGMENT, 0)) {
                    gst_pad_send_event(record_parser_sinkpad, seg_event);
                    spdlog::debug("Pushed original sticky segment from queue");
                } else {
                    GstSegment segment;
                    gst_segment_init(&segment, GST_FORMAT_TIME);
                    gst_pad_send_event(record_parser_sinkpad, gst_event_new_segment(&segment));
                    spdlog::warn("No sticky segment found, pushing default");
                }

                // --> Continue with the existing logic to push pre-recorded frames
                size_t pushed_count = 0;
                for (size_t i = start_index; i < stream->_pre_record_buffer.size(); ++i) {
                    auto buffer = stream->_pre_record_buffer[i];
                    if (buffer) {
                        auto ret = gst_pad_chain(record_parser_sinkpad, gst_buffer_ref(buffer));
                        if (ret != GST_FLOW_OK) {
                            spdlog::warn(
                                "Failed to push pre-recorded buffer {}: {}",
                                i,
                                gst_flow_get_name(ret)
                            );
                        } else {
                            pushed_count++;
                        }
                    }
                }
                spdlog::info("Successfully pushed {} pre-recorded frames", pushed_count);
                gst_object_unref(record_parser_sinkpad);

                // Release all cached references.
                size_t unreffed = 0;
                for (auto buffer : stream->_pre_record_buffer) {
                    if (buffer) {
                        gst_buffer_unref(buffer);
                        unreffed++;
                    }
                }
                spdlog::debug("Recording flush: unreffed {} buffers", unreffed);
                stream->_pre_record_buffer.clear();
                stream->_last_keyframe_index = 0;
            }

            gst_object_unref(queue_srcpad);
            gst_object_unref(fakesink_sinkpad);
            if (queue_record) gst_object_unref(queue_record);
            if (record_parser) gst_object_unref(record_parser);
            if (cf_parser) gst_object_unref(cf_parser);
            if (splitmux) gst_object_unref(splitmux);
            if (fakesink) gst_object_unref(fakesink);
            return GST_PAD_PROBE_REMOVE;
        },
        this,
        nullptr
    );
    gst_object_unref(queue_srcpad);

    gst_object_unref(queue_record);
    gst_object_unref(fakesink);

    return true;
}

bool Stream::stop_h265_recording()
{
    if (!_pipeline) {
        spdlog::error("Pipeline is null");
        return false;
    }

    _recording = false;

    auto queue_record = gst_bin_get_by_name(GST_BIN(_pipeline), "queue_record");
    if (!queue_record) return false;

    auto queue_srcpad = gst_element_get_static_pad(queue_record, "src");
    if (!queue_srcpad) {
        gst_object_unref(queue_record);
        return false;
    }

    gst_pad_add_probe(
        queue_srcpad,
        GST_PAD_PROBE_TYPE_IDLE,
        [](GstPad *, GstPadProbeInfo *, gpointer user_data) -> GstPadProbeReturn {
            auto pipeline = static_cast<GstBin *>(user_data);
            if (!pipeline) return GST_PAD_PROBE_REMOVE;

            spdlog::info("stop_h265_recording: idle probe triggered");

            auto queue_record = gst_bin_get_by_name(pipeline, "queue_record");
            auto record_parser = gst_bin_get_by_name(pipeline, "record_parser");
            auto cf_parser = gst_bin_get_by_name(pipeline, "cf_record_parser");
            auto splitmux = gst_bin_get_by_name(pipeline, "splitmux");

            if (!queue_record || !record_parser || !cf_parser || !splitmux) {
                spdlog::error("Recording elements not found");
                if (queue_record) gst_object_unref(queue_record);
                if (record_parser) gst_object_unref(record_parser);
                if (cf_parser) gst_object_unref(cf_parser);
                if (splitmux) gst_object_unref(splitmux);
                return GST_PAD_PROBE_REMOVE;
            }

            gst_element_unlink_many(queue_record, record_parser, cf_parser, splitmux, nullptr);

            auto fakesink = gst_element_factory_make("fakesink", "record_sink");
            if (!fakesink) {
                spdlog::error("Failed to create 'record_sink'");
                return GST_PAD_PROBE_REMOVE;
            }

            g_object_set(fakesink, "async", false, "sync", false, nullptr);
            gst_bin_add(GST_BIN(pipeline), fakesink);

            if (!gst_element_link(queue_record, fakesink)) {
                spdlog::error("Failed to link 'queue_record' to 'record_sink'");
                return GST_PAD_PROBE_REMOVE;
            }
            gst_element_sync_state_with_parent(fakesink);

            // Send EOS to splitmux to finalize the file
            if (auto splitmux_sinkpad = gst_element_get_static_pad(splitmux, "video")) {
                gst_pad_send_event(splitmux_sinkpad, gst_event_new_eos());
                gst_object_unref(splitmux_sinkpad);
            }

            // Set record_parser and cf_parser to NULL and remove them now
            // (they're already unlinked)
            gst_element_set_state(record_parser, GST_STATE_NULL);
            gst_element_set_state(cf_parser, GST_STATE_NULL);
            gst_bin_remove_many(GST_BIN(pipeline), record_parser, cf_parser, nullptr);

            // DON'T set splitmux to NULL here - let bus_handler handle EOS
            gst_object_unref(queue_record);
            gst_object_unref(record_parser);
            gst_object_unref(cf_parser);
            gst_object_unref(splitmux);

            spdlog::info("H.265 recording stop initiated, waiting for EOS");

            return GST_PAD_PROBE_REMOVE;
        },
        _pipeline,
        nullptr
    );
    gst_object_unref(queue_srcpad);
    gst_object_unref(queue_record);

    return true;
}
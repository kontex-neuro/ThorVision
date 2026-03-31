#pragma once

#include <fmt/chrono.h>

#include <QCoreApplication>
#include <QQuickItem>
#include <QThread>
#include <chrono>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>

#include "xdaqmetadata/metadata_handler.h"
#include "xdaqvc/xvc.h"



namespace fs = std::filesystem;

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

    std::atomic<bool> _recording_active{false};

    std::deque<GstBuffer *> _pre_record_buffer;
    std::mutex _pre_record_mutex;
    static constexpr GstClockTime PRE_RECORD_DURATION = 10 * GST_SECOND;
    gulong _buffer_collector_probe_id{0};
    size_t _last_keyframe_index{0};  // Index of the most recent keyframe in _pre_record_buffer

    // FPS monitoring for queue_record
    std::atomic<uint64_t> _fps_frame_count{0};
    std::atomic<GstClockTime> _fps_last_report_time{0};
    gulong _fps_monitor_probe_id{0};

    // TODO: media_type
    static std::string pipeline(std::string_view uri, [[maybe_unused]] std::string_view media_type)
    {
        if (media_type == "video/x-h265") {
#ifdef _WIN32
            // Windows H.265 pipeline using d3d11h265dec
            return fmt::format(
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
            // macOS H.265 pipeline using vtdec
            return fmt::format(
                "srtclientsrc name=src uri=srt://{} keep-listening=true latency=125 ! "
                "h265parse name=parser ! video/x-h265,stream-format=byte-stream,alignment=au ! "
                "tee name=t ! "
                "queue name=queue_dec leaky=2 ! "
                "h265parse ! video/x-h265,stream-format=hvc1,alignment=au ! "
                "vtdec name=dec ! video/x-raw, format=(string)NV12 ! "
                "glupload name=upload ! video/x-raw(memory:GLMemory) ! "
                "glcolorconvert name=conv ! video/x-raw(memory:GLMemory), format=(string)RGB ! "
                "queue name=queue_sink leaky=2 ! "
                "fpsdisplaysink name=sink sync=false text-overlay=false "
                "t. ! queue name=queue_record max-size-time=10000000000 max-size-buffers=10 "
                "max-size-bytes=0 ! "
                "fakesink name=record_sink async=false sync=false",
                uri
            );
#endif
        } else if (media_type == "image/jpeg") {
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
        } else {
            spdlog::error("Unsupported media type: {}", media_type);
            return "";
        }
    }

    static gboolean bus_handler([[maybe_unused]] GstBus *bus, GstMessage *msg, gpointer user_data)
    {
        auto stream = static_cast<Stream *>(user_data);
        if (!stream) {
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
            spdlog::info("Debugging info: {}", (debug) ? debug : "None");
            g_clear_error(&err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_WARNING: {
            gst_message_parse_warning(msg, &err, &debug);
            spdlog::warn("Warning from element {}: {}", GST_OBJECT_NAME(msg->src), err->message);
            spdlog::info("Debugging info: {}", (debug) ? debug : "None");
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
                    std::string name_str(element_name);

                    // H.265 recording path - splitmux cleanup
                    if (name_str.find("splitmux") != std::string::npos) {
                        auto pipeline = GST_BIN(stream->_pipeline);
                        auto splitmux = gst_bin_get_by_name(pipeline, "splitmux");

                        if (splitmux) {
                            gst_element_set_state(splitmux, GST_STATE_NULL);
                            gst_bin_remove(pipeline, splitmux);
                            gst_object_unref(splitmux);
                            spdlog::info("H.265 recording finalized and splitmux removed");
                        }
                    }
                    // JPEG recording path - filesink cleanup
                    else if (name_str == "filesink") {
                        auto pipeline = GST_BIN(stream->_pipeline);
                        auto tee = gst_bin_get_by_name(pipeline, "t");
                        auto queue = gst_bin_get_by_name(pipeline, "queue_record");
                        auto parser = gst_bin_get_by_name(pipeline, "record_parser");
                        auto filesink = gst_bin_get_by_name(pipeline, "filesink");

                        auto queue_sinkpad = gst_element_get_static_pad(queue, "sink");
                        auto tee_srcpad = gst_pad_get_peer(queue_sinkpad);

                        gst_pad_unlink(tee_srcpad, queue_sinkpad);

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

                        spdlog::info("JPEG recording finalized");
                    } else {
                        spdlog::debug("EOS from {}, ignoring", name_str);
                    }
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
            return;
        }
        // _video_item is GstD3D11Qt6VideoItem created by QML
        g_object_set(sink, "widget", _video_item, nullptr);
        spdlog::info("D3D11 sink widget set");
#elif __APPLE__
        auto sink = gst_element_factory_make("qml6glsink", "sink");
        if (!sink) {
            spdlog::error("Failed to create 'qml6glsink' element");
            gst_object_unref(_pipeline);
            return;
        }
        g_object_set(sink, "widget", _video_item, nullptr);
#endif

        auto fpsdisplaysink = gst_bin_get_by_name(GST_BIN(_pipeline), "sink");
        g_object_set(_pipeline, "message-forward", true, nullptr);
        g_object_set(sink, "sync", false, nullptr);
        g_object_set(fpsdisplaysink, "video-sink", sink, nullptr);

        auto parser = gst_bin_get_by_name(GST_BIN(_pipeline), "parser");
        auto parser_srcpad = gst_element_get_static_pad(parser, "src");
        // Check if the parser is actually jpegparse before adding the JPEG metadata probe
        gchar *factory_name = nullptr;
        auto factory = gst_element_get_factory(parser);
        if (factory) {
            factory_name = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
        }

        if (factory_name && std::string(factory_name) == "jpegparse") {
            gst_pad_add_probe(
                parser_srcpad,
                GST_PAD_PROBE_TYPE_BUFFER,
                parse_jpeg_metadata,
                _metadata_handler.get(),
                nullptr
            );
        } else if (factory_name && std::string(factory_name) == "h265parse") {
            gst_pad_add_probe(
                parser_srcpad,
                GST_PAD_PROBE_TYPE_BUFFER,
                parse_h265_metadata,
                _metadata_handler.get(),
                nullptr
            );

            // Add buffer collector probe on queue_record's src pad for pre-recording
            auto queue_record = gst_bin_get_by_name(GST_BIN(_pipeline), "queue_record");
            if (queue_record) {
                auto queue_srcpad = gst_element_get_static_pad(queue_record, "src");
                _buffer_collector_probe_id = gst_pad_add_probe(
                    queue_srcpad, GST_PAD_PROBE_TYPE_BUFFER, buffer_collector_probe, this, nullptr
                );
                spdlog::info("Buffer collector probe installed for pre-recording");

                // Also add FPS monitor probe on queue_record's sink pad to measure incoming frame
                // rate
                auto queue_sinkpad = gst_element_get_static_pad(queue_record, "sink");
                _fps_monitor_probe_id = gst_pad_add_probe(
                    queue_sinkpad, GST_PAD_PROBE_TYPE_BUFFER, fps_monitor_probe, this, nullptr
                );
                spdlog::info("FPS monitor probe installed on queue_record sink");
                gst_object_unref(queue_sinkpad);

                gst_object_unref(queue_srcpad);
                gst_object_unref(queue_record);
            }
        }
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
        // if (!stream || stream->_destroying.load()) {
        //     return GST_PAD_PROBE_OK;  // Don't access stream
        // }

        auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
        if (!buffer) {
            spdlog::error("Buffer is null");
            return GST_PAD_PROBE_DROP;
        }

        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            spdlog::error("Failed to map buffer");
            return GST_PAD_PROBE_DROP;
        }
        gst_buffer_unmap(buffer, &map);

        auto xdaqmetadata =
            stream->_metadata_handler->safe_deque.check_pts_pop_timestamp(GST_BUFFER_PTS(buffer));

        if (xdaqmetadata) {
            // emit stream->metadata_received(xdaqmetadata.value());
            // Throttle signal emission to ~20 Hz (every 50ms)
            static thread_local GstClockTime last_emit_time = 0;
            GstClockTime current_pts = GST_BUFFER_PTS(buffer);

            if (last_emit_time == 0 || (GST_BUFFER_PTS_IS_VALID(buffer) &&
                                        current_pts - last_emit_time >= 10 * GST_MSECOND)) {
                emit stream->metadata_received(xdaqmetadata.value());
                last_emit_time = current_pts;
            }
        }

        return GST_PAD_PROBE_OK;
    }

    static GstPadProbeReturn fps_monitor_probe(
        [[maybe_unused]] GstPad *pad, GstPadProbeInfo *info, gpointer user_data
    )
    {
        auto stream = static_cast<Stream *>(user_data);
        if (!stream) {
            return GST_PAD_PROBE_REMOVE;
        }

        auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);

        if (!buffer) {
            return GST_PAD_PROBE_OK;
        }

        stream->_fps_frame_count++;

        GstClockTime now = GST_BUFFER_PTS(buffer);
        GstClockTime last_report = stream->_fps_last_report_time.load();

        // Report every 2 seconds (based on PTS)
        if (GST_BUFFER_PTS_IS_VALID(buffer) &&
            (last_report == 0 || now - last_report >= 60 * GST_SECOND)) {
            if (last_report > 0) {
                GstClockTime elapsed = now - last_report;
                uint64_t frames = stream->_fps_frame_count.load();
                double fps = static_cast<double>(frames) * GST_SECOND / elapsed;
                spdlog::info(
                    "[FPS Monitor] queue_record: {} frames in {:.2f}s = {:.1f} FPS",
                    frames,
                    static_cast<double>(elapsed) / GST_SECOND,
                    fps
                );
            }
            stream->_fps_frame_count = 0;
            stream->_fps_last_report_time = now;
        }

        return GST_PAD_PROBE_OK;
    }

    static GstPadProbeReturn buffer_collector_probe(
        [[maybe_unused]] GstPad *pad, GstPadProbeInfo *info, gpointer user_data
    )
    {
        auto stream = static_cast<Stream *>(user_data);
        if (!stream) {
            return GST_PAD_PROBE_REMOVE;
        }

        // Replace the frame-counting approach with time-based logging
        static thread_local auto last_log_time = std::chrono::steady_clock::now();
        static thread_local uint64_t total_copies = 0;
        static thread_local uint64_t total_unrefs = 0;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - last_log_time);
        // Periodic logging every 1 minute
        if (elapsed.count() >= 1) {
            last_log_time = now;

            auto queue = gst_bin_get_by_name(GST_BIN(stream->_pipeline), "queue_record");
            if (queue) {
                guint current_level_buffers;
                g_object_get(queue, "current-level-buffers", &current_level_buffers, nullptr);
                spdlog::info(
                    "queue_record internal buffers: {}, pre_record_buffer: {}",
                    current_level_buffers,
                    stream->_pre_record_buffer.size()
                );
                spdlog::info(
                    "Buffer stats: copies={}, unrefs={}, delta={}, buffer_count={}",
                    total_copies,
                    total_unrefs,
                    total_copies - total_unrefs,
                    stream->_pre_record_buffer.size()
                );
                gst_object_unref(queue);
            }
        }

        auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);

        // Validate buffer before any operations
        if (!buffer || !GST_IS_BUFFER(buffer)) {
            return GST_PAD_PROBE_OK;
        }

        // Additional validation: check buffer has valid refcount
        if (GST_MINI_OBJECT_REFCOUNT_VALUE(buffer) < 1) {
            spdlog::warn("Buffer collector: invalid buffer refcount, skipping");
            return GST_PAD_PROBE_OK;
        }

        // Only collect buffers when NOT recording
        if (!stream->_recording_active.load()) {
            std::lock_guard<std::mutex> lock(stream->_pre_record_mutex);

            // Check if this is a keyframe (IDR/CRA frame)
            // DELTA_UNIT flag is set for P/B frames, NOT set for keyframes
            bool is_keyframe = !GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);

            // Add new buffer with copy
            GstBuffer *buffer_copy = gst_buffer_copy(buffer);
            if (!buffer_copy) {
                spdlog::error("Buffer collector: failed to copy buffer");
                return GST_PAD_PROBE_OK;
            }
            stream->_pre_record_buffer.push_back(buffer_copy);
            total_copies++;

            // Track the index of the most recent keyframe
            if (is_keyframe) {
                stream->_last_keyframe_index = stream->_pre_record_buffer.size() - 1;
                spdlog::debug(
                    "Buffer collector: keyframe detected at index {}", stream->_last_keyframe_index
                );
            }

            spdlog::debug(
                "Buffer collector: pushed buffer (keyframe={}), buffer size = {}",
                is_keyframe,
                stream->_pre_record_buffer.size()
            );

            // Remove old buffers beyond PRE_RECORD_DURATION
            while (stream->_pre_record_buffer.size() > 1) {
                auto oldest = stream->_pre_record_buffer.front();

                // Validate oldest buffer before accessing
                if (!oldest || !GST_IS_BUFFER(oldest)) {
                    spdlog::warn("Buffer collector: invalid oldest buffer, removing from deque");
                    stream->_pre_record_buffer.pop_front();
                    if (stream->_last_keyframe_index > 0) {
                        stream->_last_keyframe_index--;
                    }
                    continue;
                }

                auto newest = stream->_pre_record_buffer.back();

                // Validate newest buffer
                if (!newest || !GST_IS_BUFFER(newest)) {
                    break;
                }

                // Check if we have valid PTS values
                if (GST_BUFFER_PTS_IS_VALID(oldest) && GST_BUFFER_PTS_IS_VALID(newest)) {
                    if (GST_BUFFER_PTS(newest) - GST_BUFFER_PTS(oldest) > PRE_RECORD_DURATION) {
                        // Validate refcount before unref
                        if (GST_MINI_OBJECT_REFCOUNT_VALUE(oldest) >= 1) {
                            gst_buffer_unref(oldest);
                            total_unrefs++;
                        } else {
                            spdlog::warn(
                                "Buffer collector: oldest buffer has invalid refcount {}",
                                GST_MINI_OBJECT_REFCOUNT_VALUE(oldest)
                            );
                        }
                        stream->_pre_record_buffer.pop_front();
                        // Adjust keyframe index since we removed from front
                        if (stream->_last_keyframe_index > 0) {
                            stream->_last_keyframe_index--;
                        }
                        spdlog::debug(
                            "Buffer collector: removed old buffer (by PTS), buffer size = {}",
                            stream->_pre_record_buffer.size()
                        );
                    } else {
                        break;
                    }
                } else {
                    // Fallback: limit by count (assume ~30fps, 10 seconds = ~300 frames)
                    if (stream->_pre_record_buffer.size() > 300) {
                        // Validate refcount before unref
                        if (GST_MINI_OBJECT_REFCOUNT_VALUE(oldest) >= 1) {
                            gst_buffer_unref(oldest);
                            total_unrefs++;
                        } else {
                            spdlog::warn(
                                "Buffer collector: oldest buffer has invalid refcount {}",
                                GST_MINI_OBJECT_REFCOUNT_VALUE(oldest)
                            );
                        }
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
        }

        spdlog::debug(
            "Buffer collector: accepted buffer, current size = {}",
            stream->_pre_record_buffer.size()
        );
        return GST_PAD_PROBE_OK;
    }

    void clear_pre_record_buffer()
    {
        std::lock_guard<std::mutex> lock(_pre_record_mutex);
        size_t count = _pre_record_buffer.size();
        size_t unreffed = 0;
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

    void reset()
    {
        spdlog::info("Stream::reset()");
        set_streaming(false);

        // Clear pre-record buffer
        clear_pre_record_buffer();

        if (_pipeline) {
            // Remove buffer collector probe and FPS monitor probe if installed
            auto queue_record = gst_bin_get_by_name(GST_BIN(_pipeline), "queue_record");
            if (queue_record) {
                if (_buffer_collector_probe_id != 0) {
                    auto queue_srcpad = gst_element_get_static_pad(queue_record, "src");
                    gst_pad_remove_probe(queue_srcpad, _buffer_collector_probe_id);
                    gst_object_unref(queue_srcpad);
                    _buffer_collector_probe_id = 0;
                }
                if (_fps_monitor_probe_id != 0) {
                    auto queue_sinkpad = gst_element_get_static_pad(queue_record, "sink");
                    gst_pad_remove_probe(queue_sinkpad, _fps_monitor_probe_id);
                    gst_object_unref(queue_sinkpad);
                    _fps_monitor_probe_id = 0;
                }
                gst_object_unref(queue_record);
            }

            // Detach the QML video item from the sink BEFORE destroying the pipeline
            // This prevents heap corruption from the qt6d3d11 plugin
#ifdef _WIN32
            auto fpsdisplaysink = gst_bin_get_by_name(GST_BIN(_pipeline), "sink");
            if (fpsdisplaysink) {
                GstElement *video_sink = nullptr;
                g_object_get(fpsdisplaysink, "video-sink", &video_sink, nullptr);
                if (video_sink) {
                    g_object_set(video_sink, "widget", nullptr, nullptr);
                    spdlog::info("Detached QML widget from D3D11 sink");
                    gst_object_unref(video_sink);
                }
                gst_object_unref(fpsdisplaysink);
            }
#elif __APPLE__
            auto fpsdisplaysink = gst_bin_get_by_name(GST_BIN(_pipeline), "sink");
            if (fpsdisplaysink) {
                GstElement *video_sink = nullptr;
                g_object_get(fpsdisplaysink, "video-sink", &video_sink, nullptr);
                if (video_sink) {
                    g_object_set(video_sink, "widget", nullptr, nullptr);
                    spdlog::info("Detached QML widget from GL sink");
                    gst_object_unref(video_sink);
                }
                gst_object_unref(fpsdisplaysink);
            }
#endif

            auto bus = gst_pipeline_get_bus(_pipeline);
            gst_bus_remove_watch(bus);
            gst_object_unref(bus);

            gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_NULL);
            gst_object_unref(_pipeline);
            _pipeline = nullptr;
        }
    }

    void start(std::string_view media_type)
    {
        spdlog::info("Stream::start()");

        // ALWAYS reset the pipeline to ensure a fresh connection and correct format
        if (_pipeline) {
            reset();
        }

        // Re-initialize with the correct media_type
        init_pipeline(pipeline(fmt::format("{}:{}", "192.168.177.100", _port), media_type));

        if (!_pipeline) {
            spdlog::error("Pipeline creation failed");
            return;
        }

        if (gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_PLAYING) ==
            GST_STATE_CHANGE_FAILURE) {
            spdlog::error("Failed to set pipeline to PLAYING state");
        } else {
            spdlog::info("Pipeline set to PLAYING state");
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

        _streaming = false;
        reset();
    }

    void set_streaming(bool now)
    {
        if (_streaming != now) {
            _streaming = now;
            emit status_changed(now);
        }
    }

    struct FileTracker {
        std::string base_filepath;
        std::vector<fs::path> file_paths;
        int max_files;
    };

    static gchararray generate_filename(
        [[maybe_unused]] GstElement *splitmux, [[maybe_unused]] guint fragment_id, gpointer udata
    )
    {
        auto tracker = static_cast<FileTracker *>(udata);
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm_now;

#ifdef _WIN32
        localtime_s(&tm_now, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_now);
#endif

        auto timestamp = fmt::format("{:%Y-%m-%d_%H-%M-%S}", tm_now);
        auto file_path = fmt::format("{}-{}.mkv", tracker->base_filepath, timestamp);

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

    void start_h265_recording(
        const fs::path &filepath, bool continuous = true, int max_size_time = 10,
        xvc::TimeUnit unit = xvc::TimeUnit::Minutes, bool loop = false, int max_files = 10
    )
    {
        if (!_pipeline) {
            spdlog::error("Pipeline is null");
            return;
        }

        // Run pipeline modification on Qt main thread to avoid race condition
        // with Qt's rendering thread (gstqt6d3d11 plugin)
        if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
            spdlog::info("start_h265_recording: dispatching to Qt main thread");
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [this, filepath, continuous, max_size_time, unit, loop, max_files]() {
                    start_h265_recording_impl(
                        filepath, continuous, max_size_time, unit, loop, max_files
                    );
                },
                Qt::BlockingQueuedConnection
            );
            return;
        }

        start_h265_recording_impl(filepath, continuous, max_size_time, unit, loop, max_files);
    }

    void start_h265_recording_impl(
        const fs::path &filepath, bool continuous, int max_size_time, xvc::TimeUnit unit, bool loop,
        int max_files
    )
    {
        {
            std::lock_guard<std::mutex> lock(_pre_record_mutex);
            spdlog::info(
                "Pre-record buffer state: {} frames, keyframe_index={}",
                _pre_record_buffer.size(),
                _last_keyframe_index
            );
            if (!_pre_record_buffer.empty()) {
                auto oldest = _pre_record_buffer.front();
                auto newest = _pre_record_buffer.back();
                if (GST_BUFFER_PTS_IS_VALID(oldest) && GST_BUFFER_PTS_IS_VALID(newest)) {
                    GstClockTime duration = GST_BUFFER_PTS(newest) - GST_BUFFER_PTS(oldest);
                    spdlog::info(
                        "Pre-record buffer PTS range: oldest={} ns, newest={} ns, duration={} ns "
                        "({:.2f} sec)",
                        GST_BUFFER_PTS(oldest),
                        GST_BUFFER_PTS(newest),
                        duration,
                        static_cast<double>(duration) / GST_SECOND
                    );
                } else {
                    spdlog::warn(
                        "Pre-record buffer has invalid PTS: oldest_valid={}, newest_valid={}",
                        GST_BUFFER_PTS_IS_VALID(oldest),
                        GST_BUFFER_PTS_IS_VALID(newest)
                    );
                }
            }
        }

        // Create directory if needed
        auto path = filepath.parent_path();
        if (!fs::exists(path)) {
            spdlog::info("Create Directory: {}", path.generic_string());
            std::error_code ec;
            if (!fs::create_directories(path, ec)) {
                spdlog::info(
                    "Failed to create directory: {}. Error: {}", path.generic_string(), ec.message()
                );
            }
        }

        int time_seconds = max_size_time;
        switch (unit) {
        case xvc::TimeUnit::Minutes: time_seconds *= 60; break;
        case xvc::TimeUnit::Hours: time_seconds *= 3600; break;
        case xvc::TimeUnit::Days: time_seconds *= 86400; break;
        default: break;
        }

        // Get existing elements
        auto queue_record = gst_bin_get_by_name(GST_BIN(_pipeline), "queue_record");
        auto fakesink = gst_bin_get_by_name(GST_BIN(_pipeline), "record_sink");

        if (!queue_record || !fakesink) {
            spdlog::error("Could not find queue_record or record_sink");
            if (queue_record) gst_object_unref(queue_record);
            if (fakesink) gst_object_unref(fakesink);
            return;
        }

        // Create new recording elements
        auto record_parser = gst_element_factory_make("h265parse", "record_parser");
        auto cf_parser = gst_element_factory_make("capsfilter", "cf_record_parser");
        auto muxer = gst_element_factory_make("matroskamux", "muxer");
        auto splitmux = gst_element_factory_make("splitmuxsink", "splitmux");

        if (!record_parser || !cf_parser || !muxer || !splitmux) {
            spdlog::error("Failed to create recording elements");
            gst_object_unref(queue_record);
            gst_object_unref(fakesink);
            if (record_parser) gst_object_unref(record_parser);
            if (cf_parser) gst_object_unref(cf_parser);
            if (muxer) gst_object_unref(muxer);
            if (splitmux) gst_object_unref(splitmux);
            return;
        }

        // Configure caps filter for hvc1 format
        auto caps = gst_caps_new_simple(
            "video/x-h265",
            "stream-format",
            G_TYPE_STRING,
            "hev1",
            "alignment",
            G_TYPE_STRING,
            "au",
            nullptr
        );
        g_object_set(cf_parser, "caps", caps, nullptr);
        gst_caps_unref(caps);

        // Configure muxer
        g_object_set(muxer, "timecodescale", 1, "offset-to-zero", TRUE, nullptr);


        max_files = loop ? max_files : INT_MAX;

        auto tracker =
            std::make_unique<FileTracker>(FileTracker{filepath.generic_string(), {}, max_files});

        g_signal_connect_data(
            splitmux,
            "format-location",
            G_CALLBACK(generate_filename),
            tracker.release(),
            [](gpointer data, GClosure *) {
                delete static_cast<FileTracker *>(data);
                spdlog::debug("FileTracker memory successfully released");
            },
            static_cast<GConnectFlags>(0)
        );

        g_object_set(
            splitmux,
            "max-size-time",
            continuous ? max_size_time * GST_SECOND : 0,
            "async-finalize",
            false,
            "muxer",
            muxer,
            nullptr
        );

        // Add new elements to pipeline
        gst_bin_add_many(GST_BIN(_pipeline), record_parser, cf_parser, splitmux, nullptr);

        // Get pads
        auto queue_srcpad = gst_element_get_static_pad(queue_record, "src");

        gst_pad_add_probe(
            queue_srcpad,
            GST_PAD_PROBE_TYPE_IDLE,
            []([[maybe_unused]] GstPad *pad,
               [[maybe_unused]] GstPadProbeInfo *info,
               gpointer user_data) -> GstPadProbeReturn {
                auto stream = static_cast<Stream *>(user_data);

                if (!stream) {
                    return GST_PAD_PROBE_REMOVE;
                }

                // bool expected = false;
                // if (!stream->_recording_active.compare_exchange_strong(expected, true)) {
                //     return GST_PAD_PROBE_REMOVE;
                // }

                auto pipeline = GST_BIN(stream->_pipeline);

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

                // Unlink queue from fakesink
                gst_pad_unlink(queue_srcpad, fakesink_sinkpad);

                // Remove and destroy fakesink
                gst_element_set_state(fakesink, GST_STATE_NULL);
                gst_bin_remove(pipeline, fakesink);

                // Link new recording chain: queue_record -> record_parser -> cf_parser -> splitmux
                if (!gst_element_link_many(
                        queue_record, record_parser, cf_parser, splitmux, nullptr
                    )) {
                    spdlog::error("Failed to link recording elements");
                }

                // Sync states with parent (pipeline is PLAYING)
                gst_element_sync_state_with_parent(record_parser);
                gst_element_sync_state_with_parent(cf_parser);
                gst_element_sync_state_with_parent(splitmux);

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

                    size_t frames_to_flush = stream->_pre_record_buffer.size() - start_index;
                    spdlog::info(
                        "Flushing {} pre-recorded frames to record_parser (starting from keyframe "
                        "at index "
                        "{}, skipping {} frames)",
                        frames_to_flush,
                        start_index,
                        start_index
                    );

                    auto record_parser_sinkpad = gst_element_get_static_pad(record_parser, "sink");
                    if (record_parser_sinkpad) {
                        // 1. Forward sticky STREAM_START from queue
                        GstEvent *stream_start_event =
                            gst_pad_get_sticky_event(queue_srcpad, GST_EVENT_STREAM_START, 0);
                        if (stream_start_event) {
                            gst_pad_send_event(record_parser_sinkpad, stream_start_event);
                        } else {
                            gst_pad_send_event(
                                record_parser_sinkpad, gst_event_new_stream_start("pre-record")
                            );
                        }

                        // 2. Forward sticky CAPS from queue
                        GstEvent *caps_event =
                            gst_pad_get_sticky_event(queue_srcpad, GST_EVENT_CAPS, 0);
                        if (caps_event) {
                            gst_pad_send_event(record_parser_sinkpad, caps_event);
                        } else {
                            auto input_caps = gst_caps_new_simple(
                                "video/x-h265",
                                "stream-format",
                                G_TYPE_STRING,
                                "byte-stream",
                                "alignment",
                                G_TYPE_STRING,
                                "au",
                                nullptr
                            );
                            gst_pad_send_event(
                                record_parser_sinkpad, gst_event_new_caps(input_caps)
                            );
                            gst_caps_unref(input_caps);
                        }

                        // 3. Forward sticky SEGMENT from queue (This is the magic fix!)
                        GstEvent *seg_event =
                            gst_pad_get_sticky_event(queue_srcpad, GST_EVENT_SEGMENT, 0);
                        if (seg_event) {
                            gst_pad_send_event(record_parser_sinkpad, seg_event);
                            spdlog::debug("Pushed original sticky segment from queue");
                        } else {
                            GstSegment segment;
                            gst_segment_init(&segment, GST_FORMAT_TIME);
                            gst_pad_send_event(
                                record_parser_sinkpad, gst_event_new_segment(&segment)
                            );
                            spdlog::warn("No sticky segment found, pushing default");
                        }

                        // --> Continue with the existing logic to push pre-recorded frames
                        size_t pushed_count = 0;
                        for (size_t i = start_index; i < stream->_pre_record_buffer.size(); ++i) {
                            auto buffer = stream->_pre_record_buffer[i];
                            if (buffer && GST_IS_BUFFER(buffer) &&
                                GST_MINI_OBJECT_REFCOUNT_VALUE(buffer) >= 1) {
                                GstFlowReturn ret =
                                    gst_pad_chain(record_parser_sinkpad, gst_buffer_ref(buffer));
                                if (ret != GST_FLOW_OK) {
                                    spdlog::warn(
                                        "Failed to push pre-recorded buffer {}: {}",
                                        i,
                                        gst_flow_get_name(ret)
                                    );
                                } else {
                                    pushed_count++;
                                }
                            } else if (buffer) {
                                spdlog::warn("Recording flush: invalid buffer at index {}", i);
                            }
                        }
                        spdlog::info("Successfully pushed {} pre-recorded frames", pushed_count);
                        gst_object_unref(record_parser_sinkpad);
                    } else {
                        spdlog::error("Failed to get record_parser sink pad for pre-record flush");
                    }

                    // Unref all buffers with validation
                    size_t unreffed = 0;
                    for (auto buffer : stream->_pre_record_buffer) {
                        if (buffer && GST_IS_BUFFER(buffer)) {
                            if (GST_MINI_OBJECT_REFCOUNT_VALUE(buffer) >= 1) {
                                gst_buffer_unref(buffer);
                                unreffed++;
                            } else {
                                spdlog::warn(
                                    "Recording flush cleanup: buffer has invalid refcount"
                                );
                            }
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

                stream->_recording_active = true;
                return GST_PAD_PROBE_REMOVE;
            },
            this,
            nullptr
        );

        gst_object_unref(queue_srcpad);
        gst_object_unref(queue_record);
        gst_object_unref(fakesink);
    }

    void stop_h265_recording()
    {
        if (!_pipeline) {
            spdlog::error("Pipeline is null");
            return;
        }

        // Run pipeline modification on Qt main thread to avoid race condition
        // with Qt's rendering thread (gstqt6d3d11 plugin)
        if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
            spdlog::info("stop_h265_recording: dispatching to Qt main thread");
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [this]() { stop_h265_recording_impl(); },
                Qt::BlockingQueuedConnection
            );
            return;
        }

        stop_h265_recording_impl();
    }

    void stop_h265_recording_impl()
    {
        _recording_active = false;

        auto queue_record = gst_bin_get_by_name(GST_BIN(_pipeline), "queue_record");
        if (!queue_record) {
            spdlog::error("queue_record not found");
            return;
        }

        auto queue_srcpad = gst_element_get_static_pad(queue_record, "src");
        gst_object_ref(_pipeline);

        gst_pad_add_probe(
            queue_srcpad,
            GST_PAD_PROBE_TYPE_IDLE,
            []([[maybe_unused]] GstPad *pad,
               [[maybe_unused]] GstPadProbeInfo *info,
               gpointer user_data) -> GstPadProbeReturn {
                auto pipeline = GST_PIPELINE(user_data);

                spdlog::info("stop_h265_recording: idle probe triggered");

                auto queue_record = gst_bin_get_by_name(GST_BIN(pipeline), "queue_record");
                auto record_parser = gst_bin_get_by_name(GST_BIN(pipeline), "record_parser");
                auto cf_parser = gst_bin_get_by_name(GST_BIN(pipeline), "cf_record_parser");
                auto splitmux = gst_bin_get_by_name(GST_BIN(pipeline), "splitmux");

                if (!splitmux) {
                    spdlog::warn("Recording elements not found, may already be stopped");
                    if (queue_record) gst_object_unref(queue_record);
                    if (record_parser) gst_object_unref(record_parser);
                    if (cf_parser) gst_object_unref(cf_parser);
                    return GST_PAD_PROBE_REMOVE;
                }

                // Unlink the FULL recording chain
                gst_element_unlink_many(queue_record, record_parser, cf_parser, splitmux, nullptr);

                // Create and add new fakesink FIRST (so queue has somewhere to go)
                auto fakesink = gst_element_factory_make("fakesink", "record_sink");
                g_object_set(fakesink, "async", FALSE, "sync", FALSE, nullptr);
                gst_bin_add(GST_BIN(pipeline), fakesink);

                if (!gst_element_link(queue_record, fakesink)) {
                    spdlog::error("Failed to link queue_record to fakesink");
                }
                gst_element_sync_state_with_parent(fakesink);

                // Send EOS to splitmux to finalize the file
                auto splitmux_sinkpad = gst_element_get_static_pad(splitmux, "video");
                if (splitmux_sinkpad) {
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
            [](gpointer data) {
                if (data) {
                    gst_object_unref(GST_OBJECT(data));
                }
            }
        );

        gst_object_unref(queue_srcpad);
        gst_object_unref(queue_record);
    }
};
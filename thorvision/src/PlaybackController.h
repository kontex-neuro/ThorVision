#include <QtCore/qlogging.h>
#include <QtCore/qnamespace.h>
#include <QtQuick/qquickitem.h>
#include <fmt/format.h>
#include <gst/gst.h>
#include <spdlog/spdlog.h>

#include <QObject>
#include <QQuickItem>
#include <QQuickWindow>

#include "CameraModel.h"

// class SetPlaying : public QRunnable
// {
// public:
//     SetPlaying(GstElement *pipeline)
//     {
//         this->pipeline_ = pipeline ? static_cast<GstElement *>(gst_object_ref(pipeline)) :
//         nullptr;
//     }
//     ~SetPlaying()
//     {
//         if (this->pipeline_) gst_object_unref(this->pipeline_);
//     }

//     void run()
//     {
//         if (this->pipeline_) gst_element_set_state(this->pipeline_, GST_STATE_PLAYING);
//     }

// private:
//     GstElement *pipeline_;
// };

// class SetSinkJob : public QRunnable
// {
// public:
//     SetSinkJob(GstElement *sink, QQuickItem *item) : sink_(sink), item_(item)
//     {
//         if (sink_) gst_object_ref(sink_);
//     }
//     ~SetSinkJob()
//     {
//         if (sink_) gst_object_unref(sink_);
//     }
//     void run() override { g_object_set(sink_, "widget", item_, nullptr); }

// private:
//     GstElement *sink_;
//     QQuickItem *item_;
// };

struct Stream {
    GstPipeline *play;
    int index;
    std::optional<GstClockTime> base_time;
    QQuickItem *video_item;

    Stream(GstPipeline *play, int index, QQuickItem *video_item)
        : play(play), index(index), video_item(video_item)
    {
    }
    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &) = delete;
    Stream(Stream &&o) noexcept : play(o.play) { o.play = nullptr; }
    Stream &operator=(Stream &&o) noexcept
    {
        if (this == &o) return *this;
        play = std::exchange(o.play, nullptr);
        return *this;
    }

    ~Stream()
    {
        if (play) {
            gst_element_set_state(GST_ELEMENT(play), GST_STATE_NULL);
            gst_object_unref(play);
        }
    }
};

struct PlaybackController : public QObject {
    Q_OBJECT

public:
    PlaybackController(
        std::vector<std::unique_ptr<Stream>> &streams, CameraModel *camera_model,
        QQuickWindow *root_object = nullptr
    )
        : streams(streams), camera_model(camera_model), root_object(root_object)
    {
        auto video_layout = root_object->findChild<QQuickItem *>("video_layout");
        qDebug() << "Found video layout:" << video_layout;
        g_assert(video_layout);

        auto repeater = video_layout->findChild<QQuickItem *>("repeater");
        qDebug() << "Found repeater:" << repeater;
        g_assert(repeater);

        QObject::connect(
            repeater, SIGNAL(itemAdded(int, QQuickItem *)), this, SLOT(onAdded(int, QQuickItem *))
        );
        QObject::connect(
            repeater,
            SIGNAL(itemRemoved(int, QQuickItem *)),
            this,
            SLOT(onRemoved(int, QQuickItem *))
        );
    }
    bool video_loaded = false;
    std::vector<std::unique_ptr<Stream>> &streams;
    CameraModel *camera_model;
    QQuickWindow *root_object;

    void load()
    {
        // for (auto &stream : streams) {
        //     auto sink = gst_bin_get_by_name(GST_BIN(stream->play), "sink");
        //     spdlog::info("set widget ");
        //     g_object_set(sink, "widget", stream->video_item, nullptr);
        //     spdlog::info("after set widget ");
        // }
        auto clock = gst_system_clock_obtain();

        for (auto i = 0; i < streams.size(); ++i) {
            gst_pipeline_use_clock(streams[i]->play, clock);
            auto ret = gst_element_set_state(GST_ELEMENT(streams[i]->play), GST_STATE_PAUSED);
            switch (ret) {
            case GST_STATE_CHANGE_FAILURE:
                fmt::print(stderr, "Failed to set pipeline to paused.\n");
                break;
            case GST_STATE_CHANGE_SUCCESS: fmt::print("Set pipeline to paused.\n"); break;
            case GST_STATE_CHANGE_ASYNC:
                fmt::print("Set pipeline to paused asynchronously.\n");
                break;
            case GST_STATE_CHANGE_NO_PREROLL:
                fmt::print("Set pipeline to paused with no preroll.\n");
                break;
            default: fmt::print(stderr, "Unknown state change return.\n");
            }
        }
        for (int i = 0; i < streams.size(); ++i) {
            GstState state;
            GstStateChangeReturn ret = gst_element_get_state(
                GST_ELEMENT(streams[i]->play), &state, NULL, GST_CLOCK_TIME_NONE
            );
            if (ret == GST_STATE_CHANGE_FAILURE) {
                fmt::print(stderr, "Failed to get state of pipeline.\n");
            } else {
                fmt::print("State of pipeline: {}\n", gst_element_state_get_name(state));
            }
        }

        auto base_time = gst_clock_get_time(clock);
        for (auto &stream : streams) {
            if (stream->base_time) {
                gst_element_set_base_time(
                    GST_ELEMENT(stream->play), base_time + stream->base_time.value()
                );
                gst_element_set_start_time(GST_ELEMENT(stream->play), GST_CLOCK_TIME_NONE);
            } else {
                fmt::print(stderr, "Base time is not set.\n");
                return;
            }
        }
        gst_object_unref(clock);
        fmt::print("Started playback of {} streams.\n", streams.size());
    }

    void start()
    {
        for (auto &stream : streams) {
            auto _ = gst_element_set_state(GST_ELEMENT(stream->play), GST_STATE_PLAYING);
        }
    }

    void pause()
    {
        for (auto &stream : streams) {
            auto _ = gst_element_set_state(GST_ELEMENT(stream->play), GST_STATE_PAUSED);
        }
    }

    auto add_stream(QQuickItem *video_item, int index, int port)
        -> std::optional<std::unique_ptr<Stream>>
    {
        qDebug() << "Port:" << port << "video_item:" << video_item << "index:" << index;

        if (!video_item) {
            spdlog::error("video_item is null");
            return std::nullopt;
        }

        auto uri = fmt::format("{}:{}", "192.168.177.100", port);
        auto pipeline = gst_pipeline_new(nullptr);
        gst_element_set_start_time(pipeline, GST_CLOCK_TIME_NONE);

        auto src = gst_element_factory_make("srtclientsrc", "src");
        auto parser = gst_element_factory_make("jpegparse", "parser");
        auto tee = gst_element_factory_make("tee", "t");
        auto queue_display = gst_element_factory_make("queue", "queue_display");
#ifdef _WIN32
        auto dec = gst_element_factory_make("jpegdec", "dec");
#elif __APPLE__
        auto dec = gst_element_factory_make("vtdec", "dec");
#else
        auto dec = gst_element_factory_make("jpegdec", "dec");
#endif
        auto conv = gst_element_factory_make("videoconvert", "conv");
        auto cf_conv = gst_element_factory_make("capsfilter", "cf_conv");
        auto glupload = gst_element_factory_make("glupload", "glupload");
        auto sink = gst_element_factory_make("qml6glsink", "sink");
        auto fpsdisplaysink = gst_element_factory_make("fpsdisplaysink", "fpsdisplaysink");

        if (!src || !parser || !tee || !queue_display || !dec || !conv || !cf_conv || !glupload ||
            !sink) {
            fmt::print(stderr, "Failed to create elements.\n");
            return std::nullopt;
        }

        // clang-format off
        std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_conv_caps(
            gst_caps_new_simple(
            "video/x-raw",
            "format", G_TYPE_STRING, "RGB",
            nullptr),
            gst_caps_unref
        );
        // clang-format on

        g_object_set(src, "uri", fmt::format("srt://{}", uri).c_str(), nullptr);
        g_object_set(cf_conv, "caps", cf_conv_caps.get(), nullptr);
        g_object_set(sink, "sync", false, nullptr);
        g_object_set(fpsdisplaysink, "video-sink", sink, nullptr);
        g_object_set(fpsdisplaysink, "text-overlay", false, nullptr);
        g_object_set(fpsdisplaysink, "sync", false, nullptr);

        // This is the line where problem happens
        g_object_set(sink, "widget", video_item, nullptr);

        gst_bin_add_many(
            GST_BIN(pipeline),
            src,
            parser,
            tee,
            queue_display,
            dec,
            conv,
            cf_conv,
            glupload,
            fpsdisplaysink,
            nullptr
        );

        if (!gst_element_link_many(src, parser, tee, nullptr) ||
            !gst_element_link_many(
                tee, queue_display, dec, conv, cf_conv, glupload, fpsdisplaysink, nullptr
            )) {
            spdlog::error("Elements could not be linked.");
            gst_object_unref(pipeline);
            return std::nullopt;
        }

        auto stream = std::make_unique<Stream>(GST_PIPELINE(pipeline), index, video_item);

        // auto sink_pad = gst_element_get_static_pad(sink, "sink");
        // if (!sink_pad) {
        //     fmt::print(stderr, "Failed to get sink pad from sink.\n");
        //     return std::nullopt;
        // }
        // gst_pad_add_probe(sink_pad, GST_PAD_PROBE_TYPE_BUFFER, &pad_probe, stream.get(), NULL);
        // gst_object_unref(sink_pad);

        return std::move(stream);
    }

    void remove_stream(int index)
    {
        if (index < 0 || index >= streams.size()) {
            fmt::print(stderr, "Index out of range.\n");
            return;
        }
        streams.erase(streams.begin() + index);
        fmt::print("Removed stream at index {}.\n", index);
    }

    Q_INVOKABLE void startPlayback(bool isActive)
    {
        if (isActive && !video_loaded) {
            load();
            video_loaded = true;
            start();
        } else if (isActive && video_loaded) {
            start();
        } else if (!isActive && video_loaded) {
            pause();
        }
    }

public slots:
    void onAdded(int index, QQuickItem *item)
    {
        auto camera = camera_model->data(camera_model->index(index), CameraModel::CameraItemRole)
                          .value<CameraItem *>();
        auto port = camera->port();
        spdlog::info("Processing stream for port: {}", port);

        if (!item) {
            spdlog::error("item is null");
            return;
        }

        if (auto stream = add_stream(item, index, port)) {
            streams.push_back(std::move(*stream));
        }
    }

    void onRemoved(int index, QQuickItem *item)
    {
        qDebug() << "slot being called for removed " << index << item;
        remove_stream(index);
    }
};
#pragma once

#include <QCoreApplication>
#include <QQuickItem>
#include <QThread>
#include <gst/gstclock.h>
#include <deque>
#include <mutex>

#include "xdaqmetadata/safe_queue.h"
#include "xdaqvc/xvc.h"

// TODO: Move to libxvc
class Stream : public QObject
{
    Q_OBJECT

public:
    enum class Codec { MJPEG, H265 };

    Stream(QQuickItem *video_item, int index, int port, QObject *parent = nullptr);
    ~Stream() = default;

    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &) = delete;
    Stream(Stream &&) = delete;
    Stream &operator=(Stream &&) = delete;

    bool init_pipeline(std::string_view pipeline_desc);
    void reset();
    bool start(std::string_view codec);
    bool stop();

    bool streaming() const noexcept { return _streaming.load(); }
    void set_streaming(bool now) noexcept
    {
        if (_streaming == now) return;
        _streaming = now;
        emit status_changed(_streaming);
    }

    GstPipeline *_pipeline;
    SafeQueue _safe_queue;

    std::deque<GstBuffer *> _pre_record_buffer;
    std::mutex _pre_record_mutex;
    size_t _last_keyframe_index{0};

    std::atomic<bool> _recording;

    // TODO: Move to libxvc
    bool start_h265_recording(const xvc::RecordConfig &config);
    bool stop_h265_recording();

signals:
    void metadata_received(const XDAQFrameData &metadata);
    void status_changed(bool streaming);

private:
    QPointer<QQuickItem> _video_item;

    int _index;
    int _port;

    std::atomic<bool> _streaming;
};

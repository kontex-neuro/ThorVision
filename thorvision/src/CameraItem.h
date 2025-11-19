#pragma once

#ifndef CAMERAITEM_H
#define CAMERAITEM_H

#include <gst/gst.h>
#include <gst/gstbus.h>

#include <QHash>
#include <QString>
#include <QVector>
#include <memory>
#include <optional>
#include <thread>

#include "RecorderSettings.h"
#include "xdaqmetadata/metadata_handler.h"
#include "xdaqmetadata/xdaqmetadata.h"
#include "xdaqvc/camera.h"

struct Stream {
    GstPipeline *_pipeline;
    GMainLoop *_loop;
    int _index;

    std::optional<GstClockTime> _base_time;
    std::unique_ptr<MetadataHandler> _metadata_handler;

    GstBus *_bus;
    std::jthread _thread;

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
        case GST_MESSAGE_ERROR: {
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
            spdlog::info("End-Of-Stream reached.");
            break;
        }
        case GST_MESSAGE_ELEMENT: {
            auto const structure = gst_message_get_structure(msg);
            if (gst_structure_has_name(structure, "GstBinForwarded")) {
                GstMessage *forward_msg = nullptr;

                gst_structure_get(structure, "message", GST_TYPE_MESSAGE, &forward_msg, nullptr);
                if (GST_MESSAGE_TYPE(forward_msg) == GST_MESSAGE_EOS) {
                    spdlog::info(
                        "EOS from element {}", GST_OBJECT_NAME(GST_MESSAGE_SRC(forward_msg))
                    );

                    auto pipeline = GST_BIN(stream->_pipeline);
                    auto queue_record = gst_bin_get_by_name(pipeline, "queue_record");
                    auto record_parser = gst_bin_get_by_name(pipeline, "record_parser");
                    auto filesink = gst_bin_get_by_name(pipeline, "filesink");

                    gst_bin_remove_many(pipeline, queue_record, record_parser, filesink, nullptr);

                    gst_element_set_state(queue_record, GST_STATE_NULL);
                    gst_element_set_state(record_parser, GST_STATE_NULL);
                    gst_element_set_state(filesink, GST_STATE_NULL);

                    gst_object_unref(queue_record);
                    gst_object_unref(record_parser);
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

    Stream(GstPipeline *pipeline, int index) : _pipeline(pipeline), _index(index)
    {
        _metadata_handler = std::make_unique<MetadataHandler>();

        _loop = g_main_loop_new(nullptr, false);
        _bus = gst_pipeline_get_bus(_pipeline);
        gst_bus_add_signal_watch(_bus);
        g_signal_connect(G_OBJECT(_bus), "message", G_CALLBACK(bus_handler), this);

        _thread = std::jthread([this]() {
            spdlog::info("Run GStreamer stream thread");
            g_main_loop_run(_loop);
            spdlog::info("Quit GStreamer stream thread");
        });
    }
    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &) = delete;
    Stream(Stream &&stream) noexcept : _pipeline(stream._pipeline)
    {
        stream._pipeline = nullptr;
        stream._bus = nullptr;
    }
    Stream &operator=(Stream &&stream) noexcept
    {
        if (this == &stream) return *this;
        _pipeline = std::exchange(stream._pipeline, nullptr);
        return *this;
    }

    void start()
    {
        if (_pipeline) {
            gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_PLAYING);
        }
    }

    ~Stream()
    {
        if (_pipeline) {
            gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_NULL);
            gst_object_unref(_pipeline);
        }
        if (_bus) {
            gst_object_unref(_bus);
        }
        if (_loop) {
            g_main_loop_quit(_loop);
            g_main_loop_unref(_loop);
        }
    }
};

class CameraItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE set_name NOTIFY name_changed)
    Q_PROPERTY(QString cap READ cap WRITE set_cap NOTIFY cap_changed)
    Q_PROPERTY(QString codec READ codec WRITE set_codec NOTIFY codec_changed)
    Q_PROPERTY(QVector<QString> caps READ caps NOTIFY caps_changed)
    Q_PROPERTY(QVector<QString> codecs READ codecs NOTIFY codecs_changed)

    Q_PROPERTY(QString xdaq_timestamp READ xdaq_timestamp NOTIFY metadata_changed)
    Q_PROPERTY(QString rhythm_timestamp READ rhythm_timestamp NOTIFY metadata_changed)
    Q_PROPERTY(QString ttl_in READ ttl_in NOTIFY metadata_changed)
    Q_PROPERTY(QString ttl_out READ ttl_out NOTIFY metadata_changed)

public:
    explicit CameraItem(Camera *camera, QObject *parent = nullptr);
    ~CameraItem();

    int id() const { return _camera->id(); };

    Q_INVOKABLE QString name() const { return QString::fromStdString(_camera->name()); };
    Q_INVOKABLE void set_name(const QString &name);

    Q_INVOKABLE QVector<QString> caps() const { return _caps; };
    Q_INVOKABLE QVector<QString> codecs() const { return _codecs; };

    Q_INVOKABLE bool cap_selectable(const QString &cap) const;
    Q_INVOKABLE bool codec_selectable(const QString &codec) const;

    Q_INVOKABLE QString cap() const { return _cap; };
    Q_INVOKABLE void set_cap(const QString &cap);

    Q_INVOKABLE QString codec() const { return _codec; };
    Q_INVOKABLE void set_codec(const QString &codec);

    QString xdaq_timestamp() const { return QString::number(_metadata.fpga_timestamp); };
    QString rhythm_timestamp() const { return QString::number(_metadata.rhythm_timestamp); };
    QString ttl_in() const { return QString::number(_metadata.ttl_in); };
    QString ttl_out() const { return QString::number(_metadata.ttl_out); };

    void update_metadata(const XDAQFrameData &metadata);

    void start_recording(RecorderSettings *settings);
    void stop_recording();

    int port() const { return _camera->port(); };
    std::unique_ptr<Stream> _stream;

signals:
    void name_changed();
    void cap_changed();
    void codec_changed();
    void caps_changed();
    void codecs_changed();
    void metadata_changed();

private:
    Camera *_camera;

    QHash<std::pair<QString, QString>, Camera::Cap> _quality_format;
    QVector<QString> _caps;
    QVector<QString> _codecs;
    QString _cap;
    QString _codec;
    XDAQFrameData _metadata;
};

#endif
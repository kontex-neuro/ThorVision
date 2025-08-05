#pragma once

#include <gst/app/gstappsink.h>
#include <gst/gst.h>

#include <QImage>
#include <QObject>

#include "ImageProvider.h"
#include "xdaqmetadata/metadata_handler.h"
#include "xdaqvc/camera.h"

class GstVideoSink : public QObject
{
    Q_OBJECT

public:
    explicit GstVideoSink(QObject *parent = nullptr);
    GstVideoSink(Camera *camera, QObject *parent = nullptr);
    ~GstVideoSink() override;

    void start_pipeline();
    void set_image_provider(ImageProvider *provider) { _provider = provider; };
    GstElement *pipeline() const { return _pipeline; }

private:
    GstElement *_pipeline;
    ImageProvider *_provider;
    Camera *_camera;
    MetadataHandler *_metadata_handler;

    static GstFlowReturn on_new_sample_static(GstAppSink *sink, gpointer user_data)
    {
        return static_cast<GstVideoSink *>(user_data)->on_new_sample(sink);
    };
    GstFlowReturn on_new_sample(GstAppSink *sink);
};
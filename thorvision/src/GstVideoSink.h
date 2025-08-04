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

    void startPipeline();
    void setImageProvider(ImageProvider *provider);
    GstElement *pipeline() const { return _pipeline; }

private:
    GstElement *_pipeline;
    ImageProvider *_provider;
    Camera *_camera;
    MetadataHandler *_metadata_handler;

    static GstFlowReturn onNewSampleStatic(GstAppSink *sink, gpointer user_data);
    GstFlowReturn onNewSample(GstAppSink *sink);
};
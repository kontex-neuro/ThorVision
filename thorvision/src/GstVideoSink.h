#pragma once

#include <gst/app/gstappsink.h>
#include <gst/gst.h>

#include <QImage>
#include <QObject>

#include "ImageProvider.h"

class GstVideoSink : public QObject
{
    Q_OBJECT

public:
    explicit GstVideoSink(QObject *parent = nullptr);
    ~GstVideoSink() override;
    void startPipeline();

    void setImageProvider(ImageProvider *provider);

signals:
    void newImageReady(const QImage &image);
    // void imageUpdated();

private:
    GstElement *pipeline;
    ImageProvider *m_provider;

    static GstFlowReturn onNewSampleStatic(GstAppSink *sink, gpointer user_data);
    GstFlowReturn onNewSample(GstAppSink *sink);
};
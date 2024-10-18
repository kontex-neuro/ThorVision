#pragma once

#include <gst/gstelement.h>
#include <gst/gstpipeline.h>

#include <string>

#include "src/portpool.h"


namespace xvc
{

void setup_h265_srt_stream(GstPipeline *pipeline, const std::string &uri);
void setup_jpeg_srt_stream(GstPipeline *pipeline, const std::string &uri);
void mock_high_frame_rate(GstPipeline *pipeline, const std::string &uri);

void start_h265_recording(GstPipeline *pipeline, std::string &filepath);
void stop_h265_recording(GstPipeline *pipeline);

void start_jpeg_recording(GstPipeline *pipeline, std::string &filepath);
void stop_jpeg_recording(GstPipeline *pipeline);

void open_video(GstPipeline *pipeline, const std::string &filepath);

// void video_control(std::string filename);

inline std::unique_ptr<PortPool> port_pool;

}  // namespace xvc
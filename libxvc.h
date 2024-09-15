#pragma once

#include <gst/gst.h>
#include <gst/gstelement.h>
#include <gst/gstpipeline.h>

#include <vector>

#include "src/safedeque.h"
#include "src/portpool.h"


namespace xvc
{

struct Camera {
};

// std::vector<Camera> list_cameras(std::string server_ip);
std::string list_cameras(std::string url);
// auto *open_video_stream(std::string filename, Camera camera, std::string camera_settings);

void open_video_stream(GstPipeline *pipeline, std::string ip);
void record_video(GstPipeline *pipeline, std::string filename);
void parse_video(GstPipeline *pipeline, std::string filename);

void start_stream(int id, std::string camera_cap, int port);
void stop_stream(int id);

void start_recording(GstPipeline *pipeline, std::string filename);
void stop_recording(GstPipeline *pipeline);

void change_camera_status(int id, std::string status);

// void video_control(std::string filename);

// auto get_xdaq_timestamp(const video_frame &frame);
// auto get_xdaq_timestamp(void *frame);

inline PortPool *port_pool;

}  // namespace xvc
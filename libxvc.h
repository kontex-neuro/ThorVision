#pragma once

#include <gst/gstelement.h>
#include <gst/gstpipeline.h>

#include <filesystem>
#include <string>
#include <vector>

#include "src/portpool.h"


namespace fs = std::filesystem;

namespace xvc
{

void setup_h265_srt_stream(GstPipeline *pipeline, const int port);
void setup_jpeg_srt_stream(GstPipeline *pipeline, const std::string &uri);
void mock_high_frame_rate(GstPipeline *pipeline, const std::string &uri);

// void start_h265_recording(GstPipeline *pipeline, std::string &filepath);
void start_h265_recording(GstPipeline *pipeline, fs::path &filepath);
void stop_h265_recording(GstPipeline *pipeline);

void start_jpeg_recording(GstPipeline *pipeline, fs::path &filepath);
void stop_jpeg_recording(GstPipeline *pipeline);

void parse_video_save_binary_h265(std::string &video_filepath);
void parse_video_save_binary_jpeg(std::string &video_filepath);

inline std::unique_ptr<PortPool> port_pool;

inline GstBuffer *last_i_frame_buffer;
inline std::vector<GstBuffer *> last_frame_buffer;

}  // namespace xvc
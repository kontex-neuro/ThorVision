#pragma once
#ifndef __XDAQ_METADATA_H__
#define __XDAQ_METADATA_H__

// #define DllExport __declspec( dllexport )

// #if defined(_WIN32)
// #  if defined(METADATA_EXPORT)
// #    define METADATA_API __declspec(dllexport)
// #  else
// #    define METADATA_API __declspec(dllimport)
// #  endif
// #else // non windows
// #  define METADATA_API
// #endif

#include <gst/gstpad.h>

#include <span>

typedef struct _XDAQFrameData XDAQFrameData;

#pragma pack(push, 1)
struct _XDAQFrameData {
    std::uint64_t fpga_timestamp;
    std::uint32_t rhythm_timestamp;
    std::uint32_t ttl_in;
    std::uint32_t ttl_out;

    std::uint32_t spi_perf_counter;
    std::uint64_t reserved;
};
#pragma pack(pop)
// const XDAQFrameData_default = {0, 0, 0, 0, 0, 0};

GstPadProbeReturn parse_h265_metadata(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
// GstPadProbeReturn parse_jpeg_metadata(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);

XDAQFrameData get_h265_metadata(std::span<unsigned char> data);
// XDAQFrameData get_jpeg_metadata(GstBuffer *buffer);

// void assign_pts(std::uint64_t pts);

#endif
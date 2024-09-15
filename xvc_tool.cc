#include <fmt/core.h>
#include <gst/gst.h>
#include <gst/gstelement.h>
#include <gst/gstmacos.h>
#include <gst/gstobject.h>
#include <gst/gstpipeline.h>
#include <spdlog/spdlog.h>

#include <boost/program_options.hpp>
#include <iostream>
#include <string>

#include "libxvc.h"

int main(int argc, char *argv[])
{
    namespace po = boost::program_options;
    po::options_description desc("Usage");

    // clang-format off
    desc.add_options()
        ("help,h", "Show help options")
        ("file,f", po::value<std::string>(), "file to write or verify")
        ("list,l", po::bool_switch()->default_value(false), "Print cameras and their settings")
        ("url,u", po::value<std::string>(), "Input URL ex. 192.168.50.241:8000/fetchcams")
        // ("list,l", po::value<std::string>(), "Print cameras and their settings")
        ("open,o", "Open a new stream")
        ("save,s", "Save as video file")
        ("parse,p", "Parse a video file")
        // ("reboot", po::bool_switch()->default_value(false), "Reboot device to target mode and exit")
        // ("index,i", po::value<int>()->default_value(0), "index of device to open")
    ;
    // clang-format on

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        desc.print(std::cout);
        return 0;
    }

    if (vm.count("list")) {
        auto cameras = xvc::list_cameras(vm["url"].as<std::string>());
        fmt::println("{}", cameras);
        return 0;
    }

    if (vm.count("open")) {
        gst_init(&argc, &argv);
        auto pipeline = gst_pipeline_new(NULL);
        xvc::open_video_stream(GST_PIPELINE(pipeline));
        // #if defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IPHONE
        // return gst_macos_main((GstMainFunc) xvc::open_video_stream, argc, argv, NULL);
        // #else
        //     xvc::open_video_stream(GST_PIPELINE(pipeline));
        // #endif
        gst_object_unref(pipeline);
        gst_deinit();
        return 0;
    }

    if (vm.count("save")) {
        std::string filename = vm["file"].as<std::string>();

        gst_init(&argc, &argv);
        auto pipeline = gst_pipeline_new(NULL);
        xvc::save_video_file(GST_PIPELINE(pipeline), filename);
        gst_object_unref(pipeline);
        gst_deinit();
        return 0;
    }



    return 1;
}
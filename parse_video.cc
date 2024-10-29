#include <fmt/core.h>
#include <glib-object.h>
#include <glib.h>
#include <glibconfig.h>
#include <gst/app/gstappsink.h>

#define GST_USE_UNSTABLE_API
#include <gst/codecparsers/gsth265parser.h>
#include <gst/gst.h>
#include <gst/gstbin.h>
#include <gst/gstbuffer.h>
#include <gst/gstcaps.h>
#include <gst/gstelement.h>
#include <gst/gstelementfactory.h>
#include <gst/gstevent.h>
#include <gst/gstinfo.h>
#include <gst/gstmeta.h>
#include <gst/gstobject.h>
#include <gst/gstpad.h>
#include <gst/gstparse.h>
#include <gst/gstpipeline.h>
#include <gst/gststructure.h>
#include <gst/gstutils.h>
#include <gst/video/video-info.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string>

#include "key_value_store.h"
#include "src/safedeque.h"

#include <spdlog/spdlog.h>
#include <CLI/CLI.hpp>
#include <chrono>  // Add this include at the top with other includes
#include <filesystem>  // Add this for file operations

#pragma pack(push, 1)
struct PtsMetadata {
    uint64_t pts;
    XDAQFrameData metadata;
};
#pragma pack(pop)

struct ProgramOptions {
    bool verbose = false;
    std::string video_filepath;
};

void parse_video_save_binary(std::string &video_filepath)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    std::string bin_file_name = video_filepath;
    bin_file_name.replace(bin_file_name.end() - 3, bin_file_name.end(), "bin");
    
    // Delete the file if it exists
    if (std::filesystem::exists(bin_file_name)) {
        std::filesystem::remove(bin_file_name);
        spdlog::info("Successfully deleted temporary file: {}", bin_file_name);
    }
    struct UserData
    {
        int parsed_frame_count = 0;
        int parse_pts_count = 0;
        KeyValueStore bin_store;

        UserData(std::string filename): bin_store(filename) {}
    };

    spdlog::info("bin_file_name: {}", bin_file_name);

    UserData user_data(bin_file_name);
    user_data.bin_store.openFile();

    // Define the pipeline string
    std::string pipeline_str = " filesrc location=\"" + video_filepath +
                               "\" "
                               " ! matroskademux "
                               " ! h265parse name=h265parse "
                               " ! video/x-h265, stream-format=byte-stream, alignment=au "
                               " ! fakesink ";
    spdlog::debug("pipeline_str: {}", pipeline_str);
    spdlog::info("pipeline start!");

    GError *error = NULL;
    spdlog::info("pipeline start!");
    GstElement *pipeline = gst_parse_launch(pipeline_str.c_str(), &error);


    if (!pipeline) {
        spdlog::error("Failed to create pipeline: {}", error->message);
        g_clear_error(&error);
        return;
    }

    // parse pts : h265parse src
    std::unique_ptr<GstElement, decltype(&gst_object_unref)> h265parse{
        gst_bin_get_by_name(GST_BIN(pipeline), "h265parse"), gst_object_unref
    };
    if (h265parse.get() == nullptr) {
        spdlog::error("Failed to get h265parse element");
        return;
    } else {

        std::unique_ptr<GstPad, decltype(&gst_object_unref)> pad{
            gst_element_get_static_pad(h265parse.get(), "src"), gst_object_unref
        };
        if (pad.get() != nullptr) {
            gst_pad_add_probe(
                pad.get(),
                GST_PAD_PROBE_TYPE_BUFFER,
                [](GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
                    UserData *data = (UserData*)user_data;
                    data->parse_pts_count++;
                    spdlog::debug(
                        "-----------------------------parse_pts------------------------------------"
                        "--\n"
                    );
                    auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
                    // std::cout << "buffer: " << buffer << std::endl;
                    //  buffer = gst_buffer_make_writable(buffer);

                    if (buffer == NULL) {
                        spdlog::error("************************empty buffer*****************************");
                        return GST_PAD_PROBE_OK;
                    }
                    GstMapInfo map_info;
                    // if(!gst_buffer_map(buffer, &map_info, GST_MAP_READ))
                    // {
                    //     printf("gst_buffer_map failed!\n");
                    // }


                    spdlog::debug("buffer_n_memory: {}", gst_buffer_n_memory(buffer));
                    spdlog::debug("total memory_size: {}", gst_buffer_get_size(buffer));
                    spdlog::debug("buffer pts: {}", GST_BUFFER_PTS(buffer));

                    // std::cout << "buffer: " << buffer << std::endl;
                    // std::cout << "buffer offset diff: " << (buffer->offset_end - buffer->offset)
                    // << std::endl; for (int j = 0; j < gst_buffer_n_memory(buffer); ++j) {
                    //     GstMemory *mem = gst_buffer_get_memory(buffer, j);
                    //     GstMapInfo mem_map;
                    //     gst_memory_map(mem, &mem_map, GST_MAP_READ);
                    //     for (size_t i = 0; i < std::clamp<int>(mem_map.size, 0, 64); i++) {
                    //         printf(" %02x", mem_map.data[i]);
                    //         if ((i + 1) % 16 == 0) {
                    //             printf(" ");
                    //         }
                    //     }
                    //     printf("\n");
                    // }
                    // printf("\n");

                    // 创建一个nalu解析器
                    GstH265Parser *nalu_parser = gst_h265_parser_new();
                    // GstMapInfo map_info;

                    GstH265NalUnit nalu;
                    for (int k = 0; k < gst_buffer_n_memory(buffer); ++k) {
                        GstMemory *mem_in_buffer = gst_buffer_get_memory(buffer, k);
                        // GstMapInfo mem_map;
                        gst_memory_map(mem_in_buffer, &map_info, GST_MAP_READ);


                        // GstH265ParserResult parse_result =
                        // gst_h265_parser_identify_nalu(nalu_parser, map_info.data, 0,
                        // map_info.size, &nalu);
                        GstH265ParserResult parse_result = gst_h265_parser_identify_nalu_unchecked(
                            nalu_parser, map_info.data, 0, map_info.size, &nalu
                        );


                        // GstH265Parser *nalu_parser = gst_h265_parser_new();
                        // GstH265NalUnit nalu;
                        // GstH265ParserResult parse_result = gst_h265_parser_parse_nal(nalu_parser,
                        // &nalu); printf("type: %d, layer-id: %d, id2: %d\n", nalu.type,
                        // nalu.layer_id, nalu.temporal_id_plus1);
                        //  GstH265ParserResult parse_result =
                        //  gst_h265_parser_identify_nalu(nalu_parser, map_info.data, 0,
                        //  map_info.size, &nalu);


                        spdlog::debug("parse_result: {}", static_cast<int>(parse_result));
                        if(spdlog::get_level() <= spdlog::level::debug) {
                            for (int j = 0; j < gst_buffer_n_memory(buffer); ++j) {
                                GstMemory *mem2 = gst_buffer_get_memory(buffer, j);
                                GstMapInfo mem_map;
                                gst_memory_map(mem2, &mem_map, GST_MAP_READ);
                                spdlog::debug("buffer index: {}, map_size: {}", j, mem_map.size);
                                if (spdlog::get_level() <= spdlog::level::debug) {
                                    for (size_t i = 0; i < std::clamp<int>(mem_map.size, 0, 64); i++) {
                                        spdlog::debug("{:02x} ", mem_map.data[i]);
                                        if ((i + 1) % 16 == 0) {
                                            spdlog::debug(" ");
                                        }
                                    }
                                }
                                gst_memory_unmap(mem2, &mem_map);
                                gst_memory_unref(mem2);
                            }
                        }
                        spdlog::debug("\n");
                        if (parse_result == GST_H265_PARSER_OK ||
                            parse_result == GST_H265_PARSER_NO_NAL_END ||
                            parse_result == GST_H265_PARSER_NO_NAL)  // GST_H265_PARSER_NO_NAL_END)
                        {
                            spdlog::debug("type: {}, size: {}", nalu.type, nalu.size);
                            if (nalu.type == GST_H265_NAL_SLICE_CRA_NUT ||
                                nalu.type == GST_H265_NAL_VPS || nalu.type == GST_H265_NAL_SPS ||
                                nalu.type == GST_H265_NAL_PPS ||
                                nalu.type == GST_H265_NAL_PREFIX_SEI ||
                                nalu.type ==
                                    GST_H265_NAL_SUFFIX_SEI)  // 在I帧或P帧前面插SEI,也可以在其他位置插入，根据nalu.type判断
                            {
                                if (nalu.type == GST_H265_NAL_SLICE_CRA_NUT) {
                                    GstMemory *mem2 = gst_buffer_get_memory(buffer, 0);
                                    GstMapInfo mem_map;
                                    gst_memory_map(mem2, &mem_map, GST_MAP_READ);
                                    for (size_t i = 0; i < 64; i++) {
                                        spdlog::debug(" {:02x}", mem_map.data[nalu.size + i]);
                                        if ((i + 1) % 16 == 0) {
                                            spdlog::debug(" ");
                                        }
                                    }
                                    spdlog::debug("\n");

                                    char a[] = {0x00, 0x00, 0x01, 0x4e};

                                    // auto it = std::search(
                                    // mem_map.data, mem_map.data+nalu.size,
                                    // std::begin(a), std::end(a));

                                    // if (it == mem_map.data+nalu.size)
                                    // {
                                    //     printf("subrange not found\n");
                                    //     // not found
                                    // }
                                    // else
                                    // {
                                    //     printf("subrange found at
                                    //     std::distance(std::begin(Buffer), it): %lu\n",
                                    //     std::distance(mem_map.data, it));
                                    //     // subrange found at std::distance(std::begin(Buffer),
                                    //     it)
                                    // }

                                    std::string needle("\x00\x00\x01\x4e", 4);
                                    // std::string needle("\x01\x2a\x01\xaf");
                                    spdlog::debug(
                                        "needle: {}, len: {}", needle.c_str(), needle.length()
                                    );
                                    std::string haystack(
                                        mem_map.data,
                                        mem_map.data + nalu.size
                                    );  // or "+ sizeof Buffer"

                                    std::size_t n = haystack.find(needle);

                                    if (n == std::string::npos) {
                                        spdlog::debug("subrange not found\n");
                                        // not found
                                    } else {
                                        spdlog::debug(
                                            "subrange found at std::distance(std::begin(Buffer), "
                                            "it): {}",
                                            n
                                        );
                                        // position is n
                                    }
                                    gst_memory_unmap(mem2, &mem_map);
                                    gst_memory_unref(mem2);
                                }
                                // parsing sei by adding offset of vps size
                                if (nalu.type == GST_H265_NAL_VPS) {
                                    size_t vps_size =
                                        76;  // Extract size accordingly, found by trial and error

                                    gst_h265_parser_identify_nalu_unchecked(
                                        nalu_parser,
                                        map_info.data,
                                        vps_size,
                                        map_info.size - vps_size,
                                        &nalu
                                    );
                                }
                                GArray *array =
                                    g_array_new(false, false, sizeof(GstH265SEIMessage));
                                spdlog::debug(sizeof(GstH265SEIMessage));
                                spdlog::debug("before parse SEI");
                                spdlog::debug("1");
                                gst_h265_parser_parse_sei(nalu_parser, &nalu, &array);
                                spdlog::debug("2");
                                GstH265SEIMessage sei_msg =
                                    g_array_index(array, GstH265SEIMessage, 0);
                                spdlog::debug("3");
                                GstH265RegisteredUserData register_user_data =
                                    sei_msg.payload.registered_user_data;
                                spdlog::debug("4");
                                std::string str(
                                    (const char *) register_user_data.data, register_user_data.size
                                );

                                PtsMetadata pts_metadata;
                                std::memcpy(
                                    &pts_metadata, register_user_data.data, register_user_data.size
                                );
                                spdlog::debug("pts in pts_metadata: {:x}", pts_metadata.pts);
                                spdlog::debug("register_user_data.size: {}", register_user_data.size);
                                // std::cout << "\nSEI: " << std::hex << (uint8_t*)str.c_str() << "\n\n";
                                
                                g_array_free(array, true);

                                spdlog::debug("stop1");

                                XDAQFrameData metadata;
                                std::memcpy(
                                    &metadata,
                                    &(pts_metadata.metadata),
                                    sizeof(pts_metadata.metadata)
                                );
                                spdlog::debug("stop2");

                                data->bin_store.appendKeyValuePair(KeyValuePair(
                                    pts_metadata.pts, (const char *) &(pts_metadata.metadata)
                                ));
                                spdlog::debug("stop3");
                                data->parsed_frame_count++;
                                // std::string delimiter = ".";
                                // std::string token = video_name.substr(0,
                                // video_name.find(delimiter));

                                // std::string csv_file_name = token + ".csv";
                                // write data to file
                                // if(wrtie_file_with_data(csv_file_name, str + "\n"))
                                //     std::cout << "-----------------write timestamp_output.csv
                                //     success!" << std::endl;
                                // else
                                //     std::cout << "-----------------wirte timestamp_output.csv
                                //     fail!"
                                //     << std::endl;


                                // std::cout << "buffer_n_memory: " << gst_buffer_n_memory(buffer)
                                // << std::endl; std::cout << "total memory_size: " <<
                                // gst_buffer_get_size(buffer) << std::endl;

                                // std::cout << "buffer offset : " << (buffer->offset) << std::endl;
                                // std::cout << "buffer_end offset : " << (buffer->offset_end) <<
                                // std::endl; std::cout << "buffer offset diff: " <<
                                // (buffer->offset_end
                                // - buffer->offset) << std::endl; for (int j = 0; j <
                                // gst_buffer_n_memory(buffer); ++j) {
                                //     GstMemory *mem = gst_buffer_get_memory(buffer, j);
                                //     GstMapInfo mem_map;
                                //     gst_memory_map(mem, &mem_map, GST_MAP_READ);
                                //     for (size_t i = 0; i < std::clamp<int>(mem_map.size, 0, 64);
                                //     i++)
                                //     {
                                //         printf(" %02x", mem_map.data[i]);
                                //         if ((i + 1) % 16 == 0) {
                                //             printf(" ");
                                //         }
                                //     }
                                //     printf("\n");
                                // }
                                // printf("\n");
                                spdlog::debug("OOOOOOOOOOOOOOOOOOOOO SEI OOOOOOOOOOOOOOOOOOOOOOOOOO");
                            }
                        }
                    }
                    // 释放nalu解析器
                    gst_h265_parser_free(nalu_parser);


                    return GST_PAD_PROBE_OK;
                },
                &user_data,
                NULL
            );
        }
    }

    // Start playing the pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // Event loop to keep the pipeline running
    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg;
    bool terminate = false;

    while (!terminate) {
        // Wait for a message for up to 100 milliseconds
        msg = gst_bus_timed_pop_filtered(
            bus, 100 * GST_MSECOND, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)
        );

        // Handle errors and EOS messages
        if (msg != NULL) {
            GError *err;
            gchar *debug_info;

            switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg, &err, &debug_info);
                spdlog::error("Error from element {}: {}", GST_OBJECT_NAME(msg->src), err->message);
                g_clear_error(&err);
                g_free(debug_info);
                terminate = true;
                break;
            case GST_MESSAGE_EOS:
                spdlog::info("End-Of-Stream reached.");
                terminate = true;
                break;
            default: break;
            }
            gst_message_unref(msg);
        }
    }

    // Clean up and shutdown the pipeline
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    user_data.bin_store.closeFile();
    spdlog::info("parsed_frame_count: {}", user_data.parsed_frame_count);
    spdlog::info("parse_pts_count: {}", user_data.parse_pts_count);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    spdlog::info("Processing time: {} ms", duration.count());

    
}

int main(int argc, char* argv[])
{
    CLI::App app{"Video parser with H265 SEI extraction"};
    ProgramOptions options;
    
    app.add_flag("-v,--verbose", options.verbose, "Enable verbose logging");
    app.add_option("video", options.video_filepath, "Input video file path")->required();
    
    CLI11_PARSE(app, argc, argv);

    // Configure logging
    if (options.verbose) {
        spdlog::set_level(spdlog::level::debug);
    } else {
        spdlog::set_level(spdlog::level::info);
    }

    gst_init(&argc, &argv);
    parse_video_save_binary(options.video_filepath);
    return 0;
}

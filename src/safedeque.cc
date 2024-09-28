#include "safedeque.h"

#include <fmt/format.h>
#include <gst/gstinfo.h>

namespace SafeDeque
{
SafeDeque::SafeDeque()
    : push_cnt(0),
      pop_cnt(0),
      already_empty_cnt(0),
      pop_until_empty_cnt(0),
      frame_drop_cnt(0),
      timestamp_drop_cnt(0)
{
}

void SafeDeque::push(uint64_t pts, XDAQFrameData metadata)
{
    std::unique_lock<std::mutex> lck(mtx);
    dq.emplace_back(pts, metadata);
    ++push_cnt;
    // print_cnt();
}

std::optional<XDAQFrameData> SafeDeque::check_pts_pop_timestamp(uint64_t pts)
{
    std::lock_guard<std::mutex> lck(mtx);

    // if (dq.front().first > pts) {
    //     timestamp_drop_cnt++;
    //     print_cnt();
    //     return std::nullopt;
    // }

    if (dq.empty()) {
        already_empty_cnt++;
        // print_cnt();
        return std::nullopt;
    }

    while (dq.front().first < pts) {
        dq.pop_front();
        pop_cnt++;
        // frame_drop_cnt++;
        // print_cnt();
    }

    if (dq.front().first == pts) {
        auto metadata = dq.front().second;
        // auto now = std::chrono::high_resolution_clock::now();
        // auto now_ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
        // auto epoch = now_ns.time_since_epoch();
        // uint64_t now_timestamp =
        // std::chrono::duration_cast<std::chrono::nanoseconds>(epoch).count();
        dq.pop_front();
        pop_cnt++;
        // print_cnt();
        return metadata;
    }

    pop_until_empty_cnt++;
    // print_cnt();
    return std::nullopt;
}

void SafeDeque::clear()
{
    std::unique_lock<std::mutex> lck(mtx);
    dq.clear();
}

void SafeDeque::print_cnt()
{
    fmt::print(
        "push_cnt: {}\npop_cnt: {}\nalready_empty_cnt: {}\npop_until_empty_cnt: {}\n",
        push_cnt.load(),
        pop_cnt.load(),
        already_empty_cnt.load(),
        pop_until_empty_cnt.load()
    );
}
}  // namespace SafeDeque
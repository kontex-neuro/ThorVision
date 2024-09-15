#include "safedeque.h"

#include <fmt/format.h>
#include <gst/gstinfo.h>


namespace SafeDeque
{
SafeDeque::SafeDeque() {}

void SafeDeque::push(uint64_t pts, std::array<uint64_t, 4> timestamp)
{
    std::unique_lock<std::mutex> lck(mtx);
    dq.emplace_back(pts, timestamp);
    push_cnt++;
    // print_cnt();
}

std::array<uint64_t, 4> SafeDeque::check_pts_pop_timestamp(uint64_t pts)
{
    std::unique_lock<std::mutex> lck(mtx);

    if (dq.empty()) {
        already_empty_cnt++;
        // print_cnt();
        return std::array<uint64_t, 4> {0,0,0,0};
    }

    while (dq.front().first < pts) {
        dq.pop_front();
        pop_cnt++;
        // print_cnt();
    }

    if (dq.front().first == pts) {
        auto timestamp = dq.front().second;
        // long int now = static_cast<long int> (std::time(NULL));

        auto now = std::chrono::high_resolution_clock::now();
        auto now_ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
        auto epoch = now_ns.time_since_epoch();
        uint64_t now_timestamp =
            std::chrono::duration_cast<std::chrono::nanoseconds>(epoch).count();
        // auto duration = std::chrono::nanoseconds(now_timestamp - timestamp);
        // fmt::print("pipeline delay: {}\n", (now_timestamp - timestamp) / 1000000000.);


        dq.pop_front();
        pop_cnt++;
        // print_cnt();
        return timestamp;
    }

    pop_until_empty_cnt++;
    // print_cnt();
    return std::array<uint64_t, 4> {0,0,0,0};
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
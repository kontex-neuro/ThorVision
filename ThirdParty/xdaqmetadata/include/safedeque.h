#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <optional>

#include "xdaqmetadata.h"

namespace SafeDeque
{
using pts_metadata = std::pair<uint64_t, XDAQFrameData>;

class SafeDeque
{
public:
    SafeDeque();
    ~SafeDeque() = default;
    void push(uint64_t pts, XDAQFrameData metadata);
    std::optional<XDAQFrameData> check_pts_pop_timestamp(uint64_t pts);
    void clear();
    void print_cnt();
    std::mutex mtx;

private:
    std::deque<pts_metadata> dq;
    std::atomic<int> push_cnt;
    std::atomic<int> pop_cnt;
    std::atomic<int> already_empty_cnt;
    std::atomic<int> pop_until_empty_cnt;
    std::atomic<int> frame_drop_cnt;
    std::atomic<int> timestamp_drop_cnt;
};
}  // namespace SafeDeque

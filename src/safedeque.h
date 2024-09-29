#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <optional>

#pragma pack(push, 1)
struct XDAQFrameData {
    std::uint64_t fpga_timestamp;
    std::uint32_t rhythm_timestamp;
    std::uint32_t ttl_in;
    std::uint32_t ttl_out;

    std::uint32_t spi_perf_counter;
    std::uint64_t reserved;
} const XDAQFrameData_default = {0, 0, 0, 0, 0, 0};
#pragma pack(pop)


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

private:
    std::deque<pts_metadata> dq;
    std::mutex mtx;
    std::atomic<int> push_cnt;
    std::atomic<int> pop_cnt;
    std::atomic<int> already_empty_cnt;
    std::atomic<int> pop_until_empty_cnt;
    std::atomic<int> frame_drop_cnt;
    std::atomic<int> timestamp_drop_cnt;
};
}  // namespace SafeDeque

#pragma once

#include <array>
#include <atomic>
#include <deque>
#include <mutex>

namespace SafeDeque
{
using pts_timestamp = std::pair<uint64_t, std::array<uint64_t, 4> >;

class SafeDeque
{
public:
    SafeDeque();
    void push(uint64_t, std::array<uint64_t, 4>);
    std::array<uint64_t, 4> check_pts_pop_timestamp(uint64_t);
    void clear();
    void print_cnt();

private:
    std::deque<pts_timestamp> dq;
    std::mutex mtx;
    std::atomic<int> push_cnt;
    std::atomic<int> pop_cnt;
    std::atomic<int> already_empty_cnt;
    std::atomic<int> pop_until_empty_cnt;
};
}  // namespace SafeDeque
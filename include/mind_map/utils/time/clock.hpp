#pragma once

#include <chrono>

namespace mind_map::util_time {
    inline std::chrono::system_clock::time_point now() { return std::chrono::system_clock::now(); }
}

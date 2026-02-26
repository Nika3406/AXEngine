// src/core/Time.cpp
#include <chrono>
#include <cstdint>

namespace core {

// ============================================================================
// Time Utility Functions
// ============================================================================

// Get current time in microseconds
uint64_t getTimeMicroseconds() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
}

// Get current time in milliseconds
uint64_t getTimeMilliseconds() {
    return getTimeMicroseconds() / 1000;
}

// Get current time in seconds
double getTimeSeconds() {
    return static_cast<double>(getTimeMicroseconds()) / 1000000.0;
}

} // namespace core
#pragma once

#include "fars/types.hpp"
#include <functional>
#include <atomic>

namespace fars {

class MemoryMonitor {
public:
    MemoryMonitor();
    ~MemoryMonitor();

    // Non-copyable
    MemoryMonitor(const MemoryMonitor&) = delete;
    MemoryMonitor& operator=(const MemoryMonitor&) = delete;

    // Take a single sample of the current process
    MemorySample sample_once();

    // Run sampling for the given duration, calling on_sample after each reading
    void run(int duration_seconds, int interval_ms,
             const std::function<void(const MemorySample&)>& on_sample);

    std::uint64_t peak_rss() const { return peak_rss_; }

private:
    std::uint64_t peak_rss_ = 0;
};

// Platform-specific process RSS (bytes). Returns 0 on failure.
std::uint64_t get_process_rss_bytes();

// Platform-specific available system RAM (bytes). Returns 0 if unknown.
std::uint64_t get_available_ram_bytes();

} // namespace fars
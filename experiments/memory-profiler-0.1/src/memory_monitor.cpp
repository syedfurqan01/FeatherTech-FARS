#include "fars/memory_monitor.hpp"

#include <thread>
#include <chrono>

namespace fars {

MemoryMonitor::MemoryMonitor() = default;
MemoryMonitor::~MemoryMonitor() = default;

MemorySample MemoryMonitor::sample_once() {
    MemorySample s;
    s.timestamp = std::chrono::system_clock::now();
    s.rss_bytes = get_process_rss_bytes();
    if (s.rss_bytes > peak_rss_) {
        peak_rss_ = s.rss_bytes;
    }
    s.peak_rss_bytes = peak_rss_;

    auto avail = get_available_ram_bytes();
    // We do not know total here; ratio left 0 if unavailable
    (void)avail;
    s.available_ram_ratio = 0.0;
    return s;
}

void MemoryMonitor::run(int duration_seconds, int interval_ms,
                        const std::function<void(const MemorySample&)>& on_sample) {
    if (duration_seconds <= 0 || interval_ms <= 0) return;

    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(duration_seconds);
    while (std::chrono::steady_clock::now() < end) {
        auto sample = sample_once();
        if (on_sample) on_sample(sample);
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}

} // namespace fars
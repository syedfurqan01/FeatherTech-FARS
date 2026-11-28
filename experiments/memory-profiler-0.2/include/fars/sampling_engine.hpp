#pragma once
#include "fars/process_memory.hpp"
#include "fars/system_memory.hpp"
#include "fars/metric_value.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>
namespace fars {
struct MetricStats {
    std::uint64_t min_bytes=0, max_bytes=0; double avg_bytes=0; std::size_t success_count=0, fail_count=0;
    void add(const MetricValue& v) {
        if (!v.is_available()) { ++fail_count; return; }
        if (success_count==0) { min_bytes=max_bytes=v.value; avg_bytes=static_cast<double>(v.value); }
        else {
            if (v.value<min_bytes) min_bytes=v.value;
            if (v.value>max_bytes) max_bytes=v.value;
            avg_bytes += (static_cast<double>(v.value)-avg_bytes)/static_cast<double>(success_count+1);
        }
        ++success_count;
    }
};
struct SampleRecord {
    std::chrono::steady_clock::time_point timestamp;
    std::int64_t elapsed_ms=0, interval_ms=0, collect_overhead_us=0;
    ProcessMemorySnapshot process; SystemMemorySnapshot system; bool process_alive=true;
};
struct SamplingSummary {
    std::int32_t target_pid=0; std::size_t configured_duration_ms=0, configured_interval_ms=0;
    std::size_t total_samples=0, successful_samples=0, failed_samples=0, delayed_cycles=0;
    MetricStats vm_rss, vm_size, vm_hwm, vm_swap, pss, private_dirty;
    std::uint64_t window_peak_rss_bytes=0; bool window_peak_rss_valid=false;
    MetricStats mem_available;
    double avg_collect_overhead_us=0; std::int64_t max_collect_overhead_us=0, total_elapsed_ms=0;
    bool stopped_early=false; std::string stop_reason;
};
struct SamplingConfig {
    std::int32_t pid=0; std::size_t duration_ms=5000, interval_ms=200, max_history=256;
    static constexpr std::size_t kMinIntervalMs=10, kMaxIntervalMs=60000, kMinDurationMs=10, kMaxDurationMs=3600000;
    std::string validate() const {
        if (interval_ms<kMinIntervalMs) return "interval too small (min 10 ms)";
        if (interval_ms>kMaxIntervalMs) return "interval too large (max 60000 ms)";
        if (duration_ms<kMinDurationMs) return "duration too small (min 10 ms)";
        if (duration_ms>kMaxDurationMs) return "duration too large (max 3600000 ms)";
        if (pid<0) return "pid must be >= 0";
        return {};
    }
};
using SteadyClockNow = std::function<std::chrono::steady_clock::time_point()>;
using SleepFor = std::function<void(std::chrono::milliseconds)>;
inline SteadyClockNow default_clock() { return []{ return std::chrono::steady_clock::now(); }; }
inline SleepFor default_sleep() { return [](std::chrono::milliseconds d){ std::this_thread::sleep_for(d); }; }
class SamplingEngine {
public:
    SamplingEngine(ProcessMemoryCollector proc, SystemMemoryCollector sys,
                   SteadyClockNow clock=default_clock(), SleepFor sleep_fn=default_sleep());
    SamplingSummary run(const SamplingConfig& cfg, std::vector<SampleRecord>* history_out=nullptr);
    void request_stop();
private:
    ProcessMemoryCollector proc_; SystemMemoryCollector sys_;
    SteadyClockNow clock_; SleepFor sleep_; std::atomic<bool> stop_requested_{false};
};
} // namespace fars

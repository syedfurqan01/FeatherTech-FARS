#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

namespace fars {

struct MemorySample {
    std::chrono::system_clock::time_point timestamp;
    std::uint64_t rss_bytes = 0;       // Resident Set Size
    std::uint64_t peak_rss_bytes = 0;  // High-water mark observed so far
    double        available_ram_ratio = 0.0; // 0.0–1.0 if known
};

struct SystemInfo {
    std::string   platform;            // "linux", "android", "unknown"
    std::uint64_t total_ram_bytes = 0;
    std::uint64_t available_ram_bytes = 0;
    int           cpu_cores = 0;
    std::string   notes;
};

struct StorageBenchResult {
    double sequential_read_mib_s = 0.0;
    double sequential_write_mib_s = 0.0;
    std::size_t bytes_tested = 0;
    std::string path_used;
    std::string error;
};

struct ProfilerConfig {
    std::string output_json = "report.json";
    std::string output_csv  = "report.csv";
    int         sample_interval_ms = 200;
    int         duration_seconds = 5;
    bool        run_storage_bench = true;
    std::string storage_bench_path = ".";
    // Future: model path, llama.cpp integration flags (optional backend)
};

struct ProfilerReport {
    SystemInfo   system;
    ProfilerConfig config;
    std::vector<MemorySample> samples;
    std::uint64_t peak_rss_bytes = 0;
    std::uint64_t avg_rss_bytes = 0;
    StorageBenchResult storage;
    std::string  notes;
    std::string  profiler_version = "0.1.0";
};

} // namespace fars
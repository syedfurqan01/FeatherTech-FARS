#include "fars/report.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fars {

namespace {

std::string bytes_to_mib(std::uint64_t b) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << (static_cast<double>(b) / (1024.0 * 1024.0));
    return oss.str();
}

std::string iso_timestamp(const std::chrono::system_clock::time_point& tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

} // namespace

bool write_json_report(const ProfilerReport& report, const std::string& path) {
    std::ofstream out(path);
    if (!out) return false;

    out << std::fixed << std::setprecision(3);
    out << "{\n";
    out << "  \"profiler_version\": \"" << report.profiler_version << "\",\n";
    out << "  \"system\": {\n";
    out << "    \"platform\": \"" << report.system.platform << "\",\n";
    out << "    \"total_ram_bytes\": " << report.system.total_ram_bytes << ",\n";
    out << "    \"available_ram_bytes\": " << report.system.available_ram_bytes << ",\n";
    out << "    \"cpu_cores\": " << report.system.cpu_cores << ",\n";
    out << "    \"notes\": \"" << report.system.notes << "\"\n";
    out << "  },\n";
    out << "  \"config\": {\n";
    out << "    \"sample_interval_ms\": " << report.config.sample_interval_ms << ",\n";
    out << "    \"duration_seconds\": " << report.config.duration_seconds << ",\n";
    out << "    \"run_storage_bench\": " << (report.config.run_storage_bench ? "true" : "false") << "\n";
    out << "  },\n";
    out << "  \"peak_rss_bytes\": " << report.peak_rss_bytes << ",\n";
    out << "  \"avg_rss_bytes\": " << report.avg_rss_bytes << ",\n";
    out << "  \"storage\": {\n";
    out << "    \"sequential_read_mib_s\": " << report.storage.sequential_read_mib_s << ",\n";
    out << "    \"sequential_write_mib_s\": " << report.storage.sequential_write_mib_s << ",\n";
    out << "    \"bytes_tested\": " << report.storage.bytes_tested << ",\n";
    out << "    \"path_used\": \"" << report.storage.path_used << "\",\n";
    out << "    \"error\": \"" << report.storage.error << "\"\n";
    out << "  },\n";
    out << "  \"samples\": [\n";
    for (std::size_t i = 0; i < report.samples.size(); ++i) {
        const auto& s = report.samples[i];
        out << "    {\"timestamp\": \"" << iso_timestamp(s.timestamp)
            << "\", \"rss_bytes\": " << s.rss_bytes
            << ", \"peak_rss_bytes\": " << s.peak_rss_bytes << "}";
        if (i + 1 < report.samples.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";
    out << "  \"notes\": \"" << report.notes << "\"\n";
    out << "}\n";
    return true;
}

bool write_csv_report(const ProfilerReport& report, const std::string& path) {
    std::ofstream out(path);
    if (!out) return false;

    out << "timestamp,rss_bytes,peak_rss_bytes\n";
    for (const auto& s : report.samples) {
        out << iso_timestamp(s.timestamp) << ","
            << s.rss_bytes << ","
            << s.peak_rss_bytes << "\n";
    }
    return true;
}

void print_summary(const ProfilerReport& report) {
    std::cout << "=== FARS Memory Profiler 0.1 Summary ===\n";
    std::cout << "Platform      : " << report.system.platform << "\n";
    std::cout << "Total RAM     : " << bytes_to_mib(report.system.total_ram_bytes) << " MiB\n";
    std::cout << "Available RAM : " << bytes_to_mib(report.system.available_ram_bytes) << " MiB\n";
    std::cout << "CPU cores     : " << report.system.cpu_cores << "\n";
    std::cout << "Peak RSS      : " << bytes_to_mib(report.peak_rss_bytes) << " MiB\n";
    std::cout << "Avg RSS       : " << bytes_to_mib(report.avg_rss_bytes) << " MiB\n";
    std::cout << "Samples       : " << report.samples.size() << "\n";
    if (report.config.run_storage_bench) {
        std::cout << "Storage read  : " << report.storage.sequential_read_mib_s << " MiB/s\n";
        std::cout << "Storage write : " << report.storage.sequential_write_mib_s << " MiB/s\n";
        if (!report.storage.error.empty()) {
            std::cout << "Storage error : " << report.storage.error << "\n";
        }
    }
    std::cout << "Notes         : " << report.notes << "\n";
    std::cout << "========================================\n";
}

} // namespace fars
#include "fars/system_info.hpp"
#include "fars/memory_monitor.hpp"
#include "fars/storage_bench.hpp"
#include "fars/report.hpp"

#include <iostream>
#include <string>
#include <cstring>
#include <numeric>

static void print_help(const char* argv0) {
    std::cout << "FARS Memory Profiler 0.1\n\n"
              << "Usage: " << argv0 << " [options]\n\n"
              << "Options:\n"
              << "  --duration SECONDS     Sampling duration (default: 5)\n"
              << "  --interval MS          Sample interval in ms (default: 200)\n"
              << "  --json PATH            JSON report path (default: report.json)\n"
              << "  --csv PATH             CSV report path (default: report.csv)\n"
              << "  --no-storage           Skip storage benchmark\n"
              << "  --storage-dir DIR      Directory for storage bench (default: .)\n"
              << "  --help                 Show this help\n\n"
              << "This tool measures process memory and basic storage performance.\n"
              << "It does NOT download models and does NOT run inference yet.\n"
              << "llama.cpp integration is planned as an optional later backend.\n";
}

int main(int argc, char** argv) {
    fars::ProfilerConfig cfg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "--duration" && i + 1 < argc) {
            cfg.duration_seconds = std::stoi(argv[++i]);
        } else if (arg == "--interval" && i + 1 < argc) {
            cfg.sample_interval_ms = std::stoi(argv[++i]);
        } else if (arg == "--json" && i + 1 < argc) {
            cfg.output_json = argv[++i];
        } else if (arg == "--csv" && i + 1 < argc) {
            cfg.output_csv = argv[++i];
        } else if (arg == "--no-storage") {
            cfg.run_storage_bench = false;
        } else if (arg == "--storage-dir" && i + 1 < argc) {
            cfg.storage_bench_path = argv[++i];
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_help(argv[0]);
            return 1;
        }
    }

    fars::ProfilerReport report;
    report.config = cfg;
    report.system = fars::detect_system_info();
    report.notes = "Baseline memory + storage measurement only. No model loaded.";

    std::cout << "Detecting system...\n";
    std::cout << "Platform: " << report.system.platform
              << "  Total RAM: " << (report.system.total_ram_bytes / (1024*1024)) << " MiB\n";

    fars::MemoryMonitor monitor;

    std::cout << "Sampling memory for " << cfg.duration_seconds
              << "s (interval " << cfg.sample_interval_ms << " ms)...\n";

    monitor.run(cfg.duration_seconds, cfg.sample_interval_ms,
                [&](const fars::MemorySample& s) {
                    report.samples.push_back(s);
                });

    report.peak_rss_bytes = monitor.peak_rss();
    if (!report.samples.empty()) {
        std::uint64_t sum = 0;
        for (const auto& s : report.samples) sum += s.rss_bytes;
        report.avg_rss_bytes = sum / report.samples.size();
    }

    if (cfg.run_storage_bench) {
        std::cout << "Running storage benchmark...\n";
        report.storage = fars::run_storage_bench(cfg.storage_bench_path, 32); // 32 MiB default for mobile friendliness
    }

    fars::print_summary(report);

    if (!fars::write_json_report(report, cfg.output_json)) {
        std::cerr << "Warning: failed to write JSON report\n";
    } else {
        std::cout << "JSON report written to " << cfg.output_json << "\n";
    }

    if (!fars::write_csv_report(report, cfg.output_csv)) {
        std::cerr << "Warning: failed to write CSV report\n";
    } else {
        std::cout << "CSV report written to " << cfg.output_csv << "\n";
    }

    return 0;
}
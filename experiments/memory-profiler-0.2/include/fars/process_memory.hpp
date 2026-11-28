#pragma once
#include "fars/metric_value.hpp"
#include "fars/file_reader.hpp"
#include "fars/platform/proc_parser.hpp"
#include <cstdint>
#include <string>
namespace fars {
struct ProcessMemorySnapshot {
    std::int32_t pid = -1;
    bool alive = false;
    MetricValue vm_rss, vm_size, vm_hwm, vm_swap;
    MetricValue pss, private_clean, private_dirty, shared_clean, shared_dirty, smaps_rss;
    bool smaps_rollup_used = false;
    std::string notes;
};
struct ProcessCapabilities {
    bool status_readable = false;
    bool smaps_rollup_readable = false;
    std::string detail;
};
class ProcessMemoryCollector {
public:
    explicit ProcessMemoryCollector(FileReader reader = default_file_reader);
    ProcessCapabilities probe(std::int32_t pid) const;
    ProcessMemorySnapshot collect(std::int32_t pid) const;
    ProcessMemorySnapshot collect_self() const;
private:
    FileReader reader_;
};
bool process_exists(std::int32_t pid, const FileReader& reader = default_file_reader);
} // namespace fars

#pragma once
#include "fars/metric_value.hpp"
#include "fars/file_reader.hpp"
#include "fars/platform/proc_parser.hpp"
#include <cstdint>
#include <string>
namespace fars {

// Classification of a single process observation (status path).
// smaps_rollup failure does not change this if status was Ok.
enum class ProcessObserveKind : std::uint8_t {
    Ok = 0,              // status readable (process exists for this observer)
    NotFound,            // status missing / process gone / never existed
    PermissionDenied,    // status exists but access denied (not termination)
    TransientError       // I/O or unexpected read error; may recover
};

inline const char* to_string(ProcessObserveKind k) noexcept {
    switch (k) {
        case ProcessObserveKind::Ok: return "Ok";
        case ProcessObserveKind::NotFound: return "NotFound";
        case ProcessObserveKind::PermissionDenied: return "PermissionDenied";
        case ProcessObserveKind::TransientError: return "TransientError";
    }
    return "Unknown";
}

struct ProcessMemorySnapshot {
    std::int32_t pid = -1;
    bool alive = false;  // true only when observe_kind == Ok
    ProcessObserveKind observe_kind = ProcessObserveKind::NotFound;
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

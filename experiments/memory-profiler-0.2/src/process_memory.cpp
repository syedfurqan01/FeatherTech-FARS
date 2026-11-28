#include "fars/process_memory.hpp"
#include <sstream>
#include <unistd.h>
namespace fars {
namespace {
std::string proc_path(std::int32_t pid, const char* leaf) {
    std::ostringstream os;
    if (pid <= 0) os << "/proc/self/" << leaf;
    else os << "/proc/" << pid << "/" << leaf;
    return os.str();
}
} // namespace

ProcessMemoryCollector::ProcessMemoryCollector(FileReader r) : reader_(std::move(r)) {}

bool process_exists(std::int32_t pid, const FileReader& reader) {
    return reader(proc_path(pid, "status")).state == MetricState::Available;
}

ProcessCapabilities ProcessMemoryCollector::probe(std::int32_t pid) const {
    ProcessCapabilities c;
    c.status_readable = reader_(proc_path(pid, "status")).state == MetricState::Available;
    c.smaps_rollup_readable = reader_(proc_path(pid, "smaps_rollup")).state == MetricState::Available;
    return c;
}

ProcessMemorySnapshot ProcessMemoryCollector::collect(std::int32_t pid) const {
    ProcessMemorySnapshot s;
    s.pid = (pid <= 0) ? static_cast<std::int32_t>(::getpid()) : pid;

    auto status_res = reader_(proc_path(pid, "status"));

    if (status_res.state == MetricState::PermissionDenied) {
        s.alive = false;
        s.observe_kind = ProcessObserveKind::PermissionDenied;
        s.vm_rss = s.vm_size = s.vm_hwm = s.vm_swap =
            MetricValue::make_permission_denied(status_res.detail);
        s.notes = "status PermissionDenied (not treated as termination)";
        return s;
    }

    if (status_res.state == MetricState::ParseError) {
        s.alive = false;
        s.observe_kind = ProcessObserveKind::TransientError;
        s.vm_rss = s.vm_size = s.vm_hwm = s.vm_swap =
            MetricValue::make_parse_error(status_res.detail);
        s.notes = "status read/parse transient error";
        return s;
    }

    if (status_res.state == MetricState::Unavailable) {
        s.alive = false;
        s.observe_kind = ProcessObserveKind::NotFound;
        s.vm_rss = s.vm_size = s.vm_hwm = s.vm_swap =
            MetricValue::make_unavailable(
                status_res.detail.empty() ? "file not found" : status_res.detail);
        s.notes = "process not found or terminated";
        return s;
    }

    if (status_res.state != MetricState::Available) {
        s.alive = false;
        s.observe_kind = ProcessObserveKind::TransientError;
        s.vm_rss = s.vm_size = s.vm_hwm = s.vm_swap =
            MetricValue::make_parse_error(
                status_res.detail.empty() ? "unexpected status read failure" : status_res.detail);
        s.notes = "unexpected status failure (not treated as termination)";
        return s;
    }

    // File readable — parse before declaring Ok.
    // Required for Ok: VmRSS Available (bytes, including valid zero).
    // Optional: VmSize, VmHWM, VmSwap may be Unavailable or ParseError without
    // failing the sample; values are never invented as Available(0).
    auto st = parse_status(status_res.content);
    s.vm_rss = st.vm_rss;
    s.vm_size = st.vm_size;
    s.vm_hwm = st.vm_hwm;
    s.vm_swap = st.vm_swap;

    if (st.vm_rss.state == MetricState::ParseError) {
        s.alive = false;
        s.observe_kind = ProcessObserveKind::TransientError;
        s.notes = "malformed required field VmRSS";
        return s;
    }
    if (!st.vm_rss.is_available()) {
        // Missing required key in an otherwise readable status file
        s.alive = false;
        s.observe_kind = ProcessObserveKind::TransientError;
        s.notes = "missing required field VmRSS";
        return s;
    }

    s.alive = true;
    s.observe_kind = ProcessObserveKind::Ok;

    auto smaps_res = reader_(proc_path(pid, "smaps_rollup"));
    if (smaps_res.state == MetricState::Available) {
        auto sm = parse_smaps_rollup(smaps_res.content);
        s.pss = sm.pss;
        s.private_clean = sm.private_clean;
        s.private_dirty = sm.private_dirty;
        s.shared_clean = sm.shared_clean;
        s.shared_dirty = sm.shared_dirty;
        s.smaps_rss = sm.rss;
        s.smaps_rollup_used = true;
    } else if (smaps_res.state == MetricState::PermissionDenied) {
        s.pss = s.private_clean = s.private_dirty =
        s.shared_clean = s.shared_dirty = s.smaps_rss =
            MetricValue::make_permission_denied(smaps_res.detail);
        s.notes = "smaps_rollup PermissionDenied; status Ok";
    } else {
        s.pss = s.private_clean = s.private_dirty =
        s.shared_clean = s.shared_dirty = s.smaps_rss =
            MetricValue::make_unavailable(
                smaps_res.detail.empty() ? "smaps_rollup unavailable" : smaps_res.detail);
        s.notes = "smaps_rollup unavailable; status Ok";
    }
    return s;
}

ProcessMemorySnapshot ProcessMemoryCollector::collect_self() const { return collect(0); }

} // namespace fars

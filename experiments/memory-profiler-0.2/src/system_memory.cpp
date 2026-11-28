#include "fars/system_memory.hpp"
namespace fars {
SystemMemoryCollector::SystemMemoryCollector(FileReader r) : reader_(std::move(r)) {}
SystemMemorySnapshot SystemMemoryCollector::collect() const {
    SystemMemorySnapshot s;
    auto res = reader_("/proc/meminfo");
    if (res.state == MetricState::PermissionDenied) {
        s.mem_total = s.mem_free = s.mem_available = s.buffers =
        s.cached = s.swap_total = s.swap_free =
            MetricValue::make_permission_denied(res.detail);
        s.notes = "meminfo PermissionDenied";
        return s;
    }
    if (res.state != MetricState::Available) {
        s.mem_total = s.mem_free = s.mem_available = s.buffers =
        s.cached = s.swap_total = s.swap_free =
            MetricValue::make_unavailable(res.detail.empty() ? "unreadable" : res.detail);
        s.notes = "meminfo unavailable";
        return s;
    }
    auto m = parse_meminfo(res.content);
    s.mem_total = m.mem_total; s.mem_free = m.mem_free; s.mem_available = m.mem_available;
    s.buffers = m.buffers; s.cached = m.cached;
    s.swap_total = m.swap_total; s.swap_free = m.swap_free;
    return s;
}
} // namespace fars

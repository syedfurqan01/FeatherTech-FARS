#pragma once
#include "fars/metric_value.hpp"
#include <string_view>
namespace fars {
struct StatusMetrics { MetricValue vm_rss, vm_size, vm_hwm, vm_swap; };
struct MeminfoMetrics {
    MetricValue mem_total, mem_free, mem_available, buffers, cached, swap_total, swap_free;
};
struct SmapsRollupMetrics {
    MetricValue rss, pss, private_clean, private_dirty, shared_clean, shared_dirty;
};
StatusMetrics parse_status(std::string_view content);
MeminfoMetrics parse_meminfo(std::string_view content);
SmapsRollupMetrics parse_smaps_rollup(std::string_view content);
MetricValue parse_kb_token(std::string_view token);
MetricValue find_kb_field(std::string_view content, std::string_view key);
} // namespace fars

#pragma once
#include "fars/metric_value.hpp"
#include "fars/file_reader.hpp"
#include "fars/platform/proc_parser.hpp"
#include <string>
namespace fars {
struct SystemMemorySnapshot {
    MetricValue mem_total, mem_free, mem_available, buffers, cached, swap_total, swap_free;
    std::string notes;
};
class SystemMemoryCollector {
public:
    explicit SystemMemoryCollector(FileReader reader = default_file_reader);
    SystemMemorySnapshot collect() const;
private:
    FileReader reader_;
};
} // namespace fars

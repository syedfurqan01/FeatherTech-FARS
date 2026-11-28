#pragma once
#include "fars/metric_value.hpp"
#include <functional>
#include <string>
#include <string_view>
namespace fars {
struct FileReadResult {
    MetricState state = MetricState::Unavailable;
    std::string content;
    std::string detail;
};
using FileReader = std::function<FileReadResult(std::string_view path)>;
FileReadResult default_file_reader(std::string_view path);
} // namespace fars

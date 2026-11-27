#pragma once

#include "fars/types.hpp"
#include <string>

namespace fars {

bool write_json_report(const ProfilerReport& report, const std::string& path);
bool write_csv_report(const ProfilerReport& report, const std::string& path);

void print_summary(const ProfilerReport& report);

} // namespace fars
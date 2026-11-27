#pragma once

#include "fars/types.hpp"
#include <string>

namespace fars {

// Simple sequential read/write micro-benchmark.
// Creates a temporary file under `path` (directory).
StorageBenchResult run_storage_bench(const std::string& directory,
                                     std::size_t size_mib = 64);

} // namespace fars
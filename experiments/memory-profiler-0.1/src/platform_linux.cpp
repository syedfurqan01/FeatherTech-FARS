#include "fars/memory_monitor.hpp"
#include "fars/system_info.hpp"

#include <fstream>
#include <string>
#include <unistd.h>

#if defined(__linux__) && !defined(__ANDROID__)

namespace fars {

std::uint64_t get_process_rss_bytes() {
    // Prefer VmRSS from /proc/self/status (kB)
    std::ifstream status("/proc/self/status");
    if (!status) return 0;

    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            // Format: VmRSS:    1234 kB
            std::size_t pos = line.find_first_of("0123456789");
            if (pos == std::string::npos) return 0;
            long kb = std::stol(line.substr(pos));
            return static_cast<std::uint64_t>(kb) * 1024ULL;
        }
    }
    return 0;
}

std::uint64_t get_available_ram_bytes() {
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo) return 0;

    std::uint64_t mem_available_kb = 0;
    std::uint64_t mem_free_kb = 0;
    std::uint64_t buffers_kb = 0;
    std::uint64_t cached_kb = 0;

    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.rfind("MemAvailable:", 0) == 0) {
            std::size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) mem_available_kb = std::stoull(line.substr(pos));
        } else if (line.rfind("MemFree:", 0) == 0) {
            std::size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) mem_free_kb = std::stoull(line.substr(pos));
        } else if (line.rfind("Buffers:", 0) == 0) {
            std::size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) buffers_kb = std::stoull(line.substr(pos));
        } else if (line.rfind("Cached:", 0) == 0) {
            std::size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) cached_kb = std::stoull(line.substr(pos));
        }
    }

    if (mem_available_kb > 0) {
        return mem_available_kb * 1024ULL;
    }
    // Fallback approximation
    return (mem_free_kb + buffers_kb + cached_kb) * 1024ULL;
}

SystemInfo detect_system_info_linux() {
    SystemInfo info;
    info.platform = "linux";

    std::ifstream meminfo("/proc/meminfo");
    if (meminfo) {
        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.rfind("MemTotal:", 0) == 0) {
                std::size_t pos = line.find_first_of("0123456789");
                if (pos != std::string::npos) {
                    info.total_ram_bytes = std::stoull(line.substr(pos)) * 1024ULL;
                }
                break;
            }
        }
    }

    info.available_ram_bytes = get_available_ram_bytes();
    info.cpu_cores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    info.notes = "Linux /proc-based detection";
    return info;
}

} // namespace fars

#endif // __linux__ && !__ANDROID__
#include "fars/memory_monitor.hpp"
#include "fars/system_info.hpp"

#include <fstream>
#include <string>
#include <unistd.h>

// Android (including Termux) is still Linux under the hood.
// We share the same /proc approach but tag the platform differently
// and add light Android-specific notes when possible.

#if defined(__ANDROID__)

namespace fars {

std::uint64_t get_process_rss_bytes() {
    std::ifstream status("/proc/self/status");
    if (!status) return 0;

    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
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
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.rfind("MemAvailable:", 0) == 0) {
            std::size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) {
                mem_available_kb = std::stoull(line.substr(pos));
                break;
            }
        }
    }
    return mem_available_kb * 1024ULL;
}

SystemInfo detect_system_info_android() {
    SystemInfo info;
    info.platform = "android";

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
    info.notes = "Android/Termux via /proc (same as Linux kernel interface)";
    return info;
}

} // namespace fars

#endif // __ANDROID__
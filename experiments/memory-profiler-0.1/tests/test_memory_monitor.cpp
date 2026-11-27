#include "fars/memory_monitor.hpp"
#include "fars/system_info.hpp"

#include <iostream>
#include <cstdlib>

int main() {
    int failures = 0;

    // System info should return something sensible on Linux/Android
    auto info = fars::detect_system_info();
    if (info.platform != "linux" && info.platform != "android" && info.platform != "unknown") {
        std::cerr << "Unexpected platform string\n";
        ++failures;
    }
    if (info.cpu_cores <= 0) {
        std::cerr << "cpu_cores should be positive\n";
        ++failures;
    }

    // Process RSS should be non-zero for a running process on supported platforms
    auto rss = fars::get_process_rss_bytes();
#if defined(__linux__) || defined(__ANDROID__)
    if (rss == 0) {
        std::cerr << "get_process_rss_bytes returned 0 on Linux/Android\n";
        ++failures;
    }
#endif

    fars::MemoryMonitor mon;
    auto sample = mon.sample_once();
    if (sample.rss_bytes == 0 && (info.platform == "linux" || info.platform == "android")) {
        std::cerr << "sample_once returned zero RSS\n";
        ++failures;
    }
    if (mon.peak_rss() < sample.rss_bytes) {
        std::cerr << "peak_rss inconsistency\n";
        ++failures;
    }

    if (failures == 0) {
        std::cout << "All profiler unit tests passed.\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
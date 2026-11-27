#include "fars/system_info.hpp"

#include <unistd.h>

// Platform implementations are in platform_linux.cpp / platform_android.cpp
// and are conditionally compiled.

namespace fars {

// Forward declarations of platform helpers
#if defined(__ANDROID__)
SystemInfo detect_system_info_android();
#elif defined(__linux__)
SystemInfo detect_system_info_linux();
#endif

SystemInfo detect_system_info() {
#if defined(__ANDROID__)
    return detect_system_info_android();
#elif defined(__linux__)
    return detect_system_info_linux();
#else
    SystemInfo info;
    info.platform = "unknown";
    info.cpu_cores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    info.notes = "Unsupported platform; limited detection";
    return info;
#endif
}

} // namespace fars
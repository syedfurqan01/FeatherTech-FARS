#pragma once
#include <cstdint>
#include <string>
#include <string_view>
namespace fars {
enum class MetricState : std::uint8_t {
    Available = 0, Unavailable, NotCollected, PermissionDenied, ParseError
};
struct MetricValue {
    MetricState state = MetricState::NotCollected;
    std::uint64_t value = 0;
    std::string detail;
    constexpr bool is_available() const noexcept { return state == MetricState::Available; }
    constexpr std::uint64_t value_or_zero() const noexcept { return is_available() ? value : 0; }
    static MetricValue make_available(std::uint64_t v) {
        MetricValue m; m.state = MetricState::Available; m.value = v; return m;
    }
    static MetricValue make_unavailable(std::string_view why = {}) {
        MetricValue m; m.state = MetricState::Unavailable;
        if (!why.empty()) m.detail.assign(why); return m;
    }
    static MetricValue make_not_collected() {
        MetricValue m; m.state = MetricState::NotCollected; return m;
    }
    static MetricValue make_permission_denied(std::string_view why = {}) {
        MetricValue m; m.state = MetricState::PermissionDenied;
        if (!why.empty()) m.detail.assign(why); return m;
    }
    static MetricValue make_parse_error(std::string_view why = {}) {
        MetricValue m; m.state = MetricState::ParseError;
        if (!why.empty()) m.detail.assign(why); return m;
    }
};
inline const char* to_string(MetricState s) noexcept {
    switch (s) {
        case MetricState::Available: return "Available";
        case MetricState::Unavailable: return "Unavailable";
        case MetricState::NotCollected: return "NotCollected";
        case MetricState::PermissionDenied: return "PermissionDenied";
        case MetricState::ParseError: return "ParseError";
    }
    return "Unknown";
}
} // namespace fars

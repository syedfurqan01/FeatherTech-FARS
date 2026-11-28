#include "fars/platform/proc_parser.hpp"
#include <cctype>
#include <charconv>
#include <limits>
namespace fars {
namespace {
std::string_view ltrim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    return s;
}
std::string_view first_integer_token(std::string_view s) {
    s = ltrim(s);
    if (s.empty()) return {};
    std::size_t i = (s[0] == '+') ? 1 : 0, start = i;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    return (i == start) ? std::string_view{} : s.substr(start, i - start);
}
}
MetricValue parse_kb_token(std::string_view token) {
    token = ltrim(token);
    if (token.empty()) return MetricValue::make_parse_error("empty token");
    auto t = first_integer_token(token);
    if (t.empty()) return MetricValue::make_parse_error("no integer found");
    std::uint64_t kb = 0;
    auto [ptr, ec] = std::from_chars(t.data(), t.data() + t.size(), kb);
    if (ec != std::errc{} || ptr != t.data() + t.size())
        return MetricValue::make_parse_error("invalid integer");
    if (kb > std::numeric_limits<std::uint64_t>::max() / 1024ULL)
        return MetricValue::make_parse_error("kB value overflows bytes");
    return MetricValue::make_available(kb * 1024ULL);
}
MetricValue find_kb_field(std::string_view content, std::string_view key) {
    std::size_t pos = 0;
    while (pos < content.size()) {
        auto end = content.find('\n', pos);
        if (end == std::string_view::npos) end = content.size();
        auto line = content.substr(pos, end - pos);
        pos = end + 1;
        if (line.size() >= key.size() && line.substr(0, key.size()) == key)
            return parse_kb_token(line.substr(key.size()));
    }
    return MetricValue::make_unavailable();
}
StatusMetrics parse_status(std::string_view c) {
    StatusMetrics m;
    if (c.empty()) {
        m.vm_rss = m.vm_size = m.vm_hwm = m.vm_swap = MetricValue::make_unavailable("empty");
        return m;
    }
    m.vm_rss = find_kb_field(c, "VmRSS:");
    m.vm_size = find_kb_field(c, "VmSize:");
    m.vm_hwm = find_kb_field(c, "VmHWM:");
    m.vm_swap = find_kb_field(c, "VmSwap:");
    return m;
}
MeminfoMetrics parse_meminfo(std::string_view c) {
    MeminfoMetrics m;
    if (c.empty()) {
        m.mem_total = m.mem_free = m.mem_available = m.buffers =
        m.cached = m.swap_total = m.swap_free = MetricValue::make_unavailable("empty");
        return m;
    }
    m.mem_total = find_kb_field(c, "MemTotal:");
    m.mem_free = find_kb_field(c, "MemFree:");
    m.mem_available = find_kb_field(c, "MemAvailable:");
    m.buffers = find_kb_field(c, "Buffers:");
    m.cached = find_kb_field(c, "Cached:");
    m.swap_total = find_kb_field(c, "SwapTotal:");
    m.swap_free = find_kb_field(c, "SwapFree:");
    return m;
}
SmapsRollupMetrics parse_smaps_rollup(std::string_view c) {
    SmapsRollupMetrics m;
    if (c.empty()) {
        m.rss = m.pss = m.private_clean = m.private_dirty =
        m.shared_clean = m.shared_dirty = MetricValue::make_unavailable("empty");
        return m;
    }
    m.rss = find_kb_field(c, "Rss:");
    m.pss = find_kb_field(c, "Pss:");
    m.private_clean = find_kb_field(c, "Private_Clean:");
    m.private_dirty = find_kb_field(c, "Private_Dirty:");
    m.shared_clean = find_kb_field(c, "Shared_Clean:");
    m.shared_dirty = find_kb_field(c, "Shared_Dirty:");
    return m;
}
} // namespace fars

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
std::string_view rtrim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}
} // namespace

// Accept: [whitespace] [+] digits [whitespace] [kB] [whitespace]
// Reject: decimals, trailing garbage (e.g. 123garbage), empty, non-integer units.
MetricValue parse_kb_token(std::string_view token) {
    token = ltrim(token);
    if (token.empty()) return MetricValue::make_parse_error("empty token");

    std::size_t i = 0;
    if (token[i] == '+') ++i;
    if (i >= token.size() || !std::isdigit(static_cast<unsigned char>(token[i])))
        return MetricValue::make_parse_error("no integer found");

    const std::size_t num_start = i;
    while (i < token.size() && std::isdigit(static_cast<unsigned char>(token[i]))) ++i;
    // Reject decimal / fractional forms (e.g. 123.5)
    if (i < token.size() && token[i] == '.')
        return MetricValue::make_parse_error("non-integer numeric value");

    std::string_view num = token.substr(num_start, i - num_start);
    std::uint64_t kb = 0;
    auto [ptr, ec] = std::from_chars(num.data(), num.data() + num.size(), kb);
    if (ec != std::errc{} || ptr != num.data() + num.size())
        return MetricValue::make_parse_error("invalid integer");

    // Remainder after digits: only whitespace and optional trailing "kB"
    std::string_view rest = token.substr(i);
    rest = ltrim(rest);
    if (!rest.empty()) {
        if (rest.size() >= 2 && rest[0] == 'k' && rest[1] == 'B') {
            rest.remove_prefix(2);
            rest = ltrim(rest);
            if (!rest.empty())
                return MetricValue::make_parse_error("trailing garbage after unit");
        } else {
            return MetricValue::make_parse_error("trailing non-numeric characters");
        }
    }

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

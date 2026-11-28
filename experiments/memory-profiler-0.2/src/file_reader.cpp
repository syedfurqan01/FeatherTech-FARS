#include "fars/file_reader.hpp"
#include "fars/unique_fd.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <vector>

namespace fars {

FileReadResult default_file_reader(std::string_view path) {
    FileReadResult r;
    std::string p(path);

    UniqueFd fd(::open(p.c_str(), O_RDONLY | O_CLOEXEC));
    if (!fd) {
        if (errno == EACCES || errno == EPERM) {
            r.state = MetricState::PermissionDenied;
            r.detail = std::strerror(errno);
        } else if (errno == ENOENT) {
            r.state = MetricState::Unavailable;
            r.detail = "file not found";
        } else {
            r.state = MetricState::Unavailable;
            r.detail = std::strerror(errno);
        }
        return r;
    }

    std::vector<char> buf;
    char tmp[4096];
    for (;;) {
        ssize_t n = ::read(fd.get(), tmp, sizeof(tmp));
        if (n < 0) {
            if (errno == EINTR) continue;
            r.state = MetricState::ParseError;
            r.detail = std::strerror(errno);
            return r; // UniqueFd closes
        }
        if (n == 0) break;
        buf.insert(buf.end(), tmp, tmp + n);
    }

    r.state = MetricState::Available;
    r.content.assign(buf.begin(), buf.end());
    return r; // UniqueFd closes
}

} // namespace fars

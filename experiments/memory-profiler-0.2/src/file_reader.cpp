#include "fars/file_reader.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
namespace fars {
FileReadResult default_file_reader(std::string_view path) {
    FileReadResult r;
    std::string p(path);
    int fd = ::open(p.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        r.state = (errno == EACCES || errno == EPERM) ? MetricState::PermissionDenied
                                                      : MetricState::Unavailable;
        r.detail = (errno == ENOENT) ? "file not found" : std::strerror(errno);
        return r;
    }
    std::vector<char> buf;
    char tmp[4096];
    for (;;) {
        ssize_t n = ::read(fd, tmp, sizeof(tmp));
        if (n < 0) {
            if (errno == EINTR) continue;
            r.state = MetricState::ParseError;
            r.detail = std::strerror(errno);
            ::close(fd);
            return r;
        }
        if (n == 0) break;
        buf.insert(buf.end(), tmp, tmp + n);
    }
    ::close(fd);
    r.state = MetricState::Available;
    r.content.assign(buf.begin(), buf.end());
    return r;
}
} // namespace fars

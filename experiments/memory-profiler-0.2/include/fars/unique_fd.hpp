#pragma once

#include <unistd.h>
#include <utility>

namespace fars {

// Minimal RAII owner for a POSIX file descriptor.
// - Closes on destruction if still owning a non-negative fd.
// - Move-only; copy disabled to prevent double-close.
// - release() transfers ownership without closing.
// - reset() closes current (if any) and optionally takes a new fd.
class UniqueFd {
public:
    UniqueFd() noexcept = default;
    explicit UniqueFd(int fd) noexcept : fd_(fd) {}

    ~UniqueFd() { reset(); }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] explicit operator bool() const noexcept { return fd_ >= 0; }

    // Relinquish ownership without closing. Returns previous fd (or -1).
    int release() noexcept {
        int t = fd_;
        fd_ = -1;
        return t;
    }

    // Close if owning; optionally adopt new_fd.
    void reset(int new_fd = -1) noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = new_fd;
    }

private:
    int fd_ = -1;
};

} // namespace fars

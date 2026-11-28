#pragma once

#include <functional>
#include <unistd.h>
#include <utility>

namespace fars {

// Move-only RAII owner for a POSIX file descriptor.
// Closing is performed via an injectable closer (default: ::close) so
// StorageBenchmark can honor StorageIOHooks::close_fn.
class UniqueFd {
public:
    using CloseFn = std::function<void(int)>;

    UniqueFd() noexcept = default;

    explicit UniqueFd(int fd) : fd_(fd), closer_(default_closer()) {}

    UniqueFd(int fd, CloseFn closer) : fd_(fd), closer_(std::move(closer)) {
        if (!closer_) closer_ = default_closer();
    }

    ~UniqueFd() { reset(); }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept
        : fd_(other.fd_), closer_(std::move(other.closer_)) {
        other.fd_ = -1;
        other.closer_ = nullptr;
    }

    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            closer_ = std::move(other.closer_);
            other.fd_ = -1;
            other.closer_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] explicit operator bool() const noexcept { return fd_ >= 0; }

    // Transfer ownership without closing.
    int release() noexcept {
        int t = fd_;
        fd_ = -1;
        return t;
    }

    // Close current fd (if any), then optionally adopt new_fd with same closer.
    // Same-descriptor reset is a no-op (avoids close-then-reuse double-close).
    void reset(int new_fd = -1) noexcept {
        if (new_fd == fd_) return;
        if (fd_ >= 0) {
            CloseFn c = closer_ ? closer_ : default_closer();
            const int to_close = fd_;
            fd_ = -1; // clear before close to prevent re-entry double-close
            c(to_close);
        }
        fd_ = new_fd;
    }

private:
    static CloseFn default_closer() {
        return [](int fd) {
            if (fd >= 0) ::close(fd);
        };
    }

    int fd_ = -1;
    CloseFn closer_;
};

} // namespace fars

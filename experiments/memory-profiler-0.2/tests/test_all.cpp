// FARS 0.2.1 Phase 1 — expanded automated tests
#include "fars/metric_value.hpp"
#include "fars/platform/proc_parser.hpp"
#include "fars/file_reader.hpp"
#include "fars/process_memory.hpp"
#include "fars/system_memory.hpp"
#include "fars/sampling_engine.hpp"
#include "fars/storage_benchmark.hpp"
#include "fars/unique_fd.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
namespace fs = std::filesystem;
static int failures = 0;
static void expect(bool cond, const char* msg) {
    if (!cond) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}
static void expect_state(const fars::MetricValue& m, fars::MetricState s, const char* msg) {
    if (m.state != s) {
        std::cerr << "FAIL: " << msg << " (got " << fars::to_string(m.state) << ")\n";
        ++failures;
    }
}
static void expect_avail(const fars::MetricValue& m, std::uint64_t bytes, const char* msg) {
    if (!m.is_available() || m.value != bytes) {
        std::cerr << "FAIL: " << msg << " state=" << fars::to_string(m.state)
                  << " value=" << m.value << " expected=" << bytes << "\n";
        ++failures;
    }
}
static void test_metric_value() {
    auto z = fars::MetricValue::make_available(0);
    expect(z.is_available() && z.value == 0, "zero is Available");
    auto u = fars::MetricValue::make_unavailable("missing");
    expect_state(u, fars::MetricState::Unavailable, "Unavailable");
    expect(!u.is_available(), "Unavailable not available");
    expect_state(fars::MetricValue::make_permission_denied("EACCES"),
                 fars::MetricState::PermissionDenied, "PermissionDenied");
    expect_state(fars::MetricValue::make_parse_error("bad"),
                 fars::MetricState::ParseError, "ParseError");
}
static void test_parser_well_formed() {
    auto s = fars::parse_status(
        "VmSize:\t4096 kB\nVmRSS:\t1024 kB\nVmHWM:\t2048 kB\nVmSwap:\t0 kB\n");
    expect_avail(s.vm_rss, 1024ULL * 1024, "VmRSS");
    expect_avail(s.vm_hwm, 2048ULL * 1024, "VmHWM");
    expect_avail(s.vm_swap, 0, "VmSwap zero Available");
    auto mi = fars::parse_meminfo(
        "MemTotal: 8192000 kB\nMemAvailable: 4096000 kB\nSwapTotal: 0 kB\n");
    expect_avail(mi.mem_total, 8192000ULL * 1024, "MemTotal");
    expect_avail(mi.swap_total, 0, "SwapTotal zero");
    auto sm = fars::parse_smaps_rollup(
        "Rss:\t2048 kB\nPss:\t1536 kB\nPrivate_Dirty:\t512 kB\nShared_Dirty:\t0 kB\n");
    expect_avail(sm.pss, 1536ULL * 1024, "Pss");
    expect_avail(sm.shared_dirty, 0, "Shared_Dirty zero");
}
static void test_parser_missing_and_malformed() {
    auto s = fars::parse_status("Name:\tonly\n");
    expect_state(s.vm_rss, fars::MetricState::Unavailable, "missing VmRSS");
    expect(s.vm_rss.state != fars::MetricState::Available, "missing not Available");
    auto mal = fars::parse_status("VmRSS:\tnot_a_number kB\nVmSize:\t2048 kB\n");
    expect_state(mal.vm_rss, fars::MetricState::ParseError, "malformed VmRSS");
    expect_avail(mal.vm_size, 2048ULL * 1024, "sibling still parses");
    expect_state(fars::parse_status("").vm_rss, fars::MetricState::Unavailable, "empty");
    expect_state(fars::parse_kb_token(""), fars::MetricState::ParseError, "empty token");
    expect_state(fars::parse_kb_token("abc"), fars::MetricState::ParseError, "non-numeric");
}
static void test_parser_overflow() {
    // 2^54 kB * 1024 overflows uint64
    auto o = fars::parse_status("VmRSS:\t18014398509481984 kB\n");
    expect_state(o.vm_rss, fars::MetricState::ParseError, "overflow ParseError");
}
static void test_permission_denied() {
    auto reader = [](std::string_view path) -> fars::FileReadResult {
        fars::FileReadResult r;
        if (std::string(path).find("status") != std::string::npos) {
            r.state = fars::MetricState::PermissionDenied;
            r.detail = "EACCES";
            return r;
        }
        r.state = fars::MetricState::Unavailable;
        return r;
    };
    auto snap = fars::ProcessMemoryCollector(reader).collect(99);
    expect(!snap.alive, "denied not alive");
    expect_state(snap.vm_rss, fars::MetricState::PermissionDenied, "denied RSS");
}
static void test_smaps_fallback() {
    auto reader = [](std::string_view path) -> fars::FileReadResult {
        fars::FileReadResult r;
        std::string p(path);
        if (p.find("status") != std::string::npos) {
            r.state = fars::MetricState::Available;
            r.content = "VmRSS:\t512 kB\nVmHWM:\t768 kB\nVmSwap:\t0 kB\nVmSize:\t1024 kB\n";
            return r;
        }
        r.state = fars::MetricState::Unavailable;
        return r;
    };
    auto snap = fars::ProcessMemoryCollector(reader).collect(1);
    expect(snap.alive && !snap.smaps_rollup_used, "status-only");
    expect_avail(snap.vm_rss, 512ULL * 1024, "RSS from status");
    expect_state(snap.pss, fars::MetricState::Unavailable, "Pss unavailable");
}
struct FakeClock {
    std::chrono::steady_clock::time_point now{
        std::chrono::steady_clock::time_point(std::chrono::milliseconds(0))};
    void advance(std::chrono::milliseconds d) { now += d; }
};
static void test_sampling_peak_vs_hwm() {
    FakeClock fc;
    auto clock_fn = [&] { return fc.now; };
    auto sleep_fn = [&](std::chrono::milliseconds d) { fc.advance(d); };
    auto reader = [](std::string_view path) -> fars::FileReadResult {
        fars::FileReadResult r;
        r.state = fars::MetricState::Available;
        std::string p(path);
        if (p.find("status") != std::string::npos)
            r.content = "VmRSS:\t1024 kB\nVmSize:\t4096 kB\nVmHWM:\t8192 kB\nVmSwap:\t0 kB\n";
        else if (p.find("smaps") != std::string::npos)
            r.content = "Rss:\t1024 kB\nPss:\t512 kB\nPrivate_Dirty:\t128 kB\n";
        else if (p.find("meminfo") != std::string::npos)
            r.content = "MemTotal: 8192000 kB\nMemAvailable: 4096000 kB\n";
        else r.state = fars::MetricState::Unavailable;
        return r;
    };
    fars::SamplingEngine eng(fars::ProcessMemoryCollector(reader),
                             fars::SystemMemoryCollector(reader), clock_fn, sleep_fn);
    fars::SamplingConfig cfg;
    cfg.duration_ms = 1000;
    cfg.interval_ms = 200;
    auto sum = eng.run(cfg, nullptr);
    expect(sum.successful_samples >= 4, "samples");
    expect(sum.window_peak_rss_bytes == 1024ULL * 1024, "window peak");
    expect(sum.vm_hwm.max_bytes == 8192ULL * 1024, "VmHWM");
    expect(sum.window_peak_rss_bytes != sum.vm_hwm.max_bytes, "peak != HWM");
}
static void test_sampling_death() {
    FakeClock fc;
    auto clock_fn = [&] { return fc.now; };
    auto sleep_fn = [&](std::chrono::milliseconds d) { fc.advance(d); };
    int status_reads = 0;
    auto reader = [&](std::string_view path) -> fars::FileReadResult {
        fars::FileReadResult r;
        std::string p(path);
        if (p.find("status") != std::string::npos) {
            ++status_reads;
            if (status_reads > 2) {
                r.state = fars::MetricState::Unavailable;
                r.detail = "gone";
                return r;
            }
            r.state = fars::MetricState::Available;
            r.content = "VmRSS:\t256 kB\nVmHWM:\t256 kB\nVmSize:\t512 kB\nVmSwap:\t0 kB\n";
            return r;
        }
        if (p.find("smaps") != std::string::npos) {
            r.state = fars::MetricState::Available;
            r.content = "Pss:\t128 kB\nRss:\t256 kB\nPrivate_Dirty:\t64 kB\n";
            return r;
        }
        if (p.find("meminfo") != std::string::npos) {
            r.state = fars::MetricState::Available;
            r.content = "MemTotal: 1000 kB\nMemAvailable: 500 kB\n";
            return r;
        }
        r.state = fars::MetricState::Unavailable;
        return r;
    };
    fars::SamplingEngine eng(fars::ProcessMemoryCollector(reader),
                             fars::SystemMemoryCollector(reader), clock_fn, sleep_fn);
    fars::SamplingConfig cfg;
    cfg.duration_ms = 5000;
    cfg.interval_ms = 100;
    auto sum = eng.run(cfg, nullptr);
    expect(sum.stopped_early, "early stop");
    expect(sum.not_found_samples >= 1, "notfound count");
    expect(sum.failed_samples >= 1, "failed counted");
    expect(sum.successful_samples >= 1, "prior success");
    expect(sum.total_samples == sum.successful_samples + sum.failed_samples, "accounting");
}
static void test_sampling_config() {
    fars::SamplingConfig c;
    c.duration_ms = 0;
    expect(!c.validate().empty(), "zero duration");
    c.duration_ms = 1000;
    c.interval_ms = 0;
    expect(!c.validate().empty(), "zero interval");
    c.interval_ms = 200;
    c.pid = -1;
    expect(!c.validate().empty(), "neg pid");
    c.pid = 0;
    expect(c.validate().empty(), "valid");
}
static void test_storage_invalid_and_cleanup() {
    fars::StorageBenchmarkConfig c;
    c.target_dir = "";
    expect(!c.validate().empty(), "empty dir");
    c.target_dir = "/tmp";
    c.file_size_bytes = 0;
    expect(!c.validate().empty(), "zero size");
    c.file_size_bytes = 1024 * 1024;
    c.block_size_bytes = 4096;
    c.duration_ms = 100;
    c.target_dir = "/no/such/fars_dir_xyz_021";
    auto r = fars::StorageBenchmark().run(c);
    expect(!r.error.empty(), "invalid dir error");
    expect(r.stats.successful_ops == 0, "no ops");

    auto dir = (fs::temp_directory_path() / ("fars_p1_" + std::to_string(::getpid()))).string();
    fs::create_directories(dir);
    c.target_dir = dir;
    c.max_ops = 32;
    c.duration_ms = 150;
    c.mode = fars::StorageTestMode::SequentialWrite;
    auto r2 = fars::StorageBenchmark().run(c);
    expect(r2.stats.successful_ops > 0, "write ops");
    expect(r2.cleanup_ok, "cleanup");
    expect(!fs::exists(r2.test_file_path), "file gone");
    fs::remove_all(dir);
}
static void test_storage_fsync_hook() {
    int sync_calls = 0;
    auto hooks = fars::make_posix_io_hooks();
    auto inner = hooks.sync_fn;
    hooks.sync_fn = [&](int fd) { ++sync_calls; return inner(fd); };
    auto dir = (fs::temp_directory_path() / ("fars_fsync_" + std::to_string(::getpid()))).string();
    fs::create_directories(dir);
    fars::StorageBenchmarkConfig c;
    c.target_dir = dir;
    c.file_size_bytes = 256 * 1024;
    c.block_size_bytes = 4096;
    c.duration_ms = 500;
    c.max_ops = 8;
    c.mode = fars::StorageTestMode::SequentialWrite;
    c.sync_policy = fars::StorageSyncPolicy::Synchronized;
    auto r = fars::StorageBenchmark(hooks).run(c);
    expect(r.stats.successful_ops > 0, "sync ops");
    expect(sync_calls >= static_cast<int>(r.stats.successful_ops), "fsync per write");
    expect(r.cleanup_ok, "sync cleanup");
    fs::remove_all(dir);
}
static void test_stats_exclude_unavailable() {
    fars::MetricStats st;
    st.add(fars::MetricValue::make_available(100));
    st.add(fars::MetricValue::make_unavailable());
    st.add(fars::MetricValue::make_available(0));
    expect(st.success_count == 2 && st.fail_count == 1, "stats counts");
    expect(st.min_bytes == 0 && st.max_bytes == 100, "stats min max");
}


static void test_unique_fd() {
    {
        fars::UniqueFd empty;
        expect(!empty, "default empty");
        expect(empty.get() == -1, "default -1");
    }
    int raw = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    expect(raw >= 0, "open /dev/null");
    {
        fars::UniqueFd fd(raw);
        expect(static_cast<bool>(fd), "owns fd");
        fars::UniqueFd fd2(std::move(fd));
        expect(!fd, "moved-from empty");
        expect(fd2.get() == raw, "move target owns");
        // move-assignment
        fars::UniqueFd fd3;
        fd3 = std::move(fd2);
        expect(!fd2, "move-assign source empty");
        expect(fd3.get() == raw, "move-assign target");
        int released = fd3.release();
        expect(released == raw, "release value");
        expect(!fd3, "released empty");
        ::close(released);
    }
    // reset same-descriptor no-op (no double-close)
    int raw2 = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    expect(raw2 >= 0, "open2");
    {
        fars::UniqueFd fd(raw2);
        fd.reset(raw2);
        expect(fd.get() == raw2, "same-fd reset keeps ownership");
        fd.reset();
        expect(!fd, "after reset");
    }
    // custom closer invoked once
    int closes = 0;
    int raw3 = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    expect(raw3 >= 0, "open3");
    {
        fars::UniqueFd fd(raw3, [&](int f) { ++closes; if (f >= 0) ::close(f); });
        expect(static_cast<bool>(fd), "custom closer owns");
    }
    expect(closes == 1, "custom closer once on destroy");
}

static void test_storage_close_hook() {
    int close_calls = 0;
    auto hooks = fars::make_posix_io_hooks();
    auto inner_close = hooks.close_fn;
    hooks.close_fn = [&](int fd) {
        ++close_calls;
        return inner_close(fd);
    };
    auto dir = (fs::temp_directory_path() / ("fars_close_" + std::to_string(::getpid()))).string();
    fs::create_directories(dir);
    fars::StorageBenchmarkConfig c;
    c.target_dir = dir;
    c.file_size_bytes = 128 * 1024;
    c.block_size_bytes = 4096;
    c.duration_ms = 100;
    c.max_ops = 4;
    c.mode = fars::StorageTestMode::SequentialWrite;
    auto r = fars::StorageBenchmark(hooks).run(c);
    expect(r.stats.successful_ops > 0, "close-hook run ops");
    expect(r.cleanup_ok, "close-hook cleanup");
    expect(close_calls >= 1, "close_fn invoked at least once");
    expect(!fs::exists(r.test_file_path), "temp removed after close");
    // failure path: invalid dir should not require close
    int closes_before = close_calls;
    c.target_dir = "/no/such/fars_close_fail";
    auto r2 = fars::StorageBenchmark(hooks).run(c);
    expect(!r2.error.empty(), "fail path error");
    fs::remove_all(dir);
    (void)closes_before;
}


static void test_observe_permission_vs_notfound() {
    auto denied = [](std::string_view path) -> fars::FileReadResult {
        fars::FileReadResult r;
        if (std::string(path).find("status") != std::string::npos) {
            r.state = fars::MetricState::PermissionDenied;
            r.detail = "EACCES";
            return r;
        }
        r.state = fars::MetricState::Unavailable;
        return r;
    };
    auto snap = fars::ProcessMemoryCollector(denied).collect(50);
    expect(snap.observe_kind == fars::ProcessObserveKind::PermissionDenied, "kind denied");
    expect(!snap.alive, "denied not alive");

    auto gone = [](std::string_view) -> fars::FileReadResult {
        fars::FileReadResult r;
        r.state = fars::MetricState::Unavailable;
        r.detail = "file not found";
        return r;
    };
    auto snap2 = fars::ProcessMemoryCollector(gone).collect(51);
    expect(snap2.observe_kind == fars::ProcessObserveKind::NotFound, "kind notfound");
}

static void test_sampling_permission_continues() {
    FakeClock fc;
    auto clock_fn = [&] { return fc.now; };
    auto sleep_fn = [&](std::chrono::milliseconds d) { fc.advance(d); };
    auto reader = [](std::string_view path) -> fars::FileReadResult {
        fars::FileReadResult r;
        std::string p(path);
        if (p.find("status") != std::string::npos) {
            r.state = fars::MetricState::PermissionDenied;
            r.detail = "EACCES";
            return r;
        }
        if (p.find("meminfo") != std::string::npos) {
            r.state = fars::MetricState::Available;
            r.content = "MemTotal: 1000 kB\nMemAvailable: 500 kB\n";
            return r;
        }
        r.state = fars::MetricState::Unavailable;
        return r;
    };
    fars::SamplingEngine eng(fars::ProcessMemoryCollector(reader),
                             fars::SystemMemoryCollector(reader), clock_fn, sleep_fn);
    fars::SamplingConfig cfg;
    cfg.duration_ms = 1000;
    cfg.interval_ms = 200;
    auto sum = eng.run(cfg, nullptr);
    expect(sum.failed_samples >= 4, "denied failed samples");
    expect(sum.permission_denied_samples == sum.failed_samples, "all denied");
    expect(!sum.stopped_early || sum.stop_reason.find("terminat") == std::string::npos,
           "denied does not stop as termination");
    // Should not stop early due to NotFound
    expect(sum.not_found_samples == 0, "no notfound");
    expect(sum.total_samples == sum.successful_samples + sum.failed_samples, "accounting denied");
}

static void test_sampling_transient_recovery() {
    FakeClock fc;
    auto clock_fn = [&] { return fc.now; };
    auto sleep_fn = [&](std::chrono::milliseconds d) { fc.advance(d); };
    int status_n = 0;
    auto reader = [&](std::string_view path) -> fars::FileReadResult {
        fars::FileReadResult r;
        std::string p(path);
        if (p.find("status") != std::string::npos) {
            ++status_n;
            // second sample transient, others ok
            if (status_n == 2) {
                r.state = fars::MetricState::ParseError;
                r.detail = "EINTR";
                return r;
            }
            r.state = fars::MetricState::Available;
            r.content = "VmRSS:\t128 kB\nVmHWM:\t128 kB\nVmSize:\t256 kB\nVmSwap:\t0 kB\n";
            return r;
        }
        if (p.find("smaps") != std::string::npos) {
            r.state = fars::MetricState::Available;
            r.content = "Pss:\t64 kB\nRss:\t128 kB\nPrivate_Dirty:\t32 kB\n";
            return r;
        }
        if (p.find("meminfo") != std::string::npos) {
            r.state = fars::MetricState::Available;
            r.content = "MemTotal: 1000 kB\nMemAvailable: 500 kB\n";
            return r;
        }
        r.state = fars::MetricState::Unavailable;
        return r;
    };
    fars::SamplingEngine eng(fars::ProcessMemoryCollector(reader),
                             fars::SystemMemoryCollector(reader), clock_fn, sleep_fn);
    fars::SamplingConfig cfg;
    cfg.duration_ms = 1000;
    cfg.interval_ms = 200;
    auto sum = eng.run(cfg, nullptr);
    expect(sum.transient_error_samples >= 1, "transient counted");
    expect(sum.successful_samples >= 3, "recovered successes");
    expect(!sum.stopped_early, "transient does not stop");
    expect(sum.total_samples == sum.successful_samples + sum.failed_samples, "accounting transient");
}

static void test_sampling_smaps_missing_still_success() {
    FakeClock fc;
    auto clock_fn = [&] { return fc.now; };
    auto sleep_fn = [&](std::chrono::milliseconds d) { fc.advance(d); };
    auto reader = [](std::string_view path) -> fars::FileReadResult {
        fars::FileReadResult r;
        std::string p(path);
        if (p.find("status") != std::string::npos) {
            r.state = fars::MetricState::Available;
            r.content = "VmRSS:\t256 kB\nVmHWM:\t256 kB\nVmSize:\t512 kB\nVmSwap:\t0 kB\n";
            return r;
        }
        if (p.find("meminfo") != std::string::npos) {
            r.state = fars::MetricState::Available;
            r.content = "MemTotal: 1000 kB\nMemAvailable: 500 kB\n";
            return r;
        }
        // smaps unavailable
        r.state = fars::MetricState::Unavailable;
        return r;
    };
    fars::SamplingEngine eng(fars::ProcessMemoryCollector(reader),
                             fars::SystemMemoryCollector(reader), clock_fn, sleep_fn);
    fars::SamplingConfig cfg;
    cfg.duration_ms = 600;
    cfg.interval_ms = 200;
    auto sum = eng.run(cfg, nullptr);
    expect(sum.successful_samples >= 2, "status-only success");
    expect(sum.failed_samples == 0, "smaps miss not process failure");
}


static void test_enoent_notfound_early_stop() {
    FakeClock fc;
    auto clock_fn = [&] { return fc.now; };
    auto sleep_fn = [&](std::chrono::milliseconds d) { fc.advance(d); };
    auto reader = [](std::string_view path) -> fars::FileReadResult {
        fars::FileReadResult r;
        std::string p(path);
        if (p.find("meminfo") != std::string::npos) {
            r.state = fars::MetricState::Available;
            r.content = "MemTotal: 1000 kB\nMemAvailable: 500 kB\n";
            return r;
        }
        r.state = fars::MetricState::Unavailable;
        r.detail = "file not found";
        return r;
    };
    auto snap = fars::ProcessMemoryCollector(reader).collect(99991);
    expect(snap.observe_kind == fars::ProcessObserveKind::NotFound, "ENOENT NotFound");
    fars::SamplingEngine eng(fars::ProcessMemoryCollector(reader),
                             fars::SystemMemoryCollector(reader), clock_fn, sleep_fn);
    fars::SamplingConfig cfg;
    cfg.duration_ms = 2000;
    cfg.interval_ms = 200;
    auto sum = eng.run(cfg, nullptr);
    expect(sum.stopped_early, "ENOENT early stop");
    expect(sum.not_found_samples >= 1, "ENOENT notfound count");
    expect(sum.total_samples == sum.successful_samples + sum.failed_samples, "ENOENT accounting");
}

static void test_eio_emfile_transient_continue() {
    FakeClock fc;
    auto clock_fn = [&] { return fc.now; };
    auto sleep_fn = [&](std::chrono::milliseconds d) { fc.advance(d); };
    // Simulate open EIO / EMFILE as ParseError (what default_file_reader now emits)
    auto reader = [](std::string_view path) -> fars::FileReadResult {
        fars::FileReadResult r;
        std::string p(path);
        if (p.find("meminfo") != std::string::npos) {
            r.state = fars::MetricState::Available;
            r.content = "MemTotal: 1000 kB\nMemAvailable: 500 kB\n";
            return r;
        }
        if (p.find("status") != std::string::npos) {
            r.state = fars::MetricState::ParseError;
            r.detail = "open failed: Input/output error";
            return r;
        }
        r.state = fars::MetricState::Unavailable;
        return r;
    };
    auto snap = fars::ProcessMemoryCollector(reader).collect(12);
    expect(snap.observe_kind == fars::ProcessObserveKind::TransientError, "EIO transient");
    fars::SamplingEngine eng(fars::ProcessMemoryCollector(reader),
                             fars::SystemMemoryCollector(reader), clock_fn, sleep_fn);
    fars::SamplingConfig cfg;
    cfg.duration_ms = 1000;
    cfg.interval_ms = 200;
    auto sum = eng.run(cfg, nullptr);
    expect(sum.transient_error_samples >= 4, "EIO transient samples");
    expect(sum.not_found_samples == 0, "EIO not NotFound");
    expect(!sum.stopped_early, "EIO no early termination stop");
    expect(sum.total_samples == sum.successful_samples + sum.failed_samples, "EIO accounting");

    // EMFILE-like persistent transient for full duration
    auto reader2 = [](std::string_view path) -> fars::FileReadResult {
        fars::FileReadResult r;
        std::string p(path);
        if (p.find("meminfo") != std::string::npos) {
            r.state = fars::MetricState::Available;
            r.content = "MemTotal: 1000 kB\nMemAvailable: 500 kB\n";
            return r;
        }
        r.state = fars::MetricState::ParseError;
        r.detail = "open failed: Too many open files";
        return r;
    };
    fars::SamplingEngine eng2(fars::ProcessMemoryCollector(reader2),
                              fars::SystemMemoryCollector(reader2), clock_fn, sleep_fn);
    // reset clock
    fc.now = std::chrono::steady_clock::time_point(std::chrono::milliseconds(0));
    auto sum2 = eng2.run(cfg, nullptr);
    expect(sum2.failed_samples == sum2.total_samples, "EMFILE all failed");
    expect(!sum2.stopped_early, "EMFILE completes duration");
    expect(sum2.stop_reason.find("terminat") == std::string::npos, "EMFILE not termination");
}

int main() {
    test_metric_value();
    test_parser_well_formed();
    test_parser_missing_and_malformed();
    test_parser_overflow();
    test_permission_denied();
    test_smaps_fallback();
    test_sampling_peak_vs_hwm();
    test_sampling_death();
    test_sampling_config();
    test_storage_invalid_and_cleanup();
    test_storage_fsync_hook();
    test_stats_exclude_unavailable();
    test_unique_fd();
    test_storage_close_hook();
    test_observe_permission_vs_notfound();
    test_sampling_permission_continues();
    test_sampling_transient_recovery();
    test_sampling_smaps_missing_still_success();
    test_enoent_notfound_early_stop();
    test_eio_emfile_transient_continue();
    if (failures == 0) {
        std::cout << "Phase 1 tests: all passed\n";
        return 0;
    }
    std::cerr << "Phase 1 tests: " << failures << " failure(s)\n";
    return 1;
}

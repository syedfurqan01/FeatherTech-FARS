#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <vector>
namespace fars {
enum class StorageTestMode { SequentialWrite, SequentialRead, RandomWrite, RandomRead };
enum class StorageSyncPolicy { Buffered, Synchronized };
inline const char* to_string(StorageTestMode m) {
    switch(m){case StorageTestMode::SequentialWrite:return "sequential-write";
    case StorageTestMode::SequentialRead:return "sequential-read";
    case StorageTestMode::RandomWrite:return "random-write";
    case StorageTestMode::RandomRead:return "random-read";} return "unknown";
}
inline const char* to_string(StorageSyncPolicy p) {
    switch(p){case StorageSyncPolicy::Buffered:return "buffered";
    case StorageSyncPolicy::Synchronized:return "synchronized";} return "unknown";
}
struct StorageBenchmarkConfig {
    std::string target_dir;
    std::uint64_t file_size_bytes = 64ULL*1024*1024;
    std::uint64_t block_size_bytes = 4096;
    std::uint64_t duration_ms = 5000;
    StorageTestMode mode = StorageTestMode::SequentialWrite;
    StorageSyncPolicy sync_policy = StorageSyncPolicy::Buffered;
    std::uint64_t random_seed = 42;
    std::uint64_t max_file_size_bytes = 512ULL*1024*1024;
    std::uint64_t max_ops = 0;
    static constexpr std::uint64_t kMinBlock=512, kMaxBlock=16ULL*1024*1024;
    std::string validate() const;
};
struct StorageStats {
    std::uint64_t total_ops=0, successful_ops=0, failed_ops=0, bytes_transferred=0;
    double elapsed_ms=0, throughput_mib_s=0, iops=0;
    double min_latency_us=0, max_latency_us=0, avg_latency_us=0;
    bool latency_valid=false;
    void finalize(double e); void add_success(std::uint64_t b, double lat); void add_failure();
};
struct StorageBenchmarkResult {
    StorageBenchmarkConfig config; StorageStats stats;
    std::string test_file_path; bool cleanup_ok=false;
    std::string error, notes, sync_policy_label, mode_label;
};
struct StorageIOHooks {
    std::function<bool(int,void*,std::size_t&)> read_fn;
    std::function<bool(int,const void*,std::size_t&)> write_fn;
    std::function<bool(int,std::int64_t)> seek_fn;
    std::function<bool(int)> sync_fn;
    std::function<int(const char*,int,int)> open_fn;
    std::function<int(int)> close_fn;
    std::function<int(const char*)> unlink_fn;
};
StorageIOHooks make_posix_io_hooks();
class StorageBenchmark {
public:
    explicit StorageBenchmark(StorageIOHooks hooks = make_posix_io_hooks());
    StorageBenchmarkResult run(const StorageBenchmarkConfig& cfg);
private:
    StorageIOHooks hooks_;
    StorageBenchmarkResult run_sequential_write(const StorageBenchmarkConfig&, const std::string&, int);
    StorageBenchmarkResult run_sequential_read(const StorageBenchmarkConfig&, const std::string&, int);
    StorageBenchmarkResult run_random_write(const StorageBenchmarkConfig&, const std::string&, int);
    StorageBenchmarkResult run_random_read(const StorageBenchmarkConfig&, const std::string&, int);
    bool prepare_file(const StorageBenchmarkConfig&, std::string&, int&, std::string&);
    bool ensure_file_size(int, std::uint64_t, std::string&);
    bool cleanup_file(const std::string&, std::string&);
};
}

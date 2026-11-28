#include "fars/storage_benchmark.hpp"
#include "fars/unique_fd.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <sys/statvfs.h>
#include <unistd.h>
#include <vector>
namespace fs = std::filesystem;
namespace fars {
void StorageStats::add_success(std::uint64_t bytes, double latency_us) {
    ++total_ops; ++successful_ops; bytes_transferred += bytes;
    if (!latency_valid) { min_latency_us=max_latency_us=avg_latency_us=latency_us; latency_valid=true; }
    else {
        if (latency_us<min_latency_us) min_latency_us=latency_us;
        if (latency_us>max_latency_us) max_latency_us=latency_us;
        avg_latency_us += (latency_us-avg_latency_us)/static_cast<double>(successful_ops);
    }
}
void StorageStats::add_failure() { ++total_ops; ++failed_ops; }
void StorageStats::finalize(double e) {
    elapsed_ms=e;
    if (e>0) { throughput_mib_s=(bytes_transferred/(1024.0*1024.0))/(e/1000.0); iops=successful_ops/(e/1000.0); }
    else { throughput_mib_s=0; iops=0; }
}
std::string StorageBenchmarkConfig::validate() const {
    if (target_dir.empty()) return "target directory is required (--storage-dir)";
    if (block_size_bytes<kMinBlock) return "block size too small";
    if (block_size_bytes>kMaxBlock) return "block size too large";
    if (file_size_bytes==0) return "file size must be > 0";
    if (file_size_bytes>max_file_size_bytes) return "file size exceeds configured maximum";
    if (block_size_bytes>file_size_bytes) return "block size larger than file size";
    if (duration_ms==0) return "duration must be > 0";
    if (duration_ms>600000) return "duration too large (max 600000 ms)";
    if (file_size_bytes/block_size_bytes==0) return "file size smaller than one block";
    return {};
}
StorageIOHooks make_posix_io_hooks() {
    StorageIOHooks h;
    h.open_fn=[](const char* path,int flags,int mode){return ::open(path,flags,mode);};
    h.close_fn=[](int fd){return ::close(fd);};
    h.unlink_fn=[](const char* path){return ::unlink(path);};
    h.read_fn=[](int fd,void* buf,std::size_t& n){ssize_t r=::read(fd,buf,n); if(r<0){if(errno==EINTR){n=0;return true;}return false;} n=(std::size_t)r; return true;};
    h.write_fn=[](int fd,const void* buf,std::size_t& n){ssize_t r=::write(fd,buf,n); if(r<0){if(errno==EINTR){n=0;return true;}return false;} n=(std::size_t)r; return true;};
    h.seek_fn=[](int fd,std::int64_t off){return ::lseek(fd,(off_t)off,SEEK_SET)>=0;};
    h.sync_fn=[](int fd){return ::fsync(fd)==0;};
    return h;
}
StorageBenchmark::StorageBenchmark(StorageIOHooks hooks):hooks_(std::move(hooks)){}
bool StorageBenchmark::cleanup_file(const std::string& path,std::string& err){
    if(path.empty()) return true;
    if(hooks_.unlink_fn(path.c_str())!=0){ if(errno==ENOENT) return true; err=std::string("cleanup failed: ")+std::strerror(errno); return false; }
    return true;
}
bool StorageBenchmark::ensure_file_size(int fd,std::uint64_t size,std::string& err){
    if(::ftruncate(fd,(off_t)size)!=0){ err=std::string("ftruncate failed: ")+std::strerror(errno); return false; }
    return true;
}
bool StorageBenchmark::prepare_file(const StorageBenchmarkConfig& cfg,std::string& path_out,int& fd_out,std::string& err){
    std::error_code ec;
    if(!fs::exists(cfg.target_dir,ec)||!fs::is_directory(cfg.target_dir,ec)){ err="target directory does not exist or is not a directory"; return false; }
    struct statvfs st{};
    if(::statvfs(cfg.target_dir.c_str(),&st)==0){
        std::uint64_t freeb=(std::uint64_t)st.f_bavail*st.f_frsize;
        if(freeb<cfg.file_size_bytes+8ULL*1024*1024){ err="insufficient free space in target directory"; return false; }
    }
    auto unique=std::chrono::steady_clock::now().time_since_epoch().count();
    path_out=(fs::path(cfg.target_dir)/("fars_storage_bench_"+std::to_string(unique)+".tmp")).string();
    auto closer = [this](int f) { if (f >= 0) hooks_.close_fn(f); };
    UniqueFd fd(hooks_.open_fn(path_out.c_str(),O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600), closer);
    if(!fd){ err=std::string("open failed: ")+std::strerror(errno); path_out.clear(); return false; }
    if(!ensure_file_size(fd.get(),cfg.file_size_bytes,err)){
        fd.reset(); // close before unlink
        cleanup_file(path_out,err);
        path_out.clear();
        return false;
    }
    fd_out=fd.release(); // transfer ownership to caller
    return true;
}
static double ems(std::chrono::steady_clock::time_point s){
    return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-s).count();
}
StorageBenchmarkResult StorageBenchmark::run_sequential_write(const StorageBenchmarkConfig& cfg,const std::string& path,int fd){
    StorageBenchmarkResult res; res.config=cfg; res.test_file_path=path;
    res.mode_label=to_string(cfg.mode); res.sync_policy_label=to_string(cfg.sync_policy);
    std::vector<char> buf((size_t)cfg.block_size_bytes,'W');
    auto start=std::chrono::steady_clock::now(); std::uint64_t offset=0;
    while(ems(start)<(double)cfg.duration_ms){
        if(offset>=cfg.file_size_bytes) offset=0;
        if(!hooks_.seek_fn(fd,(std::int64_t)offset)){ res.stats.add_failure(); break; }
        auto t0=std::chrono::steady_clock::now();
        std::size_t left=(size_t)cfg.block_size_bytes, done=0; bool ok=true;
        while(done<left){ std::size_t n=left-done; if(!hooks_.write_fn(fd,buf.data()+done,n)){ok=false;break;} if(n==0) continue; done+=n; }
        if(ok && cfg.sync_policy==StorageSyncPolicy::Synchronized) ok=hooks_.sync_fn(fd);
        double lat=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count();
        if(ok&&done) res.stats.add_success(done,lat); else { res.stats.add_failure(); if(!ok){res.error="write failed";break;} }
        offset+=cfg.block_size_bytes;
        if(cfg.max_ops && res.stats.successful_ops>=cfg.max_ops) break;
    }
    res.stats.finalize(ems(start));
    res.notes=(cfg.sync_policy==StorageSyncPolicy::Buffered)?"Buffered sequential write; may include page-cache effects":"Synchronized sequential write (fsync per block)";
    return res;
}
StorageBenchmarkResult StorageBenchmark::run_sequential_read(const StorageBenchmarkConfig& cfg,const std::string& path,int fd){
    StorageBenchmarkResult res; res.config=cfg; res.test_file_path=path;
    res.mode_label=to_string(cfg.mode); res.sync_policy_label=to_string(cfg.sync_policy);
    { std::vector<char> fill((size_t)cfg.block_size_bytes,'R'); hooks_.seek_fn(fd,0);
      for(std::uint64_t off=0; off+cfg.block_size_bytes<=cfg.file_size_bytes; off+=cfg.block_size_bytes){
          std::size_t n=(size_t)cfg.block_size_bytes; if(!hooks_.write_fn(fd,fill.data(),n)||!n) break; }
      if(cfg.sync_policy==StorageSyncPolicy::Synchronized) hooks_.sync_fn(fd);
    }
    std::vector<char> buf((size_t)cfg.block_size_bytes);
    auto start=std::chrono::steady_clock::now(); std::uint64_t offset=0;
    while(ems(start)<(double)cfg.duration_ms){
        if(offset>=cfg.file_size_bytes) offset=0;
        if(!hooks_.seek_fn(fd,(std::int64_t)offset)){ res.stats.add_failure(); break; }
        auto t0=std::chrono::steady_clock::now();
        std::size_t left=(size_t)cfg.block_size_bytes, done=0; bool ok=true;
        while(done<left){ std::size_t n=left-done; if(!hooks_.read_fn(fd,buf.data()+done,n)){ok=false;break;} if(n==0){ok=false;res.error="unexpected EOF";break;} done+=n; }
        double lat=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count();
        if(ok) res.stats.add_success(done,lat); else { res.stats.add_failure(); if(!res.error.empty()) break; }
        offset+=cfg.block_size_bytes;
        if(cfg.max_ops && res.stats.successful_ops>=cfg.max_ops) break;
    }
    res.stats.finalize(ems(start)); res.notes="Sequential read; may be served from page cache"; return res;
}
StorageBenchmarkResult StorageBenchmark::run_random_write(const StorageBenchmarkConfig& cfg,const std::string& path,int fd){
    StorageBenchmarkResult res; res.config=cfg; res.test_file_path=path;
    res.mode_label=to_string(cfg.mode); res.sync_policy_label=to_string(cfg.sync_policy);
    std::uint64_t nblocks=cfg.file_size_bytes/cfg.block_size_bytes;
    std::mt19937_64 rng(cfg.random_seed); std::uniform_int_distribution<std::uint64_t> dist(0,nblocks-1);
    std::vector<char> buf((size_t)cfg.block_size_bytes,'X');
    auto start=std::chrono::steady_clock::now();
    while(ems(start)<(double)cfg.duration_ms){
        std::int64_t off=(std::int64_t)(dist(rng)*cfg.block_size_bytes);
        if(!hooks_.seek_fn(fd,off)){ res.stats.add_failure(); continue; }
        auto t0=std::chrono::steady_clock::now(); std::size_t n=(size_t)cfg.block_size_bytes;
        bool ok=hooks_.write_fn(fd,buf.data(),n)&&n==cfg.block_size_bytes;
        if(ok&&cfg.sync_policy==StorageSyncPolicy::Synchronized) ok=hooks_.sync_fn(fd);
        double lat=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count();
        if(ok) res.stats.add_success(n,lat); else res.stats.add_failure();
        if(cfg.max_ops && res.stats.successful_ops>=cfg.max_ops) break;
    }
    res.stats.finalize(ems(start));
    res.notes="Random write within allocated region; seed="+std::to_string(cfg.random_seed); return res;
}
StorageBenchmarkResult StorageBenchmark::run_random_read(const StorageBenchmarkConfig& cfg,const std::string& path,int fd){
    StorageBenchmarkResult res; res.config=cfg; res.test_file_path=path;
    res.mode_label=to_string(cfg.mode); res.sync_policy_label=to_string(cfg.sync_policy);
    { std::vector<char> fill((size_t)cfg.block_size_bytes,'Y');
      for(std::uint64_t off=0; off+cfg.block_size_bytes<=cfg.file_size_bytes; off+=cfg.block_size_bytes){
          if(!hooks_.seek_fn(fd,(std::int64_t)off)) break; std::size_t n=(size_t)cfg.block_size_bytes; hooks_.write_fn(fd,fill.data(),n); } }
    std::uint64_t nblocks=cfg.file_size_bytes/cfg.block_size_bytes;
    std::mt19937_64 rng(cfg.random_seed); std::uniform_int_distribution<std::uint64_t> dist(0,nblocks-1);
    std::vector<char> buf((size_t)cfg.block_size_bytes);
    auto start=std::chrono::steady_clock::now();
    while(ems(start)<(double)cfg.duration_ms){
        std::int64_t off=(std::int64_t)(dist(rng)*cfg.block_size_bytes);
        if(!hooks_.seek_fn(fd,off)){ res.stats.add_failure(); continue; }
        auto t0=std::chrono::steady_clock::now(); std::size_t n=(size_t)cfg.block_size_bytes;
        bool ok=hooks_.read_fn(fd,buf.data(),n)&&n==cfg.block_size_bytes;
        double lat=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count();
        if(ok) res.stats.add_success(n,lat); else res.stats.add_failure();
        if(cfg.max_ops && res.stats.successful_ops>=cfg.max_ops) break;
    }
    res.stats.finalize(ems(start)); res.notes="Random read within valid region; may be cached"; return res;
}
StorageBenchmarkResult StorageBenchmark::run(const StorageBenchmarkConfig& cfg){
    StorageBenchmarkResult res; res.config=cfg;
    res.mode_label=to_string(cfg.mode); res.sync_policy_label=to_string(cfg.sync_policy);
    if(auto err=cfg.validate(); !err.empty()){ res.error=err; return res; }
    std::string path; int raw_fd=-1; std::string err;
    if(!prepare_file(cfg,path,raw_fd,err)){ res.error=err; return res; }
    auto closer = [this](int f) { if (f >= 0) hooks_.close_fn(f); };
    UniqueFd fd(raw_fd, closer); // owns descriptor; closes via hooks_.close_fn
    res.test_file_path=path;
    switch(cfg.mode){
        case StorageTestMode::SequentialWrite: res=run_sequential_write(cfg,path,fd.get()); break;
        case StorageTestMode::SequentialRead: res=run_sequential_read(cfg,path,fd.get()); break;
        case StorageTestMode::RandomWrite: res=run_random_write(cfg,path,fd.get()); break;
        case StorageTestMode::RandomRead: res=run_random_read(cfg,path,fd.get()); break;
    }
    fd.reset(); // close before unlink
    std::string cerr; res.cleanup_ok=cleanup_file(path,cerr);
    if(!res.cleanup_ok){ if(!res.error.empty()) res.error+="; "; res.error+=cerr; }
    return res;
}
} // namespace fars

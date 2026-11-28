#include "fars/process_memory.hpp"
#include "fars/system_memory.hpp"
#include "fars/sampling_engine.hpp"
#include "fars/storage_benchmark.hpp"
#include "fars/metric_value.hpp"
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
namespace {
fars::SamplingEngine* g_engine = nullptr;
void on_signal(int) { if (g_engine) g_engine->request_stop(); }
void print_metric(const char* name, const fars::MetricValue& m) {
    std::cout << "  " << name << ": ";
    if (m.is_available()) std::cout << (m.value / (1024.0 * 1024.0)) << " MiB\n";
    else { std::cout << "[" << fars::to_string(m.state) << "]"; if (!m.detail.empty()) std::cout << " " << m.detail; std::cout << "\n"; }
}
void print_metric_stats(const char* name, const fars::MetricStats& s) {
    std::cout << "  " << name << ": ";
    if (s.success_count == 0) { std::cout << "[no successful samples] fails=" << s.fail_count << "\n"; return; }
    std::cout << "min=" << (s.min_bytes/(1024.0*1024.0)) << " max=" << (s.max_bytes/(1024.0*1024.0))
              << " avg=" << (s.avg_bytes/(1024.0*1024.0)) << " MiB (n=" << s.success_count << ")\n";
}
void print_storage(const fars::StorageBenchmarkResult& r) {
    std::cout << "=== Storage Benchmark Result ===\n"
              << "  mode: " << r.mode_label << "\n  sync_policy: " << r.sync_policy_label
              << "\n  target_dir: " << r.config.target_dir
              << "\n  file_size: " << (r.config.file_size_bytes/(1024.0*1024.0)) << " MiB"
              << "\n  successful_ops: " << r.stats.successful_ops
              << "\n  failed_ops: " << r.stats.failed_ops
              << "\n  bytes: " << r.stats.bytes_transferred
              << "\n  elapsed_ms: " << r.stats.elapsed_ms
              << "\n  throughput: " << r.stats.throughput_mib_s << " MiB/s"
              << "\n  IOPS: " << r.stats.iops << "\n";
    if (r.stats.latency_valid)
        std::cout << "  latency_us: min=" << r.stats.min_latency_us << " avg=" << r.stats.avg_latency_us
                  << " max=" << r.stats.max_latency_us << "\n";
    std::cout << "  cleanup_ok: " << (r.cleanup_ok?"yes":"no") << "\n";
    if (!r.notes.empty()) std::cout << "  notes: " << r.notes << "\n";
    if (!r.error.empty()) std::cout << "  error: " << r.error << "\n";
}
void usage() {
    std::cout << "FARS Memory Profiler 0.2 (unified)\n\n"
              << "Memory / sampling:\n"
              << "  --duration MS   --interval MS   --pid PID   --once\n\n"
              << "Storage:\n"
              << "  --storage-benchmark --storage-mode MODE --storage-dir PATH\n"
              << "  --storage-size-mib N --storage-block-size N --storage-duration MS\n"
              << "  --storage-sync buffered|synchronized\n";
}
}
int main(int argc, char** argv) {
    bool once=false, storage=false;
    fars::SamplingConfig scfg;
    fars::StorageBenchmarkConfig stcfg;
    stcfg.file_size_bytes=16ULL*1024*1024; stcfg.duration_ms=1000;
    for (int i=1;i<argc;++i) {
        std::string arg=argv[i];
        if (arg=="--help"||arg=="-h"){ usage(); return 0; }
        if (arg=="--once"){ once=true; continue; }
        if (arg=="--duration"&&i+1<argc){ long v=std::strtol(argv[++i],nullptr,10); scfg.duration_ms=v>0?(std::size_t)v:0; continue; }
        if (arg=="--interval"&&i+1<argc){ long v=std::strtol(argv[++i],nullptr,10); scfg.interval_ms=v>0?(std::size_t)v:0; continue; }
        if (arg=="--pid"&&i+1<argc){ scfg.pid=(std::int32_t)std::strtol(argv[++i],nullptr,10); continue; }
        if (arg=="--storage-benchmark"){ storage=true; continue; }
        if (arg=="--storage-mode"&&i+1<argc){
            std::string m=argv[++i]; storage=true;
            if(m=="sequential-write") stcfg.mode=fars::StorageTestMode::SequentialWrite;
            else if(m=="sequential-read") stcfg.mode=fars::StorageTestMode::SequentialRead;
            else if(m=="random-write") stcfg.mode=fars::StorageTestMode::RandomWrite;
            else if(m=="random-read") stcfg.mode=fars::StorageTestMode::RandomRead;
            else { std::cerr<<"unknown storage mode\n"; return 2; }
            continue;
        }
        if (arg=="--storage-dir"&&i+1<argc){ stcfg.target_dir=argv[++i]; storage=true; continue; }
        if (arg=="--storage-size-mib"&&i+1<argc){ stcfg.file_size_bytes=std::strtoull(argv[++i],nullptr,10)*1024ULL*1024ULL; continue; }
        if (arg=="--storage-block-size"&&i+1<argc){ stcfg.block_size_bytes=std::strtoull(argv[++i],nullptr,10); continue; }
        if (arg=="--storage-duration"&&i+1<argc){ stcfg.duration_ms=std::strtoull(argv[++i],nullptr,10); continue; }
        if (arg=="--storage-sync"&&i+1<argc){
            std::string s=argv[++i];
            if(s=="buffered") stcfg.sync_policy=fars::StorageSyncPolicy::Buffered;
            else if(s=="synchronized") stcfg.sync_policy=fars::StorageSyncPolicy::Synchronized;
            else { std::cerr<<"unknown sync\n"; return 2; }
            continue;
        }
    }
    if (storage) {
        if (auto err=stcfg.validate(); !err.empty()){ std::cerr<<"Invalid configuration: "<<err<<"\n"; return 2; }
        fars::StorageBenchmark bench; auto r=bench.run(stcfg); print_storage(r);
        return r.error.empty()?0:1;
    }
    fars::ProcessMemoryCollector proc_col; fars::SystemMemoryCollector sys_col;
    if (once) {
        auto sys=sys_col.collect();
        std::cout<<"=== System Memory ===\n";
        print_metric("MemTotal",sys.mem_total); print_metric("MemAvailable",sys.mem_available);
        print_metric("MemFree",sys.mem_free); print_metric("Buffers",sys.buffers);
        print_metric("Cached",sys.cached); print_metric("SwapTotal",sys.swap_total); print_metric("SwapFree",sys.swap_free);
        auto proc=(scfg.pid==0)?proc_col.collect_self():proc_col.collect(scfg.pid);
        std::cout<<"\n=== Process Memory (pid="<<proc.pid<<", alive="<<(proc.alive?"yes":"no")<<") ===\n";
        print_metric("VmRSS",proc.vm_rss); print_metric("VmSize",proc.vm_size);
        print_metric("VmHWM",proc.vm_hwm); print_metric("VmSwap",proc.vm_swap);
        print_metric("Pss",proc.pss); print_metric("Private_Dirty",proc.private_dirty);
        std::cout<<"  smaps_rollup_used: "<<(proc.smaps_rollup_used?"yes":"no")<<"\n";
        if(!proc.notes.empty()) std::cout<<"  notes: "<<proc.notes<<"\n";
        return 0;
    }
    if (auto err=scfg.validate(); !err.empty()){ std::cerr<<"Invalid configuration: "<<err<<"\n"; return 2; }
    fars::SamplingEngine engine(std::move(proc_col), std::move(sys_col));
    g_engine=&engine; std::signal(SIGINT, on_signal);
    std::cout<<"Sampling pid="<<(scfg.pid==0?"self":std::to_string(scfg.pid))
             <<" duration="<<scfg.duration_ms<<"ms interval="<<scfg.interval_ms<<"ms\n";
    auto summary=engine.run(scfg,nullptr);
    std::cout<<"\n=== Sampling Summary ===\n"
             <<"  total_samples: "<<summary.total_samples<<"\n"
             <<"  successful: "<<summary.successful_samples<<"\n"
             <<"  failed: "<<summary.failed_samples<<"\n"
             <<"  elapsed: "<<summary.total_elapsed_ms<<" ms\n"
             <<"  avg_overhead: "<<summary.avg_collect_overhead_us<<" us\n";
    print_metric_stats("VmRSS",summary.vm_rss);
    print_metric_stats("VmHWM (kernel)",summary.vm_hwm);
    print_metric_stats("Pss",summary.pss);
    std::cout<<"  window_peak_rss: ";
    if(summary.window_peak_rss_valid)
        std::cout<<(summary.window_peak_rss_bytes/(1024.0*1024.0))<<" MiB  [sampling-window peak — NOT VmHWM]\n";
    else std::cout<<"[no data]\n";
    print_metric_stats("MemAvailable",summary.mem_available);
    if(summary.stopped_early) std::cout<<"Stopped early: "<<summary.stop_reason<<"\n";
    g_engine=nullptr; return 0;
}

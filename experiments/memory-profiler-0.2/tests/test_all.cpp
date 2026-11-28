#include "fars/metric_value.hpp"
#include "fars/platform/proc_parser.hpp"
#include "fars/process_memory.hpp"
#include "fars/system_memory.hpp"
#include "fars/sampling_engine.hpp"
#include "fars/storage_benchmark.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <unistd.h>
static int failures=0;
static void expect(bool c,const char* m){ if(!c){ std::cerr<<"FAIL: "<<m<<"\n"; ++failures; } }
namespace fs=std::filesystem;
int main(){
    auto z=fars::MetricValue::make_available(0);
    expect(z.is_available()&&z.value==0,"zero Available");
    auto u=fars::MetricValue::make_unavailable();
    expect(u.state==fars::MetricState::Unavailable,"Unavailable");
    auto s=fars::parse_status("VmRSS:\t1024 kB\nVmHWM:\t2048 kB\nVmSwap:\t0 kB\n");
    expect(s.vm_rss.is_available()&&s.vm_rss.value==1024ULL*1024,"VmRSS");
    expect(s.vm_swap.is_available()&&s.vm_swap.value==0,"VmSwap zero");
    expect(fars::parse_status("Name:\tx\n").vm_rss.state==fars::MetricState::Unavailable,"missing");
    fars::FileReadResult st; st.state=fars::MetricState::Available;
    st.content="VmRSS:\t1024 kB\nVmHWM:\t2048 kB\nVmSwap:\t0 kB\n";
    fars::FileReadResult sm; sm.state=fars::MetricState::Available;
    sm.content="Pss:\t512 kB\nPrivate_Dirty:\t128 kB\nRss:\t1024 kB\n";
    auto reader=[=](std::string_view path)->fars::FileReadResult{
        std::string p(path);
        if(p.find("status")!=std::string::npos) return st;
        if(p.find("smaps")!=std::string::npos) return sm;
        if(p.find("meminfo")!=std::string::npos){
            fars::FileReadResult m; m.state=fars::MetricState::Available;
            m.content="MemTotal: 4096000 kB\nMemAvailable: 2048000 kB\n"; return m;
        }
        fars::FileReadResult u; u.state=fars::MetricState::Unavailable; return u;
    };
    fars::ProcessMemoryCollector pc(reader);
    auto snap=pc.collect_self();
    expect(snap.alive&&snap.vm_rss.is_available(),"collector");
    fars::SamplingConfig bad; bad.duration_ms=0;
    expect(!bad.validate().empty(),"duration reject");
    fars::StorageBenchmarkConfig c; c.target_dir="/tmp"; c.file_size_bytes=1024*1024;
    c.block_size_bytes=4096; c.duration_ms=100; c.max_ops=16;
    expect(c.validate().empty(),"storage cfg");
    auto dir=(fs::temp_directory_path()/("fars_t_"+std::to_string(::getpid()))).string();
    fs::create_directories(dir); c.target_dir=dir;
    fars::StorageBenchmark bench; auto r=bench.run(c);
    expect(r.stats.successful_ops>0&&r.cleanup_ok,"storage run");
    fs::remove_all(dir);
    if(failures==0){ std::cout<<"all unified tests passed\n"; return 0; }
    std::cerr<<failures<<" failure(s)\n"; return 1;
}

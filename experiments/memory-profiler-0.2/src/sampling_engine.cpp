#include "fars/sampling_engine.hpp"
namespace fars {

SamplingEngine::SamplingEngine(ProcessMemoryCollector p, SystemMemoryCollector s,
                               SteadyClockNow c, SleepFor sl)
    : proc_(std::move(p)), sys_(std::move(s)), clock_(std::move(c)), sleep_(std::move(sl)) {}

void SamplingEngine::request_stop() {
    stop_requested_.store(true, std::memory_order_relaxed);
}

SamplingSummary SamplingEngine::run(const SamplingConfig& cfg,
                                    std::vector<SampleRecord>* history_out) {
    SamplingSummary sum;
    sum.target_pid = cfg.pid;
    sum.configured_duration_ms = cfg.duration_ms;
    sum.configured_interval_ms = cfg.interval_ms;

    if (auto err = cfg.validate(); !err.empty()) {
        sum.stopped_early = true;
        sum.stop_reason = err;
        return sum;
    }

    const auto start = clock_();
    const auto deadline = start + std::chrono::milliseconds(cfg.duration_ms);
    auto prev = start;
    bool first = true;
    double oh_sum = 0;
    std::size_t oh_n = 0;
    std::vector<SampleRecord> hist;

    while (clock_() < deadline && !stop_requested_.load(std::memory_order_relaxed)) {
        const auto t0 = clock_();
        auto proc = (cfg.pid == 0) ? proc_.collect_self() : proc_.collect(cfg.pid);
        auto sys = sys_.collect();
        const auto t1 = clock_();
        auto oh = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

        SampleRecord rec;
        rec.timestamp = t0;
        rec.elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(t0 - start).count();
        rec.interval_ms = first ? 0 :
            std::chrono::duration_cast<std::chrono::milliseconds>(t0 - prev).count();
        rec.collect_overhead_us = oh;
        rec.process = std::move(proc);
        rec.system = std::move(sys);
        rec.process_alive = rec.process.alive;
        rec.observe_kind = rec.process.observe_kind;

        ++sum.total_samples;

        if (rec.observe_kind == ProcessObserveKind::Ok) {
            ++sum.successful_samples;
            sum.vm_rss.add(rec.process.vm_rss);
            sum.vm_size.add(rec.process.vm_size);
            sum.vm_hwm.add(rec.process.vm_hwm);
            sum.vm_swap.add(rec.process.vm_swap);
            sum.pss.add(rec.process.pss);
            sum.private_dirty.add(rec.process.private_dirty);
            if (rec.process.vm_rss.is_available()) {
                if (!sum.window_peak_rss_valid ||
                    rec.process.vm_rss.value > sum.window_peak_rss_bytes) {
                    sum.window_peak_rss_bytes = rec.process.vm_rss.value;
                    sum.window_peak_rss_valid = true;
                }
            }
        } else {
            ++sum.failed_samples;
            switch (rec.observe_kind) {
                case ProcessObserveKind::NotFound:
                    ++sum.not_found_samples;
                    sum.stopped_early = true;
                    sum.stop_reason = "target process not found or terminated";
                    break;
                case ProcessObserveKind::PermissionDenied:
                    ++sum.permission_denied_samples;
                    // Continue sampling — not treated as termination.
                    break;
                case ProcessObserveKind::TransientError:
                    ++sum.transient_error_samples;
                    // Continue — may recover.
                    break;
                case ProcessObserveKind::Ok:
                    break;
            }
        }

        sum.mem_available.add(rec.system.mem_available);

        if (!first && rec.interval_ms > static_cast<std::int64_t>(cfg.interval_ms) * 3 / 2)
            ++sum.delayed_cycles;

        oh_sum += static_cast<double>(oh);
        ++oh_n;
        if (oh > sum.max_collect_overhead_us) sum.max_collect_overhead_us = oh;

        if (history_out && cfg.max_history > 0) {
            if (hist.size() >= cfg.max_history) hist.erase(hist.begin());
            hist.push_back(rec);
        }

        prev = t0;
        first = false;
        if (sum.stopped_early) break;

        auto next = t0 + std::chrono::milliseconds(cfg.interval_ms);
        auto now = clock_();
        if (now < next && now < deadline) {
            auto d = std::chrono::duration_cast<std::chrono::milliseconds>(next - now);
            if (d.count() > 0) sleep_(d);
        }
    }

    if (stop_requested_.load(std::memory_order_relaxed) && !sum.stopped_early) {
        sum.stopped_early = true;
        sum.stop_reason = "stop requested";
    }
    sum.total_elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(clock_() - start).count();
    if (oh_n) sum.avg_collect_overhead_us = oh_sum / static_cast<double>(oh_n);
    if (history_out) *history_out = std::move(hist);
    return sum;
}

} // namespace fars

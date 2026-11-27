# Benchmarks — FeatherTech FARS

## Measurement Philosophy

All claims must be backed by reproducible numbers obtained under documented conditions.

### Categories Tracked

**Memory**
- Peak RSS / PSS
- Average RSS
- KV-cache size
- Memory fragmentation indicators
- OOM frequency

**Performance**
- Model load time
- Time to first token (TTFT)
- Tokens per second (decode)
- Prompt processing speed
- Storage read bandwidth (when relevant)

**Quality** (later stages)
- Standard academic benchmarks (MMLU-Pro, GSM8K, HumanEval, IFEval, etc.) where applicable
- Task-specific accuracy under reduced memory budgets
- Context retention after aggressive eviction

**Device Efficiency**
- Energy per token (where measurable)
- Thermal behavior / throttling
- Battery impact on mobile
- Long-running stability (e.g. 30-minute runs)

## Baseline Requirements

Every experiment must record:
- Exact model + quantization
- Context length
- Hardware profile (device class, available RAM, CPU, etc.)
- Software versions (llama.cpp commit, compiler, OS)
- Generation settings (temperature, top-p, etc.)
- Number of repeated runs and variance

## Initial Research Targets (Indicative Only)

These will be revised or discarded based on evidence:

- Meaningful peak-RAM reduction in selected scenarios versus strong llama.cpp baselines
- Quality degradation kept within a small, measured bound
- OOM rate kept very low under the tested budgets
- Latency regression kept acceptable in memory-saving modes

## Reporting

Results will be published in machine-readable form (JSON/CSV) from the Memory Profiler and later benchmark harnesses, plus human-readable analysis in the repository.
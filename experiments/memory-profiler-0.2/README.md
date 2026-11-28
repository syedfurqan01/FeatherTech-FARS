# FARS Memory Profiler 0.2

Unified C++20 measurement engine for FeatherTech FARS (Phases 1–4.5).

Modules: MetricValue, ProcParser, FileReader, ProcessMemory, SystemMemory, SamplingEngine, StorageBenchmark 2.0.

## Build
```bash
pkg install clang cmake make  # Termux
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## CLI
```bash
./build/fars-memory-profiler --once
./build/fars-memory-profiler --duration 2000 --interval 200
./build/fars-memory-profiler --storage-benchmark --storage-dir $TMPDIR \
  --storage-mode sequential-write --storage-size-mib 8 --storage-duration 500
```

See TERMUX.md for Android device instructions.
`memory-profiler-0.1` is frozen and unmodified.

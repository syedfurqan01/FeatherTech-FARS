# FARS Memory Profiler 0.1

Standalone measurement tool for the FeatherTech FARS project.

**Purpose:** Quantify how a process uses system memory and basic storage performance on Linux and Android/Termux. This is the first concrete milestone of FARS. It deliberately does **not** implement adaptive algorithms (AMAS, PALS, QAM, etc.).

## Features (0.1)

- System RAM detection (total + available)
- Process RSS sampling and peak tracking
- Time-series recording
- JSON + CSV export
- Simple sequential storage read/write micro-benchmark
- Separate platform collectors for Linux and Android
- Unit tests
- No model download, no inference yet

## Requirements

- C++20 compiler (clang++ or g++)
- CMake ≥ 3.16
- Linux or Android (Termux)

## Build (Linux / Termux)

```bash
cd experiments/memory-profiler-0.1
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

## Usage

```bash
./build/fars-memory-profiler --help

# Default 5-second sampling
./build/fars-memory-profiler

# Longer run, custom outputs
./build/fars-memory-profiler --duration 15 --interval 100 --json my.json --csv my.csv
```

## Termux Notes

```bash
pkg update && pkg upgrade -y
pkg install -y cmake clang make ninja git
# then follow the build steps above
```

The tool uses `/proc` interfaces that are available under Termux.

## Design Notes

- The profiler is completely independent of any future FARS runtime.
- llama.cpp integration is planned as an **optional** backend for later milestones (token throughput, load time, etc.). It is not present in 0.1.
- Storage benchmark defaults to 32 MiB to stay friendly on mobile devices.
- All results are local; nothing is uploaded.

## Output Example (summary)

```
=== FARS Memory Profiler 0.1 Summary ===
Platform      : linux
Total RAM     : 15923.45 MiB
Available RAM : 10234.12 MiB
CPU cores     : 8
Peak RSS      : 12.34 MiB
Avg RSS       : 11.98 MiB
Samples       : 25
Storage read  : 845.2 MiB/s
Storage write : 312.7 MiB/s
========================================
```

## Next Steps (not in 0.1)

- Optional llama.cpp backend for model load + generation measurements
- Configurable memory budget monitoring
- Longer-running stability tests
- Comparison harness for full-load vs memory-mapped baselines

---

Part of [FeatherTech-FARS](https://github.com/syedfurqan01/FeatherTech-FARS).
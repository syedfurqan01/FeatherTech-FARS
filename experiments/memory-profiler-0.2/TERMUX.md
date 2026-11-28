# Termux (Poco X3 Pro / ARM64) — FARS 0.2

## Install packages
```bash
pkg update && pkg install -y clang cmake make git zip
```

## Place sources
Extract the ZIP so you have:
```text
~/FeatherTech-FARS/experiments/memory-profiler-0.2/
```

Or clone:
```bash
cd ~
git clone https://github.com/syedfurqan01/FeatherTech-FARS.git
cd FeatherTech-FARS/experiments/memory-profiler-0.2
```

## Build
```bash
cd ~/FeatherTech-FARS/experiments/memory-profiler-0.2
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Validate
```bash
./build/fars-memory-profiler --once
./build/fars-memory-profiler --duration 2000 --interval 250
./build/fars-memory-profiler --storage-benchmark --storage-dir "$TMPDIR" \
  --storage-mode sequential-write --storage-size-mib 8 --storage-duration 500
./build/fars-memory-profiler --storage-benchmark --storage-dir "$TMPDIR" \
  --storage-mode sequential-read --storage-size-mib 8 --storage-duration 500
```

Use only app-writable directories (e.g. `$TMPDIR` or `~/storage/shared` after `termux-setup-storage`).

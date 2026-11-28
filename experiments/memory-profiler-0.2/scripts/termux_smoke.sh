#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc 2>/dev/null || echo 2)"
ctest --test-dir build --output-on-failure
./build/fars-memory-profiler --once
./build/fars-memory-profiler --duration 1000 --interval 200
./build/fars-memory-profiler --storage-benchmark --storage-dir "${TMPDIR:-/tmp}" \
  --storage-mode sequential-write --storage-size-mib 4 --storage-duration 300
echo "smoke OK"

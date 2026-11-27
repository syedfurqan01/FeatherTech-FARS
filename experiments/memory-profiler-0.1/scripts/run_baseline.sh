#!/data/data/com.termux/files/usr/bin/bash
# Simple helper for Termux / Linux baseline runs.
# Does not download any models.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
OUTDIR="${1:-$ROOT/results}"

mkdir -p "$OUTDIR"

if [ ! -x "$BUILD/fars-memory-profiler" ]; then
  echo "Building profiler..."
  cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$BUILD"
fi

STAMP=$(date +%Y%m%d_%H%M%S)
JSON="$OUTDIR/baseline_${STAMP}.json"
CSV="$OUTDIR/baseline_${STAMP}.csv"

echo "Running baseline (10 s)..."
"$BUILD/fars-memory-profiler" \
  --duration 10 \
  --interval 200 \
  --json "$JSON" \
  --csv "$CSV"

echo "Reports written:"
echo "  $JSON"
echo "  $CSV"
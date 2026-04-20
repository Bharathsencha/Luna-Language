#!/bin/bash
echo "=================================================="
echo "      LUNA vs PYTHON PERFORMANCE BENCHMARK        "
echo "=================================================="

ZIG_BIN="${ZIG:-zig}"
ZIG_GLOBAL_CACHE_DIR="${ZIG_GLOBAL_CACHE_DIR:-benchmark/.zig-global-cache}"
ZIG_LOCAL_CACHE_DIR="${ZIG_LOCAL_CACHE_DIR:-benchmark/.zig-cache}"
export ZIG_GLOBAL_CACHE_DIR

mkdir -p "$ZIG_GLOBAL_CACHE_DIR" "$ZIG_LOCAL_CACHE_DIR"

echo "[1/8] Building Luna with Aggressive Optimizations..."
${MAKE:-make} clean > /dev/null
${MAKE:-make} > /dev/null
echo "  -> Compiler Flags: -O3 -march=native -flto=auto"
echo "  -> Build Complete!"

echo ""
echo "--- 300x300 MATRIX MULTIPLICATION ---"
echo "[2/8] Zig Benchmark (Native Matrix Mul)..."
"$ZIG_BIN" run --cache-dir "$ZIG_LOCAL_CACHE_DIR" -O ReleaseFast benchmark/matrix_native.zig -lc
echo "[3/8] Zig Benchmark (Contiguous Matrix Mul)..."
"$ZIG_BIN" run --cache-dir "$ZIG_LOCAL_CACHE_DIR" -O ReleaseFast benchmark/matrix_contiguous.zig -lc
echo "[4/8] Luna Benchmark (Native Matrix Bridge)..."
./bin/luna benchmark/luna_bench.lu

echo ""
echo "--- 1M VECTOR MULTIPLICATION ---"
echo "[5/8] Zig Benchmark (Native Vector Math)..."
"$ZIG_BIN" run --cache-dir "$ZIG_LOCAL_CACHE_DIR" -O ReleaseFast benchmark/vector_native.zig -lc
echo "[6/8] Zig Benchmark (Contiguous Vector Math)..."
"$ZIG_BIN" run --cache-dir "$ZIG_LOCAL_CACHE_DIR" -O ReleaseFast benchmark/vector_contiguous.zig -lc
echo "[7/8] Luna Benchmark (Native Vector Bridge)..."
./bin/luna benchmark/vector_luna.lu

echo ""
echo "--- 1M ENVIRONMENT LOOKUPS ---"
echo "[8/8] Zig vs Luna..."
"$ZIG_BIN" run --cache-dir "$ZIG_LOCAL_CACHE_DIR" -O ReleaseFast benchmark/env_native.zig -lc
./bin/luna benchmark/env_luna.lu

echo ""
echo "=================================================="
echo "               BENCHMARK COMPLETE                 "
echo "=================================================="

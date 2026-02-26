#!/bin/bash
echo "=================================================="
echo "      LUNA vs PYTHON PERFORMANCE BENCHMARK        "
echo "=================================================="

echo "[1/8] Building Luna with Aggressive Optimizations..."
${MAKE:-make} clean > /dev/null
${MAKE:-make} > /dev/null
echo "  -> Compiler Flags: -O3 -march=native -flto=auto"
echo "  -> Build Complete!"

echo ""
echo "--- 300x300 MATRIX MULTIPLICATION ---"
echo "[2/8] Python Benchmark (Native Matrix Mul)..."
python3 benchmark/python_native.py
echo "[3/8] Python Benchmark (NumPy Matrix Mul)..."
python3 benchmark/python_numpy.py
echo "[4/8] Luna Benchmark (Native Matrix Bridge)..."
./bin/luna benchmark/luna_bench.lu

echo ""
echo "--- 1M VECTOR MULTIPLICATION ---"
echo "[5/8] Python Benchmark (Native Vector Math)..."
python3 benchmark/vector_native.py
echo "[6/8] Python Benchmark (NumPy Vector Math)..."
python3 benchmark/vector_numpy.py
echo "[7/8] Luna Benchmark (Native Vector Bridge)..."
./bin/luna benchmark/vector_luna.lu

echo ""
echo "--- 1M ENVIRONMENT LOOKUPS ---"
echo "[8/8] Python vs Luna..."
python3 benchmark/env_native.py
./bin/luna benchmark/env_luna.lu

echo ""
echo "=================================================="
echo "               BENCHMARK COMPLETE                 "
echo "=================================================="

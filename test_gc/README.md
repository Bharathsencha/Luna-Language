# Luna GC Stress & Benchmark Suite (`test_gc/`)

This directory contains specialized workloads for testing the garbage collector, measuring pause times, memory footprints (Max RSS), allocation throughput, and cycle collection under heavy heap pressure.

For the full detailed documentation, see [docs/testing.md](../docs/testing.md) and [docs/gc.md](../docs/gc.md).

---

## Running GC Benchmarks

```bash
# Run standard Luna vs Go benchmark
make test-gc

# Run GC safety verification under forced collection
make test-gc-safety

# Run multi-run Luna vs Go stress comparison
make test-gc-three

# Run multi-run Luna vs Python comparison
make test-py

# Run multi-run Luna vs Java G1 comparison
make test-java-gc

# Run unified multi-language benchmark
make test-gc-all
```

---

## Workload Files

| Benchmark | Description | What It Stresses |
| :--- | :--- | :--- |
| `alloc_heavy.lu` | 750k string allocations appended to a list | Allocation throughput, nursery expansion, minor GC pauses |
| `cycles.lu` | 50k template nodes wired into a circular ring graph | Cyclic reference garbage collection without refcount leaks |
| `cycles_5k.lu` | 5k node variant of circular graph | Fast regression check for cycle detection |
| `strings.lu` | 500k heavy string concatenations | String heap allocator, string interning cache, buffer recycling |
| `binary_trees.lu` | Recursive tree construction of depth 17 (12 iterations) | Stack root scanning, recursion depth, bulk tree reclamation |
| `long_live.lu` | 200k retained items while generating 400k scratch items | Generational survival, remembered sets, old vs young heap |
| `map_churn.lu` | 2M map ops (set, delete, get) with pseudo-random keys | Hash map tombstone reclamation, rehashing under GC |
| `object_graph.lu` | 100k interconnected nodes with dynamic edge mutation | SATB write barrier correctness during active marking |
| `concurrent_map.lu` | Map operations under stress | Incremental stepping and map concurrency |
| `test_inc.lu` | Micro-step incremental test | Incremental GC step boundaries |

---

## Benchmark Drivers

- `gc_bench.py`: Executes Luna and Go benchmarks, parsing GC pauses via `LUNA_GC_STATS=1` and `GODEBUG=gctrace=1`, appending results to `gc_benchmark_results.csv`.
- `gc_bench_all.py`: Unified multi-language test driver comparing Luna, Go, OpenJDK Java (G1), and CPython across 3x and 8x scale factors.
- `run_benchmarks.py`: Flexible multi-run benchmark harness with CSV aggregation.

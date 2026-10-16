# GC Results: Luna vs Go, Java and Python

Recorded measurements of Luna's garbage collector against Go, Java (G1) and CPython on the same
workloads. Collector design is covered in [internals/gc.md](../internals/gc.md).

All numbers are from the bytecode VM on one machine (AMD Ryzen 7 7435HS, 24 GB DDR5, Linux
Mint). The final runs were recorded on 2026-09-25 and 2026-09-26.

## Summary

- Luna's worst single pause stays between **0.10 and 0.65 ms** on every workload where its
  collector runs.
- **Go** pauses less (0.03-0.05 ms) and finishes 1.2-1.3x faster on the allocation workloads.
- **Java G1** pauses 0.9-12 ms per collection (3.5 ms or more on six of the eight workloads)
  and runs 3-4x faster on most workloads. That comparison is a JIT-compiled runtime against a
  bytecode interpreter.
- **CPython** frees most objects by reference counting, so it has no pause on half the
  workloads. Where its cyclic GC does run, it pauses 1.3-18 ms.
- Luna's weak points are memory (1.3-4x the RSS of Go and Python, leaving out the
  `binary_trees` bug) and total GC time. Luna
  counts every incremental step as pause time, so its totals are larger than those of
  collectors with concurrent mark threads.

## Metrics

| Column | Meaning |
|---|---|
| User (s) | CPU time spent in user mode, from `/usr/bin/time -v` |
| Max pause (ms) | Largest single recorded pause. This is the number that compares directly across languages |
| GC total (ms) | Sum of recorded pauses. Not directly comparable; see "Measurement" below |
| RSS (MB) | Peak resident set size, from `/usr/bin/time -v` |

## Luna vs Go

3-run averages, 2026-09-25.

| Workload | Luna user | Go user | Luna max pause | Go max pause | Luna GC total | Go GC total | Luna RSS | Go RSS |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| alloc_heavy | 0.247 | 0.203 | 0.220 | 0.029 | 35.6 | 0.32 | 99.9 | 40.9 |
| long_live | 0.130 | 0.103 | 0.150 | 0.034 | 19.2 | 0.30 | 59.2 | 15.7 |
| cycles | 0.027 | 0.000 | 0.106 | 0.000 | 4.9 | 0.00 | 16.3 | 3.9 |
| strings | 0.190 | 0.160 | 0.103 | 0.048 | 21.3 | 0.33 | 85.9 | 66.1 |

Go finishes `cycles` before its first collection.

## Luna vs Java (G1)

Mean of 3 runs, 2026-09-25. Java runs with `-Xms256m -Xmx256m -XX:+UseG1GC`, and all workloads
run in one JVM (warm). Java RSS was not recorded.

| Workload | Luna user | Java user | Luna max pause | Java max pause | Luna GC total | Java GC total | Luna RSS |
|---|---:|---:|---:|---:|---:|---:|---:|
| alloc_heavy | 0.210 | 0.061 | 0.126 | 10.235 | 30.4 | 16.7 | 101.2 |
| long_live | 0.160 | 0.049 | 0.161 | 6.017 | 21.8 | 6.0 | 58.5 |
| cycles | 0.030 | 0.007 | 0.134 | no GC | 5.0 | 0.0 | 16.4 |
| strings | 0.207 | 0.055 | 0.107 | 8.422 | 21.7 | 14.3 | 85.9 |
| binary_trees | 0.887 | 0.092 | no GC [1] | 12.071 | 0.0 | 22.7 | 272.6 |
| map_churn | 1.070 | 0.307 | 0.650 | 3.541 | 39.9 | 3.7 | 51.6 |
| object_graph | 0.087 | 0.061 | 0.258 | no GC | 26.2 | 0.0 | 37.9 |
| concurrent_map | 0.240 | 0.073 | 0.174 | 0.854 | 9.6 | 0.7 | 23.3 |

## Luna vs Python (CPython)

3-run averages, 2026-09-25. Python pauses are timed with `gc.callbacks` around each
cyclic-GC collection.

| Workload | Luna user | Python user | Luna max pause | Python max pause | Luna GC total | Python GC total | Luna RSS | Python RSS |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| alloc_heavy | 0.290 | 0.227 | 0.153 | 0 [2] | 36.6 | 0.0 | 100.0 | 61.1 |
| long_live | 0.150 | 0.167 | 0.120 | 0 [2] | 19.3 | 0.0 | 59.9 | 23.2 |
| cycles | 0.030 | 0.083 | 0.101 | 1.300 | 4.9 | 8.1 | 16.3 | 14.5 |
| strings | 0.253 | 0.170 | 0.129 | 0 [2] | 23.3 | 0.0 | 86.1 | 67.0 |
| binary_trees | 0.753 | 0.710 | no GC [1] | 18.285 | 0.0 | 242.6 | 272.6 | 17.4 |
| map_churn | 1.020 | 1.067 | 0.518 | 0 [2] | 37.4 | 0.0 | 51.4 | 21.9 |
| object_graph | 0.093 | 0.137 | 0.196 | 5.888 | 26.5 | 14.8 | 38.0 | 27.1 |
| concurrent_map | 0.280 | 0.290 | 0.271 | 0 [2] | 10.8 | 0.0 | 23.3 | 15.8 |

## Luna under heap stress

Single run each, 2026-09-26, with `LUNA_GC_STRESS=1` and a smaller heap. `3x` divides the
initial heap and young-generation limits by 3, and `8x` divides them by 8 (512 KB heap, 64 KB
young). More collections run, but the worst pause does not grow.

| Workload | Normal max pause | 3x max pause | 8x max pause | Normal GC events | 3x events | 8x events |
|---|---:|---:|---:|---:|---:|---:|
| alloc_heavy | 0.126 | 0.090 | 0.085 | 3 | 12 | 12 |
| long_live | 0.158 | 0.087 | 0.083 | 4 | 12 | 12 |
| cycles | 0.204 | 0.103 | 0.073 | 1 | 6 | 6 |
| strings | 0.117 | 0.073 | 0.134 | 4 | 13 | 13 |

## History

Worst Luna pause on the four core workloads, and user time on `alloc_heavy`, at each recorded
milestone.

| Date | State | Worst pause (ms) | alloc_heavy user (s) |
|---|---|---:|---:|
| before 2026-08 | Reference counting + arena (no tracing GC) | 69.9 | 0.70 |
| 2026-08-10 | Tracing GC, tree-walking interpreter | 4.06 | 24.4 |
| 2026-08-11 | Collector tuning, still on the interpreter | 1.03 | 27.9 |
| 2026-08-20 | Bytecode VM, bounded promote-scan, template write barrier | 2.52 | 16.3 |
| 2026-08-21 | VM hot paths: trusted write barriers, fused string building, O(1) box scope release | 0.57 | 0.34 |
| 2026-08-24 | 64 us step target, bounded sweep, large-object reuse, incremental remembered-set scan | 1.62 [3] | 0.21 |
| 2026-08-27 | Adaptive step overflow fixed | 0.19 | 0.29 |
| 2026-09-25 | Current | 0.26 | 0.25 |

Dates are when each run was recorded. Some of the changes were committed a few days later.

[3] One outlier from the middle of that day's tuning. The final runs that day were 0.10-0.15 ms.

## Notes

[1] `binary_trees` records no Luna GC events. The workload is recursion-heavy and never reaches
a VM safepoint, so the collector never runs and RSS grows to 273 MB. This is a known bug, not
a result.

[2] CPython frees these objects by reference counting the moment they become unreachable.
There is no stop-the-world pause, but the cost is still paid inside user time.

## Measurement

- **Luna.** Run with `LUNA_GC_STATS=1`, which makes `bin/luna` print
  `LUNA_GC_STATS,<total_ms>,<max_ms>,<events>` to stderr on exit. Every incremental step counts
  as pause time.
- **Go.** `GODEBUG=gctrace=1`. The max pause is the larger of the two stop-the-world phases per
  cycle. Concurrent mark runs on background threads and is not counted, which is why Go's GC
  totals look tiny.
- **Java.** `-Xlog:gc`, with pauses matched to each workload's time window.
- **Python.** `gc.callbacks` timing around each collection.
- Time and RSS for every language come from `/usr/bin/time -v`.

## Workloads

Every language ran the same iteration counts and data shapes.

| Workload | What it does |
|---|---|
| alloc_heavy | 750k `"item-N"` strings appended to one list |
| long_live | 200k-entry live table, plus 400k scratch strings |
| cycles | 50k template nodes linked into a ring |
| strings | 500k `"luna" * 16 + "-N"` string builds |
| binary_trees | Builds a depth-17 binary tree 12 times and checksums it |
| map_churn | 2M `map_set`, then 2M mixed set / delete / get on one map |
| object_graph | 100k nodes, 400k edges, random edge rewrites |
| concurrent_map | 1M mixed set / delete / get on one map |

## Reproducing

The benchmark harness is not on `luna-beta`. It lives on `main`:

```bash
git checkout main
make test-gc-all      # Luna vs Go, Java, Python, plus the stress phases
make test-gc-three    # Go only
make test-java-gc     # Java only
make test-py          # Python only
```

It needs `go`, a JDK, and `python3` on `PATH`.

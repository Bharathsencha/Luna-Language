# Luna GC vs Go GC Benchmark Results

This directory contains the Go implementation of the Luna GC benchmarks. The goal is to compare the performance and memory efficiency of Luna's generational Immix-style tracing GC against Go's highly optimized concurrent mark-and-sweep GC.

Luna programs run on the **bytecode VM** (AST → `vm/luna_compiler.c` → `vm/luna_vm.c` computed-goto dispatch). Results below are 3-run averages; regenerate with `make test-gc-three` (summary CSV in `stress_test/stress_results.csv`).

## Benchmark Results

| Benchmark | Language | User Time (s) | GC Max Pause (ms) | GC Total (ms) | Max RSS (MB) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **alloc_heavy** | Luna | 0.270 | **0.122** | 14.4 | 96.9 |
| | Go | 0.227 | 0.033 | 0.3 | 47.7 |
| **long_live** | Luna | 0.150 | **0.136** | 8.8 | 59.8 |
| | Go | 0.110 | 0.057 | 0.3 | 15.1 |
| **cycles** | Luna | 0.030 | **0.119** | 2.6 | 16.5 |
| | Go | 0.000 | 0.000 | 0.0 | 4.0 |
| **strings** | Luna | 0.200 | **0.108** | 8.5 | 83.4 |
| | Go | 0.163 | 0.037 | 0.3 | 65.0 |

### Metric Definitions
- **User Time (s)**: Total CPU time executing the program's own code (user-mode).
- **GC Max Pause (ms)**: Worst single recorded pause. For Go this is the largest STW pause from `GODEBUG=gctrace=1` (the A and C stop points); Luna records every incremental step.
- **GC Total (ms)**: Total pause bill across the run. Luna counts every incremental step; Go counts its STW stop points (A + C) only — Go's concurrent mark work runs on background threads and is not pause time, so this column is not directly comparable.
- **Max RSS (MB)**: Peak resident set size.

## Analysis

### 1. GC Pauses
Luna's worst-case pause is **~0.1–0.15ms** across all workloads — the same league as Go
(0.03–0.06ms). The pause profile comes from:

- a 64µs incremental pause target with deadline-bounded drain and sweep steps
- incremental remembered-set scanning (1024-entry batches) so mark-roots work never spikes
- dead large objects reused via a per-heap free list (no munmap storms mid-sweep)
- a bounded promote-scan (1024 children) replacing unbounded container traces

### 2. Execution Time (User Time)
Allocation-heavy workloads are now within ~1.2–1.4x of Go (was 200-400x). The wins came
from moving to the bytecode VM and then cutting its hot paths:

- write barriers skip the O(blocks) managed-payload walk (trusted variants + O(log n) index)
- `repeat(const, N) + to_string(int)` chains fuse into a single allocation (VM_OP_FMT)
- box scope release is O(1) when no boxes are live (high-water mark)
- interned names cached per chunk; `to_string(int)` and two-string concat fast paths

### 3. Memory Usage (Max RSS)
Luna uses ~1.5–4x Go's RSS. The generational Immix-style collector trades memory density
for pause control; block reuse/compaction is a known open item.

## Methodology Validation

Both Luna and Go benchmarks use equivalent workloads and measurement approaches:

- **Workload parity**: each Go file mirrors a corresponding Luna benchmark script
  in `test_gc/` (same iteration counts, same data structures, same allocation patterns)
- **Timing/RSS**: both measured externally via `/usr/bin/time -v`
- **GC stats**: Luna reports internal GC metrics; Go's come from `GODEBUG=gctrace=1`

## Conclusion
Luna's GC now holds ~0.1–0.15ms worst-case pauses on the shipped profile — comparable to
Go's pause behavior — with user time within ~1.2–1.4x of Go on allocation-heavy loops.

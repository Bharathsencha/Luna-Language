# Luna GC vs Go GC Benchmark Results

This directory contains the Go implementation of the Luna GC benchmarks. The goal was to compare the performance and memory efficiency of Luna's generational Immix GC against Go's highly optimized concurrent mark-and-sweep GC.

## Benchmark Results

| Benchmark | Language | User Time (s) | Sys Time (s) | Max RSS (MB) |
| :--- | :--- | :--- | :--- | :--- |
| **alloc_heavy** | Luna | 8.77 | 0.15 | 197.66 |
| | Go | 0.37 | 0.19 | 55.62 |
| **long_live** | Luna | 0.58 | 0.07 | 141.50 |
| | Go | 0.16 | 0.03 | 16.92 |
| **cycles** | Luna | 0.09 | 0.01 | 30.78 |
| | Go | 0.01 | 0.00 | 4.04 |
| **strings** | Luna | 7.34 | 0.16 | 235.44 |
| | Go | 0.27 | 0.14 | 68.17 |

### Metric Definitions
- **User Time (s)**: The total time the CPU spent executing the program's code itself (user-mode).
- **Sys Time (s)**: The total time the CPU spent on system calls (kernel-mode) on behalf of the program (e.g., memory allocation, I/O).
- **Max RSS (MB)**: The maximum "Resident Set Size," which represents the peak physical memory occupied by the process during its execution.

## Analysis

### 1. Performance (Execution Time)
Go is significantly faster in all benchmarks (often 10x-30x faster).
- **Interpretation Overhead**: Luna's performance gap is largely due to it being an interpreted language. The Go versions are compiled and executed directly by the CPU.
- **Allocation Efficiency**: Go's allocator and stack management are extremely mature, which contributes to the speed in `alloc_heavy` and `strings`.

### 2. Memory Usage (Max RSS)
Luna uses significantly more memory than Go for the same workloads.
- **Memory Footprint**: Luna's memory usage is 3x-8x higher. This suggests that Luna's object representation or heap management has higher overhead.
- **GC Strategy**: Luna uses a generational Immix-style GC, while Go uses a non-moving concurrent collector. Go's lower RSS might be due to more aggressive scavenging or more compact object representations.

### 3. GC Stats (Luna Only)
The Luna GC benchmarks provided detailed stats:
- **GC Events**: Consistently around 16 events per test.
- **Average GC Time**: Very fast (often < 0.5ms), indicating that while it triggers frequently, individual pauses are short.

## Methodology Validation

Both Luna and Go benchmarks use equivalent workloads and measurement approaches:

- **Workload parity**: each Go file mirrors a corresponding Luna benchmark script
  in `test_gc/` (same iteration counts, same data structures, same allocation patterns)
- **Wall-clock timing**: Go uses `time.Now()` / `time.Since()`, Luna uses `clock()`
- **RSS measurement**: both are measured externally via `/usr/bin/time -v`
- **GC stats**: Luna additionally reports internal GC metrics (event count, pause
  times) which Go does not expose at the same granularity

This setup gives a fair apples-to-apples comparison of allocation throughput and
peak memory footprint, while acknowledging that Luna is interpreted and Go is
compiled.

## Conclusion
While Go dominates in raw performance and memory efficiency, Luna's GC shows very low average pause times, which is promising for its intended use as a language with low-latency garbage collection. The higher memory footprint is a known trade-off for generational moving collectors like Immix.

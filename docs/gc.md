# Luna GC

## Runtime Layout

Luna uses three distinct memory domains:

- the tracing GC for runtime heap values such as strings, lists, dense lists, maps, closures, and GC-owned backing buffers
- the AST arena for parser-owned memory with bulk lifetime semantics
- the unsafe runtime for raw pointer buffers created inside `unsafe { ... }`

These domains solve different lifetime problems and intentionally remain separate.

Current implementation files:

- [include/gc.h](/home/bharath/Desktop/Luna-Language%20%20(main)/include/gc.h)
- [src/gc.c](/home/bharath/Desktop/Luna-Language%20%20(main)/src/gc.c)
- [src/gc_visit.c](/home/bharath/Desktop/Luna-Language%20%20(main)/src/gc_visit.c)

---

## Collector Model

Luna's active runtime collector is a tracing heap with:

- tri-color marking
- incremental safe points
- young/old generation tracking
- remembered-set support for old-to-young references
- SATB-style write-barrier semantics on runtime-managed heap edges

The practical goal of the current collector is pause control first, then total work and memory density second.

---

## SATB Barrier

Luna preserves a logical heap snapshot during marking.

In practice:

- marking starts from the root snapshot of the current cycle
- when a heap reference is overwritten, the overwritten edge is treated conservatively
- objects reachable at the beginning of the cycle are not lost just because the mutator removes the last visible edge mid-mark

This keeps incremental marking correct while the interpreter continues running between safe points.

---

## Arena And Unsafe

The arena stays because parser-owned memory behaves very differently from runtime heap memory.

The arena is the right fit for:

- AST nodes
- parser scratch storage
- bulk free at end of parse/program lifetime

Unsafe memory stays separate. Inside `unsafe { ... }`:

- `alloc(n)` creates raw manual buffers
- those buffers are tracked by the unsafe runtime, not by the tracing collector
- normal Luna values still remain under the regular runtime rules
- raw pointers are blocked from being stored into GC-managed containers

`unsafe` is a narrow bypass for raw pointer work, not a second general-purpose heap for the language.

---

## Benchmark Suite

The shipped GC suite lives in [test_gc/README.md](/home/bharath/Desktop/Luna-Language%20%20(main)/test_gc/README.md) and runs with:

```bash
make test-gc
```

Reference CSVs:

- [test_gc/csv/baseline_rc_arena.csv](/home/bharath/Desktop/Luna-Language%20%20(main)/test_gc/csv/baseline_rc_arena.csv)
- [test_gc/csv/tracing_gc_arena.csv](/home/bharath/Desktop/Luna-Language%20%20(main)/test_gc/csv/tracing_gc_arena.csv)

The shipped cases cover different pressure shapes:

- `alloc_heavy.lu` — high retention pressure
- `cycles.lu` — graph-style reachability
- `long_live.lu` — long-lived data with churn around it
- `strings.lu` — allocation-heavy string traffic

`test_gc` is a regression suite, not a full production capacity model. It answers the practical questions Luna needs every day: did a collector change lose live objects, did pause behavior regress badly, did a runtime optimization accidentally push the scheduler the wrong way.

---

## Reading The Metrics

| Metric | Practical reading |
|---|---|
| `script` | benchmark file that was run |
| `user_s` | CPU time spent in user space during the whole process |
| `sys_s` | CPU time spent in kernel/system calls during the whole process |
| `max_rss_mb` | peak resident set size in MB |
| `gc_ms` | total pause bill paid across the whole run |
| `gc_ms_max` | worst hitch the program experienced |
| `gc_events` | how many times the program had to stop for recorded GC work |
| `gc_ms_avg` | average size of each recorded pause |

Two numbers matter most for pause behavior: `gc_ms_max` tells you how bad the worst hitch was, `gc_ms` tells you how much total stop-the-world pause time accumulated over the full run.

---

## Current Numbers

### Tracing GC

| Metric | Practical reading |
|---|---|
| `gc_ms_max` | worst hitch the program experienced |
| `gc_ms` | total pause bill paid across the whole run |
| `gc_events` | how many times the program had to stop for recorded GC work |
| `gc_ms_avg` | average size of each recorded pause |
| `max_rss_mb` | how much live memory pressure the process reached at peak |

| script | user_s | sys_s | max_rss_mb | gc_ms | gc_ms_max | gc_events | gc_ms_avg |
|---|---:|---:|---:|---:|---:|---:|---:|
| `alloc_heavy.lu` | `8.22` | `0.16` | `197.04` | `2.093` | `0.220` | `16` | `0.131` |
| `cycles.lu` | `0.09` | `0.02` | `30.61` | `3.205` | `0.261` | `16` | `0.200` |
| `long_live.lu` | `0.56` | `0.08` | `141.52` | `1.652` | `0.135` | `16` | `0.103` |
| `strings.lu` | `8.04` | `0.21` | `235.19` | `0.847` | `0.068` | `16` | `0.053` |

The current checked suite is sub-ms on `gc_ms_max` across every shipped GC workload.

### Baseline RC + Arena

| Metric | Practical reading |
|---|---|
| `gc_ms_max` | worst hitch the program experienced |
| `gc_ms` | total pause bill paid across the whole run |
| `gc_events` | how many times the program had to stop for recorded GC work |
| `gc_ms_avg` | average size of each recorded pause |
| `max_rss_mb` | how much live memory pressure the process reached at peak |

| script | user_s | sys_s | max_rss_mb | gc_ms | gc_ms_max |
|---|---:|---:|---:|---:|---:|
| `alloc_heavy.lu` | `0.70` | `0.06` | `61.30` | `214.594` | `69.917` |
| `cycles.lu` | `0.09` | `0.01` | `22.81` | `36.969` | `9.942` |
| `long_live.lu` | `0.54` | `0.01` | `19.85` | `132.579` | `18.459` |
| `strings.lu` | `0.93` | `0.05` | `65.36` | `208.107` | `46.140` |

---

## Total GC Time

Low `gc_ms_max` is the pause-latency win, but `gc_ms` still reflects how much GC work was paid for across the full run.

Total GC time remains noticeable because:

- the collector still has to trace the live graph
- many tiny pauses still add up to meaningful cumulative pause time
- remembered-set bookkeeping and rescans still cost work
- retention-heavy workloads still push traversal locality and memory density in the wrong direction

A workload like `alloc_heavy.lu` can be comfortably sub-ms on worst pause and still accumulate non-trivial total GC time for exactly these reasons.

---

## Relation To Go

Luna and Go share broad conceptual overlap in their collector design, but are not at the same runtime depth or scale.

The overlap:

- both care about tracing correctness rather than pure RC lifetime
- both use tri-color style marking ideas
- both rely on write barriers to keep concurrent or incremental progress correct
- both treat pause behavior as a first-class runtime concern

The differences:

- Go has a much more mature concurrent scheduler and pacing system
- Go has production-grade stack scanning, assist work, heap-goal management, and decades of runtime tuning
- Luna is currently an incremental tracing collector with stop-the-world slices, not a fully concurrent runtime
- Luna still has open structural work around evacuation, compaction, and long-term layout strategy

The comparison is conceptual similarity, not runtime equivalence.

---

## Tuning Changes

The sub-ms result came from runtime and scheduling changes, not from simplifying the benchmark workloads.

| Fix | What changed | What it helped |
|---|---|---|
| Incremental minor stepping | young-generation marking now resumes across safe points instead of bunching all minor work into one pause | lower `gc_ms_max`, especially on churn-heavy workloads |
| Low-pause benchmark profile | the GC benchmark runner now gives the heap enough logical headroom before forcing expensive major work | reduced premature major collections and lowered pause spikes |
| AST-cached runtime string literals | hot string literals stop allocating a fresh runtime string every loop iteration | reduced allocation churn and write-barrier pressure |
| Direct raw string builders | concat and repeat build final strings directly instead of bouncing through extra temporary buffers | reduced string-path allocation volume and cut string-heavy pause pressure |
| Remembered-set and barrier cleanup | old-to-young edges and overwritten references are tracked more consistently during incremental work | preserved correctness while allowing smaller GC slices |

Before these changes, Luna was paying avoidable work in hot runtime paths: loop-heavy string code kept rebuilding identical literals, concat and repeat created unnecessary temporary buffers, minor collection work could cluster into larger stop-the-world chunks, and tight benchmark heap limits forced earlier major work than the workload actually needed.

---

## Known Limits

The active runtime is materially better than the old RC baseline on pause time, but it is not finished.

- evacuation is not complete end-to-end
- hole reuse and compaction are not in their final form
- memory density is still weaker than the pause numbers alone suggest
- some retention-heavy cases still push RSS much higher than the older baseline
- very large live heaps would need another round of collector architecture work

---

## Large Heap Scaling

At a scale of roughly one billion objects, the first problems would be:

- metadata overhead per object
- remembered-set and tracing pressure
- RSS growth
- fragmentation sensitivity
- traversal locality limits

The likely next structural requirements at that scale:

- tighter object layouts
- stronger compaction or evacuation
- more disciplined backing-buffer ownership
- more mature concurrent or assist-style tracing

---

## Current Status

Luna now has a tracing collector that supports an honest sub-ms max-pause claim on the shipped benchmark profile. The next work is structural rather than cosmetic: memory density, compaction strategy, and large-heap scalability still need another serious pass.

---
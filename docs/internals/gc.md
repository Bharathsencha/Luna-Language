# Garbage Collector

Luna's runtime heap is managed by an incremental, generational, tri-color tracing collector in
`src/gc.c` (object tracing in `src/gc_visit.c`, VM roots in `src/luna_vm_gc.c`, public API in
`include/gc.h`). Its first priority is pause time, then total GC work, then memory density.

Measured results against Go, Java and Python are in
[results/gc-results.md](../results/gc-results.md).

## Three memory domains

| Domain | Holds | Lifetime | Code |
|---|---|---|---|
| GC heap | strings, lists, dense lists, maps, closures, templates, upvalues, GC-owned buffers | traced | `src/gc.c` |
| AST arena | parser nodes and scratch | freed in bulk after the program | `src/arena.c`, [arena.md](arena.md) |
| Unsafe runtime | raw buffers from `alloc()` inside `unsafe { }` | manual, rule-checked | `src/unsafe_runtime.c`, [unsafe.md](unsafe.md) |

The domains never mix. A raw pointer may not be stored in a GC container (rule R7), and AST
nodes are never traced.

## Heap layout

- **Blocks.** Objects are bump-allocated into 32 KB Immix-style blocks: 128 lines of 256 bytes,
  with a 16-byte-granule mark bitmap. Each object starts with a `GCObject` header; the payload
  follows it (`GC_PAYLOAD(obj)`).
- **Block index.** A sorted index of block start addresses answers "is this pointer
  GC-managed?" in O(log n). It is rebuilt lazily when blocks are added or removed.
- **Large objects.** Anything bigger than a block lives on a separate large-object list. Dead
  large buffers are parked on a free list and reused by the next allocation of the same size,
  instead of being returned to the OS in the middle of a sweep.

## Collection

**Tri-color marking.** Roots (VM registers and frames, globals, open upvalues, deferred calls,
host roots) are shaded gray. Each step pops gray objects, traces their children through the
object's `trace` callback, and blackens them. White objects that remain at the end of the mark
are garbage.

**Generations.** New objects are young. Minor collections trace only the young generation, plus
old objects listed in the remembered set. Objects that survive are promoted to old. Every
`major_interval` minors, or when the heap passes its limit, a major collection traces
everything.

**Write barrier (SATB).** While marking is in progress, overwriting a reference shades the old
target gray. The mark therefore keeps everything that was reachable when the cycle started,
even when the program removes the last edge to an object mid-mark. The same barrier records an
old-to-young store in the remembered set, but only when the stored child is young.

**Promotion scan.** When a container is promoted, its children are scanned for young
references, up to 1024 children (`GC_REMEMBER_SCAN_MAX`). A larger container is remembered
unconditionally and traced later in deadline-bounded slices. Before this cap existed, a large
list buffer could cause a multi-millisecond pause.

**Incremental steps.** Marking, remembered-set scanning (1024 entries per batch) and sweeping
all run in steps with a wall-clock budget of 64 us by default. Each step checks the deadline
and yields, so the worst pause is the step budget plus the cost of one unit of work.

**Safepoints.** The compiler emits `VM_OP_SAFEPOINT` at loop headers and every 16 statements.
The VM enters the collector at every 1024th safepoint. Allocation alone does not trigger a
collection: deep recursion with no loops never reaches a safepoint, which is why
`binary_trees` records no GC events. This is the main known bug.

## Tuning and debugging

Every knob is an environment variable, read once at heap creation.

| Variable | Default | Effect |
|---|---|---|
| `LUNA_GC_STATS` | unset | On exit, print `LUNA_GC_STATS,<total_ms>,<max_ms>,<cycles>,<pauses>` to stderr |
| `LUNA_GC_PAUSE_TARGET_US` | 64 | Wall-clock budget per incremental step |
| `LUNA_GC_INITIAL_HEAP_LIMIT` | 4 MB | Heap size that triggers the first major collection |
| `LUNA_GC_YOUNG_LIMIT` | derived from heap limit | Young allocation volume that triggers a minor |
| `LUNA_GC_MAJOR_INTERVAL` | derived | Minors between majors |
| `LUNA_GC_INCREMENTAL` | 1 | 0 makes every collection stop-the-world |
| `LUNA_GC_INCREMENT_STEPS` | 2048 | Gray objects drained per step, before the deadline check |
| `LUNA_GC_SWEEP_BUDGET` | 16 | Blocks swept per step |
| `LUNA_GC_LARGE_SWEEP_BUDGET` | 128 | Large objects swept per step |
| `LUNA_GC_STRESS` | unset | Divide both collection triggers by 8, so collections run far more often (used by `make test-gc-safety`) |
| `LUNA_GC_VERIFY` | unset | Re-walk the heap from the roots after each collection and check that every reachable object is still valid |
| `LUNA_GC_PAUSE_TRACE` | unset | Log any phase longer than `LUNA_GC_PAUSE_TRACE_US` (default 200) |

```bash
LUNA_GC_STATS=1 ./bin/luna script.lu
env LUNA_GC_STRESS=1 LUNA_GC_VERIFY=1 ./bin/luna test/test_gc_safety.lu
```

## What brought pauses under 1 ms

| Change | Effect |
|---|---|
| 64 us step target, with the deadline checked inside drain and sweep | Worst pause around 0.1-0.15 ms on the core workloads |
| Remembered-set scan split into 1024-entry batches | Removed 0.3-0.45 ms spikes in mark-roots under store-heavy churn |
| Free list for dead large objects | Removed 0.3-1.0 ms `munmap` spikes during sweep, and lowered GC totals |
| 1024-child cap on promotion scan | Bounded promotion work for huge containers |
| Write barrier on template field stores | Fixed lost old-to-young edges through template fields |
| Young-only remembered-set inserts | Smaller remembered set to scan |
| Adaptive step overflow fix | Removed multi-millisecond `alloc_heavy` pauses |
| Move to the bytecode VM | Far fewer GC events per unit of work, and about 100x the throughput |

## Known limits

- Allocation does not trigger a collection (see Safepoints above).
- No evacuation or compaction yet, so fragmented blocks are reused only through hole reuse,
  which is not yet in its final form.
- RSS runs 1.3-4x higher than Go or CPython on the same workloads.
- Collection is single-threaded. There are no concurrent mark threads, so all GC work counts
  as pause time.
- Total GC time is noticeable on retention-heavy workloads (20-40 ms per run), even when every
  individual pause is small.

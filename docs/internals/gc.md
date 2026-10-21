# Garbage Collector

Every compiled Luna program carries a garbage collector. It is part of the runtime, written in
Luna: `compiler/runtime/luna_gc.lu`. It is a conservative, non-moving, stop-the-world
mark-and-sweep collector. It does not need type maps from the compiler: it treats every
aligned word it scans as a possible pointer.

## Memory domains

| Domain | Holds | Lifetime |
|---|---|---|
| GC heap | strings, lists, dense lists, maps, closures and their captured cells, templates, blocs | traced and freed by the collector |
| Box table | `box` buffers | freed when their scope ends, by `free`, or with their owning template |
| `alloc()` buffers | raw memory from `unsafe` blocks | freed by `free` / `defer`; registered as root ranges, never freed by the collector |

The compiler itself runs on the same runtime, so the compiler is also a client of this
collector.

## Heap layout

- **Small objects** (up to 8192 bytes) are served from 64 KB pages. A page holds objects of one
  size class; there are 26 classes from 16 to 8192 bytes. A page keeps a bump index for objects
  never handed out and a free list of recycled ones.
- **Atomic objects** (strings, doubles, byte buffers, dense list data) are allocated from
  separate pages and are never scanned for pointers.
- **Large objects** (over 8192 bytes) get pages of their own.
- A page table (a hash table keyed by page address) answers "is this word a pointer into the
  heap, and to which object?". Interior pointers keep an object alive. Every allocation is one
  byte larger than requested, so a pointer just past the end still points inside the object.
- New memory is zeroed.
- A page whose objects are all dead is returned to a pool and can be reused for any size class.

## Collection

A collection is triggered inside an allocation. It happens when the bytes allocated since the
last collection exceed a threshold: 16 MiB at the start, and after each collection the larger
of 16 MiB and the live bytes that survived. (With `LUNA_RT_GC_STRESS`, see below, also every
Nth allocation.)

1. **Roots.** The data segment (all module-level variables and constants of the program and the
   runtime), every registered root range (the `alloc()` buffers), the machine stack of the
   thread running the program, and the registers (spilled to the stack first) are scanned.
2. **Mark.** Each scanned word that points into a live object marks it, and the object's
   contents are scanned in turn, using an explicit mark stack. Atomic objects are marked but
   not scanned.
3. **Data hook.** Boxes owned by templates that were not marked are freed.
4. **Sweep.** Unmarked objects go back to their page's free list. Empty pages go to the pool.

Because the collector is conservative, a stale integer that looks like an address can keep an
object alive longer than needed. It can never free a live object, as long as every pointer is
kept in a scanned location (the stack, registers, the data segment, a registered range, or a
reachable heap object).

The collector runs on the program's single thread and pauses the program for the whole
collection.

## Environment variables

Read once at program start, by the compiled program itself:

| Variable | Effect |
|---|---|
| `LUNA_RT_GC_STRESS=N` | Collect on every Nth allocation. `1` collects on every allocation. Used to find missing roots |
| `LUNA_RT_GC_STATS=1` | At exit, print `LUNA_RT_GC_STATS,<n> collections,<bytes> live bytes,<pages> pages` to standard error |

```bash
LUNA_RT_GC_STATS=1 ./prog
LUNA_RT_GC_STRESS=1 compiler/run_tests.sh test/test_gc_safety.lu
```

## Testing the collector

`test_gc_safety.lu`, `test_gc_large_safety.lu`, `test_gc_callbacks.lu` and
`test_gc_recursion.lu` build and drop many objects and check that live data stays intact. Run
them under `LUNA_RT_GC_STRESS=1` after any change to `luna_gc.lu`, to a runtime function that
allocates, or to how generated code keeps values in memory. `make` also exercises the collector:
the compiler compiles itself twice, and the two outputs must be identical.

## Known limits

- Stop-the-world and single-threaded: all collection work counts as pause time, and there is
  no incremental or generational mode yet.
- No compaction. Fragmented pages are reused through their free lists.
- The threshold rule (the larger of 16 MiB and the live size) is not tuned.
- Memory use and collection speed have not been benchmarked on the AOT toolchain.

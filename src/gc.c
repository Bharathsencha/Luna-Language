// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include "gc.h"
#include "env.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdalign.h>

static unsigned long long total_pause_ns = 0;
static unsigned long long max_pause_ns = 0;
static unsigned long long pause_events = 0;
static GCHeap *runtime_heap = NULL;

typedef struct {
    bool has_young_ref;
} GCPromotedRememberCtx;

static size_t gc_align_up(size_t n) {
    size_t align = alignof(max_align_t);
    return (n + align - 1) & ~(align - 1);
}

static size_t gc_object_total_size(size_t payload_size) {
    return gc_align_up(sizeof(GCObject) + payload_size);
}

int gc_heap_is_managed_payload(GCHeap *heap, void *payload) {
    if (!heap || !payload) return 0;

    uint8_t *ptr = (uint8_t *)payload;
    for (ImixBlock *block = heap->blocks; block; block = block->next) {
        uint8_t *start = block->data;
        uint8_t *end = block->data + IMIX_BLOCK_SIZE;
        if (ptr >= start && ptr < end) return 1;
    }
    for (ImixBlock *block = heap->sweep_chain; block; block = block->next) {
        uint8_t *start = block->data;
        uint8_t *end = block->data + IMIX_BLOCK_SIZE;
        if (ptr >= start && ptr < end) return 1;
    }

    for (GCObject *obj = heap->large_list; obj; obj = obj->next) {
        if ((uint8_t *)GC_PAYLOAD(obj) == ptr) return 1;
    }
    for (GCObject *obj = heap->large_sweep_list; obj; obj = obj->next) {
        if ((uint8_t *)GC_PAYLOAD(obj) == ptr) return 1;
    }

    return 0;
}

void gc_stats_reset(void) {
    total_pause_ns = 0;
    max_pause_ns = 0;
    pause_events = 0;
}

void gc_stats_record_pause_ns(unsigned long long ns) {
    total_pause_ns += ns;
    if (ns > max_pause_ns) max_pause_ns = ns;
    pause_events++;
}

LunaGCStats gc_stats_snapshot(void) {
    LunaGCStats stats;
    if (runtime_heap) {
        stats.gc_ms_total = runtime_heap->gc_ms_total;
        stats.gc_ms_max = runtime_heap->gc_ms_max;
        stats.gc_events = runtime_heap->total_collections;
        return stats;
    }
    stats.gc_ms_total = (double)total_pause_ns / 1000000.0;
    stats.gc_ms_max = (double)max_pause_ns / 1000000.0;
    stats.gc_events = pause_events;
    return stats;
}

static ImixBlock *imix_block_new(void) {
    ImixBlock *block = (ImixBlock *)malloc(sizeof(ImixBlock));
    if (!block) {
        fprintf(stderr, "gc: out of memory\n");
        abort();
    }

    memset(block->line_mark, 0, sizeof(block->line_mark));
    memset(block->granule_bitmap, 0, sizeof(block->granule_bitmap));
    block->bump = 0;
    block->hole_start = 0;
    block->hole_end = 0;
    block->young_objects = 0;
    block->next = NULL;
    block->evacuating = false;
    return block;
}

static inline void nofl_granule_mark(ImixBlock *block, size_t offset, size_t total_bytes) {
    size_t start_g = offset / NOFL_GRANULE_SIZE;
    size_t end_g = (offset + total_bytes - 1) / NOFL_GRANULE_SIZE;
    for (size_t g = start_g; g <= end_g && g < NOFL_GRANULES_PER_BLOCK; g++) {
        block->granule_bitmap[g >> 3] |= (1u << (g & 7));
    }
}

static void gc_remembered_push(GCHeap *heap, GCObject *obj) {
    if (!heap || !obj || obj->generation != GC_GEN_OLD || obj->remembered) return;
    if (heap->remembered_count == heap->remembered_cap) {
        size_t new_cap = heap->remembered_cap ? heap->remembered_cap * 2 : 64;
        GCObject **new_items =
            (GCObject **)realloc(heap->remembered_set, new_cap * sizeof(GCObject *));
        if (!new_items) abort();
        heap->remembered_set = new_items;
        heap->remembered_cap = new_cap;
    }
    obj->remembered = 1;
    heap->remembered_set[heap->remembered_count++] = obj;
}

static void gc_detect_young_ref(void *ctx, GCObject *child) {
    GCTraceCtx *trace = (GCTraceCtx *)ctx;
    GCPromotedRememberCtx *scan = (GCPromotedRememberCtx *)trace->userdata;
    if (!scan || !child || child->color == GC_DEAD) return;
    if (child->generation == GC_GEN_YOUNG) {
        scan->has_young_ref = true;
    }
}

/* During minor GC sweep, ALL reachable young objects are promoted together,
 * so a promoted object cannot retain pointers to still-young objects.
 * Future old→young edges are captured by the write barrier
 * (luna_gc_runtime_remember).  The old approach of fully tracing each
 * promoted object here caused unbounded multi-ms pauses on large containers.
 * Now we simply skip the scan entirely. */
static void gc_remember_if_points_to_young(GCHeap *heap, GCObject *obj) {
    (void)heap;
    (void)obj;
}

static int gc_env_bool(const char *name, int default_value) {
    const char *raw = getenv(name);
    if (!raw || !*raw) return default_value;
    if (strcmp(raw, "0") == 0 || strcmp(raw, "false") == 0 || strcmp(raw, "FALSE") == 0) {
        return 0;
    }
    if (strcmp(raw, "1") == 0 || strcmp(raw, "true") == 0 || strcmp(raw, "TRUE") == 0) {
        return 1;
    }
    return default_value;
}

static size_t gc_env_size(const char *name, size_t default_value, size_t min_value, size_t max_value) {
    const char *raw = getenv(name);
    if (!raw || !*raw) return default_value;

    char *end = NULL;
    unsigned long long parsed = strtoull(raw, &end, 10);
    if (!end || *end != '\0') return default_value;

    size_t value = (size_t)parsed;
    if (value < min_value) value = min_value;
    if (value > max_value) value = max_value;
    return value;
}

static uint64_t gc_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void gc_heap_record_pause(GCHeap *heap, uint64_t start_ns) {
    double pause_ms = (double)(gc_now_ns() - start_ns) / 1000000.0;
    heap->gc_ms_total += pause_ms;
    if (pause_ms > heap->gc_ms_max) heap->gc_ms_max = pause_ms;
}

static inline uint64_t gc_phase_begin(GCHeap *heap) {
    return heap->pause_trace ? gc_now_ns() : 0;
}

static inline void gc_phase_end(GCHeap *heap, uint64_t start_ns, const char *phase) {
    if (!heap->pause_trace) return;
    double ms = (double)(gc_now_ns() - start_ns) / 1000000.0;
    if (ms >= heap->pause_trace_threshold_ms) {
        fprintf(stderr, "GC_PAUSE %.3fms phase=%s minor=%d\n",
                ms, phase, heap->minor_collection ? 1 : 0);
    }
}

static size_t gc_compute_young_limit(size_t heap_limit, size_t bytes_live) {
    size_t young = heap_limit / 4;
    size_t min_young = 512 * 1024;   /* 512 KB — fewer minor GCs */
    size_t max_young = 4 * 1024 * 1024; /* 4 MB — incremental drain handles large sets */
    (void)bytes_live;

    if (young < min_young) young = min_young;
    if (young > max_young) young = max_young;
    return young;
}

static size_t gc_compute_major_interval(size_t bytes_live, size_t heap_limit) {
    if (heap_limit == 0) return 12;
    if (bytes_live < heap_limit / 3) return 16;
    if (bytes_live < (heap_limit * 2) / 3) return 12;
    return 8;
}

static int gc_should_reclaim_empty_blocks_on_minor(const GCHeap *heap) {
    if (!heap) return 0;
    return ((heap->minor_since_major + 1) % 2) == 0;
}

typedef struct {
    GCObject **items;
    size_t count;
    size_t cap;
} GCVerifyStack;

typedef struct {
    GCVerifyStack work;
    GCVerifyStack seen;
} GCVerifyState;

static void gc_verify_stack_push(GCVerifyStack *stack, GCObject *obj) {
    if (!obj) return;
    if (stack->count == stack->cap) {
        size_t new_cap = stack->cap ? stack->cap * 2 : 64;
        GCObject **new_items =
            (GCObject **)realloc(stack->items, new_cap * sizeof(GCObject *));
        if (!new_items) abort();
        stack->items = new_items;
        stack->cap = new_cap;
    }
    stack->items[stack->count++] = obj;
}

static GCObject *gc_verify_stack_pop(GCVerifyStack *stack) {
    if (!stack || stack->count == 0) return NULL;
    return stack->items[--stack->count];
}

static int gc_verify_seen_contains(const GCVerifyStack *stack, GCObject *obj) {
    for (size_t i = 0; i < stack->count; i++) {
        if (stack->items[i] == obj) return 1;
    }
    return 0;
}

static bool last_collection_was_minor = false;

static void gc_verify_visit(void *ctx, GCObject *obj) {
    GCTraceCtx *trace = (GCTraceCtx *)ctx;
    GCVerifyState *state = (GCVerifyState *)trace->userdata;
    if (!obj) return;
    if (obj->color == GC_DEAD) {
        typedef struct {
            int ref_count;
            char *chars;
        } TempStringObj;
        TempStringObj *str = (TempStringObj *)((uint8_t *)obj + sizeof(GCObject));
        if (str->chars == (char *)str + sizeof(TempStringObj)) {
            fprintf(stderr, "gc verify: reachable object marked dead! size=%u, generation=%u, trace=%p, was_minor=%d, str=\"%s\"\n",
                    obj->size, obj->generation, (void*)obj->trace, last_collection_was_minor, str->chars);
        } else {
            fprintf(stderr, "gc verify: reachable object marked dead! size=%u, generation=%u, trace=%p, was_minor=%d\n",
                    obj->size, obj->generation, (void*)obj->trace, last_collection_was_minor);
        }
        abort();
    }
    if (gc_verify_seen_contains(&state->seen, obj)) return;
    gc_verify_stack_push(&state->seen, obj);
    gc_verify_stack_push(&state->work, obj);
}

static void gc_verify_heap(GCHeap *heap) {
    if (!heap || !heap->verify_mode) return;

    GCVerifyState state = {0};
    GCTraceCtx ctx = {
        .heap = heap,
        .visit = gc_verify_visit,
        .userdata = &state,
    };

    for (size_t i = 0; i < heap->root_count; i++) {
        gc_verify_visit(&ctx, heap->roots[i]);
    }
    if (heap->root_marker) {
        if (heap->minor_collection) {
            bool saved_minor = heap->minor_collection;
            heap->minor_collection = false;
            heap->root_marker(&ctx);
            heap->minor_collection = saved_minor;
        } else {
            heap->root_marker(&ctx);
        }
    }

    GCObject *obj = NULL;
    while ((obj = gc_verify_stack_pop(&state.work)) != NULL) {
        if (obj->trace) obj->trace(obj, &ctx);
    }

    free(state.work.items);
    free(state.seen.items);
}

static void gc_reset_remembered_set(GCHeap *heap) {
    if (!heap) return;
    for (size_t i = 0; i < heap->remembered_count; i++) {
        GCObject *obj = heap->remembered_set[i];
        if (obj) obj->remembered = 0;
    }
    heap->remembered_count = 0;
}

static void imix_mark_lines(ImixBlock *block, size_t offset, size_t size) {
    size_t first = offset / IMIX_LINE_SIZE;
    size_t last = (offset + size - 1) / IMIX_LINE_SIZE;

    for (size_t i = first; i <= last && i < IMIX_LINES_PER_BLOCK; i++) {
        block->line_mark[i] = 1;
    }
}

void _gc_gray_push(GCHeap *heap, GCObject *obj);

static void gray_push(GCHeap *heap, GCObject *obj) {
    if (heap->minor_collection && obj->generation == GC_GEN_OLD) {
        heap->minor_marked_old = true;
    }
    if (heap->gray_top == heap->gray_cap) {
        size_t new_cap = heap->gray_cap ? heap->gray_cap * 2 : 64;
        GCGrayEntry *new_stack =
            (GCGrayEntry *)realloc(heap->gray_stack, new_cap * sizeof(GCGrayEntry));
        if (!new_stack) {
            fprintf(stderr, "gc: gray stack out of memory\n");
            abort();
        }
        heap->gray_stack = new_stack;
        heap->gray_cap = new_cap;
    }
    heap->gray_stack[heap->gray_top].obj    = obj;
    heap->gray_stack[heap->gray_top].cursor = 0; /* fresh push — start from child 0 */
    heap->gray_top++;
}

/* Re-push with a non-zero cursor after a mid-trace deadline interruption. */
static void gray_push_cursor(GCHeap *heap, GCObject *obj, size_t cursor) {
    if (heap->gray_top == heap->gray_cap) {
        size_t new_cap = heap->gray_cap ? heap->gray_cap * 2 : 64;
        GCGrayEntry *new_stack =
            (GCGrayEntry *)realloc(heap->gray_stack, new_cap * sizeof(GCGrayEntry));
        if (!new_stack) {
            fprintf(stderr, "gc: gray stack out of memory\n");
            abort();
        }
        heap->gray_stack = new_stack;
        heap->gray_cap = new_cap;
    }
    heap->gray_stack[heap->gray_top].obj    = obj;
    heap->gray_stack[heap->gray_top].cursor = cursor;
    heap->gray_top++;
}

static GCGrayEntry gray_pop(GCHeap *heap) {
    GCGrayEntry empty = {NULL, 0};
    if (heap->gray_top == 0) return empty;
    return heap->gray_stack[--heap->gray_top];
}

void _gc_gray_push(GCHeap *heap, GCObject *obj) {
    gray_push(heap, obj);  /* cursor = 0: this is a fresh gray reference */
}

GCHeap *gc_heap_create(size_t initial_limit) {
    GCHeap *heap = (GCHeap *)calloc(1, sizeof(GCHeap));
    if (!heap) abort();

    heap->current = imix_block_new();
    heap->blocks = heap->current;
    heap->heap_limit = initial_limit ? initial_limit : (4 * 1024 * 1024);
    heap->growth_factor = 1.5;
    heap->increment_steps = 2048; /* drain more gray objects per step to keep up with fast allocators */
    heap->sweep_block_budget = 16; /* sweep more blocks per step — each block is fast without remember-scan */
    heap->incremental_mode = true;
    heap->young_limit = gc_compute_young_limit(heap->heap_limit, 0);
    heap->major_interval = gc_compute_major_interval(0, heap->heap_limit);
    heap->large_sweep_budget = 128; /* large objects handled per sweep step */
    heap->stress_mode = getenv("LUNA_GC_STRESS") != NULL;
    heap->verify_mode = getenv("LUNA_GC_VERIFY") != NULL;
    heap->pause_trace = getenv("LUNA_GC_PAUSE_TRACE") != NULL;
    /* threshold in microseconds; report any single phase exceeding it */
    heap->pause_trace_threshold_ms =
        (double)gc_env_size("LUNA_GC_PAUSE_TRACE_US", 200, 1, 1000000) / 1000.0;
    heap->heap_limit = gc_env_size("LUNA_GC_INITIAL_HEAP_LIMIT", heap->heap_limit, 256 * 1024, (size_t)8 * 1024 * 1024 * 1024ULL);
    heap->young_limit = gc_env_size("LUNA_GC_YOUNG_LIMIT", heap->young_limit, 16 * 1024, 64 * 1024 * 1024);
    heap->major_interval = gc_env_size("LUNA_GC_MAJOR_INTERVAL", heap->major_interval, 1, 1000000);
    heap->incremental_mode = gc_env_bool("LUNA_GC_INCREMENTAL", heap->incremental_mode ? 1 : 0) != 0;
    heap->increment_steps = gc_env_size("LUNA_GC_INCREMENT_STEPS", heap->increment_steps, 1, 4096);
    heap->sweep_block_budget = gc_env_size("LUNA_GC_SWEEP_BUDGET", heap->sweep_block_budget, 1, 64);
    heap->large_sweep_budget = gc_env_size("LUNA_GC_LARGE_SWEEP_BUDGET", heap->large_sweep_budget, 1, 4096);
    size_t pause_target_us = gc_env_size("LUNA_GC_PAUSE_TARGET_US", 250, 10, 1000000);
    heap->target_pause_ns = (uint64_t)pause_target_us * 1000ULL;

    heap->root_cap = 64;
    heap->roots = (GCObject **)malloc(heap->root_cap * sizeof(GCObject *));
    if (!heap->roots) abort();

    return heap;
}

void gc_heap_destroy(GCHeap *heap) {
    if (!heap) return;

    GCObject *obj = heap->large_list;
    while (obj) {
        GCObject *next = obj->next;
        if (obj->finalize) obj->finalize(obj);
        free(obj);
        obj = next;
    }
    obj = heap->large_sweep_list;
    while (obj) {
        GCObject *next = obj->next;
        if (obj->finalize) obj->finalize(obj);
        free(obj);
        obj = next;
    }

    ImixBlock *block = heap->blocks;
    while (block) {
        ImixBlock *next = block->next;
        free(block);
        block = next;
    }
    block = heap->sweep_chain;
    while (block) {
        ImixBlock *next = block->next;
        free(block);
        block = next;
    }

    free(heap->gray_stack);
    free(heap->roots);
    free(heap->remembered_set);
    free(heap);
}

static GCObject *imix_bump_alloc(ImixBlock *block, size_t total) {
    if (block->bump + total > IMIX_BLOCK_SIZE) return NULL;

    GCObject *obj = (GCObject *)(block->data + block->bump);
    imix_mark_lines(block, block->bump, total);
    nofl_granule_mark(block, block->bump, total);
    block->bump += total;
    return obj;
}

void *gc_heap_alloc(GCHeap *heap, size_t size, GCTracer trace, GCFinalizer fin) {
    size_t total = gc_object_total_size(size);
    GCObject *obj = NULL;

    if (total > IMIX_BLOCK_SIZE) {
        obj = (GCObject *)malloc(total);
        if (!obj) {
            fprintf(stderr, "gc: large alloc failed\n");
            abort();
        }
        obj->next = heap->large_list;
        heap->large_list = obj;
    } else {
        obj = imix_bump_alloc(heap->current, total);

        if (!obj) {
            ImixBlock *block = imix_block_new();
            block->next = heap->blocks;
            heap->blocks = block;
            heap->current = block;
            obj = imix_bump_alloc(block, total);
        }

        obj->next = NULL;
        heap->current->young_objects++;
    }

    obj->color = heap->collection_in_progress ? GC_BLACK : GC_WHITE;
    obj->pinned = false;
    obj->generation = GC_GEN_YOUNG;
    obj->remembered = 0;
    obj->size = (uint32_t)size;
    obj->trace = trace;
    obj->finalize = fin;

    heap->bytes_allocated += total;
    heap->young_bytes_allocated += total;
    heap->total_allocs++;
    return GC_PAYLOAD(obj);
}

void gc_heap_add_root(GCHeap *heap, GCObject *obj) {
    if (heap->root_count == heap->root_cap) {
        size_t new_cap = heap->root_cap ? heap->root_cap * 2 : 64;
        GCObject **new_roots =
            (GCObject **)realloc(heap->roots, new_cap * sizeof(GCObject *));
        if (!new_roots) abort();
        heap->roots = new_roots;
        heap->root_cap = new_cap;
    }

    heap->roots[heap->root_count++] = obj;
}

void gc_heap_remove_root(GCHeap *heap, GCObject *obj) {
    for (size_t i = 0; i < heap->root_count; i++) {
        if (heap->roots[i] == obj) {
            heap->roots[i] = heap->roots[--heap->root_count];
            return;
        }
    }
}

void gc_heap_set_root_marker(GCHeap *heap, GCRootMarker marker, void *ctx) {
    heap->root_marker = marker;
    heap->root_marker_ctx = ctx;
}

void gc_heap_write_barrier(GCHeap *heap, GCObject *oldref) {
    if (!heap->collection_in_progress) return;
    if (oldref && oldref->color == GC_WHITE) {
        oldref->color = GC_GRAY;
        gray_push(heap, oldref);
    }
}

static void gc_remember_from_roots(GCHeap *heap) {
    for (size_t i = 0; i < heap->remembered_count; i++) {
        GCObject *obj = heap->remembered_set[i];
        if (!obj || obj->color == GC_DEAD) continue;
        if (obj->color != GC_GRAY) {
            obj->color = GC_GRAY;
            gray_push(heap, obj);
        }
    }
}

static void mark_roots(GCHeap *heap) {
    uint64_t phase_ns = gc_phase_begin(heap);
    GCTraceCtx ctx = { heap, NULL, heap->root_marker_ctx, 0, false, 0, 0 };

    for (size_t i = 0; i < heap->root_count; i++) {
        GCObject *root = heap->roots[i];
        if (root && root->color != GC_GRAY) {
            root->color = GC_GRAY;
            gray_push(heap, root);
        }
    }

    if (heap->root_marker) {
        heap->root_marker(&ctx);
    }

    if (heap->minor_collection) {
        gc_remember_from_roots(heap);
        /* All pre-existing old→young edges are now captured in the gray
         * stack.  Reset the remembered set so it stays small; the write
         * barrier will repopulate it with any new edges created during or
         * after this collection. */
        gc_reset_remembered_set(heap);
    }
    gc_phase_end(heap, phase_ns, "mark_roots");
}

static bool drain_gray(GCHeap *heap, size_t count) {
    uint64_t phase_ns = gc_phase_begin(heap);
    bool time_slice = (count > 0 && heap->target_pause_ns > 0);
    uint64_t start_ns = time_slice ? gc_now_ns() : 0;
    uint64_t deadline_ns = time_slice ? (start_ns + heap->target_pause_ns) : 0;

    GCTraceCtx ctx = {
        .heap        = heap,
        .visit       = NULL,
        .userdata    = NULL,
        .deadline_ns = deadline_ns,
        .deadline_hit = false,
        .scan_start  = 0,
        .scan_resume = 0,
    };

    /* Reset the thread-local visit counter so this step gets a full
     * target_pause_ns budget rather than inheriting leftover count
     * from a previous object's partial trace. */
    gc_visit_reset_deadline_counter();

    size_t processed = 0;

    while (heap->gray_top > 0) {
        GCGrayEntry entry = gray_pop(heap);
        GCObject *obj = entry.obj;
        if (!obj) continue;

        obj->color = GC_BLACK;
        ctx.deadline_hit = false;
        ctx.scan_start   = entry.cursor;  /* tell tracer where to resume */
        ctx.scan_resume  = entry.cursor;  /* tracer will update if interrupted */

        if (obj->trace) obj->trace(obj, &ctx);

        if (ctx.deadline_hit) {
            /* Tracer was interrupted mid-way: re-gray the object and
             * store the resume cursor so next step starts where we left off. */
            obj->color = GC_GRAY;
            gray_push_cursor(heap, obj, ctx.scan_resume);
            gc_phase_end(heap, phase_ns, "drain");
            return false;
        }

        processed++;

        if (count > 0 && processed >= count) {
            gc_phase_end(heap, phase_ns, "drain");
            return heap->gray_top == 0;
        }

        if (deadline_ns > 0 && (processed & 63) == 0) {
            if (gc_now_ns() >= deadline_ns) {
                gc_phase_end(heap, phase_ns, "drain");
                return heap->gray_top == 0;
            }
        }
    }

    gc_phase_end(heap, phase_ns, "drain");
    return heap->gray_top == 0;
}

static void sweep_block(GCHeap *heap, ImixBlock *block) {
    if (heap->minor_collection && block->young_objects == 0 && !heap->minor_marked_old) {
        return;
    }

    memset(block->line_mark, 0, sizeof(block->line_mark));

    size_t off = 0;
    size_t live_end = 0;
    size_t young_objects = 0;
    while (off + sizeof(GCObject) <= block->bump) {
        GCObject *obj = (GCObject *)(block->data + off);
        size_t max_payload = block->bump - off - sizeof(GCObject);
        if (obj->size > max_payload && obj->color != GC_DEAD) break;

        size_t total = gc_object_total_size(obj->size);
        if (obj->color == GC_DEAD) {
            off += total;
            continue;
        }

        if (heap->minor_collection) {
            if (obj->generation == GC_GEN_YOUNG) {
                if (obj->color == GC_WHITE) {
                    if (obj->finalize) obj->finalize(obj);
                    heap->bytes_allocated -= total;
                    if (heap->young_bytes_allocated >= total) heap->young_bytes_allocated -= total;
                    obj->color = GC_DEAD;
                } else {
                    obj->generation = GC_GEN_OLD;
                    obj->color = GC_WHITE;
                    gc_remember_if_points_to_young(heap, obj);
                    if (heap->young_bytes_allocated >= total) heap->young_bytes_allocated -= total;
                    imix_mark_lines(block, off, total);
                    heap->bytes_live += total;
                }
            } else {
                obj->color = GC_WHITE;
                imix_mark_lines(block, off, total);
                heap->bytes_live += total;
            }
        } else if (obj->color == GC_WHITE) {
            if (obj->finalize) obj->finalize(obj);
            heap->bytes_allocated -= total;
            if (obj->generation == GC_GEN_YOUNG && heap->young_bytes_allocated >= total) {
                heap->young_bytes_allocated -= total;
            }
            obj->color = GC_DEAD;
        } else {
            obj->generation = GC_GEN_OLD;
            obj->color = GC_WHITE;
            if (heap->young_bytes_allocated >= total) heap->young_bytes_allocated -= total;
            imix_mark_lines(block, off, total);
            heap->bytes_live += total;
        }

        if (obj->color != GC_DEAD && obj->generation == GC_GEN_YOUNG) {
            young_objects++;
        }
        if (obj->color != GC_DEAD) {
            live_end = off + total;
        }

        off += total;
    }
    block->bump = live_end;
    block->young_objects = young_objects;
}

/* Incremental large-object sweep: pops one object from the detached
 * heap->large_sweep_list, frees it or reattaches it to heap->large_list.
 * New large allocations during sweep go straight to heap->large_list,
 * so the two lists never interfere. */
static void sweep_large_one(GCHeap *heap) {
    uint64_t t0 = heap->pause_trace ? gc_now_ns() : 0;
    GCObject *obj = heap->large_sweep_list;
    heap->large_sweep_list = obj->next;
    size_t total = gc_object_total_size(obj->size);
    int keep = 1;
    int promoted = 0;

    if (heap->minor_collection) {
        if (obj->generation == GC_GEN_YOUNG) {
            if (obj->color == GC_WHITE) {
                keep = 0;
            } else {
                obj->generation = GC_GEN_OLD;
                obj->color = GC_WHITE;
                if (heap->young_bytes_allocated >= total) heap->young_bytes_allocated -= total;
                heap->bytes_live += total;
                promoted = 1;
            }
        } else {
            obj->color = GC_WHITE;
            heap->bytes_live += total;
        }
    } else if (obj->color == GC_WHITE) {
        keep = 0;
    } else {
        obj->generation = GC_GEN_OLD;
        obj->color = GC_WHITE;
        if (heap->young_bytes_allocated >= total) heap->young_bytes_allocated -= total;
        heap->bytes_live += total;
    }

    if (!keep) {
        if (obj->finalize) obj->finalize(obj);
        heap->bytes_allocated -= total;
        if (obj->generation == GC_GEN_YOUNG && heap->young_bytes_allocated >= total) {
            heap->young_bytes_allocated -= total;
        }
        uint32_t sz = obj->size;
        free(obj);
        if (heap->pause_trace && t0) {
            double ms = (double)(gc_now_ns() - t0) / 1000000.0;
            if (ms >= heap->pause_trace_threshold_ms) {
                fprintf(stderr, "GC_LARGE free=%.3fms size=%u\n", ms, sz);
            }
        }
        return;
    }

    if (promoted) gc_remember_if_points_to_young(heap, obj);
    obj->next = heap->large_list;
    heap->large_list = obj;
    if (heap->pause_trace && t0) {
        double ms = (double)(gc_now_ns() - t0) / 1000000.0;
        if (ms >= heap->pause_trace_threshold_ms) {
            fprintf(stderr, "GC_LARGE keep=%.3fms size=%u gen=%u\n", ms, obj->size, obj->generation);
        }
    }
}

static void gc_select_current_block(GCHeap *heap) {
    if (!heap || !heap->blocks) return;

    ImixBlock *best = heap->blocks;
    for (ImixBlock *block = heap->blocks; block; block = block->next) {
        if (block->bump < best->bump) best = block;
    }
    heap->current = best;
}

static void gc_finish_sweep_phase(GCHeap *heap) {
    if (!heap) return;
    uint64_t phase_ns = gc_phase_begin(heap);

    gc_select_current_block(heap);
    if (!heap->minor_collection) {
        if (heap->bytes_live > heap->heap_limit / 2) {
            heap->heap_limit = (size_t)(heap->bytes_live * 2 * heap->growth_factor);
        }
        heap->young_limit = gc_compute_young_limit(heap->heap_limit, heap->bytes_live);
        heap->major_interval = gc_compute_major_interval(heap->bytes_live, heap->heap_limit);
        heap->minor_since_major = 0;
    } else {
        heap->minor_since_major++;
    }

    heap->total_collections++;
    heap->sweep_in_progress = false;
    last_collection_was_minor = heap->sweep_minor;
    heap->sweep_minor = false;
    heap->sweep_reclaim_empty = false;
    heap->sweep_cursor = NULL;
    heap->sweep_chain = NULL;
    heap->reclaim_cursor = NULL;
    heap->large_sweep_list = NULL;
    heap->collection_in_progress = false;
    heap->minor_collection = false;
    heap->minor_marked_old = false;
    gc_phase_end(heap, phase_ns, "finish_sweep");
    gc_verify_heap(heap);
}

static void gc_prepare_sweep_phase(GCHeap *heap, bool minor, bool reclaim_empty) {
    if (!heap) return;

    /* Detach the existing block chain and large-object list: they become the
     * sweep set.  Fresh allocations during the sweep phase build up new,
     * disjoint lists (heap->blocks / heap->large_list), so incremental sweep
     * can never race with the mutator over list pointers. */
    heap->sweep_chain = heap->blocks;
    heap->sweep_cursor = heap->sweep_chain;
    heap->reclaim_cursor = NULL;
    heap->large_sweep_list = heap->large_list;
    heap->large_list = NULL;

    heap->sweep_in_progress = true;
    heap->sweep_minor = minor;
    heap->sweep_reclaim_empty = reclaim_empty;
    heap->collection_in_progress = false;

    ImixBlock *fresh = imix_block_new();
    heap->blocks = fresh;
    heap->current = fresh;
}

static void gc_sweep_some_blocks(GCHeap *heap, size_t budget) {
    if (!heap || !heap->sweep_in_progress) return;
    uint64_t phase_ns = gc_phase_begin(heap);
    uint64_t deadline = heap->target_pause_ns ? gc_now_ns() + heap->target_pause_ns : 0;

    /* Pass 1: sweep blocks in the detached chain. */
    size_t swept = 0;
    while (heap->sweep_cursor && swept < budget) {
        ImixBlock *block = heap->sweep_cursor;
        heap->sweep_cursor = block->next;
        sweep_block(heap, block);
        swept++;
        if (deadline && gc_now_ns() >= deadline) {
            gc_phase_end(heap, phase_ns, "sweep_blocks");
            return;
        }
    }
    if (heap->sweep_cursor) {
        gc_phase_end(heap, phase_ns, "sweep_blocks");
        return;
    }

    /* Pass 2: walk the swept chain once more — free empty blocks (when
     * allowed) and reattach the rest to the live block list. */
    if (heap->sweep_chain) {
        if (!heap->reclaim_cursor) heap->reclaim_cursor = heap->sweep_chain;
        size_t n = 0;
        while (heap->reclaim_cursor) {
            ImixBlock *block = heap->reclaim_cursor;
            heap->reclaim_cursor = block->next;

            int empty = 1;
            for (size_t i = 0; i < IMIX_LINES_PER_BLOCK; i++) {
                if (block->line_mark[i]) { empty = 0; break; }
            }

            if (empty && heap->sweep_reclaim_empty) {
                block->bump = 0;
                memset(block->line_mark, 0, sizeof(block->line_mark));
                free(block);
            } else {
                block->next = heap->blocks;
                heap->blocks = block;
            }

            n++;
            if (deadline && gc_now_ns() >= deadline) {
                gc_phase_end(heap, phase_ns, "reclaim_blocks");
                return;
            }
        }
        heap->sweep_chain = NULL;
    }

    /* Pass 3: sweep large objects. */
    size_t lswept = 0;
    while (heap->large_sweep_list) {
        sweep_large_one(heap);
        lswept++;
        if (lswept >= heap->large_sweep_budget ||
            (deadline && gc_now_ns() >= deadline)) {
            gc_phase_end(heap, phase_ns, "sweep_large");
            return;
        }
    }

    gc_phase_end(heap, phase_ns, "sweep_finish");
    gc_finish_sweep_phase(heap);
}

void gc_heap_collect(GCHeap *heap) {
    uint64_t start_ns = gc_now_ns();
    heap->collection_in_progress = true;
    heap->minor_collection = false;
    heap->bytes_live = 0;
    gc_reset_remembered_set(heap);

    mark_roots(heap);
    drain_gray(heap, 0);
    gc_prepare_sweep_phase(heap, false, true);
    gc_sweep_some_blocks(heap, heap->sweep_block_budget);
    gc_heap_record_pause(heap, start_ns);
}

/*
 * Adaptive drain: when the incremental marker is falling behind the allocator
 * (mark_step_count keeps growing), we double the step budget so the GC
 * catches up faster within the same safepoint.  This avoids a stop-the-world
 * drain while still making progress.
 *
 * We cap the adaptive budget at 4096 objects per step; beyond that we do a
 * one-shot full drain (which is bounded by the live-set, not by allocation
 * rate, so it terminates quickly once the live set is fully marked).
 */
static size_t gc_adaptive_steps(GCHeap *heap) {
    if (heap->mark_step_count < 4) return heap->increment_steps;
    size_t steps = heap->increment_steps << (heap->mark_step_count / 4);
    if (steps > 4096) steps = 4096;
    return steps;
}

void gc_heap_step(GCHeap *heap) {
    uint64_t start_ns = gc_now_ns();

    if (heap->sweep_in_progress) {
        gc_sweep_some_blocks(heap, heap->sweep_block_budget);
        gc_heap_record_pause(heap, start_ns);
        return;
    }

    if (!heap->collection_in_progress) {
        heap->collection_in_progress = true;
        heap->mark_roots_done = false;
        heap->mark_step_count = 0;
        heap->bytes_live = 0;
        heap->minor_collection = false;
        gc_reset_remembered_set(heap);
    }

    if (!heap->mark_roots_done) {
        mark_roots(heap);
        heap->mark_roots_done = true;
        if (heap->target_pause_ns > 0 && gc_now_ns() - start_ns >= heap->target_pause_ns) {
            gc_heap_record_pause(heap, start_ns);
            return;
        }
    }

    heap->mark_step_count++;
    size_t steps = gc_adaptive_steps(heap);

    if (drain_gray(heap, steps)) {
        heap->mark_step_count = 0;
        if (heap->target_pause_ns > 0 && gc_now_ns() - start_ns >= heap->target_pause_ns) {
            /* marking done but budget spent — start sweep on the next safepoint */
            gc_heap_record_pause(heap, start_ns);
            return;
        }
        gc_prepare_sweep_phase(heap, false, true);
        gc_sweep_some_blocks(heap, heap->sweep_block_budget);
        heap->young_limit = gc_compute_young_limit(heap->heap_limit, heap->bytes_live);
    }

    gc_heap_record_pause(heap, start_ns);
}

static void gc_heap_step_minor(GCHeap *heap);

static void gc_heap_collect_minor(GCHeap *heap) {
    uint64_t start_ns = gc_now_ns();
    heap->collection_in_progress = true;
    heap->minor_collection = true;
    heap->minor_marked_old = false;
    heap->bytes_live = 0;
    mark_roots(heap);
    drain_gray(heap, 0);
    gc_prepare_sweep_phase(heap, true, gc_should_reclaim_empty_blocks_on_minor(heap));
    gc_sweep_some_blocks(heap, heap->sweep_block_budget);
    gc_heap_record_pause(heap, start_ns);
}

static void gc_heap_step_minor(GCHeap *heap) {
    uint64_t start_ns = gc_now_ns();

    if (heap->sweep_in_progress) {
        gc_sweep_some_blocks(heap, heap->sweep_block_budget);
        gc_heap_record_pause(heap, start_ns);
        return;
    }

    if (!heap->collection_in_progress) {
        heap->collection_in_progress = true;
        heap->mark_roots_done = false;
        heap->mark_step_count = 0;
        heap->minor_collection = true;
        heap->minor_marked_old = false;
        heap->bytes_live = 0;
    }

    if (!heap->mark_roots_done) {
        mark_roots(heap);
        heap->mark_roots_done = true;
        if (heap->target_pause_ns > 0 && gc_now_ns() - start_ns >= heap->target_pause_ns) {
            gc_heap_record_pause(heap, start_ns);
            return;
        }
    }

    heap->mark_step_count++;
    size_t steps = gc_adaptive_steps(heap);

    if (drain_gray(heap, steps)) {
        heap->mark_step_count = 0;
        if (heap->target_pause_ns > 0 && gc_now_ns() - start_ns >= heap->target_pause_ns) {
            gc_heap_record_pause(heap, start_ns);
            return;
        }
        gc_prepare_sweep_phase(heap, true, gc_should_reclaim_empty_blocks_on_minor(heap));
        gc_sweep_some_blocks(heap, heap->sweep_block_budget);
        heap->young_limit = gc_compute_young_limit(heap->heap_limit, heap->bytes_live);
    }

    gc_heap_record_pause(heap, start_ns);
}

void gc_heap_maybe_collect(GCHeap *heap) {
    if (!heap) return;

    if (heap->sweep_in_progress) {
        uint64_t start_ns = gc_now_ns();
        gc_sweep_some_blocks(heap, heap->sweep_block_budget);
        gc_heap_record_pause(heap, start_ns);
        /* one sweep step per safepoint; the next collection (if any) starts
         * on a later safepoint so pauses stay bounded */
        return;
    }

    // If a major incremental mark is in progress, keep advancing that state machine.
    // Do not start a minor collection mid-major, or liveness invariants can break.
    if (heap->collection_in_progress) {
        if (heap->incremental_mode) {
            if (heap->minor_collection) gc_heap_step_minor(heap);
            else gc_heap_step(heap);
        }
        else gc_heap_collect(heap);
        return;
    }

    size_t young_trigger = heap->young_limit;
    size_t heap_trigger = heap->heap_limit;
    if (heap->stress_mode) {
        size_t stressed_young = heap->young_limit / 8;
        size_t stressed_heap = heap->heap_limit / 8;
        young_trigger = stressed_young > (16 * 1024) ? stressed_young : (16 * 1024);
        heap_trigger = stressed_heap > (128 * 1024) ? stressed_heap : (128 * 1024);
    }

    if (heap->young_bytes_allocated >= young_trigger &&
        heap->minor_since_major < heap->major_interval) {
        if (heap->incremental_mode) gc_heap_step_minor(heap);
        else gc_heap_collect_minor(heap);
        return;
    }

    if (heap->bytes_allocated < heap_trigger) return;
    if (heap->incremental_mode) gc_heap_step(heap);
    else gc_heap_collect(heap);
}

GCHeapStats gc_heap_stats(GCHeap *heap) {
    GCHeapStats stats = {0};
    stats.bytes_allocated = heap->bytes_allocated;
    stats.bytes_live = heap->bytes_live;
    stats.total_collections = heap->total_collections;
    stats.total_allocs = heap->total_allocs;
    stats.gray_stack_peak = heap->gray_cap;

    for (ImixBlock *block = heap->blocks; block; block = block->next) stats.block_count++;
    for (GCObject *obj = heap->large_list; obj; obj = obj->next) stats.large_object_count++;

    stats.gc_ms_total = heap->gc_ms_total;
    stats.gc_ms_max = heap->gc_ms_max;
    return stats;
}

void gc_heap_print_stats(GCHeap *heap) {
    GCHeapStats stats = gc_heap_stats(heap);
    printf("---------------------------------------\n");
    printf("           GC Statistics               \n");
    printf("---------------------------------------\n");
    printf(" Total allocs       : %12zu\n", stats.total_allocs);
    printf(" Collections        : %12zu\n", stats.total_collections);
    printf(" Bytes allocated    : %12zu\n", stats.bytes_allocated);
    printf(" Bytes live (last)  : %12zu\n", stats.bytes_live);
    printf(" Imix blocks        : %12zu\n", stats.block_count);
    printf(" Large objects      : %12zu\n", stats.large_object_count);
    printf(" Heap limit         : %12zu\n", heap->heap_limit);
    printf(" GC ms total        : %12.3f\n", stats.gc_ms_total);
    printf(" GC ms max          : %12.3f\n", stats.gc_ms_max);
    printf("---------------------------------------\n");
}

int luna_gc_runtime_init(size_t initial_limit) {
    if (runtime_heap) return 1;
    runtime_heap = gc_heap_create(initial_limit);
    return runtime_heap != NULL;
}

void luna_gc_runtime_shutdown(void) {
    gc_heap_destroy(runtime_heap);
    runtime_heap = NULL;
}

int luna_gc_runtime_enabled(void) {
    return runtime_heap != NULL;
}

GCHeap *luna_gc_runtime_heap(void) {
    return runtime_heap;
}

void *luna_gc_alloc(size_t size, GCTracer trace, GCFinalizer fin) {
    if (!runtime_heap) return malloc(size);
    return gc_heap_alloc(runtime_heap, size, trace, fin);
}

void luna_gc_runtime_safe_point(void) {
    if (runtime_heap) gc_heap_maybe_collect(runtime_heap);
}

void luna_gc_runtime_set_root_marker(GCRootMarker marker, void *ctx) {
    if (runtime_heap) gc_heap_set_root_marker(runtime_heap, marker, ctx);
}

void luna_gc_runtime_add_root(void *payload) {
    if (!runtime_heap || !payload) return;
    if (!gc_heap_is_managed_payload(runtime_heap, payload)) return;
    gc_heap_add_root(runtime_heap, GC_FROM_PAYLOAD(payload));
}

void luna_gc_runtime_remember(void *payload) {
    if (!runtime_heap || !payload) return;
    if (!gc_heap_is_managed_payload(runtime_heap, payload)) return;
    GCObject *obj = GC_FROM_PAYLOAD(payload);
    gc_remembered_push(runtime_heap, obj);
}

void luna_gc_runtime_write_barrier(void *payload) {
    if (!runtime_heap || !payload) return;
    if (!gc_heap_is_managed_payload(runtime_heap, payload)) return;
    gc_heap_write_barrier(runtime_heap, GC_FROM_PAYLOAD(payload));
}

int luna_gc_runtime_is_managed_payload(void *payload) {
    if (!runtime_heap || !payload) return 0;
    return gc_heap_is_managed_payload(runtime_heap, payload);
}
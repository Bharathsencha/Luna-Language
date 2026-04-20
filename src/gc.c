// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include "gc.h"

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

    for (GCObject *obj = heap->large_list; obj; obj = obj->next) {
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
    block->bump = 0;
    block->young_objects = 0;
    block->next = NULL;
    block->evacuating = false;
    return block;
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

static void gc_remember_if_points_to_young(GCHeap *heap, GCObject *obj) {
    if (!heap || !obj || obj->generation != GC_GEN_OLD || !obj->trace) return;

    GCPromotedRememberCtx scan = {0};
    GCTraceCtx ctx = {
        .heap = heap,
        .visit = gc_detect_young_ref,
        .userdata = &scan,
    };
    obj->trace(obj, &ctx);
    if (scan.has_young_ref) {
        gc_remembered_push(heap, obj);
    }
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

static size_t gc_compute_young_limit(size_t heap_limit, size_t bytes_live) {
    size_t young = heap_limit / 4;
    size_t min_young = 1 * 1024 * 1024;
    size_t max_young = 8 * 1024 * 1024;
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

static void gc_verify_visit(void *ctx, GCObject *obj) {
    GCTraceCtx *trace = (GCTraceCtx *)ctx;
    GCVerifyState *state = (GCVerifyState *)trace->userdata;
    if (!obj) return;
    if (obj->color == GC_DEAD) {
        fprintf(stderr, "gc verify: reachable object marked dead\n");
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
    if (heap->gray_top == heap->gray_cap) {
        size_t new_cap = heap->gray_cap ? heap->gray_cap * 2 : 64;
        GCObject **new_stack =
            (GCObject **)realloc(heap->gray_stack, new_cap * sizeof(GCObject *));
        if (!new_stack) {
            fprintf(stderr, "gc: gray stack out of memory\n");
            abort();
        }
        heap->gray_stack = new_stack;
        heap->gray_cap = new_cap;
    }

    heap->gray_stack[heap->gray_top++] = obj;
}

static GCObject *gray_pop(GCHeap *heap) {
    if (heap->gray_top == 0) return NULL;
    return heap->gray_stack[--heap->gray_top];
}

void _gc_gray_push(GCHeap *heap, GCObject *obj) {
    gray_push(heap, obj);
}

GCHeap *gc_heap_create(size_t initial_limit) {
    GCHeap *heap = (GCHeap *)calloc(1, sizeof(GCHeap));
    if (!heap) abort();

    heap->current = imix_block_new();
    heap->blocks = heap->current;
    heap->heap_limit = initial_limit ? initial_limit : (4 * 1024 * 1024);
    heap->growth_factor = 1.5;
    heap->increment_steps = 64;
    heap->sweep_block_budget = 3;
    heap->incremental_mode = false;
    heap->young_limit = gc_compute_young_limit(heap->heap_limit, 0);
    heap->major_interval = gc_compute_major_interval(0, heap->heap_limit);
    heap->stress_mode = getenv("LUNA_GC_STRESS") != NULL;
    heap->verify_mode = getenv("LUNA_GC_VERIFY") != NULL;
    heap->heap_limit = gc_env_size("LUNA_GC_INITIAL_HEAP_LIMIT", heap->heap_limit, 256 * 1024, (size_t)8 * 1024 * 1024 * 1024ULL);
    heap->young_limit = gc_env_size("LUNA_GC_YOUNG_LIMIT", heap->young_limit, 16 * 1024, 64 * 1024 * 1024);
    heap->major_interval = gc_env_size("LUNA_GC_MAJOR_INTERVAL", heap->major_interval, 1, 1000000);
    heap->incremental_mode = gc_env_bool("LUNA_GC_INCREMENTAL", heap->incremental_mode ? 1 : 0) != 0;
    heap->increment_steps = gc_env_size("LUNA_GC_INCREMENT_STEPS", heap->increment_steps, 1, 4096);
    heap->sweep_block_budget = gc_env_size("LUNA_GC_SWEEP_BUDGET", heap->sweep_block_budget, 1, 64);

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

    ImixBlock *block = heap->blocks;
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

    obj->color = (heap->sweep_in_progress && total > IMIX_BLOCK_SIZE) ? GC_BLACK : GC_WHITE;
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
        if (heap->minor_collection && obj->generation == GC_GEN_OLD) {
            GCTraceCtx ctx = { heap, NULL, NULL };
            if (obj->trace) obj->trace(obj, &ctx);
            continue;
        }
        if (obj->color == GC_WHITE) {
            obj->color = GC_GRAY;
            gray_push(heap, obj);
        }
    }
}

static void mark_roots(GCHeap *heap) {
    GCTraceCtx ctx = { heap, NULL, heap->root_marker_ctx };

    for (size_t i = 0; i < heap->root_count; i++) {
        GCObject *root = heap->roots[i];
        if (heap->minor_collection && root && root->generation == GC_GEN_OLD) {
            if (root->trace) root->trace(root, &ctx);
            continue;
        }
        if (root && root->color == GC_WHITE) {
            root->color = GC_GRAY;
            gray_push(heap, root);
        }
    }

    if (heap->root_marker) {
        heap->root_marker(&ctx);
    }

    if (heap->minor_collection) {
        gc_remember_from_roots(heap);
    }
}

static bool drain_gray(GCHeap *heap, size_t count) {
    GCTraceCtx ctx = { heap, NULL, NULL };

    for (size_t i = 0; i < count || count == 0; i++) {
        GCObject *obj = gray_pop(heap);
        if (!obj) return true;

        obj->color = GC_BLACK;
        if (obj->trace) obj->trace(obj, &ctx);
    }

    return heap->gray_top == 0;
}

static void sweep_block(GCHeap *heap, ImixBlock *block) {
    if (heap->minor_collection && block->young_objects == 0) {
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

static void sweep_large(GCHeap *heap) {
    GCObject **prev = &heap->large_list;
    GCObject *obj = heap->large_list;

    while (obj) {
        GCObject *next = obj->next;
        size_t total = gc_object_total_size(obj->size);

        if (heap->minor_collection) {
            if (obj->generation == GC_GEN_YOUNG) {
                if (obj->color == GC_WHITE) {
                    if (obj->finalize) obj->finalize(obj);
                    heap->bytes_allocated -= total;
                    if (heap->young_bytes_allocated >= total) heap->young_bytes_allocated -= total;
                    *prev = next;
                    free(obj);
                } else {
                    obj->generation = GC_GEN_OLD;
                    obj->color = GC_WHITE;
                    gc_remember_if_points_to_young(heap, obj);
                    if (heap->young_bytes_allocated >= total) heap->young_bytes_allocated -= total;
                    heap->bytes_live += total;
                    prev = &obj->next;
                }
            } else {
                obj->color = GC_WHITE;
                heap->bytes_live += total;
                prev = &obj->next;
            }
        } else if (obj->color == GC_WHITE) {
            if (obj->finalize) obj->finalize(obj);
            heap->bytes_allocated -= total;
            if (obj->generation == GC_GEN_YOUNG && heap->young_bytes_allocated >= total) {
                heap->young_bytes_allocated -= total;
            }
            *prev = next;
            free(obj);
        } else {
            obj->generation = GC_GEN_OLD;
            obj->color = GC_WHITE;
            if (heap->young_bytes_allocated >= total) heap->young_bytes_allocated -= total;
            heap->bytes_live += total;
            prev = &obj->next;
        }

        obj = next;
    }
}

static void mark_evacuate_candidates(GCHeap *heap) {
    for (ImixBlock *block = heap->blocks; block; block = block->next) {
        size_t live_lines = 0;
        for (size_t i = 0; i < IMIX_LINES_PER_BLOCK; i++) live_lines += block->line_mark[i];
        block->evacuating = (live_lines < IMIX_LINES_PER_BLOCK / 4);
    }
}

static void reclaim_empty_blocks(GCHeap *heap) {
    ImixBlock *prev = NULL;
    ImixBlock *block = heap->blocks;
    ImixBlock *fallback = heap->blocks;

    while (block) {
        ImixBlock *next = block->next;
        int any_live = 0;
        for (size_t i = 0; i < IMIX_LINES_PER_BLOCK; i++) {
            if (block->line_mark[i]) {
                any_live = 1;
                break;
            }
        }

        if (!any_live) {
            block->bump = 0;
            memset(block->line_mark, 0, sizeof(block->line_mark));

            if (next || prev) {
                if (prev) prev->next = next;
                else heap->blocks = next;
                if (heap->current == block) heap->current = next ? next : heap->blocks;
                free(block);
                block = next;
                continue;
            }
        }

        fallback = block;
        prev = block;
        block = next;
    }

    if (!heap->blocks) {
        heap->blocks = imix_block_new();
        heap->current = heap->blocks;
        return;
    }

    if (!heap->current) heap->current = fallback ? fallback : heap->blocks;
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

    if (!heap->minor_collection || heap->sweep_reclaim_empty) {
        reclaim_empty_blocks(heap);
    }
    gc_select_current_block(heap);
    sweep_large(heap);
    if (!heap->minor_collection) {
        mark_evacuate_candidates(heap);
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
    heap->sweep_minor = false;
    heap->sweep_reclaim_empty = false;
    heap->sweep_cursor = NULL;
    heap->collection_in_progress = false;
    heap->minor_collection = false;
    gc_verify_heap(heap);
}

static void gc_prepare_sweep_phase(GCHeap *heap, bool minor, bool reclaim_empty) {
    if (!heap) return;

    heap->sweep_cursor = heap->blocks;
    heap->sweep_in_progress = true;
    heap->sweep_minor = minor;
    heap->sweep_reclaim_empty = reclaim_empty;
    heap->collection_in_progress = false;

    ImixBlock *fresh = imix_block_new();
    fresh->next = heap->blocks;
    heap->blocks = fresh;
    heap->current = fresh;
}

static void gc_sweep_some_blocks(GCHeap *heap, size_t budget) {
    if (!heap || !heap->sweep_in_progress) return;

    size_t swept = 0;
    while (heap->sweep_cursor && swept < budget) {
        ImixBlock *block = heap->sweep_cursor;
        heap->sweep_cursor = block->next;
        sweep_block(heap, block);
        swept++;
    }

    if (!heap->sweep_cursor) {
        gc_finish_sweep_phase(heap);
    }
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

void gc_heap_step(GCHeap *heap) {
    uint64_t start_ns = gc_now_ns();

    if (heap->sweep_in_progress) {
        gc_sweep_some_blocks(heap, heap->sweep_block_budget);
        gc_heap_record_pause(heap, start_ns);
        return;
    }

    if (!heap->collection_in_progress) {
        heap->collection_in_progress = true;
        heap->bytes_live = 0;
        heap->minor_collection = false;
        gc_reset_remembered_set(heap);
        mark_roots(heap);
    }

    if (drain_gray(heap, heap->increment_steps)) {
        gc_prepare_sweep_phase(heap, false, true);
        gc_sweep_some_blocks(heap, heap->sweep_block_budget);
    }

    gc_heap_record_pause(heap, start_ns);
}

static void gc_heap_collect_minor(GCHeap *heap) {
    uint64_t start_ns = gc_now_ns();
    heap->collection_in_progress = true;
    heap->minor_collection = true;
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
        heap->minor_collection = true;
        heap->bytes_live = 0;
        mark_roots(heap);
    }

    if (drain_gray(heap, heap->increment_steps)) {
        gc_prepare_sweep_phase(heap, true, gc_should_reclaim_empty_blocks_on_minor(heap));
        gc_sweep_some_blocks(heap, heap->sweep_block_budget);
    }

    gc_heap_record_pause(heap, start_ns);
}

void gc_heap_maybe_collect(GCHeap *heap) {
    if (!heap) return;

    if (heap->sweep_in_progress) {
        uint64_t start_ns = gc_now_ns();
        gc_sweep_some_blocks(heap, heap->sweep_block_budget);
        gc_heap_record_pause(heap, start_ns);
        if (heap->sweep_in_progress) return;
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
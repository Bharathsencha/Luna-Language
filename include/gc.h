// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef LUNA_GC_H
#define LUNA_GC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    double gc_ms_total;
    double gc_ms_max;
    unsigned long long gc_events;
} LunaGCStats;

typedef void (*GCRootMarker)(void *ctx);

void gc_stats_reset(void);
void gc_stats_record_pause_ns(unsigned long long ns);
LunaGCStats gc_stats_snapshot(void);

typedef enum {
    GC_WHITE = 0,
    GC_GRAY  = 1,
    GC_BLACK = 2,
    GC_DEAD  = 3,
} GCColor;

typedef enum {
    GC_GEN_YOUNG = 0,
    GC_GEN_OLD = 1,
} GCGeneration;

#define IMIX_LINE_SIZE       256
#define IMIX_LINES_PER_BLOCK 128
#define IMIX_BLOCK_SIZE      (IMIX_LINE_SIZE * IMIX_LINES_PER_BLOCK)

typedef struct GCObject GCObject;
typedef struct GCHeap GCHeap;
typedef void (*GCTracer)(GCObject *obj, void *ctx);
typedef void (*GCFinalizer)(GCObject *obj);
typedef void (*GCVisitFn)(void *ctx, GCObject *obj);

typedef struct {
    GCHeap    *heap;
    GCVisitFn  visit;
    void      *userdata;
} GCTraceCtx;

struct GCObject {
    GCColor      color;
    bool         pinned;
    uint8_t      generation;
    uint8_t      remembered;
    uint32_t     size;
    GCObject    *next;
    GCTracer     trace;
    GCFinalizer  finalize;
};

#define GC_PAYLOAD(obj)     ((void *)((GCObject *)(obj) + 1))
#define GC_FROM_PAYLOAD(p)  ((GCObject *)(p) - 1)

typedef struct ImixBlock ImixBlock;
struct ImixBlock {
    uint8_t      line_mark[IMIX_LINES_PER_BLOCK];
    uint8_t      data[IMIX_BLOCK_SIZE];
    size_t       bump;
    size_t       young_objects;
    ImixBlock   *next;
    bool         evacuating;
};

struct GCHeap {
    ImixBlock   *blocks;
    ImixBlock   *current;
    GCObject   **gray_stack;
    size_t       gray_top;
    size_t       gray_cap;
    GCObject    *large_list;
    GCObject   **roots;
    size_t       root_count;
    size_t       root_cap;
    size_t       bytes_allocated;
    size_t       bytes_live;
    size_t       total_collections;
    size_t       total_allocs;
    size_t       heap_limit;
    double       growth_factor;
    bool         incremental_mode;
    size_t       increment_steps;
    bool         collection_in_progress;
    bool         minor_collection;
    bool         sweep_in_progress;
    bool         sweep_minor;
    bool         sweep_reclaim_empty;
    double       gc_ms_total;
    double       gc_ms_max;
    GCRootMarker root_marker;
    void        *root_marker_ctx;
    size_t       young_bytes_allocated;
    size_t       young_limit;
    size_t       minor_since_major;
    size_t       major_interval;
    GCObject   **remembered_set;
    size_t       remembered_count;
    size_t       remembered_cap;
    ImixBlock   *sweep_cursor;
    size_t       sweep_block_budget;
    bool         stress_mode;
    bool         verify_mode;
};

GCHeap *gc_heap_create(size_t initial_limit);
void    gc_heap_destroy(GCHeap *heap);
void   *gc_heap_alloc(GCHeap *heap, size_t size, GCTracer trace, GCFinalizer fin);
void    gc_heap_add_root(GCHeap *heap, GCObject *obj);
void    gc_heap_remove_root(GCHeap *heap, GCObject *obj);
void    gc_heap_write_barrier(GCHeap *heap, GCObject *oldref);
void    gc_heap_collect(GCHeap *heap);
void    gc_heap_step(GCHeap *heap);
void    gc_heap_maybe_collect(GCHeap *heap);
void    gc_heap_set_root_marker(GCHeap *heap, GCRootMarker marker, void *ctx);

typedef struct {
    size_t bytes_allocated;
    size_t bytes_live;
    size_t total_collections;
    size_t total_allocs;
    size_t block_count;
    size_t large_object_count;
    size_t gray_stack_peak;
    double gc_ms_total;
    double gc_ms_max;
} GCHeapStats;

GCHeapStats gc_heap_stats(GCHeap *heap);
void        gc_heap_print_stats(GCHeap *heap);
void        gc_visit_ref(void *ctx, void **slot);
int         gc_heap_is_managed_payload(GCHeap *heap, void *payload);

int         luna_gc_runtime_init(size_t initial_limit);
void        luna_gc_runtime_shutdown(void);
int         luna_gc_runtime_enabled(void);
GCHeap     *luna_gc_runtime_heap(void);
void       *luna_gc_alloc(size_t size, GCTracer trace, GCFinalizer fin);
void        luna_gc_runtime_safe_point(void);
void        luna_gc_runtime_set_root_marker(GCRootMarker marker, void *ctx);
void        luna_gc_runtime_add_root(void *payload);
void        luna_gc_runtime_remember(void *payload);
void        luna_gc_runtime_write_barrier(void *payload);
int         luna_gc_runtime_is_managed_payload(void *payload);

#endif
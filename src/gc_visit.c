// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include "gc.h"
#include <time.h>

void _gc_gray_push(GCHeap *heap, GCObject *obj);

/* Thread-local visit counter used to amortise clock_gettime calls.
 * Exposed so drain_gray can reset it at the start of each step, giving
 * every fresh drain a full target_pause_ns budget window. */
static _Thread_local size_t gc_visit_count = 0;

void gc_visit_reset_deadline_counter(void) {
    gc_visit_count = 0;
}

void gc_visit_tick(void *ctx) {
    if (!ctx) return;
    GCTraceCtx *trace = (GCTraceCtx *)ctx;
    if (trace->deadline_hit) return;
    if (trace->deadline_ns > 0) {
        if ((++gc_visit_count & 63) == 0) {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            uint64_t now = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
            if (now >= trace->deadline_ns) {
                trace->deadline_hit = true;
            }
        }
    }
}

void gc_visit_ref(void *ctx, void **slot) {
    if (!ctx || !slot || !*slot) return;

    GCTraceCtx *trace = (GCTraceCtx *)ctx;

    gc_visit_tick(ctx);
    if (trace->deadline_hit) return;

    GCObject *child = GC_FROM_PAYLOAD(*slot);

    if (trace->visit) {
        trace->visit(ctx, child);
        return;
    }

    GCHeap *heap = trace->heap;

    if (heap->minor_collection && child->generation == GC_GEN_OLD) {
        return;
    }

    if (child->color == GC_WHITE) {
        child->color = GC_GRAY;
        _gc_gray_push(heap, child);
    }
}
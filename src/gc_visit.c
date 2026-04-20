// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include "gc.h"

void _gc_gray_push(GCHeap *heap, GCObject *obj);

void gc_visit_ref(void *ctx, void **slot) {
    if (!ctx || !slot || !*slot) return;

    GCTraceCtx *trace = (GCTraceCtx *)ctx;
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
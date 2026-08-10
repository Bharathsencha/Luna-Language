#include "luna_vm.h"
#include "env.h"
#include "luna_chunk.h"
#include "unsafe_runtime.h"

static void mark_chunk_constants(LunaChunk *chunk, void *ctx) {
    if (!chunk) return;
    for (size_t i = 0; i < chunk->const_len; i++) {
        value_gc_mark(&chunk->constants[i], ctx);
    }
    for (size_t i = 0; i < chunk->subchunk_len; i++) {
        mark_chunk_constants(chunk->subchunks[i], ctx);
    }
}

void vm_gc_mark_roots(void *ctx) {
    GCTraceCtx *trace_ctx = (GCTraceCtx *)ctx;
    LunaVM *vm = (LunaVM *)trace_ctx->userdata;
    if (!vm) return;

    // 1. Mark all active register slots on the VM value stack
    for (Value *slot = vm->stack; slot < vm->stack_top; slot++) {
        value_gc_mark(slot, ctx);
    }

    // 2. Mark all open upvalues (either on-stack locations or closed values)
    for (VMUpvalue *up = vm->open_upvalues; up != NULL; up = up->next) {
        if (up->location) {
            value_gc_mark(up->location, ctx);
        } else {
            value_gc_mark(&up->closed, ctx);
        }
    }

    // 3. Mark active environment roots (which traces global variables)
    env_gc_mark_active_roots(ctx);

    // 4. Mark all constants of all active call stack chunks
    for (int i = 0; i < vm->frame_count; i++) {
        mark_chunk_constants(vm->frames[i].chunk, ctx);
    }

    // 5. Mark unsafe runtime resources
    unsafe_runtime_gc_mark_roots(ctx);
}

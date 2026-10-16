// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef LUNA_VM_H
#define LUNA_VM_H

#include "value.h"
#include "luna_chunk.h"
#include "gc.h"

typedef struct VMUpvalue {
    Value *location; // points to stack or to 'closed' field
    Value closed;
    struct VMUpvalue *next;
} VMUpvalue;

typedef struct {
    LunaChunk *chunk;
    uint8_t   *ip;
    Value     *slots; // points into VM stack
    VMUpvalue **upvalues;
    uint8_t    argc;       // number of arguments passed to this call frame
    uint8_t    ret_dst;    // register in the caller where the return value goes
    int        scope_base; // VM scope depth when this frame was entered
} VMCallFrame;

#define FRAMES_MAX 64
#define STACK_MAX 1024
#define VM_SCOPE_MAX 256
#define VM_DEFERRED_MAX 64

typedef struct {
    Value  callee;
    Value *argv;
    int    argc;
    int    line;
} VMDeferred;

typedef struct {
    VMCallFrame frames[FRAMES_MAX];
    int frame_count;

    Value stack[STACK_MAX];
    Value *stack_top;

    VMUpvalue *open_upvalues;
    GCHeap *heap;
    struct Env *env;

    uint64_t next_scope_id;
    uint64_t scope_stack[VM_SCOPE_MAX];
    int      scope_depth;
    int      defer_base_stack[VM_SCOPE_MAX];

    VMDeferred deferred[VM_DEFERRED_MAX];
    int        deferred_count;
} LunaVM;

void luna_vm_init(LunaVM *vm, GCHeap *heap);
void luna_vm_free(LunaVM *vm);
Value luna_vm_run(LunaVM *vm, LunaChunk *chunk);
Value luna_vm_execute(LunaVM *vm);
Value luna_vm_call_closure(GCHeap *heap, struct Env *env, VMClosureObj *closure,
                           int argc, Value *argv, int line);
void luna_vm_register(LunaVM *vm);
void luna_vm_unregister(LunaVM *vm);
void vm_gc_mark_roots(void *ctx);
void vm_gc_mark_defer_roots(LunaVM *vm, void *ctx);
void vm_upvalue_trace(GCObject *obj, void *ctx);

#endif // LUNA_VM_H

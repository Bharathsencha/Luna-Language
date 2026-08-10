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
} VMCallFrame;

#define FRAMES_MAX 64
#define STACK_MAX 1024

typedef struct {
    VMCallFrame frames[FRAMES_MAX];
    int frame_count;

    Value stack[STACK_MAX];
    Value *stack_top;

    VMUpvalue *open_upvalues;
    GCHeap *heap;
    struct Env *env;
} LunaVM;

void luna_vm_init(LunaVM *vm, GCHeap *heap);
void luna_vm_free(LunaVM *vm);
Value luna_vm_run(LunaVM *vm, LunaChunk *chunk);
void vm_gc_mark_roots(void *ctx);
void vm_upvalue_trace(GCObject *obj, void *ctx);

#endif // LUNA_VM_H

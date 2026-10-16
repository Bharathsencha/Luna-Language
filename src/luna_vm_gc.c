#include "luna_vm.h"
#include "env.h"
#include "luna_chunk.h"
#include "unsafe_runtime.h"

/* Active VM registry: nested VMs (e.g. module imports executed through the
 * VM) must keep the outer VM's roots alive while they run. */
#define VM_REGISTRY_MAX 16
static LunaVM *vm_registry[VM_REGISTRY_MAX];
static int vm_registry_count = 0;

void luna_vm_register(LunaVM *vm) {
    for (int i = 0; i < vm_registry_count; i++) {
        if (vm_registry[i] == vm) return;
    }
    if (vm_registry_count < VM_REGISTRY_MAX) {
        vm_registry[vm_registry_count++] = vm;
    }
}

void luna_vm_unregister(LunaVM *vm) {
    for (int i = 0; i < vm_registry_count; i++) {
        if (vm_registry[i] == vm) {
            vm_registry[i] = vm_registry[--vm_registry_count];
            return;
        }
    }
}

static void mark_chunk_constants(LunaChunk *chunk, void *ctx) {
    if (!chunk) return;
    for (size_t i = 0; i < chunk->const_len; i++) {
        value_gc_mark(&chunk->constants[i], ctx);
    }
    for (size_t i = 0; i < chunk->subchunk_len; i++) {
        mark_chunk_constants(chunk->subchunks[i], ctx);
    }
}

static void vm_gc_mark_roots_one(LunaVM *vm, void *ctx) {
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

    // 5. Mark deferred call values
    vm_gc_mark_defer_roots(vm, ctx);

    // 6. Mark unsafe runtime resources
    unsafe_runtime_gc_mark_roots(ctx);
}

void vm_gc_mark_roots(void *ctx) {
    for (int i = 0; i < vm_registry_count; i++) {
        vm_gc_mark_roots_one(vm_registry[i], ctx);
    }
}

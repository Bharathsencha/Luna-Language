// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "unsafe_runtime.h"
#include "luna_error.h"
#include "memory.h"

typedef enum {
    PTR_KIND_ALLOC = 0,
    PTR_KIND_ADDR = 1,
} PointerKind;

typedef struct {
    uintptr_t ptr;
    uintptr_t base;
    size_t cells;
    PointerKind kind;
    int owns_allocation;
    int freed;
    unsigned char *init_flags;
} PointerMeta;

static PointerMeta *unsafe_ptrs = NULL;
static int unsafe_ptr_count = 0;
static int unsafe_ptr_cap = 0;
static int unsafe_block_depth = 0;
static uintptr_t *unsafe_defer_ptrs = NULL;
static int unsafe_defer_count = 0;
static int unsafe_defer_cap = 0;

static void unsafe_ptrs_reserve(void) {
    if (unsafe_ptr_count < unsafe_ptr_cap) return;
    int next_cap = unsafe_ptr_cap == 0 ? 16 : unsafe_ptr_cap * 2;
    PointerMeta *grown = realloc(unsafe_ptrs, sizeof(PointerMeta) * next_cap);
    if (!grown) return;
    unsafe_ptrs = grown;
    unsafe_ptr_cap = next_cap;
}

static PointerMeta *unsafe_find_exact(uintptr_t ptr) {
    for (int i = 0; i < unsafe_ptr_count; i++) {
        if (unsafe_ptrs[i].ptr == ptr) return &unsafe_ptrs[i];
    }
    return NULL;
}

static PointerMeta *unsafe_find_owner(uintptr_t ptr) {
    for (int i = 0; i < unsafe_ptr_count; i++) {
        PointerMeta *meta = &unsafe_ptrs[i];
        size_t span = meta->cells * sizeof(Value);
        uintptr_t end = meta->base + span;
        if (ptr >= meta->base && ptr < end) return meta;
    }
    return NULL;
}

static PointerMeta *unsafe_resolve_alloc_meta(PointerMeta *meta) {
    if (!meta || meta->kind != PTR_KIND_ALLOC) return meta;
    PointerMeta *owner = unsafe_find_exact(meta->base);
    return owner ? owner : meta;
}

static void unsafe_register_alloc_meta(uintptr_t ptr, size_t cells) {
    unsafe_ptrs_reserve();
    if (unsafe_ptr_count >= unsafe_ptr_cap) return;
    PointerMeta *meta = &unsafe_ptrs[unsafe_ptr_count++];
    meta->ptr = ptr;
    meta->base = ptr;
    meta->cells = cells;
    meta->kind = PTR_KIND_ALLOC;
    meta->owns_allocation = 1;
    meta->freed = 0;
    meta->init_flags = calloc(cells, sizeof(unsigned char));
}

static void unsafe_register_addr_meta(uintptr_t ptr) {
    PointerMeta *exact = unsafe_find_exact(ptr);
    if (exact) return;
    unsafe_ptrs_reserve();
    if (unsafe_ptr_count >= unsafe_ptr_cap) return;
    PointerMeta *meta = &unsafe_ptrs[unsafe_ptr_count++];
    meta->ptr = ptr;
    meta->base = ptr;
    meta->cells = 1;
    meta->kind = PTR_KIND_ADDR;
    meta->owns_allocation = 0;
    meta->freed = 0;
    meta->init_flags = NULL;
}

static void unsafe_register_alias_meta(uintptr_t ptr, PointerMeta *owner) {
    PointerMeta *exact = unsafe_find_exact(ptr);
    if (exact) return;
    unsafe_ptrs_reserve();
    if (unsafe_ptr_count >= unsafe_ptr_cap) return;
    PointerMeta *meta = &unsafe_ptrs[unsafe_ptr_count++];
    meta->ptr = ptr;
    meta->base = owner->base;
    meta->cells = owner->cells;
    meta->kind = owner->kind;
    meta->owns_allocation = 0;
    meta->freed = owner->freed;
    meta->init_flags = owner->init_flags;
}

static const char *unsafe_error_hint(int code) {
    switch (code) {
        case 1: return "address_of() only works on normal values. If you already have a pointer, use it directly.";
        case 2: return "Pointers must stay inside the current unsafe block. Copy the pointed-to value out instead.";
        case 3: return "That pointer step moved past the allocated buffer. Keep ptr_add() within the allocated slot range.";
        case 4: return "That pointer is no longer valid. Prefer defer(ptr) so cleanup happens once at block exit.";
        case 5: return "address_of() needs a variable name such as address_of(x), not a temporary expression.";
        case 6: return "Nested unsafe blocks are not supported. Merge the inner block into the outer one.";
        case 7: return "Pointers cannot go into lists or maps because those values are GC-managed.";
        case 8: return "alloc() needs a positive whole-number slot count, for example alloc(4).";
        case 9: return "Only compare pointers with <, >, <=, or >= when they come from the same allocation.";
        case 10: return "Only int(ptr) is allowed for pointer casts.";
        case 11: return "Inside unsafe, call builtins only. Move user-defined function calls outside the block.";
        case 12: return "Do not put free(ptr) inside if/while/for/switch branches. Use defer(ptr) or free it unconditionally.";
        case 13: return "Use a pointer returned by alloc(), address_of(), or ptr_add().";
        default: return "Check the unsafe memory rule and the pointer lifetime in this block.";
    }
}

static int report_unsafe_error(int code, int line) {
    char msg[256];
    luna_mem_error_message(code, msg, sizeof(msg));
    error_report_with_context(ERR_RUNTIME, line, 0, msg, unsafe_error_hint(code));
    return 0;
}

static int unsafe_runtime_check(int code, int line) {
    if (code == 0) return 1;
    return report_unsafe_error(code, line);
}

static void unsafe_defer_push_local(uintptr_t ptr) {
    if (unsafe_defer_count >= unsafe_defer_cap) {
        int next_cap = unsafe_defer_cap == 0 ? 16 : unsafe_defer_cap * 2;
        uintptr_t *grown = realloc(unsafe_defer_ptrs, sizeof(uintptr_t) * next_cap);
        if (!grown) return;
        unsafe_defer_ptrs = grown;
        unsafe_defer_cap = next_cap;
    }
    unsafe_defer_ptrs[unsafe_defer_count++] = ptr;
}

static void unsafe_free_alloc_meta(PointerMeta *meta) {
    if (!meta || meta->kind != PTR_KIND_ALLOC || !meta->owns_allocation || meta->freed) return;
    Value *slots = (Value *)meta->base;
    for (size_t i = 0; i < meta->cells; i++) {
        if (meta->init_flags && meta->init_flags[i]) value_free(slots[i]);
    }
    free((void *)meta->base);
    meta->freed = 1;
}

static void unsafe_process_deferred_frees(void) {
    while (unsafe_defer_count > 0) {
        uintptr_t ptr = unsafe_defer_ptrs[--unsafe_defer_count];
        PointerMeta *meta = unsafe_find_exact(ptr);
        if (meta && meta->kind == PTR_KIND_ALLOC && meta->owns_allocation) {
            unsafe_free_alloc_meta(meta);
        }
    }
}

static void unsafe_ptrs_reset(void) {
    for (int i = 0; i < unsafe_ptr_count; i++) {
        if (unsafe_ptrs[i].kind == PTR_KIND_ALLOC && unsafe_ptrs[i].owns_allocation) {
            if (!unsafe_ptrs[i].freed && unsafe_ptrs[i].ptr) {
                Value *slots = (Value *)unsafe_ptrs[i].ptr;
                if (unsafe_ptrs[i].init_flags) {
                    for (size_t j = 0; j < unsafe_ptrs[i].cells; j++) {
                        if (unsafe_ptrs[i].init_flags[j]) value_free(slots[j]);
                    }
                }
                free((void *)unsafe_ptrs[i].ptr);
            }
            free(unsafe_ptrs[i].init_flags);
        }
    }
    free(unsafe_ptrs);
    unsafe_ptrs = NULL;
    unsafe_ptr_count = 0;
    unsafe_ptr_cap = 0;
    free(unsafe_defer_ptrs);
    unsafe_defer_ptrs = NULL;
    unsafe_defer_count = 0;
    unsafe_defer_cap = 0;
}

void unsafe_runtime_reset(void) {
    unsafe_block_depth = 0;
    unsafe_ptrs_reset();
    luna_mem_init();
}

void unsafe_runtime_shutdown(void) {
    unsafe_ptrs_reset();
    luna_mem_init();
}

int unsafe_runtime_inside_block(void) {
    return unsafe_block_depth > 0;
}

int unsafe_runtime_is_pointer(Value v) {
    return v.type == VAL_POINTER && unsafe_find_owner(v.ptr) != NULL;
}

int unsafe_runtime_begin_block(int line) {
    if (!unsafe_runtime_check(luna_mem_begin(), line)) return 0;
    unsafe_block_depth++;
    return 1;
}

void unsafe_runtime_end_block(void) {
    if (unsafe_block_depth > 0) unsafe_block_depth--;
    luna_mem_end();
    unsafe_process_deferred_frees();
}

int unsafe_runtime_check_gc_store(Value v, int line) {
    if (!unsafe_runtime_is_pointer(v)) return 1;
    return unsafe_runtime_check(luna_mem_gc_store_ok(v.ptr), line);
}

int unsafe_runtime_check_escape(Value v, int line) {
    if (!unsafe_runtime_is_pointer(v)) return 1;
    return unsafe_runtime_check(luna_mem_escape_ok(v.ptr), line);
}

int unsafe_runtime_check_compare(Value left, Value right, int op, int line) {
    if (!unsafe_runtime_is_pointer(left) || !unsafe_runtime_is_pointer(right)) return 1;
    return unsafe_runtime_check(luna_mem_cmp_ok(left.ptr, right.ptr, op), line);
}

int unsafe_runtime_check_cast(Value v, int target_type, int line) {
    if (!unsafe_runtime_is_pointer(v)) return 1;
    return unsafe_runtime_check(luna_mem_cast_ok(v.ptr, target_type), line);
}

int unsafe_runtime_check_call(int is_builtin, int line) {
    return unsafe_runtime_check(luna_mem_call_ok(is_builtin), line);
}

Value unsafe_runtime_alloc(Value size, int line) {
    int is_integer = size.type == VAL_INT;
    long long cells = is_integer ? size.i : 0;
    if (!unsafe_runtime_check(luna_mem_alloc_size_ok(cells, is_integer), line)) {
        return value_null();
    }

    Value *mem = calloc((size_t)cells, sizeof(Value));
    if (!mem) {
        error_report_with_context(ERR_RUNTIME, line, 0,
            "alloc() failed to reserve memory",
            "Try a smaller allocation.");
        return value_null();
    }

    uintptr_t ptr = (uintptr_t)mem;
    unsafe_register_alloc_meta(ptr, (size_t)cells);
    luna_mem_track_alloc(ptr, (size_t)cells * sizeof(Value));
    return value_pointer(ptr);
}

Value unsafe_runtime_free(Value ptrv, int line) {
    if (!unsafe_runtime_is_pointer(ptrv)) {
        error_report_with_context(ERR_TYPE, line, 0,
            "free() expects a pointer value",
            "Pass a pointer returned by alloc(), address_of(), or ptr_add().");
        return value_null();
    }

    PointerMeta *meta = unsafe_find_exact(ptrv.ptr);
    if (!meta || meta->kind != PTR_KIND_ALLOC || !meta->owns_allocation) {
        report_unsafe_error(13, line);
        return value_null();
    }
    if (!unsafe_runtime_check(luna_mem_free_ok(ptrv.ptr), line)) {
        return value_null();
    }

    unsafe_free_alloc_meta(meta);
    return value_null();
}

Value unsafe_runtime_deref(Value ptrv, int line) {
    if (!unsafe_runtime_is_pointer(ptrv)) {
        report_unsafe_error(13, line);
        return value_null();
    }

    PointerMeta *meta = unsafe_find_owner(ptrv.ptr);
    if (!meta) {
        report_unsafe_error(13, line);
        return value_null();
    }
    PointerMeta *alloc_meta = unsafe_resolve_alloc_meta(meta);
    if (meta->kind == PTR_KIND_ALLOC &&
        !unsafe_runtime_check(luna_mem_deref_ok(ptrv.ptr), line)) {
        return value_null();
    }

    Value *slot = (Value *)(uintptr_t)ptrv.ptr;
    if (meta->kind == PTR_KIND_ALLOC) {
        size_t idx = (ptrv.ptr - alloc_meta->base) / sizeof(Value);
        return (alloc_meta->init_flags && alloc_meta->init_flags[idx]) ? value_copy(*slot) : value_null();
    }
    return value_copy(*slot);
}

Value unsafe_runtime_store(Value ptrv, Value rhs, int line) {
    if (!unsafe_runtime_is_pointer(ptrv)) {
        report_unsafe_error(13, line);
        return value_null();
    }

    PointerMeta *meta = unsafe_find_owner(ptrv.ptr);
    if (!meta) {
        report_unsafe_error(13, line);
        return value_null();
    }
    PointerMeta *alloc_meta = unsafe_resolve_alloc_meta(meta);
    if (meta->kind == PTR_KIND_ALLOC &&
        !unsafe_runtime_check(luna_mem_store_ok(ptrv.ptr), line)) {
        return value_null();
    }
    if (!unsafe_runtime_check_gc_store(rhs, line)) {
        return value_null();
    }

    Value *slot = (Value *)(uintptr_t)ptrv.ptr;
    if (meta->kind == PTR_KIND_ALLOC) {
        size_t idx = (ptrv.ptr - alloc_meta->base) / sizeof(Value);
        if (alloc_meta->init_flags && alloc_meta->init_flags[idx]) value_free(*slot);
        *slot = value_copy(rhs);
        if (alloc_meta->init_flags) alloc_meta->init_flags[idx] = 1;
    } else {
        value_free(*slot);
        *slot = value_copy(rhs);
    }
    return value_null();
}

Value unsafe_runtime_ptr_add(Value basev, Value offv, int line) {
    if (!unsafe_runtime_is_pointer(basev) || offv.type != VAL_INT) {
        error_report_with_context(ERR_TYPE, line, 0,
            "ptr_add() expects a pointer and an integer offset",
            "Use ptr_add(buf, 1).");
        return value_null();
    }

    PointerMeta *owner = unsafe_find_owner(basev.ptr);
    if (!owner) {
        report_unsafe_error(13, line);
        return value_null();
    }

    uintptr_t out_ptr = 0;
    size_t byte_offset = (size_t)offv.i * sizeof(Value);
    if (owner->kind == PTR_KIND_ALLOC &&
        !unsafe_runtime_check(luna_mem_ptr_add_ok(basev.ptr, byte_offset, &out_ptr), line)) {
        return value_null();
    }
    if (owner->kind != PTR_KIND_ALLOC) {
        out_ptr = basev.ptr + byte_offset;
    }

    unsafe_register_alias_meta(out_ptr, owner);
    return value_pointer(out_ptr);
}

Value unsafe_runtime_addr(Value *slot, int line) {
    uintptr_t target = unsafe_runtime_is_pointer(*slot) ? slot->ptr : (uintptr_t)slot;
    if (!unsafe_runtime_check(luna_mem_addr_ok(target, 1), line)) {
        return value_null();
    }

    unsafe_register_addr_meta((uintptr_t)slot);
    return value_pointer((uintptr_t)slot);
}

Value unsafe_runtime_defer(Value ptrv, int line) {
    if (!unsafe_runtime_is_pointer(ptrv)) {
        report_unsafe_error(13, line);
        return value_null();
    }
    luna_mem_defer(ptrv.ptr);
    unsafe_defer_push_local(ptrv.ptr);
    return value_null();
}

void unsafe_runtime_gc_mark_roots(void *ctx) {
    for (int i = 0; i < unsafe_ptr_count; i++) {
        PointerMeta *meta = &unsafe_ptrs[i];
        if (meta->kind != PTR_KIND_ALLOC || !meta->owns_allocation || meta->freed) continue;
        Value *slots = (Value *)(uintptr_t)meta->base;
        for (size_t j = 0; j < meta->cells; j++) {
            if (meta->init_flags && meta->init_flags[j]) {
                value_gc_mark(&slots[j], ctx);
            }
        }
    }
}
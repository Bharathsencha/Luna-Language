// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "luna_vm.h"
#include "luna_opcode.h"
#include "env.h"
#include "luna_error.h"
#include "interpreter.h"
#include "intern.h"
#include "parser.h"
#include "util.h"
#include "unsafe_runtime.h"
#include "luna_compiler.h"
#include "vec_lib.h"

void luna_vm_init(LunaVM *vm, GCHeap *heap) {
    vm->frame_count = 0;
    vm->stack_top = vm->stack;
    vm->open_upvalues = NULL;
    vm->heap = heap;
    vm->env = NULL;
    vm->next_scope_id = 0;
    vm->scope_depth = 0;
    vm->deferred_count = 0;
}

void luna_vm_free(LunaVM *vm) {
    // Free open upvalues
    vm->open_upvalues = NULL;
}

static void close_upvalues(LunaVM *vm, Value *last) {
    while (vm->open_upvalues && vm->open_upvalues->location >= last) {
        VMUpvalue *upval = vm->open_upvalues;
        upval->closed = *upval->location;
        upval->location = &upval->closed;
        vm->open_upvalues = upval->next;
    }
}

static VMUpvalue *capture_upvalue(LunaVM *vm, Value *local) {
    VMUpvalue *prev = NULL;
    VMUpvalue *curr = vm->open_upvalues;
    while (curr && curr->location > local) {
        prev = curr;
        curr = curr->next;
    }

    if (curr && curr->location == local) {
        return curr;
    }

    VMUpvalue *upval = (VMUpvalue *)luna_gc_alloc(sizeof(VMUpvalue), vm_upvalue_trace, NULL);
    upval->location = local;
    upval->closed = value_null();
    upval->next = curr;

    if (prev == NULL) {
        vm->open_upvalues = upval;
    } else {
        prev->next = upval;
    }

    return upval;
}

static double value_to_double(Value v) {
    if (v.type == VAL_INT) return (double)v.i;
    if (v.type == VAL_FLOAT) return v.f;
    return 0.0;
}

/* Inside an unsafe block, pointers may not be stored into GC containers. */
static int vm_ptr_store_ok(Value v, int line) {
    if (!unsafe_runtime_inside_block()) return 1;
    if (!unsafe_runtime_is_pointer(v)) return 1;
    return unsafe_runtime_check_gc_store(v, line);
}

static int vm_is_truthy(Value v) {
    switch (v.type) {
        case VAL_BOOL: return v.b;
        case VAL_INT: return v.i != 0;
        case VAL_FLOAT: return v.f != 0.0;
        case VAL_POINTER: return v.ptr != 0;
        case VAL_STRING: return v.string && v.string->chars && v.string->chars[0] != '\0';
        case VAL_NULL: return 0;
        case VAL_CHAR: return v.c != 0;
        case VAL_FILE: return v.file != NULL;
        case VAL_BLOC:
        case VAL_BLOC_TYPE:
        case VAL_BOX:
        case VAL_TEMPLATE:
        case VAL_LIST:
        case VAL_DENSE_LIST:
        case VAL_MAP:
        case VAL_NATIVE:
        case VAL_CLOSURE:
        case VAL_FUNCTION:
        case VAL_VM_CLOSURE:
        case VAL_DATA_TYPE:
            return 1;
        default: return 0;
    }
}

static void vm_defer_push(LunaVM *vm, Value callee, int argc, const Value *argv, int line) {
    if (!vm || vm->deferred_count >= VM_DEFERRED_MAX) return;
    VMDeferred *d = &vm->deferred[vm->deferred_count++];
    d->callee = value_copy(callee);
    d->argc = argc;
    d->line = line;
    d->argv = argc > 0 ? (Value *)malloc(sizeof(Value) * (size_t)argc) : NULL;
    if (d->argv) {
        for (int i = 0; i < argc; i++) {
            d->argv[i] = value_copy(argv[i]);
        }
    }
}

static void vm_run_defers_since(LunaVM *vm, int base) {
    if (!vm) return;
    while (vm->deferred_count > base) {
        VMDeferred *d = &vm->deferred[--vm->deferred_count];
        Value ret = luna_call_value(vm->env, d->callee, d->argc, d->argv, d->line);
        value_free(ret);
        for (int i = 0; i < d->argc; i++) value_free(d->argv[i]);
        free(d->argv);
        value_free(d->callee);
    }
}

static void vm_run_defers(LunaVM *vm) {
    vm_run_defers_since(vm, 0);
}

static void vm_mark_defers(LunaVM *vm, void *ctx) {
    if (!vm) return;
    for (int i = 0; i < vm->deferred_count; i++) {
        value_gc_mark(&vm->deferred[i].callee, ctx);
        for (int j = 0; j < vm->deferred[i].argc; j++) {
            value_gc_mark(&vm->deferred[i].argv[j], ctx);
        }
    }
}

void vm_gc_mark_defer_roots(LunaVM *vm, void *ctx) {
    vm_mark_defers(vm, ctx);
}

static void vm_collect_exports(AstNode *n, const char ***names, int *count) {
    if (!n) return;
    if (n->kind == NODE_BLOCK) {
        for (int i = 0; i < n->block.items.count; i++) {
            vm_collect_exports(n->block.items.items[i], names, count);
        }
        return;
    }
    const char *name = NULL;
    if (n->kind == NODE_LET && n->let.is_export) name = n->let.name;
    else if (n->kind == NODE_FUNC_DEF && n->funcdef.is_export && n->funcdef.name) name = n->funcdef.name;
    else if (n->kind == NODE_DATA_DEF && n->data_def.is_export) name = n->data_def.name;
    else if (n->kind == NODE_BLOC_DEF && n->bloc_def.is_export) name = n->bloc_def.name;
    if (!name) return;

    for (int i = 0; i < *count; i++) {
        if ((*names)[i] == name) return;
    }
    const char **grown = (const char **)realloc((void *)*names, sizeof(const char *) * (size_t)(*count + 1));
    if (!grown) abort();
    grown[*count] = name;
    *names = grown;
    (*count)++;
}

static int vm_name_in_list(const char *name, const char **names, int count) {
    for (int i = 0; i < count; i++) {
        if (names[i] == name) return 1;
    }
    return 0;
}

static int vm_op_line(LunaChunk *chunk, uint8_t *ip) {
    size_t off = (size_t)(ip - chunk->code);
    if (off == 0 || chunk->line_len == 0) return 1;
    return chunk->line_map[off - 1];
}

static void vm_run_import(LunaVM *vm, uint16_t path_idx, const uint16_t *name_idxs,
                          uint8_t name_count, int line) {
    LunaChunk *chunk = vm->frames[vm->frame_count - 1].chunk;
    Value path_val = chunk->constants[path_idx];
    const char *path = path_val.type == VAL_STRING ? path_val.string->chars : NULL;
    if (!path) return;

    char *src = read_file(path);
    if (!src) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Could not import file '%s'", path);
        error_report_with_context(ERR_NAME, line, 0, msg,
            "Check that the import path exists and ends with .lu");
        return;
    }

    error_init(src, path);
    Parser parser;
    parser_init(&parser, src);
    AstNode *prog = parser_parse_program(&parser);
    parser_close(&parser);

    if (!prog) {
        free(src);
        return;
    }

    const char **exported = NULL;
    int exported_count = 0;
    vm_collect_exports(prog, &exported, &exported_count);

    Env *module_env = env_create(env_root(vm->env));
    LunaChunk *module_chunk = luna_compile_program(prog);

    LunaVM module_vm;
    luna_vm_init(&module_vm, vm->heap);
    module_vm.env = module_env;
    luna_vm_run(&module_vm, module_chunk);

    if (!luna_had_error) {
        if (name_count > 0) {
            for (int i = 0; i < name_count; i++) {
                Value name_val = chunk->constants[name_idxs[i]];
                const char *name = intern_string(name_val.string->chars);
                if (!vm_name_in_list(name, exported, exported_count)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Module '%s' does not export '%s'", path, name);
                    error_report_with_context(ERR_NAME, line, 0, msg,
                        "Export names explicitly in the module before using them");
                    break;
                }
                Value *slot = env_get_local(module_env, name);
                if (!slot) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Export '%s' is missing from module '%s'", name, path);
                    error_report_with_context(ERR_NAME, line, 0, msg,
                        "Make sure the exported value is defined at module top level");
                    break;
                }
                env_def(vm->env, name, *slot);
            }
        } else {
            for (int i = 0; i < exported_count; i++) {
                Value *slot = env_get_local(module_env, exported[i]);
                if (slot) env_def(vm->env, exported[i], *slot);
            }
        }
    }

    free((void *)exported);
    if (prog->kind == NODE_BLOCK) nodelist_free(&prog->block.items);
    free(src);
    /* module_chunk intentionally leaked: closures exported from the module
     * still reference its subchunks. */
}

Value luna_vm_run(LunaVM *vm, LunaChunk *chunk) {
    if (vm->frame_count >= FRAMES_MAX) {
        fprintf(stderr, "VM Error: stack overflow\n");
        abort();
    }

    // Set up frame 0
    VMCallFrame *frame = &vm->frames[vm->frame_count++];
    frame->chunk = chunk;
    frame->ip = chunk->code;
    frame->slots = vm->stack;
    frame->upvalues = NULL;
    frame->argc = 0;
    frame->ret_dst = 0;
    frame->scope_base = vm->scope_depth;

    luna_vm_register(vm);

    // Clear registers for main
    for (int i = 0; i < chunk->reg_count; i++) {
        vm->stack[i] = value_null();
    }
    vm->stack_top = vm->stack + chunk->reg_count;

    return luna_vm_execute(vm);
}

Value luna_vm_call_closure(GCHeap *heap, Env *env, VMClosureObj *closure,
                           int argc, Value *argv, int line) {
    (void)line;
    LunaVM vm;
    luna_vm_init(&vm, heap);
    vm.env = env;

    if (argc > 255) argc = 255;
    VMCallFrame *frame = &vm.frames[vm.frame_count++];
    frame->chunk = closure->chunk;
    frame->ip = closure->chunk->code;
    frame->slots = vm.stack;
    frame->upvalues = closure->upvalues;
    frame->argc = (uint8_t)argc;
    frame->ret_dst = 0;
    frame->scope_base = 0;

    for (int i = 0; i < argc; i++) {
        vm.stack[i] = value_copy(argv[i]);
    }
    for (int i = argc; i < closure->chunk->reg_count; i++) {
        vm.stack[i] = value_null();
    }
    vm.stack_top = vm.stack + closure->chunk->reg_count;

    Value ret = luna_vm_execute(&vm);

    for (int i = 0; i < closure->chunk->reg_count; i++) {
        value_free(vm.stack[i]);
    }
    return ret;
}

Value luna_vm_execute(LunaVM *vm) {
    VMCallFrame *frame = &vm->frames[vm->frame_count - 1];
    uint8_t *ip = frame->ip;
    Value *slots = frame->slots;
    LunaChunk *chunk = frame->chunk;

    #ifdef LUNA_VM_DEBUG
    printf("[VM] Running chunk %s, code length = %zu\n", chunk->name, chunk->code_len);
    #endif

    // Dispatch table for computed gotos
    #ifdef __GNUC__
    static void* dispatch_table[] = {
        &&do_halt, &&do_load_int, &&do_load_float, &&do_load_const, &&do_load_true,
        &&do_load_false, &&do_load_null, &&do_move, &&do_add, &&do_sub, &&do_mul,
        &&do_div, &&do_mod, &&do_eq, &&do_neq, &&do_lt, &&do_lte, &&do_gt, &&do_gte,
        &&do_not, &&do_neg, &&do_jump, &&do_jump_if_true, &&do_jump_if_false,
        &&do_get_global, &&do_set_global, &&do_get_upval, &&do_set_upval,
        &&do_new_list, &&do_list_append, &&do_index_get, &&do_index_set,
        &&do_new_map, &&do_map_set, &&do_box_alloc, &&do_addr_of, &&do_addr_of_global,
        &&do_field_get, &&do_field_set, &&do_call, &&do_call_named, &&do_defer,
        &&do_has_arg,
        &&do_return, &&do_closure,
        &&do_scope_begin, &&do_scope_exit, &&do_unsafe_begin, &&do_unsafe_end,
        &&do_import,
        &&do_print, &&do_safepoint
    };
    #ifdef LUNA_VM_DEBUG
    #define DISPATCH() do { \
        printf("[VM] ip = %d, opcode = %d\n", (int)(ip - chunk->code), *ip); \
        luna_current_line = frame->chunk->line_map[ip - frame->chunk->code]; \
        goto *dispatch_table[*ip++]; \
    } while(0)
    #else
    #define DISPATCH() do { \
        luna_current_line = frame->chunk->line_map[ip - frame->chunk->code]; \
        goto *dispatch_table[*ip++]; \
    } while(0)
    #endif
    #else
    #define DISPATCH() goto switch_dispatch
    #endif

    #define READ_BYTE() (*ip++)
    #define READ_SHORT() (ip += 2, (uint16_t)((ip[-2]) | (ip[-1] << 8)))
    #define READ_INT64() (ip += 8, (uint64_t)((ip[-8]) | ((uint64_t)ip[-7] << 8) | ((uint64_t)ip[-6] << 16) | ((uint64_t)ip[-5] << 24) | ((uint64_t)ip[-4] << 32) | ((uint64_t)ip[-3] << 40) | ((uint64_t)ip[-2] << 48) | ((uint64_t)ip[-1] << 56)))

    #ifdef __GNUC__
    DISPATCH();
    #else
    switch_dispatch:
    while (1) {
        switch (*ip++) {
    #endif

    #ifdef __GNUC__
    do_halt:
    #else
    case VM_OP_HALT:
    #endif
        close_upvalues(vm, vm->stack);
        vm_run_defers(vm);
        while (vm->scope_depth > 0) {
            uint64_t id = vm->scope_stack[--vm->scope_depth];
            value_box_release_scope(id);
        }
        luna_vm_unregister(vm);
        return value_null();
    #ifdef __GNUC__
    do_load_int:
    #else
    case VM_OP_LOAD_INT:
    #endif
    {
        uint8_t dst = READ_BYTE();
        long long val = (long long)READ_INT64();
        value_free(slots[dst]);
        slots[dst] = value_int(val);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_load_float:
    #else
    case VM_OP_LOAD_FLOAT:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint64_t bits = READ_INT64();
        double val;
        memcpy(&val, &bits, sizeof(double));
        value_free(slots[dst]);
        slots[dst] = value_float(val);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_load_const:
    #else
    case VM_OP_LOAD_CONST:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint16_t idx = READ_SHORT();
        value_free(slots[dst]);
        slots[dst] = value_copy(chunk->constants[idx]);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_load_true:
    #else
    case VM_OP_LOAD_TRUE:
    #endif
    {
        uint8_t dst = READ_BYTE();
        value_free(slots[dst]);
        slots[dst] = value_bool(1);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_load_false:
    #else
    case VM_OP_LOAD_FALSE:
    #endif
    {
        uint8_t dst = READ_BYTE();
        value_free(slots[dst]);
        slots[dst] = value_bool(0);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_load_null:
    #else
    case VM_OP_LOAD_NULL:
    #endif
    {
        uint8_t dst = READ_BYTE();
        value_free(slots[dst]);
        slots[dst] = value_null();
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_move:
    #else
    case VM_OP_MOVE:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t src = READ_BYTE();
        value_free(slots[dst]);
        slots[dst] = value_copy(slots[src]);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_add:
    #else
    case VM_OP_ADD:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t lhs = READ_BYTE();
        uint8_t rhs = READ_BYTE();
        Value l = slots[lhs];
        Value r = slots[rhs];
        Value res;
        if ((l.type == VAL_LIST || l.type == VAL_DENSE_LIST) &&
            (r.type == VAL_LIST || r.type == VAL_DENSE_LIST)) {
            res = vec_add_values(l, r);
        } else if (l.type == VAL_INT && r.type == VAL_INT) {
            res = value_int(l.i + r.i);
        } else if (l.type == VAL_STRING || r.type == VAL_STRING) {
            char *sl = value_to_string(l);
            char *sr = value_to_string(r);
            res = value_string_concat_raw(sl, strlen(sl), sr, strlen(sr));
            free(sl);
            free(sr);
        } else {
            res = value_float(value_to_double(l) + value_to_double(r));
        }
        value_free(slots[dst]);
        slots[dst] = res;
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_sub:
    #else
    case VM_OP_SUB:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t lhs = READ_BYTE();
        uint8_t rhs = READ_BYTE();
        Value l = slots[lhs];
        Value r = slots[rhs];
        Value res;
        if ((l.type == VAL_LIST || l.type == VAL_DENSE_LIST) &&
            (r.type == VAL_LIST || r.type == VAL_DENSE_LIST)) {
            res = vec_sub_values(l, r);
        } else if (l.type == VAL_INT && r.type == VAL_INT) {
            res = value_int(l.i - r.i);
        } else {
            res = value_float(value_to_double(l) - value_to_double(r));
        }
        value_free(slots[dst]);
        slots[dst] = res;
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_mul:
    #else
    case VM_OP_MUL:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t lhs = READ_BYTE();
        uint8_t rhs = READ_BYTE();
        Value l = slots[lhs];
        Value r = slots[rhs];
        Value res;
        if ((l.type == VAL_LIST || l.type == VAL_DENSE_LIST) &&
            (r.type == VAL_LIST || r.type == VAL_DENSE_LIST)) {
            res = vec_mul_values(l, r);
        } else if (l.type == VAL_INT && r.type == VAL_INT) {
            res = value_int(l.i * r.i);
        } else {
            res = value_float(value_to_double(l) * value_to_double(r));
        }
        value_free(slots[dst]);
        slots[dst] = res;
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_div:
    #else
    case VM_OP_DIV:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t lhs = READ_BYTE();
        uint8_t rhs = READ_BYTE();
        Value l = slots[lhs];
        Value r = slots[rhs];
        Value res;
        if ((l.type == VAL_LIST || l.type == VAL_DENSE_LIST) &&
            (r.type == VAL_LIST || r.type == VAL_DENSE_LIST)) {
            res = vec_div_values(l, r);
        } else if (l.type == VAL_INT && r.type == VAL_INT) {
            if (r.i == 0) res = value_int(0);
            else if (l.i % r.i == 0) res = value_int(l.i / r.i);
            else res = value_float((double)l.i / (double)r.i);
        } else {
            double dr = value_to_double(r);
            res = value_float(dr == 0.0 ? 0.0 : value_to_double(l) / dr);
        }
        value_free(slots[dst]);
        slots[dst] = res;
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_mod:
    #else
    case VM_OP_MOD:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t lhs = READ_BYTE();
        uint8_t rhs = READ_BYTE();
        Value l = slots[lhs];
        Value r = slots[rhs];
        Value res;
        if (l.type == VAL_INT && r.type == VAL_INT) {
            res = value_int(r.i == 0 ? 0 : l.i % r.i);
        } else {
            res = value_int(r.f == 0.0 ? 0 : (long long)value_to_double(l) % (long long)value_to_double(r));
        }
        value_free(slots[dst]);
        slots[dst] = res;
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_eq:
    #else
    case VM_OP_EQ:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t lhs = READ_BYTE();
        uint8_t rhs = READ_BYTE();
        Value l = slots[lhs];
        Value r = slots[rhs];
        bool eq = false;
        if (l.type == r.type) {
            if (l.type == VAL_INT) eq = (l.i == r.i);
            else if (l.type == VAL_FLOAT) eq = (l.f == r.f);
            else if (l.type == VAL_BOOL) eq = (l.b == r.b);
            else if (l.type == VAL_CHAR) eq = (l.c == r.c);
            else if (l.type == VAL_POINTER) eq = (l.ptr == r.ptr);
            else if (l.type == VAL_BLOC) eq = value_bloc_equal(l, r);
            else if (l.type == VAL_STRING) eq = (strcmp(l.string->chars, r.string->chars) == 0);
            else if (l.type == VAL_NULL) eq = true;
        } else if ((l.type == VAL_INT && r.type == VAL_FLOAT) ||
                   (l.type == VAL_FLOAT && r.type == VAL_INT)) {
            eq = (value_to_double(l) == value_to_double(r));
        }
        value_free(slots[dst]);
        slots[dst] = value_bool(eq ? 1 : 0);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_neq:
    #else
    case VM_OP_NEQ:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t lhs = READ_BYTE();
        uint8_t rhs = READ_BYTE();
        Value l = slots[lhs];
        Value r = slots[rhs];
        bool eq = false;
        if (l.type == r.type) {
            if (l.type == VAL_INT) eq = (l.i == r.i);
            else if (l.type == VAL_FLOAT) eq = (l.f == r.f);
            else if (l.type == VAL_BOOL) eq = (l.b == r.b);
            else if (l.type == VAL_CHAR) eq = (l.c == r.c);
            else if (l.type == VAL_POINTER) eq = (l.ptr == r.ptr);
            else if (l.type == VAL_BLOC) eq = value_bloc_equal(l, r);
            else if (l.type == VAL_STRING) eq = (strcmp(l.string->chars, r.string->chars) == 0);
            else if (l.type == VAL_NULL) eq = true;
        } else if ((l.type == VAL_INT && r.type == VAL_FLOAT) ||
                   (l.type == VAL_FLOAT && r.type == VAL_INT)) {
            eq = (value_to_double(l) == value_to_double(r));
        }
        value_free(slots[dst]);
        slots[dst] = value_bool(eq ? 0 : 1);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_lt:
    #else
    case VM_OP_LT:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t lhs = READ_BYTE();
        uint8_t rhs = READ_BYTE();
        Value l = slots[lhs];
        Value r = slots[rhs];
        bool res;
        if (l.type == VAL_POINTER && r.type == VAL_POINTER) res = (l.ptr < r.ptr);
        else if (l.type == VAL_INT && r.type == VAL_INT) res = (l.i < r.i);
        else res = (value_to_double(l) < value_to_double(r));
        value_free(slots[dst]);
        slots[dst] = value_bool(res ? 1 : 0);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_lte:
    #else
    case VM_OP_LTE:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t lhs = READ_BYTE();
        uint8_t rhs = READ_BYTE();
        Value l = slots[lhs];
        Value r = slots[rhs];
        bool res;
        if (l.type == VAL_POINTER && r.type == VAL_POINTER) res = (l.ptr <= r.ptr);
        else if (l.type == VAL_INT && r.type == VAL_INT) res = (l.i <= r.i);
        else res = (value_to_double(l) <= value_to_double(r));
        value_free(slots[dst]);
        slots[dst] = value_bool(res ? 1 : 0);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_gt:
    #else
    case VM_OP_GT:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t lhs = READ_BYTE();
        uint8_t rhs = READ_BYTE();
        Value l = slots[lhs];
        Value r = slots[rhs];
        bool res;
        if (l.type == VAL_POINTER && r.type == VAL_POINTER) res = (l.ptr > r.ptr);
        else if (l.type == VAL_INT && r.type == VAL_INT) res = (l.i > r.i);
        else res = (value_to_double(l) > value_to_double(r));
        value_free(slots[dst]);
        slots[dst] = value_bool(res ? 1 : 0);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_gte:
    #else
    case VM_OP_GTE:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t lhs = READ_BYTE();
        uint8_t rhs = READ_BYTE();
        Value l = slots[lhs];
        Value r = slots[rhs];
        bool res;
        if (l.type == VAL_POINTER && r.type == VAL_POINTER) res = (l.ptr >= r.ptr);
        else if (l.type == VAL_INT && r.type == VAL_INT) res = (l.i >= r.i);
        else res = (value_to_double(l) >= value_to_double(r));
        value_free(slots[dst]);
        slots[dst] = value_bool(res ? 1 : 0);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_not:
    #else
    case VM_OP_NOT:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t src = READ_BYTE();
        Value v = slots[src];
        bool falsy = !vm_is_truthy(v);
        value_free(slots[dst]);
        slots[dst] = value_bool(falsy ? 1 : 0);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_neg:
    #else
    case VM_OP_NEG:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t src = READ_BYTE();
        Value v = slots[src];
        Value res;
        if (v.type == VAL_INT) res = value_int(-v.i);
        else res = value_float(-value_to_double(v));
        value_free(slots[dst]);
        slots[dst] = res;
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_jump:
    #else
    case VM_OP_JUMP:
    #endif
    {
        int16_t offset = (int16_t)READ_SHORT();
        ip += offset;
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_jump_if_true:
    #else
    case VM_OP_JUMP_IF_TRUE:
    #endif
    {
        uint8_t cond_reg = READ_BYTE();
        int16_t offset = (int16_t)READ_SHORT();
        Value v = slots[cond_reg];
        bool truthy = vm_is_truthy(v);
        if (truthy) ip += offset;
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_jump_if_false:
    #else
    case VM_OP_JUMP_IF_FALSE:
    #endif
    {
        uint8_t cond_reg = READ_BYTE();
        int16_t offset = (int16_t)READ_SHORT();
        Value v = slots[cond_reg];
        bool truthy = vm_is_truthy(v);
        if (!truthy) ip += offset;
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_get_global:
    #else
    case VM_OP_GET_GLOBAL:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint16_t name_idx = READ_SHORT();
        Value name_val = chunk->constants[name_idx];
        const char *interned = intern_string(name_val.string->chars);
        #ifdef LUNA_VM_DEBUG
        printf("[GET_GLOBAL] Searching for %s (interned ptr: %p, constant chars ptr: %p)\n",
               name_val.string->chars, (void*)interned, (void*)name_val.string->chars);
        #endif
        Value *gval = env_get(vm->env, interned);
        if (!gval) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Variable '%s' is not defined", name_val.string->chars);
            error_report_with_context(ERR_NAME, vm_op_line(chunk, ip), 0, msg,
                "Declare variables with 'let' before using them");
            value_free(slots[dst]);
            slots[dst] = value_null();
            #ifdef __GNUC__
            DISPATCH();
            #else
            break;
            #endif
        }
        value_free(slots[dst]);
        slots[dst] = value_copy(*gval);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_set_global:
    #else
    case VM_OP_SET_GLOBAL:
    #endif
    {
        uint16_t name_idx = READ_SHORT();
        uint8_t src = READ_BYTE();
        int line = vm_op_line(chunk, ip);
        Value name_val = chunk->constants[name_idx];
        const char *interned = intern_string(name_val.string->chars);
        if (unsafe_runtime_inside_block() && unsafe_runtime_is_pointer(slots[src]) &&
            !unsafe_runtime_check_escape(slots[src], line)) {
            #ifdef __GNUC__
            DISPATCH();
            #else
            break;
            #endif
        }
        env_def(vm->env, interned, value_copy(slots[src]));
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_get_upval:
    #else
    case VM_OP_GET_UPVAL:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t idx = READ_BYTE();
        value_free(slots[dst]);
        slots[dst] = value_copy(*frame->upvalues[idx]->location);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_set_upval:
    #else
    case VM_OP_SET_UPVAL:
    #endif
    {
        uint8_t idx = READ_BYTE();
        uint8_t src = READ_BYTE();
        Value *loc = frame->upvalues[idx]->location;
        value_free(*loc);
        *loc = value_copy(slots[src]);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_new_list:
    #else
    case VM_OP_NEW_LIST:
    #endif
    {
        uint8_t dst = READ_BYTE();
        value_free(slots[dst]);
        slots[dst] = value_list();
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_list_append:
    #else
    case VM_OP_LIST_APPEND:
    #endif
    {
        uint8_t list_reg = READ_BYTE();
        uint8_t val_reg = READ_BYTE();
        Value *list_val = &slots[list_reg];
        Value val = slots[val_reg];
        if (list_val->type == VAL_LIST &&
            vm_ptr_store_ok(val, vm_op_line(chunk, ip))) {
            value_list_append(list_val, value_copy(val));
        }
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_index_get:
    #else
    case VM_OP_INDEX_GET:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t target_reg = READ_BYTE();
        uint8_t idx_reg = READ_BYTE();
        Value target = slots[target_reg];
        Value index = slots[idx_reg];
        Value ret = value_null();
        if (target.type == VAL_LIST && index.type == VAL_INT) {
            long long idx = index.i;
            if (idx < 0) idx += target.list->count;
            if (idx >= 0 && idx < target.list->count) {
                ret = value_copy(target.list->items[idx]);
            }
        } else if (target.type == VAL_DENSE_LIST && index.type == VAL_INT) {
            long long idx = index.i;
            if (idx < 0) idx += target.dlist->count;
            if (idx >= 0 && idx < target.dlist->count) {
                ret = value_float(target.dlist->data[idx]);
            }
        } else if (target.type == VAL_STRING && index.type == VAL_INT) {
            long long idx = index.i;
            long long len = (long long)strlen(target.string->chars);
            if (idx < 0) idx += len;
            if (idx >= 0 && idx < len) {
                ret = value_char(target.string->chars[idx]);
            }
        } else if (target.type == VAL_TEMPLATE && index.type == VAL_STRING) {
            int found = 0;
            ret = value_template_get_field(target, intern_string(index.string->chars), &found);
        } else if (target.type == VAL_MAP && index.type == VAL_STRING) {
            Value *got = value_map_get(&target, intern_string(index.string->chars));
            if (got) ret = value_copy(*got);
        } else if (target.type == VAL_BLOC && index.type == VAL_STRING) {
            int found = 0;
            ret = value_bloc_get_field(target, intern_string(index.string->chars), &found);
        }
        value_free(slots[dst]);
        slots[dst] = ret;
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_index_set:
    #else
    case VM_OP_INDEX_SET:
    #endif
    {
        uint8_t target_reg = READ_BYTE();
        uint8_t idx_reg = READ_BYTE();
        uint8_t val_reg = READ_BYTE();
        int line = vm_op_line(chunk, ip);
        Value target = slots[target_reg];
        Value index = slots[idx_reg];
        Value val = slots[val_reg];
        if (target.type == VAL_LIST && index.type == VAL_INT) {
            long long idx = index.i;
            if (idx < 0) idx += target.list->count;
            if (idx >= 0 && idx < target.list->count &&
                vm_ptr_store_ok(val, line)) {
                value_free(target.list->items[idx]);
                target.list->items[idx] = value_copy(val);
            }
        } else if (target.type == VAL_DENSE_LIST && index.type == VAL_INT) {
            long long idx = index.i;
            if (idx < 0) idx += target.dlist->count;
            if (idx >= 0 && idx < target.dlist->count) {
                target.dlist->data[idx] = value_to_double(val);
            }
        } else if (target.type == VAL_MAP) {
            if (index.type == VAL_STRING && vm_ptr_store_ok(val, line)) {
                value_map_set(&target, intern_string(index.string->chars), value_copy(val));
            }
        } else if (target.type == VAL_TEMPLATE) {
            if (index.type == VAL_STRING && vm_ptr_store_ok(val, line)) {
                char msg[256];
                Value val_copy = value_copy(val);
                value_template_set_field(&target, intern_string(index.string->chars), &val_copy, msg, sizeof(msg));
            }
        }
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_new_map:
    #else
    case VM_OP_NEW_MAP:
    #endif
    {
        uint8_t dst = READ_BYTE();
        value_free(slots[dst]);
        slots[dst] = value_map();
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_map_set:
    #else
    case VM_OP_MAP_SET:
    #endif
    {
        uint8_t map_reg = READ_BYTE();
        uint16_t key_idx = READ_SHORT();
        uint8_t val_reg = READ_BYTE();
        int line = vm_op_line(chunk, ip);
        Value *map_val = &slots[map_reg];
        Value val = slots[val_reg];
        if (map_val->type == VAL_MAP && vm_ptr_store_ok(val, line)) {
            Value key_val = chunk->constants[key_idx];
            value_map_set(map_val, intern_string(key_val.string->chars), value_copy(val));
        }
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_box_alloc:
    #else
    case VM_OP_BOX_ALLOC:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t size_reg = READ_BYTE();
        int line = vm_op_line(chunk, ip);
        Value size = slots[size_reg];
        Value res = value_null();
        if (size.type != VAL_INT) {
            error_report_with_context(ERR_TYPE, line, 0,
                "box[...] expects an integer byte size",
                "Use box[128] or another positive integer size");
        } else {
            char msg[256];
            res = value_box((size_t)size.i, msg, sizeof(msg));
            if (res.type == VAL_NULL) {
                error_report_with_context(ERR_ARGUMENT, line, 0, msg,
                    "Boxes are stack-scoped scratch buffers up to one cache line");
            } else {
                uint64_t scope_id = vm->scope_depth > 0 ?
                    vm->scope_stack[vm->scope_depth - 1] : 0;
                value_box_mark_scope(res, scope_id);
            }
        }
        value_free(slots[dst]);
        slots[dst] = res;
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_addr_of:
    #else
    case VM_OP_ADDR_OF:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t src = READ_BYTE();
        int line = vm_op_line(chunk, ip);
        value_free(slots[dst]);
        slots[dst] = unsafe_runtime_addr(&slots[src], line);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_addr_of_global:
    #else
    case VM_OP_ADDR_OF_GLOBAL:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint16_t name_idx = READ_SHORT();
        int line = vm_op_line(chunk, ip);
        Value name_val = chunk->constants[name_idx];
        const char *interned = intern_string(name_val.string->chars);
        Value *slot = env_get(vm->env, interned);
        if (!slot) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Variable '%s' is not defined", name_val.string->chars);
            error_report_with_context(ERR_NAME, line, 0, msg,
                "Declare variables with 'let' before using them");
            value_free(slots[dst]);
            slots[dst] = value_null();
        } else {
            value_free(slots[dst]);
            slots[dst] = unsafe_runtime_addr(slot, line);
        }
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_field_get:
    #else
    case VM_OP_FIELD_GET:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t target_reg = READ_BYTE();
        uint16_t name_idx = READ_SHORT();
        int line = vm_op_line(chunk, ip);
        Value target = slots[target_reg];
        Value name_val = chunk->constants[name_idx];
        Value ret = value_null();
        if (target.type == VAL_MAP) {
            const char *fname = intern_string(name_val.string->chars);
            Value *got = value_map_get(&target, fname);
            if (got) ret = value_copy(*got);
        } else if (target.type == VAL_TEMPLATE) {
            int found = 0;
            ret = value_template_get_field(target, intern_string(name_val.string->chars), &found);
        } else if (target.type == VAL_BLOC) {
            int found = 0;
            ret = value_bloc_get_field(target, intern_string(name_val.string->chars), &found);
        } else if (target.type == VAL_BOX) {
            const char *f = intern_string(name_val.string->chars);
            if (f == intern_string("len")) {
                ret = value_int((long long)value_box_len(target));
            } else if (f == intern_string("cap")) {
                ret = value_int((long long)value_box_cap(target));
            } else {
                char msg[256];
                snprintf(msg, sizeof(msg), "Field '%s' does not exist on this box value", f);
                error_report_with_context(ERR_NAME, line, 0, msg,
                    "Use box.len or box.cap for phase-1 box values");
            }
        }
        value_free(slots[dst]);
        slots[dst] = ret;
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_field_set:
    #else
    case VM_OP_FIELD_SET:
    #endif
    {
        uint8_t target_reg = READ_BYTE();
        uint16_t name_idx = READ_SHORT();
        uint8_t val_reg = READ_BYTE();
        Value target = slots[target_reg];
        Value name_val = chunk->constants[name_idx];
        Value val = slots[val_reg];
        if (target.type == VAL_MAP) {
            value_map_set(&target, name_val.string->chars, value_copy(val));
        } else if (target.type == VAL_TEMPLATE) {
            char msg[256];
            Value val_copy = value_copy(val);
            value_template_set_field(&target, intern_string(name_val.string->chars), &val_copy, msg, sizeof(msg));
        }
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_call:
    #else
    case VM_OP_CALL:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t callee_reg = READ_BYTE();
        uint8_t argc = READ_BYTE();
        Value callee = slots[callee_reg];
        
        if (callee.type == VAL_VM_CLOSURE) {
            VMClosureObj *closure = callee.vm_closure;
            LunaChunk *sub = closure->chunk;
            if (vm->frame_count >= FRAMES_MAX) {
                fprintf(stderr, "VM Error: stack overflow\n");
                abort();
            }

            // Save IP back to frame
            frame->ip = ip;

            // Push Call Frame
            VMCallFrame *new_frame = &vm->frames[vm->frame_count++];
            new_frame->chunk = sub;
            new_frame->ip = sub->code;
            new_frame->slots = slots + callee_reg + 1;
            new_frame->upvalues = closure->upvalues;
            new_frame->argc = argc;
            new_frame->ret_dst = dst;
            new_frame->scope_base = vm->scope_depth;

            // Clear registers for new frame (starting from parameters)
            for (int i = sub->param_count; i < sub->reg_count; i++) {
                new_frame->slots[i] = value_null();
            }
            
            // Adjust vm->stack_top
            vm->stack_top = new_frame->slots + sub->reg_count;

            frame = new_frame;
            chunk = frame->chunk;
            ip = frame->ip;
            slots = frame->slots;
        } else if (callee.type == VAL_DATA_TYPE) {
            Value *args = slots + callee_reg + 1;
            int line = frame->chunk->line_map[ip - frame->chunk->code - 4];
            Value ret = instantiate_data_type(callee, argc, args, line);
            value_free(slots[dst]);
            slots[dst] = ret;
        } else if (callee.type == VAL_BLOC_TYPE) {
            Value *args = slots + callee_reg + 1;
            int line = frame->chunk->line_map[ip - frame->chunk->code - 4];
            char msg[256];
            Value ret = value_null();
            if (!value_bloc_check_construct(callee, argc, args, msg, sizeof(msg))) {
                error_report_with_context(ERR_TYPE, line, 0, msg,
                    "Bloc values must stay inline and only use int, float, bool, char, or nested blocs");
            } else {
                ret = value_bloc_construct(callee, argc, args);
            }
            value_free(slots[dst]);
            slots[dst] = ret;
        } else {
            Value *args = slots + callee_reg + 1;
            int line = frame->chunk->line_map[ip - frame->chunk->code - 4];
            #ifdef LUNA_VM_DEBUG
            char *s = value_to_string(callee);
            printf("[VM_OP_CALL] Fallback call. Callee type: %d, value: %s, line: %d\n", callee.type, s, line);
            free(s);
            #endif
            Value ret = luna_call_value(vm->env, callee, argc, args, line);
            value_free(slots[dst]);
            slots[dst] = ret;
        }
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_call_named:
    #else
    case VM_OP_CALL_NAMED:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t callee_reg = READ_BYTE();
        uint8_t argc = READ_BYTE();
        uint16_t name_idx = READ_SHORT();
        int line = vm_op_line(chunk, ip);
        Value callee = slots[callee_reg];

        int callable = (callee.type == VAL_VM_CLOSURE || callee.type == VAL_DATA_TYPE ||
                        callee.type == VAL_BLOC_TYPE || callee.type == VAL_NATIVE ||
                        callee.type == VAL_CLOSURE || callee.type == VAL_FUNCTION);
        if (!callable) {
            Value name_val = chunk->constants[name_idx];
            const char *nm = name_val.type == VAL_STRING ? name_val.string->chars : "value";
            char emsg[128];
            snprintf(emsg, sizeof(emsg), "'%s' is not a function", nm);
            error_report_with_context(ERR_TYPE, line, 0, emsg,
                "Make sure you are calling a function value, closure, or native builtin");
            value_free(slots[dst]);
            slots[dst] = value_null();
            #ifdef __GNUC__
            DISPATCH();
            #else
            break;
            #endif
        }

        if (callee.type == VAL_VM_CLOSURE) {
            VMClosureObj *closure = callee.vm_closure;
            LunaChunk *sub = closure->chunk;
            if (vm->frame_count >= FRAMES_MAX) {
                fprintf(stderr, "VM Error: stack overflow\n");
                abort();
            }

            frame->ip = ip;

            VMCallFrame *new_frame = &vm->frames[vm->frame_count++];
            new_frame->chunk = sub;
            new_frame->ip = sub->code;
            new_frame->slots = slots + callee_reg + 1;
            new_frame->upvalues = closure->upvalues;
            new_frame->argc = argc;
            new_frame->ret_dst = dst;
            new_frame->scope_base = vm->scope_depth;

            for (int i = sub->param_count; i < sub->reg_count; i++) {
                new_frame->slots[i] = value_null();
            }

            vm->stack_top = new_frame->slots + sub->reg_count;

            frame = new_frame;
            chunk = frame->chunk;
            ip = frame->ip;
            slots = frame->slots;
        } else if (callee.type == VAL_DATA_TYPE) {
            Value *args = slots + callee_reg + 1;
            Value ret = instantiate_data_type(callee, argc, args, line);
            value_free(slots[dst]);
            slots[dst] = ret;
        } else if (callee.type == VAL_BLOC_TYPE) {
            Value *args = slots + callee_reg + 1;
            char msg[256];
            Value ret = value_null();
            if (!value_bloc_check_construct(callee, argc, args, msg, sizeof(msg))) {
                error_report_with_context(ERR_TYPE, line, 0, msg,
                    "Bloc values must stay inline and only use int, float, bool, char, or nested blocs");
            } else {
                ret = value_bloc_construct(callee, argc, args);
            }
            value_free(slots[dst]);
            slots[dst] = ret;
        } else {
            Value *args = slots + callee_reg + 1;
            #ifdef LUNA_VM_DEBUG
            char *s = value_to_string(callee);
            printf("[VM_OP_CALL_NAMED] Callee type: %d, value: %s, line: %d\n", callee.type, s, line);
            free(s);
            #endif
            Value ret = luna_call_value(vm->env, callee, argc, args, line);
            value_free(slots[dst]);
            slots[dst] = ret;
        }
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_defer:
    #else
    case VM_OP_DEFER:
    #endif
    {
        uint8_t callee_reg = READ_BYTE();
        uint8_t argc = READ_BYTE();
        int line = vm_op_line(chunk, ip);
        Value callee = slots[callee_reg];
        vm_defer_push(vm, callee, argc, slots + callee_reg + 1, line);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_has_arg:
    #else
    case VM_OP_HAS_ARG:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t idx = READ_BYTE();
        value_free(slots[dst]);
        slots[dst] = value_bool(frame->argc > idx);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_return:
    #else
    case VM_OP_RETURN:
    #endif
    {
        uint8_t val_reg = READ_BYTE();
        Value ret_val = value_copy(slots[val_reg]);

        // Close upvalues for local stack slots leaving scope
        close_upvalues(vm, slots);

        // Release box scopes opened inside this frame (its block scope
        // exits may have been skipped by an early return).
        while (vm->scope_depth > frame->scope_base) {
            int base = vm->defer_base_stack[vm->scope_depth - 1];
            uint64_t id = vm->scope_stack[--vm->scope_depth];
            vm_run_defers_since(vm, base);
            value_box_release_scope(id);
        }

        // Free registers in returning frame
        for (int i = 0; i < chunk->reg_count; i++) {
            value_free(slots[i]);
        }
        uint8_t dst_reg = frame->ret_dst;

        vm->frame_count--;
        if (vm->frame_count == 0) {
            vm->stack_top = vm->stack; // reset
            luna_vm_unregister(vm);
            return ret_val;
        }

        // Return to caller frame
        frame = &vm->frames[vm->frame_count - 1];
        slots = frame->slots;
        chunk = frame->chunk;
        ip = frame->ip;

        // Store the return value into the caller's destination register
        value_free(slots[dst_reg]);
        slots[dst_reg] = ret_val;

        // Reset stack top
        vm->stack_top = slots + chunk->reg_count;
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_closure:
    #else
    case VM_OP_CLOSURE:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint16_t sub_idx = READ_SHORT();
        LunaChunk *sub = chunk->subchunks[sub_idx];

        Value closure = value_vm_closure(sub, sub->upvalue_count);
        VMClosureObj *cl = closure.vm_closure;

        // Capture upvalues
        for (int i = 0; i < sub->upvalue_count; i++) {
            uint8_t is_local = READ_BYTE();
            uint8_t index = READ_BYTE();
            if (is_local) {
                cl->upvalues[i] = capture_upvalue(vm, &slots[index]);
            } else {
                cl->upvalues[i] = frame->upvalues[index];
            }
        }

        value_free(slots[dst]);
        slots[dst] = closure;
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_scope_begin:
    #else
    case VM_OP_SCOPE_BEGIN:
    #endif
    {
        vm->next_scope_id++;
        if (vm->scope_depth < VM_SCOPE_MAX) {
            vm->scope_stack[vm->scope_depth] = vm->next_scope_id;
            vm->defer_base_stack[vm->scope_depth] = vm->deferred_count;
            vm->scope_depth++;
        }
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_scope_exit:
    #else
    case VM_OP_SCOPE_EXIT:
    #endif
    {
        if (vm->scope_depth > 0) {
            int base = vm->defer_base_stack[vm->scope_depth - 1];
            uint64_t id = vm->scope_stack[--vm->scope_depth];
            vm_run_defers_since(vm, base);
            value_box_release_scope(id);
        }
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_unsafe_begin:
    #else
    case VM_OP_UNSAFE_BEGIN:
    #endif
    {
        unsafe_runtime_begin_block(vm_op_line(chunk, ip));
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_unsafe_end:
    #else
    case VM_OP_UNSAFE_END:
    #endif
    {
        unsafe_runtime_end_block();
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_import:
    #else
    case VM_OP_IMPORT:
    #endif
    {
        uint16_t path_idx = READ_SHORT();
        uint8_t name_count = READ_BYTE();
        uint16_t name_idxs[256];
        for (int i = 0; i < name_count && i < 256; i++) {
            name_idxs[i] = READ_SHORT();
        }
        READ_BYTE(); /* is_module_use flag (names are bound the same way) */
        int line = vm_op_line(chunk, ip);
        vm_run_import(vm, path_idx, name_idxs, name_count, line);
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_print:
    #else
    case VM_OP_PRINT:
    #endif
    {
        uint8_t src = READ_BYTE();
        value_fprint(stdout, slots[src]);
        printf("\n");
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifdef __GNUC__
    do_safepoint:
    #else
    case VM_OP_SAFEPOINT:
    #endif
    {
        static _Thread_local int counter = 0;
        if (++counter >= 1024) {
            counter = 0;
            // Expose stack pointer to GC runtime
            frame->ip = ip;
            luna_gc_runtime_safe_point();
        }
        #ifdef __GNUC__
        DISPATCH();
        #else
        break;
        #endif
    }

    #ifndef __GNUC__
        }
    }
    #endif

    return value_null();
}

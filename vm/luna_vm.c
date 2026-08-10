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

void luna_vm_init(LunaVM *vm, GCHeap *heap) {
    vm->frame_count = 0;
    vm->stack_top = vm->stack;
    vm->open_upvalues = NULL;
    vm->heap = heap;
    vm->env = NULL;
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

    // Clear registers for main
    for (int i = 0; i < chunk->reg_count; i++) {
        vm->stack[i] = value_null();
    }
    vm->stack_top = vm->stack + chunk->reg_count;

    uint8_t *ip = frame->ip;
    Value *slots = frame->slots;

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
        &&do_field_get, &&do_field_set, &&do_call, &&do_return, &&do_closure,
        &&do_print, &&do_safepoint
    };
    #ifdef LUNA_VM_DEBUG
    #define DISPATCH() do { \
        printf("[VM] ip = %d, opcode = %d\n", (int)(ip - chunk->code), *ip); \
        goto *dispatch_table[*ip++]; \
    } while(0)
    #else
    #define DISPATCH() goto *dispatch_table[*ip++]
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
        if (l.type == VAL_INT && r.type == VAL_INT) {
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
        if (l.type == VAL_INT && r.type == VAL_INT) {
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
        if (l.type == VAL_INT && r.type == VAL_INT) {
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
        if (l.type == VAL_INT && r.type == VAL_INT) {
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
            else if (l.type == VAL_STRING) eq = (strcmp(l.string->chars, r.string->chars) == 0);
            else if (l.type == VAL_NULL) eq = true;
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
            else if (l.type == VAL_STRING) eq = (strcmp(l.string->chars, r.string->chars) == 0);
            else if (l.type == VAL_NULL) eq = true;
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
        if (l.type == VAL_INT && r.type == VAL_INT) res = (l.i < r.i);
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
        if (l.type == VAL_INT && r.type == VAL_INT) res = (l.i <= r.i);
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
        if (l.type == VAL_INT && r.type == VAL_INT) res = (l.i > r.i);
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
        if (l.type == VAL_INT && r.type == VAL_INT) res = (l.i >= r.i);
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
        bool falsy = (v.type == VAL_NULL || (v.type == VAL_BOOL && !v.b));
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
        bool truthy = !(v.type == VAL_NULL || (v.type == VAL_BOOL && !v.b));
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
        bool truthy = !(v.type == VAL_NULL || (v.type == VAL_BOOL && !v.b));
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
        value_free(slots[dst]);
        if (gval) {
            slots[dst] = value_copy(*gval);
        } else {
            slots[dst] = value_null();
        }
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
        Value name_val = chunk->constants[name_idx];
        const char *interned = intern_string(name_val.string->chars);
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
        if (list_val->type == VAL_LIST) {
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
        Value target = slots[target_reg];
        Value index = slots[idx_reg];
        Value val = slots[val_reg];
        if (target.type == VAL_LIST && index.type == VAL_INT) {
            long long idx = index.i;
            if (idx < 0) idx += target.list->count;
            if (idx >= 0 && idx < target.list->count) {
                value_free(target.list->items[idx]);
                target.list->items[idx] = value_copy(val);
            }
        } else if (target.type == VAL_MAP) {
            if (index.type == VAL_STRING) {
                value_map_set(&target, index.string->chars, value_copy(val));
            }
        } else if (target.type == VAL_TEMPLATE) {
            if (index.type == VAL_STRING) {
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
    do_field_get:
    #else
    case VM_OP_FIELD_GET:
    #endif
    {
        uint8_t dst = READ_BYTE();
        uint8_t target_reg = READ_BYTE();
        uint16_t name_idx = READ_SHORT();
        Value target = slots[target_reg];
        Value name_val = chunk->constants[name_idx];
        Value ret = value_null();
        if (target.type == VAL_MAP) {
            Value *got = value_map_get(&target, name_val.string->chars);
            if (got) ret = value_copy(*got);
        } else if (target.type == VAL_TEMPLATE) {
            int found = 0;
            ret = value_template_get_field(target, name_val.string->chars, &found);
        } else if (target.type == VAL_BLOC) {
            int found = 0;
            ret = value_bloc_get_field(target, name_val.string->chars, &found);
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
    do_return:
    #else
    case VM_OP_RETURN:
    #endif
    {
        uint8_t val_reg = READ_BYTE();
        Value ret_val = value_copy(slots[val_reg]);

        // Close upvalues for local stack slots leaving scope
        close_upvalues(vm, slots);

        // Free registers in returning frame
        for (int i = 0; i < chunk->reg_count; i++) {
            value_free(slots[i]);
        }

        vm->frame_count--;
        if (vm->frame_count == 0) {
            vm->stack_top = vm->stack; // reset
            return ret_val;
        }

        // Return to caller frame
        frame = &vm->frames[vm->frame_count - 1];
        slots = frame->slots;
        chunk = frame->chunk;
        ip = frame->ip;

        // Retrieve dst register from VM_OP_CALL instruction which is 4 bytes back
        uint8_t dst_reg = ip[-3];
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

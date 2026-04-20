// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <math.h>  // Added for fabs()
#include "interpreter.h"
#include "ast.h"
#include "value.h"
#include "mystr.h"
#include "env.h"     
#include "library.h" 
#include "luna_error.h"
#include "intern.h"
#include "unsafe_runtime.h"
#include "data_runtime.h"
#include "vec_lib.h"
#include "parser.h"
#include "util.h"
#include "gc.h"


//I hate working on this file .. ptsd 

#define EPSILON 0.000001 // Tolerance for float comparison

typedef struct {
    Env *scope;
    Value callee;
    Value *argv;
    int argc;
    int line;
} DeferredCall;

#define CALL_ARG_STACK_MAX 8

static Value *call_arg_buffer(int argc, Value stack[CALL_ARG_STACK_MAX]) {
    if (argc <= CALL_ARG_STACK_MAX) return stack;
    Value *heap = malloc(sizeof(Value) * (size_t)argc);
    if (!heap) abort();
    return heap;
}

static int *call_flag_buffer(int argc, int stack[CALL_ARG_STACK_MAX]) {
    if (argc <= CALL_ARG_STACK_MAX) return stack;
    int *heap = calloc((size_t)argc, sizeof(int));
    if (!heap) abort();
    return heap;
}

static void call_arg_buffer_release(Value *argv, Value stack[CALL_ARG_STACK_MAX]) {
    if (argv != stack) free(argv);
}

static void call_flag_buffer_release(int *flags, int stack[CALL_ARG_STACK_MAX]) {
    if (flags != stack) free(flags);
}

static DeferredCall *deferred_calls = NULL;
static int deferred_call_count = 0;
static int deferred_call_cap = 0;

static void gc_note_heap_value_overwrite(const Value *value) {
    if (!value || !VALUE_IS_HEAP(*value)) return;
    switch (value->type) {
        case VAL_STRING:
            if (value->string) luna_gc_runtime_write_barrier(value->string);
            break;
        case VAL_LIST:
            if (value->list) luna_gc_runtime_write_barrier(value->list);
            break;
        case VAL_DENSE_LIST:
            if (value->dlist) luna_gc_runtime_write_barrier(value->dlist);
            break;
        case VAL_MAP:
            if (value->map) luna_gc_runtime_write_barrier(value->map);
            break;
        case VAL_CLOSURE:
            if (value->closure) luna_gc_runtime_write_barrier(value->closure);
            break;
        case VAL_TEMPLATE:
            if (value->template_obj) luna_gc_runtime_write_barrier(value->template_obj);
            break;
        default:
            break;
    }
}

static int gc_value_is_young_heap(const Value *value) {
    if (!value || !VALUE_IS_HEAP(*value)) return 0;
    switch (value->type) {
        case VAL_STRING:
            return value->string && GC_FROM_PAYLOAD(value->string)->generation == GC_GEN_YOUNG;
        case VAL_LIST:
            return value->list && GC_FROM_PAYLOAD(value->list)->generation == GC_GEN_YOUNG;
        case VAL_DENSE_LIST:
            return value->dlist && GC_FROM_PAYLOAD(value->dlist)->generation == GC_GEN_YOUNG;
        case VAL_MAP:
            return value->map && GC_FROM_PAYLOAD(value->map)->generation == GC_GEN_YOUNG;
        case VAL_CLOSURE:
            return value->closure && GC_FROM_PAYLOAD(value->closure)->generation == GC_GEN_YOUNG;
        case VAL_TEMPLATE:
            return value->template_obj && GC_FROM_PAYLOAD(value->template_obj)->generation == GC_GEN_YOUNG;
        default:
            return 0;
    }
}

static int interp_name_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int interp_name_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static void interp_append(char **buf, size_t *len, size_t *cap, const char *src, size_t src_len) {
    if (*len + src_len + 1 > *cap) {
        while (*len + src_len + 1 > *cap) *cap *= 2;
        *buf = realloc(*buf, *cap);
        if (!*buf) abort();
    }
    memcpy(*buf + *len, src, src_len);
    *len += src_len;
    (*buf)[*len] = '\0';
}

static int try_interpolate_string(Env *e, const char *src, Value *out) {
    if (!e || !src || !out || !strchr(src, '{')) return 0;

    size_t cap = strlen(src) + 32;
    size_t len = 0;
    int changed = 0;
    char *buf = malloc(cap);
    if (!buf) abort();
    buf[0] = '\0';

    const char *p = src;
    while (*p) {
        if (*p != '{') {
            interp_append(&buf, &len, &cap, p, 1);
            p++;
            continue;
        }

        const char *close = strchr(p + 1, '}');
        if (!close) {
            interp_append(&buf, &len, &cap, p, 1);
            p++;
            continue;
        }

        size_t name_len = (size_t)(close - (p + 1));
        if (name_len == 0 || !interp_name_start(p[1])) {
            interp_append(&buf, &len, &cap, p, (size_t)(close - p + 1));
            p = close + 1;
            continue;
        }

        int valid = 1;
        for (size_t i = 1; i < name_len; i++) {
            if (!interp_name_char(p[1 + i])) {
                valid = 0;
                break;
            }
        }
        if (!valid) {
            interp_append(&buf, &len, &cap, p, (size_t)(close - p + 1));
            p = close + 1;
            continue;
        }

        char name[256];
        if (name_len >= sizeof(name)) {
            interp_append(&buf, &len, &cap, p, (size_t)(close - p + 1));
            p = close + 1;
            continue;
        }
        memcpy(name, p + 1, name_len);
        name[name_len] = '\0';

        Value *slot = env_get_text(e, name);
        if (!slot) {
            interp_append(&buf, &len, &cap, p, (size_t)(close - p + 1));
            p = close + 1;
            continue;
        }

        char *rendered = value_to_string(*slot);
        interp_append(&buf, &len, &cap, rendered, strlen(rendered));
        free(rendered);
        changed = 1;
        p = close + 1;
    }

    if (!changed) {
        free(buf);
        return 0;
    }

    *out = value_string_len(buf, len);
    free(buf);
    return 1;
}

// Flags to handle 'return' statements across recursive calls
typedef struct {
    int active;
    Value value;
} ReturnException;

// Flags to handle 'break' and 'continue' inside loops
typedef struct {
    int break_active;
    int continue_active;
} LoopException;

static ReturnException return_exception = {0}; 
static LoopException loop_exception = {0};

// Tail-Call Optimization state
typedef struct {
    int active;         // 1 if a tail-call was detected in NODE_RETURN
    AstNode *func;      // the function definition being tail-called
    Value *args;        // evaluated args to rebind (heap-allocated array)
    int argc;           // number of args
} TcoPending;

static TcoPending tco_pending = {0};

// Tracks the function currently being executed (for TCO detection in NODE_RETURN)
static AstNode *current_executing_fn = NULL;
static Env *current_executing_env = NULL;

static int unsafe_block_depth = 0;

static void gc_safe_point(void) {
    luna_gc_runtime_safe_point();
}

static void deferred_calls_reset(void) {
    for (int i = 0; i < deferred_call_count; i++) {
        value_free(deferred_calls[i].callee);
        for (int j = 0; j < deferred_calls[i].argc; j++) value_free(deferred_calls[i].argv[j]);
        free(deferred_calls[i].argv);
    }
    free(deferred_calls);
    deferred_calls = NULL;
    deferred_call_count = 0;
    deferred_call_cap = 0;
}

static void deferred_calls_push(Env *scope, Value callee, int argc, Value *argv, int line) {
    if (deferred_call_count >= deferred_call_cap) {
        int next_cap = deferred_call_cap == 0 ? 8 : deferred_call_cap * 2;
        DeferredCall *grown = realloc(deferred_calls, sizeof(DeferredCall) * next_cap);
        if (!grown) abort();
        deferred_calls = grown;
        deferred_call_cap = next_cap;
    }
    deferred_calls[deferred_call_count].scope = scope;
    deferred_calls[deferred_call_count].callee = callee;
    deferred_calls[deferred_call_count].argv = argv;
    deferred_calls[deferred_call_count].argc = argc;
    deferred_calls[deferred_call_count].line = line;
    deferred_call_count++;
}

static void deferred_calls_run_scope(Env *scope) {
    if (!scope) return;
    while (1) {
        int found = -1;
        for (int i = deferred_call_count - 1; i >= 0; i--) {
            if (deferred_calls[i].scope == scope) {
                found = i;
                break;
            }
        }
        if (found < 0) break;

        DeferredCall call = deferred_calls[found];
        deferred_calls[found] = deferred_calls[deferred_call_count - 1];
        deferred_call_count--;

        Value result = luna_call_value(scope, call.callee, call.argc, call.argv, call.line);
        value_free(result);
        value_free(call.callee);
        for (int i = 0; i < call.argc; i++) value_free(call.argv[i]);
        free(call.argv);
    }
}

static int module_name_requested(const char *name, const char **names, int count) {
    for (int i = 0; i < count; i++) {
        if (names[i] == name) return 1;
    }
    return 0;
}

static void collect_exported_names_from_stmt(AstNode *n, const char ***names, int *count, int *cap) {
    if (!n) return;
    if (n->kind == NODE_LET && n->let.is_export) {
        if (!module_name_requested(n->let.name, *names, *count)) {
            if (*count >= *cap) {
                int next_cap = *cap == 0 ? 8 : *cap * 2;
                const char **grown = realloc((void *)*names, sizeof(const char *) * next_cap);
                if (!grown) abort();
                *names = grown;
                *cap = next_cap;
            }
            (*names)[(*count)++] = n->let.name;
        }
        return;
    }
    if (n->kind == NODE_FUNC_DEF && n->funcdef.is_export && n->funcdef.name) {
        if (!module_name_requested(n->funcdef.name, *names, *count)) {
            if (*count >= *cap) {
                int next_cap = *cap == 0 ? 8 : *cap * 2;
                const char **grown = realloc((void *)*names, sizeof(const char *) * next_cap);
                if (!grown) abort();
                *names = grown;
                *cap = next_cap;
            }
            (*names)[(*count)++] = n->funcdef.name;
        }
        return;
    }
    if (n->kind == NODE_DATA_DEF && n->data_def.is_export && n->data_def.name) {
        if (!module_name_requested(n->data_def.name, *names, *count)) {
            if (*count >= *cap) {
                int next_cap = *cap == 0 ? 8 : *cap * 2;
                const char **grown = realloc((void *)*names, sizeof(const char *) * next_cap);
                if (!grown) abort();
                *names = grown;
                *cap = next_cap;
            }
            (*names)[(*count)++] = n->data_def.name;
        }
        return;
    }
    if (n->kind == NODE_BLOC_DEF && n->bloc_def.is_export && n->bloc_def.name) {
        if (!module_name_requested(n->bloc_def.name, *names, *count)) {
            if (*count >= *cap) {
                int next_cap = *cap == 0 ? 8 : *cap * 2;
                const char **grown = realloc((void *)*names, sizeof(const char *) * next_cap);
                if (!grown) abort();
                *names = grown;
                *cap = next_cap;
            }
            (*names)[(*count)++] = n->bloc_def.name;
        }
        return;
    }
    if (n->kind == NODE_GROUP) {
        for (int i = 0; i < n->block.items.count; i++) {
            collect_exported_names_from_stmt(n->block.items.items[i], names, count, cap);
        }
    }
}

static void collect_module_exports(AstNode *program, const char ***names, int *count) {
    int cap = 0;
    *names = NULL;
    *count = 0;
    if (!program || program->kind != NODE_BLOCK) return;
    for (int i = 0; i < program->block.items.count; i++) {
        collect_exported_names_from_stmt(program->block.items.items[i], names, count, &cap);
    }
}

void interpreter_gc_mark_runtime_roots(void *ctx) {
    if (return_exception.active) {
        value_gc_mark(&return_exception.value, ctx);
    }
    if (tco_pending.active && tco_pending.args) {
        for (int i = 0; i < tco_pending.argc; i++) {
            value_gc_mark(&tco_pending.args[i], ctx);
        }
    }
    for (int i = 0; i < deferred_call_count; i++) {
        value_gc_mark(&deferred_calls[i].callee, ctx);
        for (int j = 0; j < deferred_calls[i].argc; j++) {
            value_gc_mark(&deferred_calls[i].argv[j], ctx);
        }
    }
}

// Centralized Truthiness Logic
static int is_truthy(Value v) {
    switch (v.type) {
        case VAL_BOOL: return v.b;
        case VAL_INT: return v.i != 0;
        case VAL_FLOAT: return v.f != 0.0;
        case VAL_POINTER: return v.ptr != 0;
        case VAL_BLOC:
        case VAL_BLOC_TYPE:
        case VAL_BOX:
        case VAL_TEMPLATE:
            return 1;
        case VAL_STRING: return v.string && v.string->chars && v.string->chars[0] != '\0'; // Empty strings are false
        case VAL_NULL: return 0;
        case VAL_LIST: 
        case VAL_DENSE_LIST:
        case VAL_MAP: return 1; // containers are truthy
        case VAL_NATIVE: return 1;
        case VAL_CHAR: return v.c != 0;
        case VAL_FILE:   return v.file != NULL; // Files are truthy if open
        default: return 0;
    }
}

static Value instantiate_data_type(Value dtype, int argc, Value *argv, int line) {
    if (dtype.type != VAL_DATA_TYPE || !dtype.dtype) {
        error_report_with_context(ERR_TYPE, line, 0,
            "Value is not a data constructor",
            "Call a data type such as Vec2(1, 2)");
        return value_null();
    }
    if (argc != dtype.dtype->field_count) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Data constructor '%s' expects %d field value(s), but got %d",
                 dtype.dtype->name ? dtype.dtype->name : "<data>", dtype.dtype->field_count, argc);
        error_report_with_context(ERR_ARGUMENT, line, 0, msg,
            "Pass one value for each declared data field");
        return value_null();
    }

    if (dtype.dtype->is_template) {
        char msg[256];
        Value out = value_template_from_dtype(dtype, argc, argv, msg, sizeof(msg));
        if (out.type == VAL_NULL) {
            error_report_with_context(ERR_ARGUMENT, line, 0, msg,
                "Pass one value for each declared template field");
        }
        return out;
    }

    Value out = value_map();
    value_map_set(&out, intern_string("__tag"), value_string(dtype.dtype->name ? dtype.dtype->name : ""));
    for (int i = 0; i < dtype.dtype->field_count; i++) {
        value_map_set(&out, dtype.dtype->fields[i], argv[i]);
    }
    return out;
}

static Value eval_expr(Env *e, AstNode *n);
static Value exec_stmt(Env *e, AstNode *n);

static long long normalize_index(long long idx, long long count) {
    if (idx < 0) idx += count;
    return idx;
}
static Value make_closure(Env *e, AstNode *fn) {
    int use_live_env = env_is_global(e);
    Env *captured = use_live_env ? e : env_snapshot(e);
    Value closure = value_closure(fn, captured, !use_live_env);
    if (fn->funcdef.name && captured && !use_live_env) {
        env_def(captured, fn->funcdef.name, closure);
    }
    return closure;
}

static Value call_user_function_with_args(Env *caller_env, Value callee, int argc, Value *argv, int line) {
    AstNode *fn = NULL;
    Env *captured = NULL;
    if (callee.type == VAL_CLOSURE && callee.closure) {
        fn = callee.closure->funcdef;
        captured = callee.closure->env;
    } else if (callee.type == VAL_FUNCTION) {
        fn = callee.func;
    } else {
        error_report_with_context(ERR_TYPE, line, 0,
            "Value is not a callable function",
            "Call a function or closure value, e.g. greet() or storedFunc()");
        return value_null();
    }

    int required = 0;
    for (int i = 0; i < fn->funcdef.param_count; i++) {
        if (!fn->funcdef.defaults[i]) required++;
    }
    if (argc < required || argc > fn->funcdef.param_count) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Function '%s' expects %d-%d argument(s), but got %d",
                 fn->funcdef.name ? fn->funcdef.name : "<anonymous>", required, fn->funcdef.param_count, argc);
        error_report_with_context(ERR_ARGUMENT, line, 0, msg,
            "Adjust the number of arguments to match the function definition");
        return value_null();
    }

    Env *scope = env_create(captured);
    for (int i = 0; i < fn->funcdef.param_count; i++) {
        Value v;
        if (i < argc) {
            v = value_copy(argv[i]);
        } else if (fn->funcdef.defaults[i]) {
            v = eval_expr(scope, fn->funcdef.defaults[i]);
        } else {
            v = value_null();
        }
        env_def_move(scope, fn->funcdef.params[i], &v);
    }

    AstNode *prev_fn = current_executing_fn;
    Env *prev_env = current_executing_env;
    current_executing_fn = fn;
    current_executing_env = scope;

tco_restart:
    for (int i = 0; i < fn->funcdef.body.count; i++) {
        exec_stmt(scope, fn->funcdef.body.items[i]);
        gc_safe_point();
        if (return_exception.active) {
            if (tco_pending.active && tco_pending.func == fn) {
                return_exception.active = 0;
                value_free(return_exception.value);
                tco_pending.active = 0;

                env_clear_locals(scope);
                env_reset_version(scope);
                for (int j = 0; j < fn->funcdef.param_count; j++) {
                    if (j < tco_pending.argc) {
                        env_def_move(scope, fn->funcdef.params[j], &tco_pending.args[j]);
                    } else if (fn->funcdef.defaults[j]) {
                        Value dv = eval_expr(scope, fn->funcdef.defaults[j]);
                        env_def_move(scope, fn->funcdef.params[j], &dv);
                    } else {
                        Value nv = value_null();
                        env_def_move(scope, fn->funcdef.params[j], &nv);
                    }
                }
                free(tco_pending.args);
                tco_pending.args = NULL;
                current_executing_env = scope;
                goto tco_restart;
            }
            break;
        }
    }

    Value ret = return_exception.active ? value_move(&return_exception.value) : value_null();
    if (return_exception.active) return_exception.active = 0;
    current_executing_fn = prev_fn;
    current_executing_env = prev_env;
    deferred_calls_run_scope(scope);
    env_free(scope);
    return ret;
}

Value luna_call_value(Env *caller_env, Value callee, int argc, Value *argv, int line) {
    if (callee.type == VAL_CLOSURE || callee.type == VAL_FUNCTION) {
        return call_user_function_with_args(caller_env, callee, argc, argv, line);
    }
    if (callee.type == VAL_NATIVE) {
        return ((NativeFunc)callee.native)(argc, argv, caller_env);
    }
    error_report_with_context(ERR_TYPE, line, 0,
        "Value is not callable",
        "Pass a function, closure, or native builtin here");
    return value_null();
}

// Recursively finds the actual memory location of a variable or list item
// Used for assigning values to specific list indices (e.g. x[0] = 5)
static Value *get_mutable_value(Env *e, AstNode *n) {
    if (n->kind == NODE_IDENT) {
        return env_get(e, n->ident.name);
    } 
    else if (n->kind == NODE_INDEX) {
        // Recursively get the parent list
        Value *list = get_mutable_value(e, n->index.target);
        if (!list || (list->type != VAL_LIST && list->type != VAL_DENSE_LIST)) return NULL;
        
        // Return list reference for dense lists; indexing is handled in assignment logic
        if (list->type == VAL_DENSE_LIST) return list;

        // Evaluate index
        Value idx = eval_expr(e, n->index.index);
        if (idx.type != VAL_INT) {
            value_free(idx);
            return NULL;
        }
        
        long long normalized = normalize_index(idx.i, list->list->count);
        if (normalized < 0 || normalized >= list->list->count) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Index %lld is out of bounds for list of length %d", idx.i, list->list->count);
            // Updated to use n->line from the AST node
            error_report_with_context(ERR_INDEX, n->line, 0, msg,
                "Check that your index is between 0 and len(list)-1");
            value_free(idx);
            return NULL;
        }
        
        // Return pointer to the specific item slot
        Value *item_ptr = &list->list->items[normalized];
        value_free(idx);
        return item_ptr;
    } else if (n->kind == NODE_FIELD) {
        Value *target = get_mutable_value(e, n->field.target);
        if (!target) return NULL;
        if (target->type == VAL_TEMPLATE) return value_template_field_slot(target, n->field.field);
        if (target->type == VAL_MAP) return value_map_get(target, n->field.field);
    }
    return NULL;
}

// Handles binary operations like +, -, *, /, comparison
static Value eval_binop(BinOpKind op, Value l, Value r, int line) {
    if (l.type == VAL_POINTER && r.type == VAL_POINTER) {
        switch (op) {
            case OP_EQ: return value_bool(l.ptr == r.ptr);
            case OP_NEQ: return value_bool(l.ptr != r.ptr);
            case OP_LT: return value_bool(l.ptr < r.ptr);
            case OP_GT: return value_bool(l.ptr > r.ptr);
            case OP_LTE: return value_bool(l.ptr <= r.ptr);
            case OP_GTE: return value_bool(l.ptr >= r.ptr);
            default: break;
        }
    }

    if (l.type == VAL_BLOC && r.type == VAL_BLOC) {
        if (op == OP_EQ) return value_bool(value_bloc_equal(l, r));
        if (op == OP_NEQ) return value_bool(!value_bloc_equal(l, r));
    }

    // 1. Handle Pure Integer Operations separately to preserve precision/types
    if (l.type == VAL_INT && r.type == VAL_INT) {
        switch (op) {
            case OP_ADD: return value_int(l.i + r.i);
            case OP_SUB: return value_int(l.i - r.i);
            case OP_MUL: return value_int(l.i * r.i);
            case OP_DIV: 
                if (r.i == 0) return value_int(0); 
                // Return int if exact division, float otherwise (10/2=5, 10/3=3.333)
                if (l.i % r.i == 0) return value_int(l.i / r.i);
                return value_float((double)l.i / (double)r.i);
            case OP_MOD: return value_int(l.i % r.i);
            case OP_EQ: return value_bool(l.i == r.i);
            case OP_NEQ: return value_bool(l.i != r.i);
            case OP_LT: return value_bool(l.i < r.i);
            case OP_GT: return value_bool(l.i > r.i);
            case OP_LTE: return value_bool(l.i <= r.i);
            case OP_GTE: return value_bool(l.i >= r.i);
            default: break;
        }
    }

    // 2. Handle Mixed/Float Operations
    if ((l.type == VAL_INT || l.type == VAL_FLOAT) && (r.type == VAL_INT || r.type == VAL_FLOAT)) {
        double dl = (l.type == VAL_INT) ? (double)l.i : l.f;
        double dr = (r.type == VAL_INT) ? (double)r.i : r.f;
        int is_res_float = 1; 
        
        double res = 0;
        
        switch (op) {
            case OP_ADD: res = dl + dr; break;
            case OP_SUB: res = dl - dr; break;
            case OP_MUL: res = dl * dr; break;
            case OP_DIV: res = dr == 0 ? 0 : dl / dr; break;
            case OP_MOD: res = (long long)dl % (long long)dr; is_res_float=0; break;
            
            //Fuzzy Comparison for Floats
            case OP_EQ: return value_bool(fabs(dl - dr) < EPSILON);
            case OP_NEQ: return value_bool(fabs(dl - dr) >= EPSILON);
            case OP_LT: return value_bool(dl < dr);
            case OP_GT: return value_bool(dl > dr);
            case OP_LTE: return value_bool(dl <= dr);
            case OP_GTE: return value_bool(dl >= dr);
            default: break;
        }
        
        if (is_res_float) {
            return value_float(res);
        } else {
            return value_int((long long)res);
        }
    }
    
    // Handle String Equality
    if (l.type == VAL_STRING && r.type == VAL_STRING && l.string && r.string) {
        if (op == OP_EQ) return value_bool(strcmp(l.string->chars, r.string->chars) == 0);
        if (op == OP_NEQ) return value_bool(strcmp(l.string->chars, r.string->chars) != 0);
    }

    // Handle Boolean and Null Equality
    if (op == OP_EQ) {
        if (l.type == VAL_BOOL && r.type == VAL_BOOL) return value_bool(l.b == r.b);
        if (l.type == VAL_NULL && r.type == VAL_NULL) return value_bool(1);
        if (l.type == VAL_NULL || r.type == VAL_NULL) return value_bool(0);
    }
    if (op == OP_NEQ) {
        if (l.type == VAL_BOOL && r.type == VAL_BOOL) return value_bool(l.b != r.b);
        if (l.type == VAL_NULL && r.type == VAL_NULL) return value_bool(0);
        if (l.type == VAL_NULL || r.type == VAL_NULL) return value_bool(1);
    }

    // Handle Char Equality
    if (l.type == VAL_CHAR && r.type == VAL_CHAR) {
        if (op == OP_EQ) return value_bool(l.c == r.c);
        if (op == OP_NEQ) return value_bool(l.c != r.c);
    }

    // Handle String Concatenation
    if (op == OP_ADD && (l.type == VAL_STRING || r.type == VAL_STRING)) {
        // Fast path: both sides are strings — concat directly (2 mallocs instead of 5)
        if (l.type == VAL_STRING && r.type == VAL_STRING) {
            const char *lc = (l.string && l.string->chars) ? l.string->chars : "";
            const char *rc = (r.string && r.string->chars) ? r.string->chars : "";
            size_t ll = strlen(lc), rl = strlen(rc);
            return value_string_concat_raw(lc, ll, rc, rl);
        }
        // Slow path: one side isn't a string, needs value_to_string conversion
        char *sl = value_to_string(l);
        char *sr = value_to_string(r);
        size_t ll = strlen(sl), rl = strlen(sr);
        Value v = value_string_concat_raw(sl, ll, sr, rl);
        free(sl);
        free(sr);
        return v;
    }
    if ((l.type == VAL_LIST || l.type == VAL_DENSE_LIST) && 
        (r.type == VAL_LIST || r.type == VAL_DENSE_LIST)) {
        switch (op) {
            case OP_ADD: return vec_add_values(l, r);
            case OP_SUB: return vec_sub_values(l, r);
            case OP_MUL: return vec_mul_values(l, r);
            case OP_DIV: return vec_div_values(l, r);
            default: break; // Fall through for other ops (like ==)
        }
    }
    
    error_report_with_context(ERR_TYPE, line, 0,
        "Invalid binary operation for these types",
        "Check that you are using compatible types (e.g. not adding a string and a number)");
    return value_null();
}

// Evaluates an expression node and returns a Value
static Value eval_expr(Env *e, AstNode *n) {
    if (luna_had_error) return value_null();
    if (!n) {
        return value_null();
    }
    luna_current_line = n->line;
    switch (n->kind) {
        case NODE_NUMBER: return value_int(n->number.value);
        case NODE_FLOAT: return value_float(n->fnumber.value);
        case NODE_STRING:
            if (strchr(n->string.text, '{')) {
                Value interpolated;
                if (try_interpolate_string(e, n->string.text, &interpolated)) {
                    return interpolated;
                }
            }
            if (!luna_gc_runtime_enabled()) return value_string(n->string.text);
            if (n->string.cached.type != VAL_STRING || !n->string.cached.string) {
                n->string.cached = value_string(n->string.text);
                if (n->string.cached.string) {
                    luna_gc_runtime_add_root(n->string.cached.string);
                }
            }
            return n->string.cached;
        case NODE_TEMPLATE: {
            size_t cap = 64;
            size_t len = 0;
            char *buf = malloc(cap);
            if (!buf) abort();
            buf[0] = '\0';

            for (int i = 0; i < n->template_string.expr_count; i++) {
                const char *chunk = n->template_string.chunks[i] ? n->template_string.chunks[i] : "";
                interp_append(&buf, &len, &cap, chunk, strlen(chunk));
                Value part = eval_expr(e, n->template_string.exprs[i]);
                char *rendered = value_to_string(part);
                interp_append(&buf, &len, &cap, rendered, strlen(rendered));
                free(rendered);
                value_free(part);
            }
            const char *tail = n->template_string.chunks[n->template_string.expr_count] ?
                               n->template_string.chunks[n->template_string.expr_count] : "";
            interp_append(&buf, &len, &cap, tail, strlen(tail));
            Value out = value_string_len(buf, len);
            free(buf);
            return out;
        }
        case NODE_CHAR: return value_char(n->character.value);
        case NODE_BOOL: return value_bool(n->boolean.value);
        
        // Recursively evaluate items in a list literal
        case NODE_LIST: {
            Value v = value_list();
            for (int i = 0; i < n->list.items.count; i++) {
                Value item = eval_expr(e, n->list.items.items[i]);
                if (unsafe_block_depth > 0 && unsafe_runtime_is_pointer(item) &&
                    !unsafe_runtime_check_gc_store(item, n->line)) {
                    value_free(item);
                    value_free(v);
                    return value_null();
                }
                value_list_append_move(&v, &item); // move item into list, no copy
            }
            return v;
        }
        case NODE_MAP: {
            Value v = value_map();
            for (int i = 0; i < n->map.count; i++) {
                Value item = eval_expr(e, n->map.values[i]);
                if (unsafe_block_depth > 0 && unsafe_runtime_is_pointer(item) &&
                    !unsafe_runtime_check_gc_store(item, n->line)) {
                    value_free(item);
                    value_free(v);
                    return value_null();
                }
                value_map_set_move(&v, n->map.keys[i], &item);
            }
            return v;
        }
        case NODE_FIELD: {
            Value target = eval_expr(e, n->field.target);
            if (target.type == VAL_BOX) {
                Value res = value_null();
                int found = 1;
                if (n->field.field == intern_string("len")) {
                    res = value_int((long long)value_box_len(target));
                } else if (n->field.field == intern_string("cap")) {
                    res = value_int((long long)value_box_cap(target));
                } else {
                    found = 0;
                }
                if (!found) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Field '%s' does not exist on this box value", n->field.field);
                    error_report_with_context(ERR_NAME, n->line, 0, msg,
                        "Use box.len or box.cap for phase-1 box values");
                }
                value_free(target);
                return found ? res : value_null();
            }

            if (target.type == VAL_TEMPLATE) {
                int found = 0;
                Value res = value_template_get_field(target, n->field.field, &found);
                if (!found) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Template field '%s' does not exist", n->field.field);
                    error_report_with_context(ERR_NAME, n->line, 0, msg,
                        "Access only declared fields for this template");
                }
                value_free(target);
                return found ? res : value_null();
            }

            int found = 0;
            Value res = value_bloc_get_field(target, n->field.field, &found);
            if (!found) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Field '%s' does not exist on this bloc value", n->field.field);
                error_report_with_context(ERR_NAME, n->line, 0, msg,
                    "Use dot access with a declared bloc field such as point.x");
            }
            value_free(target);
            return found ? res : value_null();
        }
        case NODE_TYPED_INIT: {
            Value *ctor = env_get(e, n->typed_init.name);
            if (!ctor) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Type '%s' is not defined", n->typed_init.name);
                error_report_with_context(ERR_NAME, n->line, 0, msg,
                    "Define the bloc before constructing it");
                return value_null();
            }
            if (ctor->type == VAL_BLOC_TYPE) {
                int argc = n->typed_init.args.count;
                Value *argv = argc > 0 ? malloc(sizeof(Value) * (size_t)argc) : NULL;
                for (int i = 0; i < argc; i++) {
                    argv[i] = eval_expr(e, n->typed_init.args.items[i]);
                }

                char msg[256];
                Value res = value_null();
                if (!value_bloc_check_construct(*ctor, argc, argv, msg, sizeof(msg))) {
                    error_report_with_context(ERR_TYPE, n->line, 0, msg,
                        "Bloc values must stay inline and only use int, float, bool, char, or nested blocs");
                } else {
                    res = value_bloc_construct(*ctor, argc, argv);
                }

                for (int i = 0; i < argc; i++) value_free(argv[i]);
                free(argv);
                return res;
            }

            char msg[256];
            snprintf(msg, sizeof(msg), "'%s{...}' is not a bloc constructor", n->typed_init.name);
            error_report_with_context(ERR_TYPE, n->line, 0, msg,
                "Use Name{...} only with bloc types for now");
            return value_null();
        }
        case NODE_BOX_ALLOC: {
            Value size = eval_expr(e, n->box_alloc.size);
            if (size.type != VAL_INT) {
                error_report_with_context(ERR_TYPE, n->line, 0,
                    "box[...] expects an integer byte size",
                    "Use box[128] or another positive integer size");
                value_free(size);
                return value_null();
            }
            char msg[256];
            Value res = value_box((size_t)size.i, msg, sizeof(msg));
            if (res.type == VAL_NULL) {
                error_report_with_context(ERR_ARGUMENT, n->line, 0, msg,
                    "Boxes are stack-scoped scratch buffers up to one cache line");
            } else {
                value_box_mark_scope(res, env_scope_id(e));
            }
            value_free(size);
            return res;
        }
        case NODE_FUNC_DEF:
            return make_closure(e, n);
        
        // Variable lookup (O(0) Fast Local Cache)
        case NODE_IDENT: {
            if (n->ident.cached_val && n->ident.cached_env_version == env_get_version(e)) {
                return value_copy(*(n->ident.cached_val));
            }

            Value *v = env_get(e, n->ident.name);
            if (!v) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Variable '%s' is not defined", n->ident.name);
                error_report_with_context(ERR_NAME, n->line, 0, msg,
                    "Declare variables with 'let' before using them");
                return value_null();
            }
            // Bind the cache pointer
            n->ident.cached_val = v;
            n->ident.cached_env_version = env_get_version(e);
            return value_copy(*v);
        }
        
        case NODE_BINOP: {
            // Logic Short-circuiting 
            if (n->binop.op == OP_AND) {
                Value l = eval_expr(e, n->binop.left);
                if (!is_truthy(l)) {
                    return l; // Short-circuit: return left (false)
                }
                value_free(l);
                return eval_expr(e, n->binop.right);
            }
            if (n->binop.op == OP_OR) {
                Value l = eval_expr(e, n->binop.left);
                if (is_truthy(l)) {
                    return l; // Short-circuit: return left (true)
                }
                value_free(l);
                return eval_expr(e, n->binop.right);
            }

            Value l = eval_expr(e, n->binop.left);
            Value r = eval_expr(e, n->binop.right);
            if (unsafe_runtime_is_pointer(l) && unsafe_runtime_is_pointer(r)) {
                int cmp_op = -1;
                switch (n->binop.op) {
                    case OP_EQ: cmp_op = 0; break;
                    case OP_NEQ: cmp_op = 1; break;
                    case OP_LT: cmp_op = 2; break;
                    case OP_GT: cmp_op = 3; break;
                    case OP_LTE: cmp_op = 4; break;
                    case OP_GTE: cmp_op = 5; break;
                    default: break;
                }
                if (cmp_op >= 0 &&
                    !unsafe_runtime_check_compare(l, r, cmp_op, n->line)) {
                    value_free(l);
                    value_free(r);
                    return value_null();
                }
            }
            Value res = eval_binop(n->binop.op, l, r, n->line);
            value_free(l);
            value_free(r);
            return res;
        }
        
        // Unary NOT
        case NODE_NOT: {
            Value v = eval_expr(e, n->logic_not.expr);
            Value res = value_bool(!is_truthy(v));
            value_free(v);
            return res;
        }
        // List Indexing: list[index]
        case NODE_INDEX: {
            Value target = eval_expr(e, n->index.target);
            Value idx = eval_expr(e, n->index.index);
            if (idx.type == VAL_INT) {
                if (target.type == VAL_LIST && target.list) {
                    long long normalized = normalize_index(idx.i, target.list->count);
                    if (normalized >= 0 && normalized < target.list->count) {
                        Value res = value_copy(target.list->items[normalized]);
                        value_free(target);
                        value_free(idx);
                        return res;
                    }
                } else if (target.type == VAL_DENSE_LIST && target.dlist) {
                    long long normalized = normalize_index(idx.i, target.dlist->count);
                    if (normalized >= 0 && normalized < target.dlist->count) {
                        Value res = value_float(target.dlist->data[normalized]);
                        value_free(target);
                        value_free(idx);
                        return res;
                    }
                } else if (target.type == VAL_STRING && target.string) {
                    long long len = (long long)strlen(target.string->chars);
                    long long normalized = normalize_index(idx.i, len);
                    if (normalized >= 0 && normalized < len) {
                        Value res = value_char(target.string->chars[normalized]);
                        value_free(target);
                        value_free(idx);
                        return res;
                    }
                } else if (target.type == VAL_MAP) {
                    value_free(idx);
                    value_free(target);
                    return value_null();
                }
            } else if (idx.type == VAL_STRING && idx.string && target.type == VAL_MAP && target.map) {
                Value *slot = value_map_get(&target, intern_string(idx.string->chars));
                Value res = slot ? value_copy(*slot) : value_null();
                value_free(target);
                value_free(idx);
                return res;
            }
            value_free(target);
            value_free(idx);
            return value_null();
        }

        //  Increment Operator (++)
        case NODE_INC: {
            Value *v = NULL;
            if (n->inc.cached_val && n->inc.cached_env_version == env_get_version(e)) {
                v = n->inc.cached_val;
            } else {
                v = env_get(e, n->inc.name);
                if (v) {
                    n->inc.cached_val = v;
                    n->inc.cached_env_version = env_get_version(e);
                }
            }
            
            if (v && v->type == VAL_INT) {
                Value old = value_copy(*v);
                v->i++;
                return old;
            } else if (v && v->type == VAL_FLOAT) {
                Value old = value_copy(*v);
                v->f++;
                return old;
            }
            return value_null();
        }
        
        // Decrement Operator (--)
        case NODE_DEC: {
            Value *v = NULL;
            if (n->dec.cached_val && n->dec.cached_env_version == env_get_version(e)) {
                v = n->dec.cached_val;
            } else {
                v = env_get(e, n->dec.name);
                if (v) {
                    n->dec.cached_val = v;
                    n->dec.cached_env_version = env_get_version(e);
                }
            }
            
            if (v && v->type == VAL_INT) {
                Value old = value_copy(*v);
                v->i--;
                return old;
            } else if (v && v->type == VAL_FLOAT) {
                Value old = value_copy(*v);
                v->f--;
                return old;
            }
            return value_null();
        }

        // Function Calls
        case NODE_CALL: {
            const char *callee_name = NULL;
            if (n->call.callee && n->call.callee->kind == NODE_IDENT) {
                callee_name = n->call.callee->ident.name;
            }

            switch (n->call.kind) {
                case CALL_ALLOC: {
                    if (unsafe_block_depth <= 0) {
                        error_report_with_context(ERR_RUNTIME, n->line, 0,
                            "alloc() may only be used inside unsafe blocks",
                            "Wrap pointer operations in unsafe { ... }");
                        return value_null();
                    }
                    if (n->call.args.count != 1) {
                        error_report_with_context(ERR_ARGUMENT, n->line, 0,
                            "alloc() expects exactly 1 argument",
                            "Use alloc(slotCount)");
                        return value_null();
                    }
                    Value size = eval_expr(e, n->call.args.items[0]);
                    Value res = unsafe_runtime_alloc(size, n->line);
                    value_free(size);
                    return res;
                }
                case CALL_FREE: {
                    if (n->call.args.count != 1) {
                        error_report_with_context(ERR_ARGUMENT, n->line, 0,
                            "free() expects exactly 1 argument",
                            "Use free(boxValue) or free(ptr)");
                        return value_null();
                    }
                    Value target = eval_expr(e, n->call.args.items[0]);
                    if (target.type == VAL_BOX) {
                        char msg[256];
                        Value res = value_null();
                        if (!value_box_free(target, msg, sizeof(msg))) {
                            error_report_with_context(ERR_RUNTIME, n->line, 0, msg,
                                "A box must be freed exactly once");
                        }
                        value_free(target);
                        return res;
                    }
                    if (unsafe_block_depth <= 0) {
                        error_report_with_context(ERR_RUNTIME, n->line, 0,
                            "free() may only be used inside unsafe blocks",
                            "Wrap pointer operations in unsafe { ... }");
                        return value_null();
                    }
                    Value res = unsafe_runtime_free(target, n->line);
                    value_free(target);
                    return res;
                }
                case CALL_LOAD: {
                    if (unsafe_block_depth <= 0) {
                        error_report_with_context(ERR_RUNTIME, n->line, 0,
                            "load() may only be used inside unsafe blocks",
                            "Wrap pointer operations in unsafe { ... }");
                        return value_null();
                    }
                    if (n->call.args.count != 1) {
                        error_report_with_context(ERR_ARGUMENT, n->line, 0,
                            "load() expects exactly 1 pointer argument",
                            "Use load(ptr)");
                        return value_null();
                    }
                    Value ptrv = eval_expr(e, n->call.args.items[0]);
                    Value res = unsafe_runtime_deref(ptrv, n->line);
                    value_free(ptrv);
                    return res;
                }
                case CALL_STORE: {
                    if (unsafe_block_depth <= 0) {
                        error_report_with_context(ERR_RUNTIME, n->line, 0,
                            "store() may only be used inside unsafe blocks",
                            "Wrap pointer operations in unsafe { ... }");
                        return value_null();
                    }
                    if (n->call.args.count != 2) {
                        error_report_with_context(ERR_ARGUMENT, n->line, 0,
                            "store() expects exactly 2 arguments",
                            "Use store(ptr, value)");
                        return value_null();
                    }
                    Value ptrv = eval_expr(e, n->call.args.items[0]);
                    Value rhs = eval_expr(e, n->call.args.items[1]);
                    Value res = unsafe_runtime_store(ptrv, rhs, n->line);
                    value_free(ptrv);
                    value_free(rhs);
                    return res;
                }
                case CALL_PTR_OFFSET: {
                    if (unsafe_block_depth <= 0) {
                        error_report_with_context(ERR_RUNTIME, n->line, 0,
                            "ptr_offset() may only be used inside unsafe blocks",
                            "Wrap pointer operations in unsafe { ... }");
                        return value_null();
                    }
                    if (n->call.args.count != 2) {
                        error_report_with_context(ERR_ARGUMENT, n->line, 0,
                            "ptr_offset() expects exactly 2 arguments",
                            "Use ptr_offset(ptr, offset)");
                        return value_null();
                    }
                    Value basev = eval_expr(e, n->call.args.items[0]);
                    Value offv = eval_expr(e, n->call.args.items[1]);
                    Value res = unsafe_runtime_ptr_add(basev, offv, n->line);
                    value_free(basev);
                    value_free(offv);
                    return res;
                }
                case CALL_ADDRESS_OF: {
                    if (unsafe_block_depth <= 0) {
                        error_report_with_context(ERR_RUNTIME, n->line, 0,
                            "address_of() may only be used inside unsafe blocks",
                            "Wrap pointer operations in unsafe { ... }");
                        return value_null();
                    }
                    if (n->call.args.count != 1) {
                        error_report_with_context(ERR_ARGUMENT, n->line, 0,
                            "address_of() expects exactly 1 argument",
                            "Use address_of(variableName)");
                        return value_null();
                    }
                    AstNode *arg = n->call.args.items[0];
                    if (!arg || arg->kind != NODE_IDENT) {
                        error_report_with_context(ERR_RUNTIME, n->line, 0,
                            "address_of() only works on named values",
                            "address_of() needs a variable name such as address_of(x), not a temporary expression.");
                        return value_null();
                    }
                    Value *slot = env_get(e, arg->ident.name);
                    if (!slot) {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "Variable '%s' is not defined", arg->ident.name);
                        error_report_with_context(ERR_NAME, n->line, 0, msg,
                            "Declare the variable before taking its address");
                        return value_null();
                    }
                    return unsafe_runtime_addr(slot, n->line);
                }
                case CALL_DEFER: {
                    if (n->call.args.count != 1) {
                        error_report_with_context(ERR_ARGUMENT, n->line, 0,
                            "defer() expects exactly 1 argument",
                            "Use defer(close(file)) for general cleanup, or defer(ptr) inside unsafe");
                        return value_null();
                    }
                    AstNode *arg = n->call.args.items[0];
                    if (unsafe_block_depth > 0) {
                        Value ptrv = eval_expr(e, arg);
                        if (unsafe_runtime_is_pointer(ptrv)) {
                            Value res = unsafe_runtime_defer(ptrv, n->line);
                            value_free(ptrv);
                            return res;
                        }
                        value_free(ptrv);
                    }

                    if (!arg || arg->kind != NODE_CALL) {
                        error_report_with_context(ERR_ARGUMENT, n->line, 0,
                            "defer() expects a call expression",
                            "Use defer(close(file)) or another function call to schedule cleanup");
                        return value_null();
                    }

                    Value deferred_callee = eval_expr(e, arg->call.callee);
                    if (deferred_callee.type != VAL_CLOSURE &&
                        deferred_callee.type != VAL_FUNCTION &&
                        deferred_callee.type != VAL_NATIVE) {
                        error_report_with_context(ERR_TYPE, n->line, 0,
                            "defer() needs a callable expression",
                            "Wrap the cleanup work in a function call, for example defer(close(file))");
                        value_free(deferred_callee);
                        return value_null();
                    }

                    int argc = arg->call.args.count;
                    Value deferred_stack[CALL_ARG_STACK_MAX];
                    Value *argv = argc > 0 ? call_arg_buffer(argc, deferred_stack) : NULL;
                    for (int i = 0; i < argc; i++) {
                        argv[i] = eval_expr(e, arg->call.args.items[i]);
                    }
                    if (argv == deferred_stack) {
                        Value *owned = malloc(sizeof(Value) * (size_t)argc);
                        if (!owned) abort();
                        memcpy(owned, argv, sizeof(Value) * (size_t)argc);
                        argv = owned;
                    }
                    deferred_calls_push(e, deferred_callee, argc, argv, n->line);
                    return value_null();
                }
                case CALL_DEBUG: {
                    if (n->call.args.count == 1) {
                        const char *label = "value";
                        AstNode *arg = n->call.args.items[0];
                        if (arg->kind == NODE_IDENT) label = arg->ident.name;
                        Value v = eval_expr(e, arg);
                        printf("%s = ", label);
                        value_fprint(stdout, v);
                        putchar('\n');
                        return v;
                    }
                    break;
                }
                case CALL_SHAPE: {
                    if (n->call.args.count == 1) {
                        Value v = eval_expr(e, n->call.args.items[0]);
                        if (v.type == VAL_BLOC) {
                            const char *name = value_bloc_name(v);
                            Value out = name ? value_string(name) : value_null();
                            value_free(v);
                            return out;
                        }
                        if (v.type == VAL_BOX) {
                            value_free(v);
                            return value_string("box");
                        }
                        if (v.type == VAL_TEMPLATE) {
                            const char *name = value_template_name(v);
                            Value out = name ? value_string(name) : value_null();
                            value_free(v);
                            return out;
                        }
                        if (v.type == VAL_MAP && v.map) {
                            Value *tag = value_map_get(&v, intern_string("__tag"));
                            if (tag && tag->type == VAL_STRING && tag->string) {
                                Value out = value_copy(*tag);
                                value_free(v);
                                return out;
                            }
                        }
                        value_free(v);
                        return value_null();
                    }
                    break;
                }
                case CALL_LEN: {
                    if (n->call.args.count == 1) {
                        Value v = eval_expr(e, n->call.args.items[0]);
                        int len = 0;
                        if (v.type == VAL_STRING && v.string) len = strlen(v.string->chars);
                        if (v.type == VAL_LIST && v.list) len = v.list->count;
                        if (v.type == VAL_DENSE_LIST && v.dlist) len = v.dlist->count;
                        if (v.type == VAL_MAP && v.map) len = v.map->count;
                        if (v.type == VAL_BOX) len = (int)value_box_len(v);
                        if (v.type == VAL_TEMPLATE) len = value_template_len(v);
                        value_free(v);
                        return value_int(len);
                    }
                    break;
                }
                case CALL_APPEND: {
                    if (n->call.args.count != 2) {
                        fprintf(stderr, "Runtime Error: append() takes 2 arguments (list, value)\n");
                        return value_null();
                    }

                    Value *list_ptr = get_mutable_value(e, n->call.args.items[0]);
                    Value item_val = eval_expr(e, n->call.args.items[1]);
                    if (unsafe_block_depth > 0 && unsafe_runtime_is_pointer(item_val) &&
                        !unsafe_runtime_check_gc_store(item_val, n->line)) {
                        value_free(item_val);
                        return value_null();
                    }

                    if (list_ptr && list_ptr->type == VAL_LIST) {
                        value_list_append_move(list_ptr, &item_val);
                    } else if (list_ptr && list_ptr->type == VAL_DENSE_LIST) {
                        double dv = (item_val.type == VAL_INT) ? (double)item_val.i : item_val.f;
                        value_dlist_append(list_ptr, dv);
                    } else {
                        error_report_with_context(ERR_ARGUMENT, n->line, 0,
                            "append() expects a list variable as the first argument",
                            "Use append(myList, value) where myList is a list variable");
                    }

                    value_free(item_val);
                    return value_null();
                }
                case CALL_TYPE: {
                    if (n->call.args.count == 1) {
                        Value v = eval_expr(e, n->call.args.items[0]);
                        char *tname = "unknown";
                        switch (v.type) {
                            case VAL_INT:
                                tname = (v.i > INT_MAX || v.i < INT_MIN) ? "long" : "int";
                                break;
                            case VAL_FLOAT: tname = "float"; break;
                            case VAL_STRING: tname = "string"; break;
                            case VAL_CHAR: tname = "char"; break;
                            case VAL_BOOL: tname = "boolean"; break;
                            case VAL_POINTER: tname = "pointer"; break;
                            case VAL_BLOC: tname = "bloc"; break;
                            case VAL_BLOC_TYPE: tname = "bloc_type"; break;
                            case VAL_BOX: tname = "box"; break;
                            case VAL_LIST:
                            case VAL_DENSE_LIST: tname = "list"; break;
                            case VAL_MAP: tname = "map"; break;
                            case VAL_TEMPLATE: tname = "template"; break;
                            case VAL_DATA_TYPE: tname = "data_type"; break;
                            case VAL_NATIVE: tname = "native_function"; break;
                            case VAL_FUNCTION:
                            case VAL_CLOSURE: tname = "function"; break;
                            case VAL_NULL: tname = "null"; break;
                            case VAL_FILE: tname = "file"; break;
                        }
                        value_free(v);
                        return value_string(tname);
                    }
                    break;
                }
                case CALL_INT: {
                    if (n->call.args.count == 1) {
                        Value v = eval_expr(e, n->call.args.items[0]);
                        long long res = 0;
                        if (unsafe_runtime_is_pointer(v)) res = (long long)v.ptr;
                        else if (v.type == VAL_STRING && v.string) res = atoll(v.string->chars);
                        else if (v.type == VAL_FLOAT) res = (long long)v.f;
                        else if (v.type == VAL_INT) res = v.i;
                        else if (v.type == VAL_BOOL) res = v.b;
                        else if (v.type == VAL_CHAR) res = (long long)v.c;
                        value_free(v);
                        return value_int(res);
                    }
                    break;
                }
                case CALL_FLOAT: {
                    if (n->call.args.count == 1) {
                        Value v = eval_expr(e, n->call.args.items[0]);
                        if (!unsafe_runtime_check_cast(v, 1, n->line)) {
                            value_free(v);
                            return value_null();
                        }
                        double res = 0.0;
                        if (v.type == VAL_STRING && v.string) res = atof(v.string->chars);
                        else if (v.type == VAL_INT) res = (double)v.i;
                        else if (v.type == VAL_FLOAT) res = v.f;
                        else if (v.type == VAL_BOOL) res = v.b ? 1.0 : 0.0;
                        value_free(v);
                        return value_float(res);
                    }
                    break;
                }
                case CALL_GENERIC:
                    break;
            }

            Value callee = eval_expr(e, n->call.callee);
            if (callee.type == VAL_DATA_TYPE) {
                int argc = n->call.args.count;
                Value argv_stack[CALL_ARG_STACK_MAX];
                Value *argv = argc > 0 ? call_arg_buffer(argc, argv_stack) : NULL;
                for (int i = 0; i < argc; i++) argv[i] = eval_expr(e, n->call.args.items[i]);
                Value res = instantiate_data_type(callee, argc, argv, n->line);
                for (int i = 0; i < argc; i++) value_free(argv[i]);
                call_arg_buffer_release(argv, argv_stack);
                value_free(callee);
                return res;
            }
            if (callee.type == VAL_CLOSURE || callee.type == VAL_FUNCTION) {
                if (unsafe_block_depth > 0 &&
                    !unsafe_runtime_check_call(0, n->line)) {
                    value_free(callee);
                    return value_null();
                }
                int argc = n->call.args.count;
                Value argv_stack[CALL_ARG_STACK_MAX];
                Value *argv = argc > 0 ? call_arg_buffer(argc, argv_stack) : NULL;
                for (int i = 0; i < argc; i++) {
                    argv[i] = eval_expr(e, n->call.args.items[i]);
                }
                Value res = luna_call_value(e, callee, argc, argv, n->line);
                for (int i = 0; i < argc; i++) value_free(argv[i]);
                call_arg_buffer_release(argv, argv_stack);
                value_free(callee);
                return res;
            }

            if (callee.type == VAL_NATIVE) {
                if (unsafe_block_depth > 0 &&
                    !unsafe_runtime_check_call(1, n->line)) {
                    value_free(callee);
                    return value_null();
                }
                // Evaluate Arguments first
                int argc = n->call.args.count;
                Value argv_stack[CALL_ARG_STACK_MAX];
                int direct_ref_stack[CALL_ARG_STACK_MAX];
                Value *argv = argc > 0 ? call_arg_buffer(argc, argv_stack) : NULL;
                int *is_direct_ref = argc > 0 ? call_flag_buffer(argc, direct_ref_stack) : NULL;
                for (int i = 0; i < argc; i++) {
                    // Now it Passes list identifiers by reference to allow in-place modification
                    if (n->call.args.items[i]->kind == NODE_IDENT) {
                        Value *env_ref = env_get(e, n->call.args.items[i]->ident.name);
                        if (env_ref && (env_ref->type == VAL_LIST || env_ref->type == VAL_DENSE_LIST ||
                                        env_ref->type == VAL_MAP || env_ref->type == VAL_TEMPLATE)) {
                            argv[i] = *env_ref; // Pass direct reference
                            is_direct_ref[i] = 1;
                        } else {
                            argv[i] = eval_expr(e, n->call.args.items[i]);
                        }
                    } else {
                        argv[i] = eval_expr(e, n->call.args.items[i]);
                    }
                }

                // Call the C Function Pointer - Updated to pass environment 'e' for variable binding
                Value res = ((NativeFunc)callee.native)(argc, argv, e);

                // Clean up arguments
                for (int i = 0; i < argc; i++) {
                    if (!is_direct_ref[i]) {
                        value_free(argv[i]);
                    }
                }
                call_flag_buffer_release(is_direct_ref, direct_ref_stack);
                call_arg_buffer_release(argv, argv_stack);
                value_free(callee);
                return res;
            }

            // 3. Fallback
            char err_msg[128];
            snprintf(err_msg, sizeof(err_msg), "'%s' is not a function", callee_name ? callee_name : "value");
            error_report_with_context(ERR_TYPE, n->line, 0, err_msg,
                "Make sure you are calling a function value, closure, or native builtin");

            value_free(callee);
            return value_null();
        }
        
        case NODE_INPUT: {
            char buf[256];
            if (n->input.prompt) {
                printf("%s", n->input.prompt);
            }
            if (fgets(buf, 256, stdin)) {
                buf[strcspn(buf, "\n")] = 0;
            } else {
                buf[0] = 0;
            }
            return value_string(buf);
        }
        default: return value_null();
    }
}

// Executes a statement node (side effects, control flow)
static Value exec_stmt(Env *e, AstNode *n) {
    if (luna_had_error) return value_null();
    if (!n) {
        return value_null();
    }

    luna_current_line = n->line;

    switch (n->kind) {
        case NODE_LET: {
            Value v = eval_expr(e, n->let.expr);
            if (n->let.is_const) env_def_const_move(e, n->let.name, &v);
            else env_def_move(e, n->let.name, &v);
            return value_null();
        }
        case NODE_ASSIGN: {
            Value v = eval_expr(e, n->assign.expr);
            if (unsafe_block_depth > 0 && unsafe_runtime_is_pointer(v) && !env_has_local(e, n->assign.name) &&
                !unsafe_runtime_check_escape(v, n->line)) {
                value_free(v);
                return value_null();
            }
            if (n->assign.cached_val && n->assign.cached_env_version == env_get_version(e)) {
                // O(0) cache hit update
                value_free(*(n->assign.cached_val));
                *(n->assign.cached_val) = v;
            } else {
                Value *target = env_get(e, n->assign.name);
                if (target) {
                    n->assign.cached_val = target;
                    n->assign.cached_env_version = env_get_version(e);
                    value_free(*target);
                    *target = v;
                } else {
                    env_assign_move(e, n->assign.name, &v); // fallback: move into parent scope
                }
            }
            return value_null();
        }
        case NODE_ASSIGN_INDEX: {
            Value val = eval_expr(e, n->assign_index.value);
            if (unsafe_block_depth > 0 && unsafe_runtime_is_pointer(val) &&
                !unsafe_runtime_check_gc_store(val, n->line)) {
                value_free(val);
                return value_null();
            }
            
            // Get pointer to the actual list item in the environment
            Value *target = get_mutable_value(e, n->assign_index.list);
            
            // Verify target is actually a list
            if (!target || (target->type != VAL_LIST && target->type != VAL_DENSE_LIST &&
                            target->type != VAL_MAP && target->type != VAL_TEMPLATE)) {
                // Use node line number
                error_report_with_context(ERR_TYPE, n->line, 0,
                    "Cannot assign through this target",
                    "Use integer indices for lists, or string keys for maps and templates");
                value_free(val);
                return value_null();
            }

            // Evaluate the index
            Value idx = eval_expr(e, n->assign_index.index);
            if ((target->type == VAL_LIST || target->type == VAL_DENSE_LIST) && idx.type != VAL_INT) {
                // Use node line number
                error_report_with_context(ERR_TYPE, n->line, 0,
                    "List index must be an integer",
                    "Use integer values for list indices, e.g., myList[0] or myList[i]");
                value_free(val);
                value_free(idx);
                return value_null();
            }

            if (target->type == VAL_LIST && target->list) {
                long long normalized = normalize_index(idx.i, target->list->count);
                // Bounds Check
                if (normalized < 0 || normalized >= target->list->count) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Index %lld is out of bounds for list of length %d",
                            idx.i, target->list->count);
                    // Use node line number
                    error_report_with_context(ERR_INDEX, n->line, 0, msg,
                        "Ensure your index is between 0 and len(list)-1");
                    value_free(val);
                    value_free(idx);
                    return value_null();
                }

                // Assign to the specific slot
                // Move value directly into list slot (no copy needed)
                Value *slot = &target->list->items[normalized];
                gc_note_heap_value_overwrite(slot);
                if (target->list && target->list->items && gc_value_is_young_heap(&val)) {
                    luna_gc_runtime_remember(target->list->items);
                }
                value_free(*slot);
                *slot = val;
                val.type = VAL_NULL;
            } else if (target->type == VAL_MAP && target->map) {
                if (idx.type != VAL_STRING || !idx.string) {
                    error_report_with_context(ERR_TYPE, n->line, 0,
                        "Map assignment index must be a string",
                        "Use map[\"field\"] = value for map and data values");
                    value_free(val);
                    value_free(idx);
                    return value_null();
                }
                value_map_set_move(target, intern_string(idx.string->chars), &val);
            } else if (target->type == VAL_TEMPLATE) {
                if (idx.type != VAL_STRING || !idx.string) {
                    error_report_with_context(ERR_TYPE, n->line, 0,
                        "Template assignment index must be a string",
                        "Use template[\"field\"] = value or template.field = value");
                    value_free(val);
                    value_free(idx);
                    return value_null();
                }
                char msg[256];
                if (!value_template_set_field(target, intern_string(idx.string->chars), &val, msg, sizeof(msg))) {
                    error_report_with_context(ERR_NAME, n->line, 0, msg,
                        "Assign only to declared template fields");
                    value_free(val);
                    value_free(idx);
                    return value_null();
                }
            } else if (target->type == VAL_DENSE_LIST && target->dlist) {
                // Direct high-performance assignment for dense lists
                long long normalized = normalize_index(idx.i, target->dlist->count);
                if (normalized >= 0 && normalized < target->dlist->count) {
                    target->dlist->data[normalized] = (val.type == VAL_INT) ? (double)val.i : val.f;
                }
            }
            
            value_free(val);
            value_free(idx);
            return value_null();
        }
        case NODE_PRINT: {
            for (int i = 0; i < n->print.args.count; i++) {
                Value v = eval_expr(e, n->print.args.items[i]);
                value_fprint(stdout, v);
                putchar(' ');
                value_free(v);
            }
            putchar('\n');
            return value_null();
        }
        case NODE_IF: {
            Value v = eval_expr(e, n->ifstmt.cond);
            int t = is_truthy(v);
            value_free(v);

            NodeList block = t ? n->ifstmt.then_block : n->ifstmt.else_block;
            Env *scope = env_create(e);
            for (int i = 0; i < block.count; i++) {
                exec_stmt(scope, block.items[i]);
                gc_safe_point();
                // Stop if a control flow event occurred
                if (return_exception.active || loop_exception.break_active || loop_exception.continue_active) {
                    break;
                }
            }
            deferred_calls_run_scope(scope);
            env_free(scope);
            return value_null();
        }
        case NODE_WHILE: {
            Env *scope = env_create(e); // Create scope ONCE outside the loop
            while (1) {
                Value v = eval_expr(scope, n->whilestmt.cond);
                int t = is_truthy(v);
                value_free(v);
                if (!t) {
                    break;
                }

                for (int i = 0; i < n->whilestmt.body.count; i++) {
                    exec_stmt(scope, n->whilestmt.body.items[i]);
                    gc_safe_point();
                    if (return_exception.active || loop_exception.break_active) {
                        break;
                    }
                    if (loop_exception.continue_active) {
                        break;
                    }
                }
                deferred_calls_run_scope(scope);
                // Clear all variables bounded during this iteration, 
                // keeping the same Scope memory block for cache hits
                env_clear_locals(scope);

                if (return_exception.active) {
                    break;
                }
                if (loop_exception.break_active) {
                    loop_exception.break_active = 0;
                    break;
                }
                if (loop_exception.continue_active) {
                    loop_exception.continue_active = 0;
                    continue;
                }
            }
            return value_null();
        }
        case NODE_FOR: {
            Env *scope = env_create(e); // Create scope for the loop variable (i)
            
            // 1. Run Initializer (once)
            exec_stmt(scope, n->forstmt.init);
            gc_safe_point();

            Env *inner_scope = env_create(scope); // Create inner scope ONCE
            while (1) {
                // 2. Check Condition
                Value c = eval_expr(scope, n->forstmt.cond);
                int truthy = is_truthy(c);
                value_free(c);

                if (!truthy) break; // Exit loop

                // 3. Execute Body
                for (int i = 0; i < n->forstmt.body.count; i++) {
                exec_stmt(inner_scope, n->forstmt.body.items[i]);
                    gc_safe_point();
                    if (return_exception.active || loop_exception.break_active || loop_exception.continue_active) break;
                }
                deferred_calls_run_scope(inner_scope);
                env_clear_locals(inner_scope);

                if (return_exception.active) break;
                if (loop_exception.break_active) {
                    loop_exception.break_active = 0;
                    break;
                }
                // (Continue is handled implicitly by going to the increment step)
                if (loop_exception.continue_active) {
                    loop_exception.continue_active = 0;
                }

                // 4. Run Increment
                exec_stmt(scope, n->forstmt.incr);
                gc_safe_point();
            }
            
            deferred_calls_run_scope(inner_scope);
            env_free(inner_scope); // Cleanup loop body scope
            deferred_calls_run_scope(scope);
            env_free(scope); // Cleanup loop variable 'i'
            return value_null();
        }
        case NODE_FOR_IN: {
            Value iterable = eval_expr(e, n->forin.iterable);
            Env *scope = env_create(e);
            long long count = 0;

            if (iterable.type == VAL_LIST && iterable.list) count = iterable.list->count;
            else if (iterable.type == VAL_DENSE_LIST && iterable.dlist) count = iterable.dlist->count;
            else if (iterable.type == VAL_STRING && iterable.string) count = (long long)strlen(iterable.string->chars);
            else {
                error_report_with_context(ERR_TYPE, n->line, 0,
                    "for-in expects a list, dense list, or string",
                    "Use for (let item in list) { ... }");
                value_free(iterable);
                env_free(scope);
                return value_null();
            }

            for (long long i = 0; i < count; i++) {
                Value item = value_null();
                if (iterable.type == VAL_LIST) item = value_copy(iterable.list->items[i]);
                else if (iterable.type == VAL_DENSE_LIST) item = value_float(iterable.dlist->data[i]);
                else item = value_char(iterable.string->chars[i]);

                env_clear_locals(scope);
                env_def_move(scope, n->forin.name, &item);

                for (int j = 0; j < n->forin.body.count; j++) {
                    exec_stmt(scope, n->forin.body.items[j]);
                    gc_safe_point();
                    if (return_exception.active || loop_exception.break_active || loop_exception.continue_active) break;
                }
                deferred_calls_run_scope(scope);
                if (return_exception.active) break;
                if (loop_exception.break_active) {
                    loop_exception.break_active = 0;
                    break;
                }
                if (loop_exception.continue_active) {
                    loop_exception.continue_active = 0;
                    continue;
                }
            }

            value_free(iterable);
            deferred_calls_run_scope(scope);
            env_free(scope);
            return value_null();
        }
        case NODE_SWITCH: {
            Value val = eval_expr(e, n->switchstmt.expr);
            int matched = 0;
            
            // Check all cases
            for (int i = 0; i < n->switchstmt.cases.count; i++) {
                AstNode *c = n->switchstmt.cases.items[i];
                Value cval = eval_expr(e, c->casestmt.value);
                int eq = 0;
                
                // Compare switch value with case value
                if (val.type == cval.type) {
                    if (val.type == VAL_INT) eq = (val.i == cval.i);
                    else if (val.type == VAL_FLOAT) eq = (val.f == cval.f);
                    else if (val.type == VAL_STRING && val.string && cval.string) eq = !strcmp(val.string->chars, cval.string->chars);
                    else if (val.type == VAL_BOOL) eq = (val.b == cval.b);
                    else if (val.type == VAL_CHAR) eq = (val.c == cval.c);
                } else if (val.type == VAL_INT && cval.type == VAL_FLOAT) eq = (val.i == cval.f);
                else if (val.type == VAL_FLOAT && cval.type == VAL_INT) eq = (val.f == cval.i);
                value_free(cval);

                if (eq) {
                    matched = 1;
                    Env *scope = env_create(e);
                    for (int j = 0; j < c->casestmt.body.count; j++) {
                        exec_stmt(scope, c->casestmt.body.items[j]);
                        gc_safe_point();
                        if (return_exception.active || loop_exception.continue_active) break;
                        if (loop_exception.break_active) break;
                    }
                    deferred_calls_run_scope(scope);
                    env_free(scope);
                    if (loop_exception.break_active) {
                        loop_exception.break_active = 0;
                    }
                    break;
                }
            }
            
            // Execute default block if no match found
            if (!matched && n->switchstmt.default_case.count > 0) {
                Env *scope = env_create(e);
                for (int j = 0; j < n->switchstmt.default_case.count; j++) {
                    exec_stmt(scope, n->switchstmt.default_case.items[j]);
                    gc_safe_point();
                    if (return_exception.active || loop_exception.continue_active) break;
                    if (loop_exception.break_active) break;
                }
                deferred_calls_run_scope(scope);
                env_free(scope);
                if (loop_exception.break_active) {
                    loop_exception.break_active = 0;
                }
            }
            value_free(val);
            return value_null();
        }
        case NODE_BLOCK: {
            Env *scope = env_create(e);
            for (int i = 0; i < n->block.items.count; i++) {
                exec_stmt(scope, n->block.items.items[i]);
                gc_safe_point();
                if (return_exception.active || loop_exception.break_active || loop_exception.continue_active) {
                    break;
                }
            }
            deferred_calls_run_scope(scope);
            env_free(scope);
            return value_null();
        }
        case NODE_GROUP: {
            // Execute statements in the CURRENT environment (e)
            for (int i = 0; i < n->block.items.count; i++) {
                exec_stmt(e, n->block.items.items[i]);
                gc_safe_point();
                
                // Still need to check for control flow (return/break)
                if (return_exception.active || loop_exception.break_active || loop_exception.continue_active) {
                    break;
                }
            }
            return value_null();
        }

        case NODE_IMPORT: {
            char *src = read_file(n->import_stmt.path);
            if (!src) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Could not import file '%s'", n->import_stmt.path);
                error_report_with_context(ERR_NAME, n->line, 0, msg,
                    "Check that the import path exists and ends with .lu");
                return value_null();
            }

            error_init(src, n->import_stmt.path);
            Parser parser;
            parser_init(&parser, src);
            AstNode *prog = parser_parse_program(&parser);
            parser_close(&parser);
            if (prog) {
                if (n->import_stmt.is_module_use) {
                    const char **exported_names = NULL;
                    int exported_count = 0;
                    collect_module_exports(prog, &exported_names, &exported_count);

                    Env *module_parent = env_root(e);
                    Env *module_env = env_create(module_parent);
                    if (prog->kind == NODE_BLOCK) {
                        for (int i = 0; i < prog->block.items.count; i++) {
                            exec_stmt(module_env, prog->block.items.items[i]);
                            gc_safe_point();
                            if (luna_had_error || return_exception.active) break;
                        }
                    } else {
                        exec_stmt(module_env, prog);
                        gc_safe_point();
                    }

                    if (!luna_had_error) {
                        const char **names = n->import_stmt.name_count > 0 ? n->import_stmt.names : exported_names;
                        int name_count = n->import_stmt.name_count > 0 ? n->import_stmt.name_count : exported_count;
                        for (int i = 0; i < name_count; i++) {
                            if (!module_name_requested(names[i], exported_names, exported_count)) {
                                char msg[256];
                                snprintf(msg, sizeof(msg), "Module '%s' does not export '%s'", n->import_stmt.path, names[i]);
                                error_report_with_context(ERR_NAME, n->line, 0, msg,
                                    "Export names explicitly in the module before using them");
                                break;
                            }
                            Value *slot = env_get_local(module_env, names[i]);
                            if (!slot) {
                                char msg[256];
                                snprintf(msg, sizeof(msg), "Export '%s' is missing from module '%s'", names[i], n->import_stmt.path);
                                error_report_with_context(ERR_NAME, n->line, 0, msg,
                                    "Make sure the exported value is defined at module top level");
                                break;
                            }
                            env_def(e, names[i], *slot);
                        }
                    }

                    deferred_calls_run_scope(module_env);
                    env_free(module_env);
                    free((void *)exported_names);
                } else if (prog->kind == NODE_BLOCK) {
                    for (int i = 0; i < prog->block.items.count; i++) {
                        exec_stmt(e, prog->block.items.items[i]);
                        gc_safe_point();
                        if (luna_had_error || return_exception.active) break;
                    }
                } else {
                    exec_stmt(e, prog);
                    gc_safe_point();
                }
                nodelist_free(&prog->block.items);
            }
            free(src);
            return value_null();
        }
        case NODE_UNSAFE: {
            if (!unsafe_runtime_begin_block(n->line)) {
                return value_null();
            }
            unsafe_block_depth++;
            Env *scope = env_create(e);
            for (int i = 0; i < n->unsafe_block.body.count; i++) {
                exec_stmt(scope, n->unsafe_block.body.items[i]);
                gc_safe_point();
                if (return_exception.active || loop_exception.break_active || loop_exception.continue_active || luna_had_error) {
                    break;
                }
            }
            deferred_calls_run_scope(scope);
            env_free(scope);
            unsafe_block_depth--;
            unsafe_runtime_end_block();
            return value_null();
        }
        
        case NODE_FUNC_DEF: {
            if (!n->funcdef.name) {
                Value ignored = make_closure(e, n);
                value_free(ignored);
                return value_null();
            }
            Value fn = make_closure(e, n);
            env_def_move(e, n->funcdef.name, &fn);
            return value_null();
        }

        case NODE_DATA_DEF: {
            Value dtype = value_data_type(n->data_def.name, n->data_def.fields, n->data_def.field_count,
                                          n->data_def.is_template);
            env_def_move(e, n->data_def.name, &dtype);
            return value_null();
        }
        case NODE_BLOC_DEF: {
            Value btype = value_bloc_type(n->bloc_def.name, n->bloc_def.fields, n->bloc_def.field_count);
            if (btype.type == VAL_NULL) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Bloc '%s' conflicts with an existing bloc definition", n->bloc_def.name);
                error_report_with_context(ERR_NAME, n->line, 0, msg,
                    "Keep the field list stable for a given bloc name");
                return value_null();
            }
            env_def_move(e, n->bloc_def.name, &btype);
            return value_null();
        }
            
        case NODE_RETURN: {
            // Tail-Call Optimization: if returning a call to the currently executing function,
            // don't actually recurse — stash the new args and let the caller loop.
            if (n->ret.expr && n->ret.expr->kind == NODE_CALL && current_executing_fn) {
                AstNode *call = n->ret.expr;
                // Check if this is a self-recursive call (same function name pointer)
                if (call->call.callee && call->call.callee->kind == NODE_IDENT &&
                    call->call.callee->ident.name == current_executing_fn->funcdef.name &&
                    call->call.args.count == current_executing_fn->funcdef.param_count) {
                    // Evaluate args in the CURRENT scope before rebinding
                    int argc = call->call.args.count;
                    Value *args = NULL;
                    if (argc > 0) {
                        args = malloc(sizeof(Value) * argc);
                        for (int i = 0; i < argc; i++) {
                            args[i] = eval_expr(e, call->call.args.items[i]);
                        }
                    }
                    // Set TCO marker — the NODE_CALL handler will pick this up
                    tco_pending.active = 1;
                    tco_pending.func = current_executing_fn;
                    tco_pending.args = args;
                    tco_pending.argc = argc;
                    
                    return_exception.active = 1;
                    return_exception.value = value_null();
                    return value_null();
                }
            }
            
            // Normal return: evaluate and set exception flag
            Value v = n->ret.expr ? eval_expr(e, n->ret.expr) : value_null();
            return_exception.active = 1;
            return_exception.value = v;
            return value_null();
        }
        
        case NODE_BREAK:
            loop_exception.break_active = 1;
            return value_null();
            
        case NODE_CONTINUE:
            loop_exception.continue_active = 1;
            return value_null();
            
        default: {
            // Evaluate standalone expressions (e.g., function calls without assignment)
            Value v = eval_expr(e, n);
            value_free(v);
            return value_null();
        }
    }
}

Value interpret(AstNode *prog, Env *env) {
    // Reset global flags to prevent state leaking between tests
    return_exception.active = 0;        
    loop_exception.break_active = 0;    
    loop_exception.continue_active = 0;
    tco_pending.active = 0;
    tco_pending.func = NULL;
    tco_pending.args = NULL;
    current_executing_fn = NULL;
    current_executing_env = NULL;
    unsafe_block_depth = 0;
    unsafe_runtime_reset();
    data_runtime_init();
    deferred_calls_reset();

    if (!prog) {
        return value_null();
    }
    
    // Use the passed environment directly
    if (prog->kind == NODE_BLOCK) {
        for (int i = 0; i < prog->block.items.count; i++) {
            exec_stmt(env, prog->block.items.items[i]);
            gc_safe_point();
        }
    } else {
        exec_stmt(env, prog);
        gc_safe_point();
    }

    // Auto-call main() if defined and takes no arguments
    const char *main_name = intern_string("main");
    AstNode *main_fn = env_get_func(env, main_name);
    if (main_fn && main_fn->funcdef.param_count == 0) {
        Env *scope = env_create(env);
        for (int i = 0; i < main_fn->funcdef.body.count; i++) {
            exec_stmt(scope, main_fn->funcdef.body.items[i]);
            gc_safe_point();
            if (return_exception.active) break;
        }
        if (return_exception.active) {
            value_free(return_exception.value);
            return_exception.active = 0;
        }
        deferred_calls_run_scope(scope);
        env_free(scope);
    }

    deferred_calls_run_scope(env);

    return value_null();
}

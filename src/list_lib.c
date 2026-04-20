// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list_lib.h"
#include "value.h"
#include "math_lib.h" // For math_internal_next()
#include "luna_error.h"
#include "env.h"
#include "arena.h"
#include "interpreter.h"

extern Arena *ast_arena;

// Threshold for switching from Merge Sort to Insertion Sort
#define SORT_THRESHOLD 16
#define RADIX_SORT_THRESHOLD 256

// Helper: Check if a < b for Luna Values
static int value_less_than(Value a, Value b) {
    if (a.type == VAL_INT && b.type == VAL_INT) return a.i < b.i;
    if (a.type == VAL_FLOAT && b.type == VAL_FLOAT) return a.f < b.f;
    if (a.type == VAL_INT && b.type == VAL_FLOAT) return (double)a.i < b.f;
    if (a.type == VAL_FLOAT && b.type == VAL_INT) return a.f < (double)b.i;
    if (a.type == VAL_STRING && b.type == VAL_STRING && a.string && b.string) return strcmp(a.string->chars, b.string->chars) < 0;
    return 0; 
}

static int value_equals(Value a, Value b) {
    if (a.type == b.type) {
        if (a.type == VAL_INT) return a.i == b.i;
        if (a.type == VAL_FLOAT) return a.f == b.f;
        if (a.type == VAL_BOOL) return a.b == b.b;
        if (a.type == VAL_CHAR) return a.c == b.c;
        if (a.type == VAL_STRING && a.string && b.string) return strcmp(a.string->chars, b.string->chars) == 0;
        if (a.type == VAL_NULL) return 1;
    }
    if (a.type == VAL_INT && b.type == VAL_FLOAT) return (double)a.i == b.f;
    if (a.type == VAL_FLOAT && b.type == VAL_INT) return a.f == (double)b.i;
    return 0;
}

static int list_all_ints(const ListObj *list) {
    if (!list) return 0;
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].type != VAL_INT) return 0;
    }
    return 1;
}

static void radix_sort_int_list(ListObj *list) {
    int n = list->count;
    if (n <= 1) return;

    Value *in = list->items;
    Value *tmp = malloc((size_t)n * sizeof(Value));
    if (!tmp) return;

    Value *out = tmp;

    for (int pass = 0; pass < 8; pass++) {
        size_t counts[256] = {0};
        size_t offsets[256];
        unsigned shift = (unsigned)pass * 8u;

        for (int i = 0; i < n; i++) {
            unsigned long long key = ((unsigned long long)in[i].i) ^ 0x8000000000000000ULL;
            unsigned byte = (unsigned)((key >> shift) & 0xFFu);
            counts[byte]++;
        }

        offsets[0] = 0;
        for (int b = 1; b < 256; b++) {
            offsets[b] = offsets[b - 1] + counts[b - 1];
        }

        for (int i = 0; i < n; i++) {
            unsigned long long key = ((unsigned long long)in[i].i) ^ 0x8000000000000000ULL;
            unsigned byte = (unsigned)((key >> shift) & 0xFFu);
            out[offsets[byte]++] = in[i];
        }

        Value *swap = in;
        in = out;
        out = swap;
    }

    if (in != list->items) {
        memcpy(list->items, in, (size_t)n * sizeof(Value));
    }

    free(tmp);
}

static int lib_is_truthy(Value v) {
    switch (v.type) {
        case VAL_BOOL: return v.b;
        case VAL_INT: return v.i != 0;
        case VAL_FLOAT: return v.f != 0.0;
        case VAL_STRING: return v.string && v.string->chars && v.string->chars[0] != '\0';
        case VAL_NULL: return 0;
        case VAL_LIST:
        case VAL_DENSE_LIST:
        case VAL_MAP:
        case VAL_TEMPLATE: return 1;
        case VAL_NATIVE:
        case VAL_CLOSURE:
        case VAL_FUNCTION: return 1;
        case VAL_CHAR: return v.c != 0;
        case VAL_FILE: return v.file != NULL;
        default: return 0;
    }
}

// Insertion Sort (Used for small sub-lists)
static void insertion_sort(Value *items, int left, int right) {
    for (int i = left + 1; i <= right; i++) {
        Value key = items[i];
        int j = i - 1;
        while (j >= left && value_less_than(key, items[j])) {
            items[j + 1] = items[j];
            j--;
        }
        items[j + 1] = key;
    }
}

// Merge — uses pre-allocated scratch buffer instead of malloc per call
static void merge(Value *items, int l, int m, int r, Value *scratch) {
    int n1 = m - l + 1;
    int n2 = r - m;

    // Copy left and right halves into scratch buffer
    Value *L = scratch;
    Value *R = scratch + n1;

    for (int i = 0; i < n1; i++) L[i] = items[l + i];
    for (int j = 0; j < n2; j++) R[j] = items[m + 1 + j];

    int i = 0, j = 0, k = l;
    while (i < n1 && j < n2) {
        if (value_less_than(L[i], R[j]) || (!value_less_than(R[j], L[i]))) {
            items[k++] = L[i++];
        } else {
            items[k++] = R[j++];
        }
    }

    while (i < n1) items[k++] = L[i++];
    while (j < n2) items[k++] = R[j++];
}

// Recursive Hybrid Sort Logic
static void hybrid_sort(Value *items, int l, int r, Value *scratch) {
    if (l < r) {
        // Switch to Insertion Sort for small segments
        if (r - l < SORT_THRESHOLD) {
            insertion_sort(items, l, r);
            return;
        }

        int m = l + (r - l) / 2;
        hybrid_sort(items, l, m, scratch);
        hybrid_sort(items, m + 1, r, scratch);
        merge(items, l, m, r, scratch);
    }
}

Value lib_list_sort(int argc, Value *argv, Env *env) {
    if (argc != 1 || argv[0].type != VAL_LIST) {
        error_report(ERR_ARGUMENT, 0, 0, "sort() expects 1 list", "Usage: sort(myList)");
        return value_null();
    }

    Value list = argv[0];
    if (list.list && list.list->count > 1) {
        int n = list.list->count;
        if (n >= RADIX_SORT_THRESHOLD && list_all_ints(list.list)) {
            radix_sort_int_list(list.list);
        } else {
            // Single scratch buffer for all merges — avoids malloc/free per merge
            Value *scratch = malloc((size_t)n * sizeof(Value));
            if (!scratch) return value_null();
            hybrid_sort(list.list->items, 0, n - 1, scratch);
            free(scratch);
        }
    }
    return value_null();
}

Value lib_list_ssort(int argc, Value *argv, Env *env) {
    if (argc != 1) {
        error_report(ERR_ARGUMENT, 0, 0, "ssort() expects 1 list", "Usage: ssort(myList)");
        return value_null();
    }

    if (argv[0].type == VAL_LIST) {
        Value list = argv[0];
        if (!list.list || list.list->count <= 1) return value_null();

        int write = 1;
        Value last = list.list->items[0];
        for (int read = 1; read < list.list->count; read++) {
            Value cur = list.list->items[read];
            if (!value_less_than(cur, last)) {
                if (write != read) list.list->items[write] = cur;
                last = cur;
                write++;
            } else {
                value_free(cur);
            }
        }
        list.list->count = write;
        return value_null();
    }

    if (argv[0].type == VAL_DENSE_LIST) {
        Value list = argv[0];
        if (!list.dlist || list.dlist->count <= 1) return value_null();

        int write = 1;
        double last = list.dlist->data[0];
        for (int read = 1; read < list.dlist->count; read++) {
            double cur = list.dlist->data[read];
            if (cur >= last) {
                if (write != read) list.dlist->data[write] = cur;
                last = cur;
                write++;
            }
        }
        list.dlist->count = write;
        return value_null();
    }

    error_report(ERR_ARGUMENT, 0, 0, "ssort() expects 1 list", "Usage: ssort(myList)");
    return value_null();
}

Value lib_list_append(int argc, Value *argv, Env *env) {
    if (argc != 2 || argv[0].type != VAL_LIST) {
        error_report(ERR_ARGUMENT, 0, 0, "list_append() expects a list and a value", "Usage: list_append(list, value)");
        return value_null();
    }
    
    value_list_append(&argv[0], argv[1]);
    return value_null();
}

Value lib_list_remove(int argc, Value *argv, Env *env) {
    if (argc != 2 || argv[0].type != VAL_LIST || argv[1].type != VAL_INT) {
        error_report(ERR_ARGUMENT, 0, 0, "remove() expects (list, index)", "Usage: remove(list, index)");
        return value_null();
    }

    ListObj *list = argv[0].list;
    long long idx = argv[1].i;
    if (!list) return value_null();
    if (idx < 0) idx += list->count;
    if (idx < 0 || idx >= list->count) {
        error_report(ERR_INDEX, 0, 0, "remove() index out of bounds", "Use an index between 0 and len(list)-1");
        return value_null();
    }

    Value removed = value_copy(list->items[idx]);
    value_free(list->items[idx]);
    for (int i = (int)idx; i < list->count - 1; i++) {
        list->items[i] = list->items[i + 1];
    }
    list->count--;
    return removed;
}

Value lib_list_find(int argc, Value *argv, Env *env) {
    if (argc != 2 || argv[0].type != VAL_LIST) {
        error_report(ERR_ARGUMENT, 0, 0, "find() expects (list, value)", "Usage: find(list, value)");
        return value_null();
    }

    ListObj *list = argv[0].list;
    if (!list) return value_int(-1);
    for (int i = 0; i < list->count; i++) {
        if (value_equals(list->items[i], argv[1])) {
            return value_int(i);
        }
    }
    return value_int(-1);
}

Value lib_list_map(int argc, Value *argv, Env *env) {
    if (argc != 2 || argv[0].type != VAL_LIST) {
        error_report(ERR_ARGUMENT, 0, 0, "map() expects (list, func)", "Usage: map(list, func(x) { ... })");
        return value_null();
    }
    if (argv[1].type != VAL_CLOSURE && argv[1].type != VAL_FUNCTION && argv[1].type != VAL_NATIVE) {
        error_report(ERR_ARGUMENT, 0, 0, "map() expects a callable second argument", "Pass a function value as the mapper");
        return value_null();
    }

    Value result = value_list();
    ListObj *list = argv[0].list;
    if (!list) return result;
    for (int i = 0; i < list->count; i++) {
        Value args[1];
        args[0] = value_copy(list->items[i]);
        Value mapped = luna_call_value(env, argv[1], 1, args, 0);
        value_free(args[0]);
        value_list_append_move(&result, &mapped);
    }
    return result;
}

Value lib_list_filter(int argc, Value *argv, Env *env) {
    if (argc != 2 || argv[0].type != VAL_LIST) {
        error_report(ERR_ARGUMENT, 0, 0, "filter() expects (list, func)", "Usage: filter(list, func(x) { ... })");
        return value_null();
    }
    if (argv[1].type != VAL_CLOSURE && argv[1].type != VAL_FUNCTION && argv[1].type != VAL_NATIVE) {
        error_report(ERR_ARGUMENT, 0, 0, "filter() expects a callable second argument", "Pass a function value as the predicate");
        return value_null();
    }

    Value result = value_list();
    ListObj *list = argv[0].list;
    if (!list) return result;
    for (int i = 0; i < list->count; i++) {
        Value args[1];
        args[0] = value_copy(list->items[i]);
        Value keep = luna_call_value(env, argv[1], 1, args, 0);
        value_free(args[0]);
        if (lib_is_truthy(keep)) {
            value_list_append(&result, list->items[i]);
        }
        value_free(keep);
    }
    return result;
}

Value lib_list_reduce(int argc, Value *argv, Env *env) {
    if (argc != 3 || argv[0].type != VAL_LIST) {
        error_report(ERR_ARGUMENT, 0, 0, "reduce() expects (list, func, init)", "Usage: reduce(list, func(acc, x) { ... }, init)");
        return value_null();
    }
    if (argv[1].type != VAL_CLOSURE && argv[1].type != VAL_FUNCTION && argv[1].type != VAL_NATIVE) {
        error_report(ERR_ARGUMENT, 0, 0, "reduce() expects a callable second argument", "Pass a function value as the reducer");
        return value_null();
    }

    Value acc = value_copy(argv[2]);
    ListObj *list = argv[0].list;
    if (!list) return acc;
    for (int i = 0; i < list->count; i++) {
        Value args[2];
        args[0] = value_copy(acc);
        args[1] = value_copy(list->items[i]);
        Value next = luna_call_value(env, argv[1], 2, args, 0);
        value_free(args[0]);
        value_free(args[1]);
        value_free(acc);
        acc = next;
    }
    return acc;
}

// Fisher-Yates Shuffle Implementation
//It should work now I suppose
Value lib_list_shuffle(int argc, Value *argv, Env *env) {
    if (argc != 1 || argv[0].type != VAL_LIST) {
        error_report(ERR_ARGUMENT, 0, 0, "shuffle() expects 1 list", "Usage: shuffle(myList)");
        return value_null();
    }

    Value list = argv[0];
    if (!list.list) return value_null();
    int n = list.list->count;
    if (n <= 1) return value_null();

    for (int i = n - 1; i > 0; i--) {
        // Pick random index using xoroshiro128++ engine
        int j = (int)(math_internal_next() % (i + 1));
        
        // Swap
        Value temp = list.list->items[i];
        list.list->items[i] = list.list->items[j];
        list.list->items[j] = temp;
    }
    
    return value_null();
}

// Generate a pre-flattened contiguous C-Array for Zero-Copy Math Operations
Value lib_dense_list(int argc, Value *argv, Env *env) {
    if (argc != 2 || argv[0].type != VAL_INT || argv[1].type != VAL_FLOAT) {
        error_report(ERR_ARGUMENT, 0, 0, "dense_list() expects (size: int, fill: float)", "Usage: dense_list(100, 1.5)");
        return value_null();
    }

    int size = argv[0].i;
    double fill_value = argv[1].f;

    if (size <= 0) return value_dense_list();

    Value res = value_dense_list();
    res.dlist->count = size;
    res.dlist->capacity = size;
    res.dlist->data = malloc(sizeof(double) * size);

    for (int i = 0; i < size; i++) {
        res.dlist->data[i] = fill_value;
    }

    return res;
}
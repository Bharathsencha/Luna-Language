// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#include <stdio.h>
#include <stdlib.h>
#include "value.h"
#include "vec_lib.h" 

// Define the ASM function pointer type
typedef void (*VecOp)(long long count, double *a, double *b, double *out);

// Declare external ASM functions
extern void vec_add_asm(long long, double*, double*, double*);
extern void vec_sub_asm(long long, double*, double*, double*);
extern void vec_mul_asm(long long, double*, double*, double*);
extern void vec_div_asm(long long, double*, double*, double*);

// Helper to extract double
static double get_val(Value v) {
    if (v.type == VAL_INT) return (double)v.i;
    if (v.type == VAL_FLOAT) return v.f;
    return 0.0;
}

// CORE LOGIC

// Generic handler that takes Values directly
static Value vec_op_direct(Value list_a, Value list_b, VecOp op) {
    // Safety check: both must be lists
    // Updated to use IS_OBJ and OBJ_LIST check
    if (!IS_OBJ(list_a) || AS_OBJ(list_a)->type != OBJ_LIST ||
        !IS_OBJ(list_b) || AS_OBJ(list_b)->type != OBJ_LIST) {
        return value_null();
    }
// Use AS_LIST macro to access internal data
    ObjList *la = AS_LIST(list_a);
    ObjList *lb = AS_LIST(list_b);

    int count = la->count < lb->count ? la->count : lb->count;
    if (count == 0) return value_list();

    // Allocate & Pack
    double *raw_a = malloc(sizeof(double) * count);
    double *raw_b = malloc(sizeof(double) * count);
    double *raw_out = malloc(sizeof(double) * count);

    for (int i = 0; i < count; i++) {
        raw_a[i] = get_val(la->items[i]);
        raw_b[i] = get_val(lb->items[i]);
    }

    // Call ASM
    op(count, raw_a, raw_b, raw_out);

    // Unpack
    Value res_list = value_list();
    for (int i = 0; i < count; i++) {
        value_list_append(&res_list, value_float(raw_out[i]));
    }

    free(raw_a);
    free(raw_b);
    free(raw_out);

    return res_list;
}

// Exposed Direct Functions for Interpreter
Value vec_add_values(Value a, Value b) { return vec_op_direct(a, b, vec_add_asm); }
Value vec_sub_values(Value a, Value b) { return vec_op_direct(a, b, vec_sub_asm); }
Value vec_mul_values(Value a, Value b) { return vec_op_direct(a, b, vec_mul_asm); }
Value vec_div_values(Value a, Value b) { return vec_op_direct(a, b, vec_div_asm); }

// NATIVE WRAPPERS (Callable from Luna Scripts)\

static Value vec_generic_wrapper(int argc, Value *argv, Value (*func)(Value, Value), const char *name) {
    if (argc != 2) {
        printf("Error: %s expects 2 lists\n", name);
        return value_null();
    }
    return func(argv[0], argv[1]);
}

Value lib_vec_add(int argc, Value *argv) { return vec_generic_wrapper(argc, argv, vec_add_values, "vec_add"); }
Value lib_vec_sub(int argc, Value *argv) { return vec_generic_wrapper(argc, argv, vec_sub_values, "vec_sub"); }
Value lib_vec_mul(int argc, Value *argv) { return vec_generic_wrapper(argc, argv, vec_mul_values, "vec_mul"); }
Value lib_vec_div(int argc, Value *argv) { return vec_generic_wrapper(argc, argv, vec_div_values, "vec_div"); }
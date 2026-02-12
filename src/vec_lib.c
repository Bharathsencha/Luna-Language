// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

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

// Internal helper to get a raw double array from either standard or dense lists
static double* get_raw_buffer(Value v, int *count) {
    if (v.type == VAL_DENSE_LIST) {
        *count = v.dlist.count;
        return v.dlist.data;
    }
    if (v.type == VAL_LIST) {
        *count = v.list.count;
        double *buf = malloc(sizeof(double) * (*count));
        for (int i = 0; i < *count; i++) {
            buf[i] = get_val(v.list.items[i]);
        }
        return buf;
    }
    return NULL;
}

// CORE LOGIC

// Generic handler that takes Values directly
static Value vec_op_direct(Value list_a, Value list_b, VecOp op) {
    // Fast path for Dense Lists (Zero-copy)
    if (list_a.type == VAL_DENSE_LIST && list_b.type == VAL_DENSE_LIST) {
        int count = list_a.dlist.count < list_b.dlist.count ? list_a.dlist.count : list_b.dlist.count;
        if (count == 0) return value_dense_list();

        Value res = value_dense_list();
        res.dlist.data = malloc(sizeof(double) * count);
        res.dlist.count = count;
        res.dlist.capacity = count;

        // Call ASM directly on existing buffers
        op(count, list_a.dlist.data, list_b.dlist.data, res.dlist.data);
        return res;
    }

    // Safety check: both must be lists
    if (list_a.type != VAL_LIST || list_b.type != VAL_LIST) {
        return value_null();
    }

    int count = list_a.list.count < list_b.list.count ? list_a.list.count : list_b.list.count;
    if (count == 0) return value_list();

    // Allocate & Pack
    double *raw_a = malloc(sizeof(double) * count);
    double *raw_b = malloc(sizeof(double) * count);
    double *raw_out = malloc(sizeof(double) * count);

    for (int i = 0; i < count; i++) {
        raw_a[i] = get_val(list_a.list.items[i]);
        raw_b[i] = get_val(list_b.list.items[i]);
    }

    // Call ASM
    op(count, raw_a, raw_b, raw_out);

    // Unpack - Changed to Dense List for better downstream performance
    Value res = value_dense_list();
    res.dlist.data = raw_out;
    res.dlist.count = count;
    res.dlist.capacity = count;

    free(raw_a);
    free(raw_b);
    // raw_out is now managed by 'res'

    return res;
}

// Exposed Direct Functions for Interpreter
Value vec_add_values(Value a, Value b) { return vec_op_direct(a, b, vec_add_asm); }
Value vec_sub_values(Value a, Value b) { return vec_op_direct(a, b, vec_sub_asm); }
Value vec_mul_values(Value a, Value b) { return vec_op_direct(a, b, vec_mul_asm); }
Value vec_div_values(Value a, Value b) { return vec_op_direct(a, b, vec_div_asm); }

// Matrix Multiplication
Value lib_mat_mul(int argc, Value *argv) {
    if (argc != 2) return value_null();
    Value A = argv[0];
    Value B = argv[1];

    // Matrices are represented as List of Lists (Standard or Dense)
    if ((A.type != VAL_LIST && A.type != VAL_DENSE_LIST) || 
        (B.type != VAL_LIST && B.type != VAL_DENSE_LIST)) return value_null();

    int rows_a = (A.type == VAL_LIST) ? A.list.count : 1;
    int rows_b = (B.type == VAL_LIST) ? B.list.count : 1;
    
    if (rows_a == 0 || rows_b == 0) return value_list();

    // Determine cols_a from the first row
    int cols_a = 0;
    Value first_row_a = (A.type == VAL_LIST) ? A.list.items[0] : A;
    if (first_row_a.type == VAL_LIST) cols_a = first_row_a.list.count;
    else if (first_row_a.type == VAL_DENSE_LIST) cols_a = first_row_a.dlist.count;

    // Determine cols_b from the first row of B
    int cols_b = 0;
    Value first_row_b = (B.type == VAL_LIST) ? B.list.items[0] : B;
    if (first_row_b.type == VAL_LIST) cols_b = first_row_b.list.count;
    else if (first_row_b.type == VAL_DENSE_LIST) cols_b = first_row_b.dlist.count;

    // Check inner dimensions: A[m x n] * B[n x p] -> Res[m x p]
    if (cols_a != rows_b || cols_a == 0) {
        printf("Runtime Error: Matrix dimension mismatch (%d cols vs %d rows)\n", cols_a, rows_b);
        return value_null();
    }

    Value res = value_list();
    for (int i = 0; i < rows_a; i++) {
        Value row = value_dense_list();
        row.dlist.data = (double*)calloc(cols_b, sizeof(double)); // Zero initialized
        row.dlist.count = cols_b;
        row.dlist.capacity = cols_b;

        int a_len;
        Value row_a_val = (A.type == VAL_LIST) ? A.list.items[i] : A;
        double *a_row_ptr = get_raw_buffer(row_a_val, &a_len);
        
        // Cache-friendly (i, k, j) loop order
        for (int k = 0; k < cols_a; k++) {
            double a_val = a_row_ptr[k];
            
            int b_len;
            Value row_b_val = (B.type == VAL_LIST) ? B.list.items[k] : B;
            double *b_row_ptr = get_raw_buffer(row_b_val, &b_len);
            
            double *out_row_ptr = row.dlist.data;
            for (int j = 0; j < cols_b; j++) {
                out_row_ptr[j] += a_val * b_row_ptr[j];
            }

            // Cleanup temporary buffers promoted from standard lists
            if (row_b_val.type == VAL_LIST) free(b_row_ptr);
        }
        if (row_a_val.type == VAL_LIST) free(a_row_ptr);

        value_list_append(&res, row);
        value_free(row);
    }
    return res;
}

// NATIVE WRAPPERS (Callable from Luna Scripts)

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
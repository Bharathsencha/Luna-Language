// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "value.h"
#include "vec_lib.h" 
#include "env.h"
#include "arena.h"

extern Arena *ast_arena;

// Define the ASM function pointer type (retained signature for compatibility)
typedef void (*VecOp)(long long count, double *a, double *b, double *out);

#include <immintrin.h>

// Explicit AVX2 Intrinsics for single-threaded maximal throughput
#define IMPL_VEC_OP_AVX2(name, op_intrin, scalar_op) \
static void name(long long count, double *a, double *b, double *out) { \
    long long i = 0; \
    long long vec_limit = count - (count % 4); \
    for (; i < vec_limit; i += 4) { \
        __m256d va = _mm256_loadu_pd(&a[i]); \
        __m256d vb = _mm256_loadu_pd(&b[i]); \
        __m256d vout = op_intrin(va, vb); \
        _mm256_storeu_pd(&out[i], vout); \
    } \
    for (; i < count; i++) { \
        out[i] = a[i] scalar_op b[i]; \
    } \
}

// Instantiate the AVX2 C functions
IMPL_VEC_OP_AVX2(vec_add_c, _mm256_add_pd, +)
IMPL_VEC_OP_AVX2(vec_sub_c, _mm256_sub_pd, -)
IMPL_VEC_OP_AVX2(vec_mul_c, _mm256_mul_pd, *)
IMPL_VEC_OP_AVX2(vec_div_c, _mm256_div_pd, /)

// Helper to extract double
static double get_val(Value v) {
    if (v.type == VAL_INT) return (double)v.i;
    if (v.type == VAL_FLOAT) return v.f;
    return 0.0;
}

// Internal helper to get a raw double array from either standard or dense lists
static double* get_raw_buffer(Value v, int *count) {
    if (v.type == VAL_DENSE_LIST && v.dlist) {
        *count = v.dlist->count;
        return v.dlist->data;
    }
    if (v.type == VAL_LIST && v.list) {
        *count = v.list->count;
        double *buf = malloc(sizeof(double) * (*count));
        for (int i = 0; i < *count; i++) {
            buf[i] = get_val(v.list->items[i]);
        }
        return buf;
    }
    return NULL;
}

// CORE LOGIC

// Generic handler that takes Values directly
static Value vec_op_direct(Value list_a, Value list_b, VecOp op) {
    // Fast path for Dense Lists (Zero-copy)
    if (list_a.type == VAL_DENSE_LIST && list_b.type == VAL_DENSE_LIST && list_a.dlist && list_b.dlist) {
        int count = list_a.dlist->count < list_b.dlist->count ? list_a.dlist->count : list_b.dlist->count;
        if (count == 0) return value_dense_list();

        Value res = value_dense_list();
        // The resulting structure survives to the interpreter heap, so use standard malloc
        res.dlist->data = arena_alloc(ast_arena, sizeof(double) * count);
        res.dlist->count = count;
        res.dlist->capacity = count;

        // Call ASM directly on existing buffers
        op(count, list_a.dlist->data, list_b.dlist->data, res.dlist->data);
        return res;
    }

    // Safety check: both must be lists
    if (list_a.type != VAL_LIST || list_b.type != VAL_LIST) {
        return value_null();
    }

    int count = list_a.list->count < list_b.list->count ? list_a.list->count : list_b.list->count;
    if (count == 0) return value_list();

    // Allocate & Pack using ultra-fast AST Arena instead of OS malloc
    double *raw_a = arena_alloc(ast_arena, sizeof(double) * count);
    double *raw_b = arena_alloc(ast_arena, sizeof(double) * count);
    
    // The output buffer is going returned to the engine via Dense List, so use malloc
    double *raw_out = malloc(sizeof(double) * count);

    for (int i = 0; i < count; i++) {
        raw_a[i] = get_val(list_a.list->items[i]);
        raw_b[i] = get_val(list_b.list->items[i]);
    }

    // Call ASM macro
    op(count, raw_a, raw_b, raw_out);

    // Unpack - Changed to Dense List for better downstream performance
    Value res = value_dense_list();
    res.dlist->data = raw_out;
    res.dlist->count = count;
    res.dlist->capacity = count;

    // raw_a and raw_b are automatically bulk deallocated by ast_arena at statement end
    // raw_out is now managed by 'res'

    return res;
}

// Exposed Direct Functions for Interpreter
Value vec_add_values(Value a, Value b) { return vec_op_direct(a, b, vec_add_c); }
Value vec_sub_values(Value a, Value b) { return vec_op_direct(a, b, vec_sub_c); }
Value vec_mul_values(Value a, Value b) { return vec_op_direct(a, b, vec_mul_c); }
Value vec_div_values(Value a, Value b) { return vec_op_direct(a, b, vec_div_c); }

// Matrix Multiplication

Value lib_mat_mul(int argc, Value *argv, Env *env) {
    if (argc != 2) return value_null();
    Value A = argv[0];
    Value B = argv[1];

    if ((A.type != VAL_LIST && A.type != VAL_DENSE_LIST) || 
        (B.type != VAL_LIST && B.type != VAL_DENSE_LIST)) return value_null();

    int rows_a = (A.type == VAL_LIST && A.list) ? A.list->count : 1;
    int rows_b = (B.type == VAL_LIST && B.list) ? B.list->count : 1;
    if (rows_a == 0 || rows_b == 0) return value_list();

    int cols_a = 0;
    Value first_row_a = (A.type == VAL_LIST && A.list && A.list->count > 0) ? A.list->items[0] : A;
    if (first_row_a.type == VAL_LIST && first_row_a.list) cols_a = first_row_a.list->count;
    else if (first_row_a.type == VAL_DENSE_LIST && first_row_a.dlist) cols_a = first_row_a.dlist->count;

    int cols_b = 0;
    Value first_row_b = (B.type == VAL_LIST && B.list && B.list->count > 0) ? B.list->items[0] : B;
    if (first_row_b.type == VAL_LIST && first_row_b.list) cols_b = first_row_b.list->count;
    else if (first_row_b.type == VAL_DENSE_LIST && first_row_b.dlist) cols_b = first_row_b.dlist->count;

    if (cols_a != rows_b || cols_a == 0) {
        printf("Runtime Error: Matrix dimension mismatch (%d cols vs %d rows)\n", cols_a, rows_b);
        return value_null();
    }

    // Pre-flatten Matrix A using AST Arena pointer bump
    double *flat_A = arena_alloc(ast_arena, rows_a * cols_a * sizeof(double));
    for (int i = 0; i < rows_a; i++) {
        int a_len;
        Value row_a_val = (A.type == VAL_LIST && A.list) ? A.list->items[i] : A;
        double *a_row_ptr = get_raw_buffer(row_a_val, &a_len);
        for (int j = 0; j < cols_a; j++) flat_A[i * cols_a + j] = a_row_ptr[j];
        if (row_a_val.type == VAL_LIST) free(a_row_ptr); // free works here because get_raw_buffer uses malloc for standard lists
    }

    // Pre-flatten Matrix B using AST Arena pointer bump
    double *flat_B = arena_alloc(ast_arena, rows_b * cols_b * sizeof(double));
    for (int i = 0; i < rows_b; i++) {
        int b_len;
        Value row_b_val = (B.type == VAL_LIST && B.list) ? B.list->items[i] : B;
        double *b_row_ptr = get_raw_buffer(row_b_val, &b_len);
        for (int j = 0; j < cols_b; j++) flat_B[i * cols_b + j] = b_row_ptr[j];
        if (row_b_val.type == VAL_LIST) free(b_row_ptr);
    }

    // Allocate flat result matrix using arena, and zero initialize it
    double *flat_C = arena_alloc(ast_arena, rows_a * cols_b * sizeof(double));
    memset(flat_C, 0, rows_a * cols_b * sizeof(double));

    // Pure C optimized inner loop - Auto-vectorized by GCC
    #pragma omp parallel for
    for (int i = 0; i < rows_a; i++) {
        for (int k = 0; k < cols_a; k++) {
            double a_val = flat_A[i * cols_a + k];
            for (int j = 0; j < cols_b; j++) {
                flat_C[i * cols_b + j] += a_val * flat_B[k * cols_b + j];
            }
        }
    }

    // Repack into Luna Value lists
    Value res = value_list();
    for (int i = 0; i < rows_a; i++) {
        Value row = value_dense_list();
        // Since dense lists are long-lived structures, leave them on standard heap
        // If they were on the Arena, they'd be wiped during the next interpreter teardown.
        row.dlist->data = malloc(cols_b * sizeof(double));
        row.dlist->count = cols_b;
        row.dlist->capacity = cols_b;
        for (int j = 0; j < cols_b; j++) {
            row.dlist->data[j] = flat_C[i * cols_b + j];
        }
        value_list_append(&res, row);
        value_free(row);
    }
    return res;
}

// NATIVE WRAPPERS (Callable from Luna Scripts)

static Value vec_generic_wrapper(int argc, Value *argv, Value (*func)(Value, Value), const char *name, Env *env) {
    if (argc != 2) {
        printf("Error: %s expects 2 lists\n", name);
        return value_null();
    }
    return func(argv[0], argv[1]);
}

Value lib_vec_add(int argc, Value *argv, Env *env) { return vec_generic_wrapper(argc, argv, vec_add_values, "vec_add", env); }
Value lib_vec_sub(int argc, Value *argv, Env *env) { return vec_generic_wrapper(argc, argv, vec_sub_values, "vec_sub", env); }
Value lib_vec_mul(int argc, Value *argv, Env *env) { return vec_generic_wrapper(argc, argv, vec_mul_values, "vec_mul", env); }
Value lib_vec_div(int argc, Value *argv, Env *env) { return vec_generic_wrapper(argc, argv, vec_div_values, "vec_div", env); }

// In-place vector multiplication (A = A * B)
Value lib_vec_mul_inline(int argc, Value *argv, Env *env) {
    if (argc != 2) {
        printf("Error: vec_mul_inline expects 2 lists\n");
        return value_null();
    }
    Value list_a = argv[0];
    Value list_b = argv[1];
    
    // Only support dense lists for ultra-fast in-place math
    if (list_a.type == VAL_DENSE_LIST && list_b.type == VAL_DENSE_LIST && list_a.dlist && list_b.dlist) {
        int count = list_a.dlist->count < list_b.dlist->count ? list_a.dlist->count : list_b.dlist->count;
        if (count == 0) return value_null();
        
        // Output directly into list_a (in-place)
        vec_mul_c(count, list_a.dlist->data, list_b.dlist->data, list_a.dlist->data);
    } else {
        printf("Error: vec_mul_inline requires dense lists for A and B.\n");
    }
    
    return value_null(); // Mutates in place, returns null
}
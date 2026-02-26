// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef VEC_LIB_H
#define VEC_LIB_H

#include "value.h"

// Forward declaration
struct Env;

// Exposed logic for internal interpreter use (SIMD operations)
Value vec_add_values(Value a, Value b);
Value vec_sub_values(Value a, Value b);
Value vec_mul_values(Value a, Value b);
Value vec_div_values(Value a, Value b);

// Native Wrappers for Luna Scripts
Value lib_vec_add(int argc, Value *argv, struct Env *env);
Value lib_vec_sub(int argc, Value *argv, struct Env *env);
Value lib_vec_mul(int argc, Value *argv, struct Env *env);
Value lib_vec_div(int argc, Value *argv, struct Env *env);
Value lib_vec_mul_inline(int argc, Value *argv, struct Env *env);
Value lib_mat_mul(int argc, Value *argv, struct Env *env); 

#endif
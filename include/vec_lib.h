// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#ifndef VEC_LIB_H
#define VEC_LIB_H

#include "value.h"

// Direct C-callable functions (for Operator Overloading)
Value vec_add_values(Value a, Value b);
Value vec_sub_values(Value a, Value b);
Value vec_mul_values(Value a, Value b);
Value vec_div_values(Value a, Value b);

// Native Wrappers (for manual function calls)
Value lib_vec_add(int argc, Value *argv);
Value lib_vec_sub(int argc, Value *argv);
Value lib_vec_mul(int argc, Value *argv);
Value lib_vec_div(int argc, Value *argv);
Value lib_mat_mul(int argc, Value *argv); // Prototype for native matrix multiplication

#endif
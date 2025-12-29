// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#ifndef MATH_LIB_H
#define MATH_LIB_H

#include <stdint.h>
#include "value.h"

// Internal helper for list_lib to access the random engine
uint64_t math_internal_next(void);

// Basic Arithmetic & Utility
Value lib_math_abs(int argc, Value *argv);
Value lib_math_min(int argc, Value *argv);
Value lib_math_max(int argc, Value *argv);
Value lib_math_clamp(int argc, Value *argv);// No idea how this works tbh
Value lib_math_sign(int argc, Value *argv); // Returns -1, 0, 1

// Powers & Roots
Value lib_math_pow(int argc, Value *argv);
Value lib_math_sqrt(int argc, Value *argv);
Value lib_math_cbrt(int argc, Value *argv); // Cube root
Value lib_math_exp(int argc, Value *argv);
Value lib_math_ln(int argc, Value *argv);
Value lib_math_log10(int argc, Value *argv);

// Trigonometry (Radians) 
Value lib_math_sin(int argc, Value *argv);
Value lib_math_cos(int argc, Value *argv);
Value lib_math_tan(int argc, Value *argv);
Value lib_math_asin(int argc, Value *argv);
Value lib_math_acos(int argc, Value *argv);
Value lib_math_atan(int argc, Value *argv);
Value lib_math_atan2(int argc, Value *argv);

// Hyperbolic Functions
Value lib_math_sinh(int argc, Value *argv);
Value lib_math_cosh(int argc, Value *argv);
Value lib_math_tanh(int argc, Value *argv);

// Rounding
Value lib_math_floor(int argc, Value *argv);
Value lib_math_ceil(int argc, Value *argv);
Value lib_math_round(int argc, Value *argv);
Value lib_math_trunc(int argc, Value *argv);
Value lib_math_fract(int argc, Value *argv); // Returns fractional part
Value lib_math_mod(int argc, Value *argv);

//  Random (Updated for xoroshiro128++ and unified dispatcher)
Value lib_math_rand(int argc, Value *argv);
Value lib_math_srand(int argc, Value *argv);
Value lib_math_trand(int argc, Value *argv); // True randomness via OS

// Conversions
Value lib_math_deg_to_rad(int argc, Value *argv);
Value lib_math_rad_to_deg(int argc, Value *argv);
Value lib_math_lerp(int argc, Value *argv);

#endif
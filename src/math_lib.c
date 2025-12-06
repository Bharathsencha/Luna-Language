// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "math_lib.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper: Extract double from Value safely
static double val_to_double(Value v) {
    if (v.type == VAL_INT) return (double)v.i;
    if (v.type == VAL_FLOAT) return v.f;
    return 0.0; // Default or error value
}

// Helper: Check arg count
static int check_args(int argc, int expected, const char *name) {
    if (argc != expected) {
        fprintf(stderr, "Runtime Error: %s() takes %d arguments.\n", name, expected);
        return 0;
    }
    return 1;
}

// Basic Utilities

Value lib_math_abs(int argc, Value *argv) {
    if (!check_args(argc, 1, "abs")) return value_null();
    
    Value v = argv[0];
    if (v.type == VAL_INT) {
        return value_int(v.i < 0 ? -v.i : v.i);
    } else if (v.type == VAL_FLOAT) {
        return value_float(fabs(v.f));
    }
    return value_null();
}

Value lib_math_min(int argc, Value *argv) {
    if (!check_args(argc, 2, "min")) return value_null();
    double a = val_to_double(argv[0]);
    double b = val_to_double(argv[1]);
    
    // Return appropriate type
    if (argv[0].type == VAL_INT && argv[1].type == VAL_INT)
        return value_int(a < b ? (long long)a : (long long)b);
    else
        return value_float(a < b ? a : b);
}

Value lib_math_max(int argc, Value *argv) {
    if (!check_args(argc, 2, "max")) return value_null();
    double a = val_to_double(argv[0]);
    double b = val_to_double(argv[1]);
    
    if (argv[0].type == VAL_INT && argv[1].type == VAL_INT)
        return value_int(a > b ? (long long)a : (long long)b);
    else
        return value_float(a > b ? a : b);
}

Value lib_math_clamp(int argc, Value *argv) {
    if (!check_args(argc, 3, "clamp")) return value_null();
    double x = val_to_double(argv[0]);
    double min = val_to_double(argv[1]);
    double max = val_to_double(argv[2]);

    double res = (x < min) ? min : ((x > max) ? max : x);

    // If all inputs are ints, return int
    if (argv[0].type == VAL_INT && argv[1].type == VAL_INT && argv[2].type == VAL_INT) {
        return value_int((long long)res);
    }
    return value_float(res);
}

Value lib_math_sign(int argc, Value *argv) {
    if (!check_args(argc, 1, "sign")) return value_null();
    double x = val_to_double(argv[0]);
    if (x > 0) return value_int(1);
    if (x < 0) return value_int(-1);
    return value_int(0);
}

// Powers & Roots

Value lib_math_pow(int argc, Value *argv) {
    if (!check_args(argc, 2, "pow")) return value_null();
    return value_float(pow(val_to_double(argv[0]), val_to_double(argv[1])));
}

Value lib_math_sqrt(int argc, Value *argv) {
    if (!check_args(argc, 1, "sqrt")) return value_null();
    return value_float(sqrt(val_to_double(argv[0])));
}

Value lib_math_cbrt(int argc, Value *argv) {
    if (!check_args(argc, 1, "cbrt")) return value_null();
    return value_float(cbrt(val_to_double(argv[0])));
}

Value lib_math_exp(int argc, Value *argv) {
    if (!check_args(argc, 1, "exp")) return value_null();
    return value_float(exp(val_to_double(argv[0])));
}

Value lib_math_ln(int argc, Value *argv) {
    if (!check_args(argc, 1, "ln")) return value_null();
    return value_float(log(val_to_double(argv[0])));
}

Value lib_math_log10(int argc, Value *argv) {
    if (!check_args(argc, 1, "log10")) return value_null();
    return value_float(log10(val_to_double(argv[0])));
}

// Trigonometry ~~~

Value lib_math_sin(int argc, Value *argv) {
    if (!check_args(argc, 1, "sin")) return value_null();
    return value_float(sin(val_to_double(argv[0])));
}

Value lib_math_cos(int argc, Value *argv) {
    if (!check_args(argc, 1, "cos")) return value_null();
    return value_float(cos(val_to_double(argv[0])));
}

Value lib_math_tan(int argc, Value *argv) {
    if (!check_args(argc, 1, "tan")) return value_null();
    return value_float(tan(val_to_double(argv[0])));
}

Value lib_math_asin(int argc, Value *argv) {
    if (!check_args(argc, 1, "asin")) return value_null();
    return value_float(asin(val_to_double(argv[0])));
}

Value lib_math_acos(int argc, Value *argv) {
    if (!check_args(argc, 1, "acos")) return value_null();
    return value_float(acos(val_to_double(argv[0])));
}

Value lib_math_atan(int argc, Value *argv) {
    if (!check_args(argc, 1, "atan")) return value_null();
    return value_float(atan(val_to_double(argv[0])));
}

Value lib_math_atan2(int argc, Value *argv) {
    if (!check_args(argc, 2, "atan2")) return value_null();
    return value_float(atan2(val_to_double(argv[0]), val_to_double(argv[1])));
}

// Hyperbolic 

Value lib_math_sinh(int argc, Value *argv) {
    if (!check_args(argc, 1, "sinh")) return value_null();
    return value_float(sinh(val_to_double(argv[0])));
}

Value lib_math_cosh(int argc, Value *argv) {
    if (!check_args(argc, 1, "cosh")) return value_null();
    return value_float(cosh(val_to_double(argv[0])));
}

Value lib_math_tanh(int argc, Value *argv) {
    if (!check_args(argc, 1, "tanh")) return value_null();
    return value_float(tanh(val_to_double(argv[0])));
}

// Rounding 

Value lib_math_floor(int argc, Value *argv) {
    if (!check_args(argc, 1, "floor")) return value_null();
    return value_int((long long)floor(val_to_double(argv[0])));
}

Value lib_math_ceil(int argc, Value *argv) {
    if (!check_args(argc, 1, "ceil")) return value_null();
    return value_int((long long)ceil(val_to_double(argv[0])));
}

Value lib_math_round(int argc, Value *argv) {
    if (!check_args(argc, 1, "round")) return value_null();
    return value_int((long long)round(val_to_double(argv[0])));
}

Value lib_math_trunc(int argc, Value *argv) {
    if (!check_args(argc, 1, "trunc")) return value_null();
    return value_int((long long)trunc(val_to_double(argv[0])));
}

Value lib_math_fract(int argc, Value *argv) {
    if (!check_args(argc, 1, "fract")) return value_null();
    double d = val_to_double(argv[0]);
    double i;
    return value_float(modf(d, &i));
}

Value lib_math_mod(int argc, Value *argv) {
    if (!check_args(argc, 2, "mod")) return value_null();
    // fmod returns the floating-point remainder
    return value_float(fmod(val_to_double(argv[0]), val_to_double(argv[1])));
}

// Random-

Value lib_math_rand(int argc, Value *argv) {
    (void)argc; (void)argv; // Unused
    // Returns 0.0 to 1.0
    return value_float((double)rand() / (double)RAND_MAX);
}

Value lib_math_randint(int argc, Value *argv) {
    if (!check_args(argc, 2, "randint")) return value_null();
    long long min = (argv[0].type == VAL_INT) ? argv[0].i : (long long)argv[0].f;
    long long max = (argv[1].type == VAL_INT) ? argv[1].i : (long long)argv[1].f;
    
    if (min > max) { // Swap if wrong order
        long long t = min; min = max; max = t;
    }
    
    return value_int(min + rand() % (max - min + 1));
}

Value lib_math_srand(int argc, Value *argv) {
    if (!check_args(argc, 1, "srand")) return value_null();
    long long seed = 0;
    if (argv[0].type == VAL_INT) seed = argv[0].i;
    else seed = (long long)argv[0].f;
    
    srand((unsigned int)seed);
    return value_null();
}

// Extras

Value lib_math_deg_to_rad(int argc, Value *argv) {
    if (!check_args(argc, 1, "deg_to_rad")) return value_null();
    return value_float(val_to_double(argv[0]) * (M_PI / 180.0));
}

Value lib_math_rad_to_deg(int argc, Value *argv) {
    if (!check_args(argc, 1, "rad_to_deg")) return value_null();
    return value_float(val_to_double(argv[0]) * (180.0 / M_PI));
}

Value lib_math_lerp(int argc, Value *argv) {
    if (!check_args(argc, 3, "lerp")) return value_null();
    double a = val_to_double(argv[0]);
    double b = val_to_double(argv[1]);
    double t = val_to_double(argv[2]);
    
    // a + t * (b - a)
    return value_float(a + t * (b - a));
}
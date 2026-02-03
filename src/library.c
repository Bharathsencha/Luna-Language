// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

// This file connects the raw C library implementations to the Luna environment.
// It maps internal C functions (like lib_math_abs) to Luna function names (like "abs").
// It also defines utility native functions like 'assert'.

#include <stdio.h>
#include <stdlib.h>
#include "library.h"
#include "value.h"
#include "luna_error.h"
#include "math_lib.h"
#include "string_lib.h"
#include "time_lib.h"
#include "vec_lib.h"
#include "file_lib.h" 
#include "list_lib.h" // Added for sort and shuffle

// Helper: Local truthiness check for assert
// (This logic mirrors the interpreter's is_truthy to keep modules decoupled)
static int lib_is_truthy(Value v) {
    switch (v.type) {
        case VAL_BOOL:   return v.b;
        case VAL_INT:    return v.i != 0;
        case VAL_FLOAT:  return v.f != 0.0;
        // Updated: Access object-based strings and lists
        case VAL_OBJ: {
            Obj *obj = AS_OBJ(v);
            if (obj->type == OBJ_STRING) {
                char *s = AS_STRING(v)->chars;
                return s && s[0] != '\0';
            }
            if (obj->type == OBJ_LIST) return 1;
            return 0;
        }
        case VAL_NULL:   return 0;
        case VAL_NATIVE: return 1;
        case VAL_CHAR:   return v.c != 0;
        case VAL_FILE:   return v.file != NULL; // Files are truthy if open
        default:         return 0;
    }
}

// Native implementation of assert()
// We moved this here from interpreter.c to keep the core logic clean.
// We use exit(1) here to fulfill the "Force crash" requirement in test scripts.
static Value lib_assert(int argc, Value *argv) {
    if (argc != 1) {
        error_report(ERR_ARGUMENT, 0, 0,
            "assert() takes exactly 1 argument",
            "Use assert(condition) to verify logic.");
        exit(1);
    }
    
    if (!lib_is_truthy(argv[0])) {
        // Passing 0 here is fine; error.c will use luna_current_line
        error_report(ERR_ASSERTION, 0, 0,
            "Assertion failed",
            "The condition evaluated to false.");
        exit(1); // Exit here so that FAILED tests stop the process
    }
    return value_bool(1);
}

void env_register_stdlib(Env *env) {
    // Core Utilities 
    env_def(env, "assert", value_native(lib_assert));

    // Math Library
    env_def(env, "abs", value_native(lib_math_abs));
    env_def(env, "min", value_native(lib_math_min));
    env_def(env, "max", value_native(lib_math_max));
    env_def(env, "clamp", value_native(lib_math_clamp));
    env_def(env, "sign", value_native(lib_math_sign));
    
    env_def(env, "pow", value_native(lib_math_pow));
    env_def(env, "sqrt", value_native(lib_math_sqrt));
    env_def(env, "cbrt", value_native(lib_math_cbrt));
    env_def(env, "exp", value_native(lib_math_exp));
    env_def(env, "ln", value_native(lib_math_ln));
    env_def(env, "log10", value_native(lib_math_log10));
    
    env_def(env, "sin", value_native(lib_math_sin));
    env_def(env, "cos", value_native(lib_math_cos));
    env_def(env, "tan", value_native(lib_math_tan));
    env_def(env, "asin", value_native(lib_math_asin));
    env_def(env, "acos", value_native(lib_math_acos));
    env_def(env, "atan", value_native(lib_math_atan));
    env_def(env, "atan2", value_native(lib_math_atan2));
    
    env_def(env, "sinh", value_native(lib_math_sinh));
    env_def(env, "cosh", value_native(lib_math_cosh));
    env_def(env, "tanh", value_native(lib_math_tanh));
    
    env_def(env, "floor", value_native(lib_math_floor));
    env_def(env, "ceil", value_native(lib_math_ceil));
    env_def(env, "round", value_native(lib_math_round));
    env_def(env, "trunc", value_native(lib_math_trunc));
    env_def(env, "fract", value_native(lib_math_fract));
    env_def(env, "mod", value_native(lib_math_mod));
    
    // Unified Random Interface (xoroshiro128++)
    env_def(env, "rand", value_native(lib_math_rand));
    env_def(env, "srand", value_native(lib_math_srand));
    env_def(env, "trand", value_native(lib_math_trand));
    
    env_def(env, "deg_to_rad", value_native(lib_math_deg_to_rad));
    env_def(env, "rad_to_deg", value_native(lib_math_rad_to_deg));
    env_def(env, "lerp", value_native(lib_math_lerp));
    
    // String Library
    // Both 'len' and 'str_len' now point to the polymorphic lib_str_len
    env_def(env, "len", value_native(lib_str_len));
    env_def(env, "str_len", value_native(lib_str_len)); 
    
    env_def(env, "is_empty", value_native(lib_str_is_empty));
    env_def(env, "concat", value_native(lib_str_concat));
    
    env_def(env, "substring", value_native(lib_str_substring));
    env_def(env, "slice", value_native(lib_str_slice));
    env_def(env, "char_at", value_native(lib_str_char_at));
    
    env_def(env, "index_of", value_native(lib_str_index_of));
    env_def(env, "last_index_of", value_native(lib_str_last_index_of));
    env_def(env, "contains", value_native(lib_str_contains));
    env_def(env, "starts_with", value_native(lib_str_starts_with));
    env_def(env, "ends_with", value_native(lib_str_ends_with));
    
    env_def(env, "to_upper", value_native(lib_str_to_upper));
    env_def(env, "to_lower", value_native(lib_str_to_lower));
    env_def(env, "trim", value_native(lib_str_trim));
    env_def(env, "trim_left", value_native(lib_str_trim_left));
    env_def(env, "trim_right", value_native(lib_str_trim_right));
    env_def(env, "replace", value_native(lib_str_replace));
    env_def(env, "reverse", value_native(lib_str_reverse));
    env_def(env, "repeat", value_native(lib_str_repeat));
    env_def(env, "pad_left", value_native(lib_str_pad_left));
    env_def(env, "pad_right", value_native(lib_str_pad_right));
    
    env_def(env, "split", value_native(lib_str_split));
    env_def(env, "join", value_native(lib_str_join));
    
    env_def(env, "is_digit", value_native(lib_str_is_digit));
    env_def(env, "is_alpha", value_native(lib_str_is_alpha));
    env_def(env, "is_alnum", value_native(lib_str_is_alnum));
    env_def(env, "is_space", value_native(lib_str_is_space));
    
    env_def(env, "to_int", value_native(lib_str_to_int));
    env_def(env, "to_float", value_native(lib_str_to_float));

    // List Library (Hybrid Sort & Fisher-Yates Shuffle)
    env_def(env, "sort", value_native(lib_list_sort));
    env_def(env, "shuffle", value_native(lib_list_shuffle));

    // Time Library
    env_def(env, "clock", value_native(lib_time_clock));
   
    // Vector Math Library
    env_def(env, "vec_add", value_native(lib_vec_add));
    env_def(env, "vec_sub", value_native(lib_vec_sub));
    env_def(env, "vec_mul", value_native(lib_vec_mul));
    env_def(env, "vec_div", value_native(lib_vec_div));

    // File I/O Library
    env_def(env, "open", value_native(lib_file_open));
    env_def(env, "close", value_native(lib_file_close));
    env_def(env, "read", value_native(lib_file_read));
    env_def(env, "read_line", value_native(lib_file_read_line));
    env_def(env, "write", value_native(lib_file_write));
    
    env_def(env, "file_exists", value_native(lib_file_exists));
    env_def(env, "remove_file", value_native(lib_file_remove));
    env_def(env, "flush", value_native(lib_file_flush));
}
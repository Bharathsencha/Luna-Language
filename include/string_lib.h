// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef STRING_LIB_H
#define STRING_LIB_H

#include "value.h"

// Forward declaration
struct Env;

// Basic Inspection
Value lib_str_len(int argc, Value *argv, struct Env *env);
Value lib_str_is_empty(int argc, Value *argv, struct Env *env);
Value lib_str_concat(int argc, Value *argv, struct Env *env);

// Slicing & Access 
Value lib_str_substring(int argc, Value *argv, struct Env *env);
Value lib_str_slice(int argc, Value *argv, struct Env *env);
Value lib_str_char_at(int argc, Value *argv, struct Env *env);

// Searching 
Value lib_str_index_of(int argc, Value *argv, struct Env *env);
Value lib_str_last_index_of(int argc, Value *argv, struct Env *env);
Value lib_str_contains(int argc, Value *argv, struct Env *env);
Value lib_str_starts_with(int argc, Value *argv, struct Env *env);
Value lib_str_ends_with(int argc, Value *argv, struct Env *env);

// Manipulation & Formatting 
Value lib_str_to_upper(int argc, Value *argv, struct Env *env);
Value lib_str_to_lower(int argc, Value *argv, struct Env *env);
Value lib_str_trim(int argc, Value *argv, struct Env *env);
Value lib_str_trim_left(int argc, Value *argv, struct Env *env);
Value lib_str_trim_right(int argc, Value *argv, struct Env *env);
Value lib_str_replace(int argc, Value *argv, struct Env *env);
Value lib_str_reverse(int argc, Value *argv, struct Env *env);
Value lib_str_repeat(int argc, Value *argv, struct Env *env);
Value lib_str_pad_left(int argc, Value *argv, struct Env *env);
Value lib_str_pad_right(int argc, Value *argv, struct Env *env);

// Lists
Value lib_str_split(int argc, Value *argv, struct Env *env);
Value lib_str_join(int argc, Value *argv, struct Env *env);

// Character Checks
Value lib_str_is_digit(int argc, Value *argv, struct Env *env);
Value lib_str_is_alpha(int argc, Value *argv, struct Env *env);
Value lib_str_is_alnum(int argc, Value *argv, struct Env *env);
Value lib_str_is_space(int argc, Value *argv, struct Env *env);

// Type Conversion
Value lib_str_to_int(int argc, Value *argv, struct Env *env);
Value lib_str_to_float(int argc, Value *argv, struct Env *env);
Value lib_str_to_string(int argc, Value *argv, struct Env *env);

#endif
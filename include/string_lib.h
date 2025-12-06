// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#ifndef STRING_LIB_H
#define STRING_LIB_H

#include "value.h"

// Basic Inspection
Value lib_str_len(int argc, Value *argv);
Value lib_str_is_empty(int argc, Value *argv);
Value lib_str_concat(int argc, Value *argv);

// Slicing & Access 
Value lib_str_substring(int argc, Value *argv);
Value lib_str_slice(int argc, Value *argv);
Value lib_str_char_at(int argc, Value *argv);

// Searching 
Value lib_str_index_of(int argc, Value *argv);
Value lib_str_last_index_of(int argc, Value *argv);
Value lib_str_contains(int argc, Value *argv);
Value lib_str_starts_with(int argc, Value *argv);
Value lib_str_ends_with(int argc, Value *argv);

// Manipulation & Formatting 
Value lib_str_to_upper(int argc, Value *argv);
Value lib_str_to_lower(int argc, Value *argv);
Value lib_str_trim(int argc, Value *argv);
Value lib_str_trim_left(int argc, Value *argv);
Value lib_str_trim_right(int argc, Value *argv);
Value lib_str_replace(int argc, Value *argv);
Value lib_str_reverse(int argc, Value *argv);           // "hello" -> "olleh"
Value lib_str_repeat(int argc, Value *argv);            // "a", 3 -> "aaa"
Value lib_str_pad_left(int argc, Value *argv);          // "42", 5, '0' -> "00042"
Value lib_str_pad_right(int argc, Value *argv);         // "Hi", 5, '.' -> "Hi..."

// Lists
Value lib_str_split(int argc, Value *argv);
Value lib_str_join(int argc, Value *argv);

// Character Checks
Value lib_str_is_digit(int argc, Value *argv);
Value lib_str_is_alpha(int argc, Value *argv);
Value lib_str_is_alnum(int argc, Value *argv);
Value lib_str_is_space(int argc, Value *argv);

// Type Conversion [[]]]]
Value lib_str_to_int(int argc, Value *argv);
Value lib_str_to_float(int argc, Value *argv);

#endif
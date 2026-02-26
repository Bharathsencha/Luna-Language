// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef LIST_LIB_H
#define LIST_LIB_H

#include "value.h"

// Forward declaration
struct Env;

Value lib_list_sort(int argc, Value *argv, struct Env *env);
Value lib_list_shuffle(int argc, Value *argv, struct Env *env);
Value lib_list_append(int argc, Value *argv, struct Env *env);
Value lib_dense_list(int argc, Value *argv, struct Env *env);

#endif
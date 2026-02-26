// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef TIME_LIB_H
#define TIME_LIB_H

#include "value.h"

// Forward declaration
struct Env;

// Returns the current monotonic time in seconds
Value lib_time_clock(int argc, Value *argv, struct Env *env);

#endif
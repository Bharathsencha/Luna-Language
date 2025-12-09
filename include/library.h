// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

// This header acts as the configuration center for the Luna runtime.
// It declares the registration function responsible for wiring up all 
// native standard library functions (Math, String, Time, Vectors).

#ifndef LIBRARY_H
#define LIBRARY_H

#include "env.h"

// Registers all built-in native functions (Math, String, Time, Vec) 
// into the given environment so they can be called from Luna scripts.
void env_register_stdlib(Env *env);

#endif
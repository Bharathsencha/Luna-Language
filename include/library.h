// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

// This header acts as the configuration center for the Luna runtime.
// It declares the registration function responsible for wiring up all 
// native standard library functions (Math, String, Time, Vectors).

#ifndef LIBRARY_H
#define LIBRARY_H

#include "env.h"

// Registers all built-in native functions (Math, String, Time, Vec) 
// into the given environment so they can be called from Luna scripts.
/* Command-line arguments that follow the script name, for args(). */
void library_set_args(int argc, char **argv);
void env_register_stdlib(Env *env);

#endif
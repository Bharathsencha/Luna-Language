// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#ifndef MODULE_RUNTIME_H
#define MODULE_RUNTIME_H

#include <stddef.h>
#include "env.h"

// Record the entry script so relative imports resolve against its dir.
// Pass argv[1]-style script path (or NULL to keep CWD-relative behavior).
void module_runtime_set_main_script(const char *path);

const char *module_runtime_current_dir(void);

// Resolve raw against the current script dir; malloc'd canonical path,
// or NULL when the file does not exist.
char *module_runtime_resolve(const char *raw);

int module_runtime_is_loading(const char *canon);
int module_runtime_push_file(const char *canon); // also switches current dir
void module_runtime_pop_file(void);

Env *module_cache_get(const char *canon);
const char **module_cache_get_exports(const char *canon, int *count_out);
// Takes ownership of exports array; env must outlive the process
int module_cache_put(const char *canon, Env *env,
                     const char **exports, int export_count);

#endif // MODULE_RUNTIME_H

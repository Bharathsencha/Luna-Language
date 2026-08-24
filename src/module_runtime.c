// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// module_runtime.c — import path resolution, cycle detection, module cache
//
// Shared by the bytecode VM (vm_run_import) and the tree-walking
// interpreter (NODE_IMPORT). Provides:
//   - script-relative resolution of "use ... from" paths
//   - canonicalized paths via realpath() so the same file always
//     maps to one cache entry and one cycle-detection key
//   - a loading stack for circular-import errors
//   - a process-lifetime module cache: each file executes once;
//     repeat imports re-copy bindings from the cached Env.
//
// Cached Envs are plain heap structs (not GC-managed); their values stay
// reachable because every live Env is marked by env_gc_mark_active_roots.

#include "module_runtime.h"
#include "env.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MODULE_CACHE_MAX 128
#define IMPORT_STACK_MAX 32

typedef struct {
    char *path;          // canonical, owned
    Env *env;            // module top-level env (kept alive)
    const char **exports;// interned export names, owned
    int export_count;
} ModuleCacheEntry;

static ModuleCacheEntry g_module_cache[MODULE_CACHE_MAX];
static int g_module_cache_count = 0;

static char *g_loading_paths[IMPORT_STACK_MAX]; // owned canon paths
static int g_loading_count = 0;

static char g_main_script_dir[PATH_MAX] = {0};
static char g_import_dirs[IMPORT_STACK_MAX][PATH_MAX];
static int g_import_depth = 0;


static void dir_of(const char *path, char *out, size_t outsz) {
    const char *slash = strrchr(path, '/');
    if (!slash || slash == path) {
        snprintf(out, outsz, "%s", (slash == path) ? "/" : "");
        return;
    }
    size_t len = (size_t)(slash - path);
    if (len >= outsz) len = outsz - 1;
    memcpy(out, path, len);
    out[len] = '\0';
}

static int is_absolute_path(const char *p) {
    return p && p[0] == '/';
}

void module_runtime_set_main_script(const char *path) {
    g_main_script_dir[0] = '\0';
    if (path) dir_of(path, g_main_script_dir, sizeof(g_main_script_dir));
}

const char *module_runtime_current_dir(void) {
    if (g_import_depth > 0) return g_import_dirs[g_import_depth - 1];
    return g_main_script_dir;
}

// Resolve raw against the current script dir; returns malloc'd canonical
// path, or NULL if the file does not exist.
char *module_runtime_resolve(const char *raw) {
    if (!raw || !raw[0]) return NULL;

    char joined[PATH_MAX];
    if (is_absolute_path(raw)) {
        snprintf(joined, sizeof(joined), "%s", raw);
    } else {
        const char *dir = module_runtime_current_dir();
        if (dir[0] == '\0') {
            snprintf(joined, sizeof(joined), "%s", raw);
        } else {
            snprintf(joined, sizeof(joined), "%s/%s", dir, raw);
        }
    }

    char canon[PATH_MAX];
    if (!realpath(joined, canon)) return NULL;

    char *out = malloc(strlen(canon) + 1);
    if (out) strcpy(out, canon);
    return out;
}

int module_runtime_is_loading(const char *canon) {
    for (int i = 0; i < g_loading_count; i++) {
        if (strcmp(g_loading_paths[i], canon) == 0) return 1;
    }
    return 0;
}

int module_runtime_push_file(const char *canon) {
    if (g_loading_count >= IMPORT_STACK_MAX) return 0;
    if (g_import_depth >= IMPORT_STACK_MAX) return 0;
    char *copy = malloc(strlen(canon) + 1);
    if (!copy) return 0;
    strcpy(copy, canon);
    g_loading_paths[g_loading_count++] = copy;
    dir_of(canon, g_import_dirs[g_import_depth], sizeof(g_import_dirs[0]));
    g_import_depth++;
    return 1;
}

void module_runtime_pop_file(void) {
    if (g_loading_count > 0) {
        free(g_loading_paths[--g_loading_count]);
    }
    if (g_import_depth > 0) g_import_depth--;
}


Env *module_cache_get(const char *canon) {
    for (int i = 0; i < g_module_cache_count; i++) {
        if (strcmp(g_module_cache[i].path, canon) == 0) return g_module_cache[i].env;
    }
    return NULL;
}

const char **module_cache_get_exports(const char *canon, int *count_out) {
    for (int i = 0; i < g_module_cache_count; i++) {
        if (strcmp(g_module_cache[i].path, canon) == 0) {
            if (count_out) *count_out = g_module_cache[i].export_count;
            return g_module_cache[i].exports;
        }
    }
    if (count_out) *count_out = 0;
    return NULL;
}

int module_cache_put(const char *canon, Env *env,
                     const char **exports, int export_count) {
    if (g_module_cache_count >= MODULE_CACHE_MAX) return 0;
    char *copy = malloc(strlen(canon) + 1);
    if (!copy) return 0;
    strcpy(copy, canon);
    g_module_cache[g_module_cache_count].path = copy;
    g_module_cache[g_module_cache_count].env = env;
    g_module_cache[g_module_cache_count].exports = exports;
    g_module_cache[g_module_cache_count].export_count = export_count;
    g_module_cache_count++;
    return 1;
}

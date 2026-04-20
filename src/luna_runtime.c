// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <locale.h>
#include <string.h>
#include "gc.h"
#include "intern.h"
#include "interpreter.h"
#include "luna_error.h"
#include "luna_runtime.h"
#include "memory.h"
#include "parser.h"
#include "unsafe_runtime.h"

static void luna_runtime_mark_roots(void *ctx) {
    env_gc_mark_active_roots(ctx);
    interpreter_gc_mark_runtime_roots(ctx);
    unsafe_runtime_gc_mark_roots(ctx);
}

int luna_runtime_init(LunaRuntime *runtime, int with_stdlib) {
    if (!runtime) return 0;

    memset(runtime, 0, sizeof(*runtime));
    setlocale(LC_ALL, "C");

    ast_init();
    intern_init();
    gc_stats_reset();
    if (!luna_gc_runtime_init(4 * 1024 * 1024)) {
        return 0;
    }
    luna_gc_runtime_set_root_marker(luna_runtime_mark_roots, NULL);

    runtime->global_env = env_create_global();
    if (!runtime->global_env) {
        luna_gc_runtime_shutdown();
        return 0;
    }

    luna_mem_init();
    runtime->stdlib_enabled = with_stdlib ? 1 : 0;

    runtime->initialized = 1;
    return 1;
}

void luna_runtime_shutdown(LunaRuntime *runtime) {
    if (!runtime || !runtime->initialized) return;

    unsafe_runtime_shutdown();
    luna_mem_shutdown();
    env_free_global(runtime->global_env);
    runtime->global_env = NULL;
    ast_cleanup();
    intern_free_all();
    luna_gc_runtime_shutdown();
    runtime->initialized = 0;
    runtime->stdlib_enabled = 0;
}

AstNode *luna_parse_source(const char *source, const char *filename) {
    Parser parser;
    AstNode *program;

    error_clear_last();
    error_init(source, filename ? filename : "<memory>");

    parser_init(&parser, source);
    program = parser_parse_program(&parser);
    parser_close(&parser);

    return program;
}

int luna_run_source(LunaRuntime *runtime, const char *source, const char *filename, AstNode **out_program) {
    AstNode *program;

    if (!runtime || !runtime->initialized || !runtime->global_env) {
        return 0;
    }

    program = luna_parse_source(source, filename);
    if (!program) {
        if (out_program) *out_program = NULL;
        return 0;
    }

    interpret(program, runtime->global_env);

    if (out_program) {
        *out_program = program;
    } else {
        ast_release(program);
    }

    return luna_had_error == 0;
}

Env *luna_runtime_env(LunaRuntime *runtime) {
    return runtime ? runtime->global_env : NULL;
}

Value *luna_runtime_get_global(LunaRuntime *runtime, const char *name) {
    if (!runtime || !runtime->global_env || !name) {
        return NULL;
    }
    return env_get(runtime->global_env, intern_string(name));
}
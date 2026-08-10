// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h> // Required for setlocale
#include "util.h"
#include "parser.h"
#include "interpreter.h"
#include "ast.h"
#include "luna_error.h"
#include "env.h"
#include "library.h"
#include "math_lib.h" // Required for lib_math_srand auto-seed
#include "gui_lib.h"
#include "intern.h" // For intern_init and intern_cleanup
#include "memory.h"
#include "mystr.h"
#include "unsafe_runtime.h"
#include "gc.h"
#include "luna_vm.h"
#include "luna_compiler.h"

#define MAX_INPUT 1024
#define HISTORY_MAX 128

typedef struct {
    char *items[HISTORY_MAX];
    int count;
} ReplHistory;

static void luna_mark_runtime_roots(void *ctx) {
    env_gc_mark_active_roots(ctx);
    interpreter_gc_mark_runtime_roots(ctx);
    unsafe_runtime_gc_mark_roots(ctx);
}

// Helper to define global color constants in Luna
void register_color_constants(Env *env) {
    (void)env;

    // Colors are represented as Luna Lists: [R, G, B, A]
    // double red[] = {255, 0, 0, 255};
    // double green[] = {0, 255, 0, 255};
    // double blue[] = {0, 0, 255, 255};
    // double white[] = {255, 255, 255, 255};
    // double black[] = {0, 0, 0, 255};
    // double raywhite[] = {245, 245, 245, 255};
}

// Interactive Read-Eval-Print Loop
static void repl_history_add(ReplHistory *history, const char *entry) {
    if (!history || !entry || !entry[0]) return;
    if (history->count == HISTORY_MAX) {
        free(history->items[0]);
        memmove(&history->items[0], &history->items[1], sizeof(char*) * (HISTORY_MAX - 1));
        history->count--;
    }
    history->items[history->count++] = my_strdup(entry);
}

static void repl_history_print(const ReplHistory *history) {
    for (int i = 0; i < history->count; i++) {
        printf("%d: %s\n", i + 1, history->items[i]);
    }
}

static void repl_history_free(ReplHistory *history) {
    for (int i = 0; i < history->count; i++) {
        free(history->items[i]);
    }
    history->count = 0;
}

static int is_blank_line(const char *s) {
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static int repl_needs_more_input(const char *src) {
    int paren = 0, brace = 0, bracket = 0;
    int in_string = 0, in_char = 0, escaped = 0;

    for (const char *p = src; *p; p++) {
        char c = *p;
        if (escaped) {
            escaped = 0;
            continue;
        }
        if (c == '\\') {
            escaped = 1;
            continue;
        }
        if (!in_char && c == '"') {
            in_string = !in_string;
            continue;
        }
        if (!in_string && c == '\'') {
            in_char = !in_char;
            continue;
        }
        if (in_string || in_char) continue;

        if (c == '(') paren++;
        else if (c == ')' && paren > 0) paren--;
        else if (c == '{') brace++;
        else if (c == '}' && brace > 0) brace--;
        else if (c == '[') bracket++;
        else if (c == ']' && bracket > 0) bracket--;
    }

    return in_string || in_char || paren > 0 || brace > 0 || bracket > 0;
}

static char *read_repl_entry(void) {
    size_t cap = MAX_INPUT;
    size_t len = 0;
    char *buffer = malloc(cap);
    if (!buffer) return NULL;
    buffer[0] = '\0';

    while (1) {
        char line[MAX_INPUT];
        printf(len == 0 ? "> " : "... ");
        if (!fgets(line, sizeof(line), stdin)) {
            if (len == 0) {
                free(buffer);
                return NULL;
            }
            break;
        }

        size_t line_len = strlen(line);
        if (len + line_len + 1 > cap) {
            while (len + line_len + 1 > cap) cap *= 2;
            char *grown = realloc(buffer, cap);
            if (!grown) {
                free(buffer);
                return NULL;
            }
            buffer = grown;
        }

        memcpy(buffer + len, line, line_len + 1);
        len += line_len;

        if (!repl_needs_more_input(buffer)) {
            break;
        }
    }

    return buffer;
}

void run_repl(Env *env) {
    ReplHistory history = {0};
    printf("Luna v0.1 REPL\nType 'exit' or Ctrl+C to quit.\n");
    printf("Use ':history' to list entries and '!n' to rerun one.\n");

    while (1) {
        char *line = read_repl_entry();
        if (!line) {
            printf("\n");
            break; // Handle EOF (Ctrl+D) gracefully
        }

        // Check for exit command
        if (strcmp(line, "exit\n") == 0 || strcmp(line, "exit") == 0) {
            free(line);
            break;
        }

        if (strcmp(line, ":history\n") == 0 || strcmp(line, ":history") == 0) {
            repl_history_print(&history);
            free(line);
            continue;
        }

        if (line[0] == '!' && isdigit((unsigned char)line[1])) {
            int idx = atoi(line + 1);
            free(line);
            if (idx <= 0 || idx > history.count) {
                fprintf(stderr, "History entry %d not found.\n", idx);
                continue;
            }
            line = my_strdup(history.items[idx - 1]);
            if (!line) break;
            printf("%s\n", line);
        }

        if (is_blank_line(line)) {
            free(line);
            continue;
        }

        repl_history_add(&history, line);

        // Initialize error system with REPL input
        error_init(line, "<stdin>");

        Parser parser;
        parser_init(&parser, line);
        
        AstNode *prog = parser_parse_program(&parser);

        //Clean up the parser (freeing the last token) immediately after parsing
        parser_close(&parser);

        if (prog) {
            interpret(prog, env);
            if (prog->kind == NODE_BLOCK) {
                nodelist_free(&prog->block.items);
            }
        }
        free(line);
        // If !prog, the parser already printed the error to stderr, so we just loop again
    }

    repl_history_free(&history);
}

static void register_definitions(Env *env, AstNode *n) {
    if (!n) return;
    if (n->kind == NODE_BLOCK) {
        for (int i = 0; i < n->block.items.count; i++) {
            register_definitions(env, n->block.items.items[i]);
        }
    } else if (n->kind == NODE_DATA_DEF) {
        Value dtype = value_data_type(n->data_def.name, n->data_def.fields, n->data_def.field_count,
                                      n->data_def.is_template);
        #ifdef LUNA_VM_DEBUG
        printf("[REGISTER] Registering data type %s at pointer %p\n", n->data_def.name, (void*)n->data_def.name);
        #endif
        env_def_move(env, n->data_def.name, &dtype);
    } else if (n->kind == NODE_BLOC_DEF) {
        Value btype = value_bloc_type(n->bloc_def.name, n->bloc_def.fields, n->bloc_def.field_count);
        if (btype.type != VAL_NULL) {
            env_def_move(env, n->bloc_def.name, &btype);
        }
    }
}

static int ends_with_lu(const char *s) {
    size_t n = strlen(s);
    return n >= 3 && strcmp(s + n - 3, ".lu") == 0;
}

int main(int argc, char **argv) {
    // Force standard "C" locale to ensure '.' is treated as a decimal point
    // regardless of the user's system language settings.
    setlocale(LC_ALL, "C");
    setbuf(stdout, NULL);

    // Initialize the AST Arena Allocator
    ast_init();
    intern_init(); // Initialize global string intern system
    gc_stats_reset();
    luna_gc_runtime_init(4 * 1024 * 1024);
    luna_gc_runtime_set_root_marker(luna_mark_runtime_roots, NULL);

    // Initialize the global environment once to persist variables
    Env *global_env = env_create_global();

    // Register all built-in standard library functions
    env_register_stdlib(global_env);
    luna_mem_init();
    
    // Define GUI color constants so they are available globally
    register_color_constants(global_env);

    // AUTO-SEED: Initialize xoroshiro128++ state using OS entropy (/dev/urandom)
    // Pass 'global_env' as the third argument to match the signature
    lib_math_srand(0, NULL, global_env);

    if (argc < 2) {
        // No file provided: Run REPL mode
        run_repl(global_env);
    } else {
        
        if (!ends_with_lu(argv[1])) {
            fprintf(stderr, "Error: expected a .lu file\n");
            env_free_global(global_env);
            return 1;
        }

        // File provided: Run File mode
        char *src = read_file(argv[1]);
        if (!src) {
            fprintf(stderr, "Could not read file: %s\n", argv[1]);
            env_free_global(global_env);
            return 1;
        }

        // Initialize error system with file source
        error_init(src, argv[1]);

        Parser parser;
        parser_init(&parser, src);

        AstNode *prog = parser_parse_program(&parser);

        //Clean up the parser (freeing the last token) immediately after parsing
        parser_close(&parser);

        if (!prog) {
            fprintf(stderr, "Parsing failed.\n");
            free(src);
            env_free_global(global_env);
            return 1;
        }
        // Execute the parsed program (top-level statements and definitions)
        if (getenv("LUNA_USE_VM")) {
            register_definitions(global_env, prog);
            LunaChunk *chunk = luna_compile_program(prog);
            LunaVM vm;
            luna_vm_init(&vm, luna_gc_runtime_heap());
            vm.env = global_env;
            luna_gc_runtime_set_root_marker(vm_gc_mark_roots, &vm);
            Value ret = luna_vm_run(&vm, chunk);
            value_free(ret);
            luna_vm_free(&vm);
            luna_gc_runtime_set_root_marker(luna_mark_runtime_roots, NULL);
            luna_chunk_free(chunk);
            free(chunk);
        } else {
            interpret(prog, global_env);
        }

        ast_free(prog);
        free(src);
    }

    if (getenv("LUNA_GC_STATS")) {
        LunaGCStats stats = gc_stats_snapshot();
        fprintf(stderr, "LUNA_GC_STATS,%.3f,%.3f,%llu\n",
                stats.gc_ms_total,
                stats.gc_ms_max,
                stats.gc_events);
    }
    unsafe_runtime_shutdown();
    luna_mem_shutdown();
    ast_cleanup();
    env_free_global(global_env);
    luna_gc_runtime_shutdown();
    return 0;
}
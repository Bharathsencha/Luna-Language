// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define MAX_INPUT 1024

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
void run_repl(Env *env) {
    char line[MAX_INPUT];
    printf("Luna v0.1 REPL\nType 'exit' or Ctrl+C to quit.\n");

    while (1) {
        printf("> ");
        
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break; // Handle EOF (Ctrl+D) gracefully
        }

        // Check for exit command
        if (strncmp(line, "exit", 4) == 0) {
            break;
        }

        // Initialize error system with REPL input
        error_init(line, "<stdin>");

        Parser parser;
        parser_init(&parser, line);
        
        AstNode *prog = parser_parse_program(&parser);

        //Clean up the parser (freeing the last token) immediately after parsing
        parser_close(&parser);

        if (prog) {
            interpret(prog, env);
            ast_free(prog);
        }
        // If !prog, the parser already printed the error to stderr, so we just loop again
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

    // Initialize the AST Arena Allocator
    ast_init();
    intern_init(); // Initialize global string intern system

    // Initialize the global environment once to persist variables
    Env *global_env = env_create_global();

    // Register all built-in standard library functions
    env_register_stdlib(global_env);
    
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
        interpret(prog, global_env);

        // Auto-Main Logic: Check if 'main' was defined and call it automatically
        AstNode *main_def = env_get_func(global_env, "main");
        if (main_def && !luna_had_error) {
            NodeList no_args;
            nodelist_init(&no_args);
            
            // Construct a synthetic AST node for the call: main()
            AstNode *main_call = ast_call("main", no_args, prog->line);
            interpret(main_call, global_env);
            
            ast_free(main_call);
        }

        ast_free(prog);
        free(src);
    }

    ast_cleanup();
    env_free_global(global_env);
    return 0;
}
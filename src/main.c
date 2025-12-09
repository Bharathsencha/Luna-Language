// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

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

#define MAX_INPUT 1024

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

    // Initialize the global environment once to persist variables
    Env *global_env = env_create_global();

    env_register_stdlib(global_env);

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

        interpret(prog, global_env);

        ast_free(prog);
        free(src);
    }

    env_free_global(global_env);
    return 0;
}
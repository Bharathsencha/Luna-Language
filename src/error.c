// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "luna_error.h"

// Initialize the global tracker
int luna_current_line = 0;
int luna_had_error = 0;

// Global source information for context display
static SourceInfo g_source_info = {NULL, NULL};
static LunaErrorInfo g_last_error = {0};
static int g_error_quiet = 0;

// ANSI color codes (can be disabled on Windows if needed)
#ifdef _WIN32
    // Windows console colors - can be disabled by setting to empty strings
    #define COLOR_RED ""
    #define COLOR_YELLOW ""
    #define COLOR_BLUE ""
    #define COLOR_GREEN ""
    #define COLOR_BOLD ""
    #define COLOR_RESET ""
#else
    #define COLOR_RED "\033[1;31m"
    #define COLOR_YELLOW "\033[1;33m"
    #define COLOR_BLUE "\033[1;34m"
    #define COLOR_GREEN "\033[1;32m"
    #define COLOR_BOLD "\033[1m"
    #define COLOR_RESET "\033[0m"
#endif

static int error_use_color(void) {
#ifdef _WIN32
    return 0;
#else
    const char *no_color = getenv("NO_COLOR");
    if (no_color && no_color[0] != '\0') return 0;
    return isatty(fileno(stderr));
#endif
}

static const char *color_if_enabled(const char *code) {
    return error_use_color() ? code : "";
}

void error_init(const char *source, const char *filename) {
    g_source_info.source = source;
    g_source_info.filename = filename;
}

void error_clear_last(void) {
    memset(&g_last_error, 0, sizeof(g_last_error));
    luna_had_error = 0;
}

int error_get_last(LunaErrorInfo *out) {
    if (!out || !g_last_error.had_error) {
        return 0;
    }
    *out = g_last_error;
    return 1;
}

void error_set_quiet(int quiet) {
    g_error_quiet = quiet ? 1 : 0;
}

static void error_store_last(ErrorType type, int line, int col, const char *message, const char *suggestion) {
    memset(&g_last_error, 0, sizeof(g_last_error));
    g_last_error.had_error = 1;
    g_last_error.type = type;
    g_last_error.line = line;
    g_last_error.col = col;

    if (g_source_info.filename) {
        snprintf(g_last_error.filename, sizeof(g_last_error.filename), "%s", g_source_info.filename);
    }
    if (message) {
        snprintf(g_last_error.message, sizeof(g_last_error.message), "%s", message);
    }
    if (suggestion) {
        snprintf(g_last_error.suggestion, sizeof(g_last_error.suggestion), "%s", suggestion);
    }
}

const char *error_type_name(ErrorType type) {
    switch (type) {
        case ERR_SYNTAX: return "Syntax Error (Skill issue)";
        case ERR_STATIC: return "Static Error";
        case ERR_RUNTIME: return "Runtime Error";
        case ERR_TYPE: return "Type Error";
        case ERR_NAME: return "Name Error";
        case ERR_INDEX: return "Index Error";
        case ERR_ARGUMENT: return "Argument Error";
        case ERR_ASSERTION: return "Assertion Error";
        default: return "Error";
    }
}

// Get the line from source code
static char *get_line_from_source(const char *source, int line_num) {
    if (!source) return NULL;
    
    int current_line = 1;
    const char *line_start = source;
    const char *ptr = source;
    
    // Find the start of the target line
    while (*ptr && current_line < line_num) {
        if (*ptr == '\n') {
            current_line++;
            line_start = ptr + 1;
        }
        ptr++;
    }
    
    // If we didn't find the line
    if (current_line != line_num) return NULL;
    
    // Find the end of the line
    const char *line_end = line_start;
    while (*line_end && *line_end != '\n') {
        line_end++;
    }
    
    // Copy the line
    size_t len = line_end - line_start;
    char *line = malloc(len + 1);
    memcpy(line, line_start, len);
    line[len] = '\0';
    
    return line;
}

void error_report(ErrorType type, int line, int col, const char *message, const char *suggestion) {
    luna_had_error = 1;
    // Fallback to global tracker if line is unknown
    if (line <= 0) line = luna_current_line;
    error_store_last(type, line, col, message, suggestion);
    if (g_error_quiet) return;

    fprintf(stderr, "%s%s%s", color_if_enabled(COLOR_RED), error_type_name(type), color_if_enabled(COLOR_RESET));
    
    if (g_source_info.filename) {
        fprintf(stderr, " in %s%s%s", color_if_enabled(COLOR_BOLD), g_source_info.filename, color_if_enabled(COLOR_RESET));
    }
    
    fprintf(stderr, " at line %s%d%s", color_if_enabled(COLOR_BOLD), line, color_if_enabled(COLOR_RESET));
    
    if (col > 0) {
        fprintf(stderr, ", column %s%d%s", color_if_enabled(COLOR_BOLD), col, color_if_enabled(COLOR_RESET));
    }
    
    fprintf(stderr, ":\n  %s\n", message);
    
    if (suggestion) {
        fprintf(stderr, "%sHint:%s %s\n", color_if_enabled(COLOR_BLUE), color_if_enabled(COLOR_RESET), suggestion);
    }
}

void error_report_with_context(ErrorType type, int line, int col, const char *message, const char *suggestion) {
    luna_had_error = 1;
    // Fallback to global tracker if line is unknown
    if (line <= 0) line = luna_current_line;
    error_store_last(type, line, col, message, suggestion);
    if (g_error_quiet) return;

    fprintf(stderr, "%s%s%s", color_if_enabled(COLOR_RED), error_type_name(type), color_if_enabled(COLOR_RESET));
    
    if (g_source_info.filename) {
        fprintf(stderr, " in %s%s%s", color_if_enabled(COLOR_BOLD), g_source_info.filename, color_if_enabled(COLOR_RESET));
    }
    
    fprintf(stderr, " at line %s%d%s", color_if_enabled(COLOR_BOLD), line, color_if_enabled(COLOR_RESET));
    
    if (col > 0) {
        fprintf(stderr, ", column %s%d%s", color_if_enabled(COLOR_BOLD), col, color_if_enabled(COLOR_RESET));
    }
    
    fprintf(stderr, ":\n  %s\n", message);
    
    // Display source context if available
    if (g_source_info.source) {
        char *source_line = get_line_from_source(g_source_info.source, line);
        if (source_line) {
            // Display line number and source
            fprintf(stderr, "\n%s%4d |%s %s\n", color_if_enabled(COLOR_BLUE), line, color_if_enabled(COLOR_RESET), source_line);
            
            // Display pointer to error position
            if (col > 0) {
                fprintf(stderr, "     %s|%s ", color_if_enabled(COLOR_BLUE), color_if_enabled(COLOR_RESET));
                for (int i = 1; i < col; i++) {
                    fprintf(stderr, " ");
                }
                fprintf(stderr, "%s^~~~%s here\n", color_if_enabled(COLOR_YELLOW), color_if_enabled(COLOR_RESET));
            }
            
            free(source_line);
            fprintf(stderr, "\n");
        }
    }
    
    if (suggestion) {
        fprintf(stderr, "%sHint:%s %s\n", color_if_enabled(COLOR_GREEN), color_if_enabled(COLOR_RESET), suggestion);
    }
}

const char *suggest_for_unexpected_token(const char *found, const char *expected) {
    static char buffer[256];
    
    // Common mistakes
    if (strcmp(found, "IDENT") == 0 && strstr(expected, "keyword")) {
        snprintf(buffer, sizeof(buffer), "Did you forget a keyword? Expected %s", expected);
        return buffer;
    }
    
    if (strcmp(expected, ")") == 0) {
        return "Missing closing parenthesis - check if all opening '(' have matching ')'";
    }
    
    if (strcmp(expected, "}") == 0) {
        return "Missing closing brace - check if all opening '{' have matching '}'";
    }
    
    if (strcmp(expected, "]") == 0) {
        return "Missing closing bracket - check if all opening '[' have matching ']'";
    }
    
    if (strcmp(expected, ";") == 0) {
        return "Missing semicolon - statements in for/while may need to end with ';'";
    }
    
    if (strcmp(expected, "=") == 0) {
        return "Missing assignment operator - use '=' to assign values";
    }
    
    if (strcmp(found, "=") == 0 && strstr(expected, "==")) {
        return "Use '==' for comparison, '=' is for assignment";
    }
    
    snprintf(buffer, sizeof(buffer), "Expected %s but found %s", expected, found);
    return buffer;
}

const char *suggest_for_undefined_var(const char *var_name) {
    static char buffer[256];
    
    // Check for common typos
    if (strlen(var_name) > 0) {
        snprintf(buffer, sizeof(buffer), 
                 "Variable '%s' is not defined. Did you forget to declare it with 'let %s = ...'?",
                 var_name, var_name);
        return buffer;
    }
    
    return "Variable is not defined. Declare it with 'let' before using.";
}
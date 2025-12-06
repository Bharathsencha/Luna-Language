// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "luna_error.h"

// Global source information for context display
static SourceInfo g_source_info = {NULL, NULL};

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

void error_init(const char *source, const char *filename) {
    g_source_info.source = source;
    g_source_info.filename = filename;
}

const char *error_type_name(ErrorType type) {
    switch (type) {
        case ERR_SYNTAX: return "Syntax Error (Skill issue)";
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
    fprintf(stderr, "%s%s%s", COLOR_RED, error_type_name(type), COLOR_RESET);
    
    if (g_source_info.filename) {
        fprintf(stderr, " in %s%s%s", COLOR_BOLD, g_source_info.filename, COLOR_RESET);
    }
    
    fprintf(stderr, " at line %s%d%s", COLOR_BOLD, line, COLOR_RESET);
    
    if (col > 0) {
        fprintf(stderr, ", column %s%d%s", COLOR_BOLD, col, COLOR_RESET);
    }
    
    fprintf(stderr, ":\n  %s\n", message);
    
    if (suggestion) {
        fprintf(stderr, "%sHint:%s %s\n", COLOR_BLUE, COLOR_RESET, suggestion);
    }
}

void error_report_with_context(ErrorType type, int line, int col, const char *message, const char *suggestion) {
    fprintf(stderr, "%s%s%s", COLOR_RED, error_type_name(type), COLOR_RESET);
    
    if (g_source_info.filename) {
        fprintf(stderr, " in %s%s%s", COLOR_BOLD, g_source_info.filename, COLOR_RESET);
    }
    
    fprintf(stderr, " at line %s%d%s", COLOR_BOLD, line, COLOR_RESET);
    
    if (col > 0) {
        fprintf(stderr, ", column %s%d%s", COLOR_BOLD, col, COLOR_RESET);
    }
    
    fprintf(stderr, ":\n  %s\n", message);
    
    // Display source context if available
    if (g_source_info.source) {
        char *source_line = get_line_from_source(g_source_info.source, line);
        if (source_line) {
            // Display line number and source
            fprintf(stderr, "\n%s%4d |%s %s\n", COLOR_BLUE, line, COLOR_RESET, source_line);
            
            // Display pointer to error position
            if (col > 0) {
                fprintf(stderr, "     %s|%s ", COLOR_BLUE, COLOR_RESET);
                for (int i = 1; i < col; i++) {
                    fprintf(stderr, " ");
                }
                fprintf(stderr, "%s^~~~%s here\n", COLOR_YELLOW, COLOR_RESET);
            }
            
            free(source_line);
            fprintf(stderr, "\n");
        }
    }
    
    if (suggestion) {
        fprintf(stderr, "%sHint:%s %s\n", COLOR_GREEN, COLOR_RESET, suggestion);
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

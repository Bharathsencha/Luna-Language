// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#ifndef ERROR_H
#define ERROR_H

// Added global line tracker to solve the "Line 0" problem
extern int luna_current_line;

// Error types for better categorization
typedef enum {
    ERR_SYNTAX,
    ERR_RUNTIME,
    ERR_TYPE,
    ERR_NAME,
    ERR_INDEX,
    ERR_ARGUMENT,
    ERR_ASSERTION
} ErrorType;

// Structure to store source information for error context
typedef struct {
    const char *source;
    const char *filename;
} SourceInfo;

// Initialize the error reporting system with source code
void error_init(const char *source, const char *filename);

// Report an error with line/column info and suggestions
void error_report(ErrorType type, int line, int col, const char *message, const char *suggestion);

// Report an error with automatic context display from source
void error_report_with_context(ErrorType type, int line, int col, const char *message, const char *suggestion);

// Get the error type name as a string
const char *error_type_name(ErrorType type);

// Helper to suggest based on common mistakes
const char *suggest_for_unexpected_token(const char *found, const char *expected);
const char *suggest_for_undefined_var(const char *var_name);

#endif
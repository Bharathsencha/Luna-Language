// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <string.h>
#include "data_runtime.h"
#include "luna_error.h"

// Error hint strings
static const char *data_error_hint(int code) {
    switch (code) {
        case 1:  return "Give the bloc a non-empty name, e.g. bloc Vec2 { x, y }.";
        case 2:  return "Each field in a bloc must have a unique name.";
        case 3:  return "A bloc cannot exceed 32 bytes. Reduce the number of fields or use a template instead.";
        case 4:  return "box[] size must be between 1 and 4096.";
        case 5:  return "This box was already freed. Do not access or free it again.";
        case 6:  return "This box handle is not valid. Use a box value returned by box[size].";
        case 7:  return "Give the template a non-empty name, e.g. template Player { name, hp }.";
        case 8:  return "Each field in a template must have a unique name.";
        case 9:  return "A template with this name already exists. Choose a different name.";
        case 10: return "Pass the correct number of arguments to the template constructor.";
        case 11: return "Check the field name. Use shape() to see the template's type name.";
        case 12: return "Bloc is leaf-only. Box can hold Bloc. Template can hold Bloc and Box. No other nesting is allowed.";
        default: return "Check the data type rules in luna_rules.md.";
    }
}

// Common error reporter
static int report_data_error(int code, int line) {
    char msg[256];
    luna_data_error_message(code, msg, sizeof(msg));
    error_report_with_context(ERR_RUNTIME, line, 0, msg, data_error_hint(code));
    return 0;
}

static int data_runtime_check(int code, int line) {
    if (code == 0) return 1;
    return report_data_error(code, line);
}

// Lifecycle
void data_runtime_init(void) {
    luna_data_init();
}

void data_runtime_shutdown(void) {
    luna_data_shutdown();
}

// Bloc
int data_runtime_check_bloc(const char *name, const char **fields, int count, int line) {
    return data_runtime_check(luna_data_bloc_validate(name, fields, count), line);
}

// Box
int data_runtime_check_box_size(size_t size, int line) {
    return data_runtime_check(luna_data_box_size_ok(size), line);
}

void data_runtime_track_box(uint64_t handle) {
    luna_data_box_track(handle);
}

int data_runtime_check_box_access(uint64_t handle, int line) {
    return data_runtime_check(luna_data_box_access_ok(handle), line);
}

int data_runtime_check_box_free(uint64_t handle, int line) {
    return data_runtime_check(luna_data_box_free_ok(handle), line);
}

// Template
int data_runtime_check_template_register(const char *name, const char **fields, int count, int line) {
    return data_runtime_check(luna_data_template_register(name, fields, count), line);
}

int data_runtime_check_template_arity(const char *name, int got, int line) {
    return data_runtime_check(luna_data_template_arity_ok(name, got), line);
}

int data_runtime_check_template_field(const char *name, const char *field, int line) {
    return data_runtime_check(luna_data_template_field_ok(name, field), line);
}

// Containment
int data_runtime_check_containment(int outer, int inner, int line) {
    return data_runtime_check(luna_data_containment_ok(outer, inner), line);
}

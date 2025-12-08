// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>  // Added for fabs()
#include "interpreter.h"
#include "ast.h"
#include "value.h"
#include "mystr.h"
#include "math_lib.h"
#include "string_lib.h"
#include "time_lib.h" 
#include "luna_error.h"

#define MAX_VARS 256
#define MAX_FUNCS 64
#define EPSILON 0.000001 // Tolerance for float comparison

// Structure to hold a variable name and its current value
typedef struct {
    char *name;
    Value val;
} VarEntry;

// Structure to hold a function name and its AST definition
typedef struct {
    char *name;
    AstNode *funcdef;
} FuncEntry;

// The Environment structure, representing a scope (e.g., global, function, block)
typedef struct Env {
    VarEntry vars[MAX_VARS];
    int var_count;
    FuncEntry funcs[MAX_FUNCS];
    int func_count;
    struct Env *parent; // Pointer to the enclosing scope
} Env;

// Flags to handle 'return' statements across recursive calls
typedef struct {
    int active;
    Value value;
} ReturnException;

// Flags to handle 'break' and 'continue' inside loops
typedef struct {
    int break_active;
    int continue_active;
} LoopException;

static ReturnException return_exception = {0};
static LoopException loop_exception = {0};

// Centralized Truthiness Logic ~~~
static int is_truthy(Value v) {
    switch (v.type) {
        case VAL_BOOL: return v.b;
        case VAL_INT: return v.i != 0;
        case VAL_FLOAT: return v.f != 0.0;
        case VAL_STRING: return v.s && v.s[0] != '\0'; // Empty strings are false
        case VAL_NULL: return 0;
        case VAL_LIST: return 1; // Lists are always true (even empty ones, usually)
        case VAL_NATIVE: return 1;
        case VAL_CHAR: return v.c != 0;
        default: return 0;
    }
}

// Creates a new environment scope, linking it to a parent scope
static Env *env_create(Env *parent) {
    Env *e = malloc(sizeof(Env));
    e->var_count = 0;
    e->func_count = 0;
    e->parent = parent;
    for (int i = 0; i < MAX_VARS; i++) {
        e->vars[i].name = NULL;
    }
    for (int i = 0; i < MAX_FUNCS; i++) {
        e->funcs[i].name = NULL;
    }
    return e;
}

// Frees the environment and the memory used by its variables
static void env_free(Env *e) {
    if (!e) {
        return;
    }
    for (int i = 0; i < e->var_count; i++) {
        free(e->vars[i].name);
        value_free(e->vars[i].val);
    }
    for (int i = 0; i < e->func_count; i++) {
        free(e->funcs[i].name);
    }
    free(e);
}

// Looks up a variable by name, traversing up the scope chain if necessary
static Value *env_get(Env *e, const char *name) {
    while (e) {
        // Search backwards to find the most recently declared variable (shadowing)
        for (int i = e->var_count - 1; i >= 0; i--) {
            if (!strcmp(e->vars[i].name, name)) {
                return &e->vars[i].val;
            }
        }
        e = e->parent;
    }
    return NULL;
}

// Defines a new variable in the current scope
static void env_def(Env *e, const char *name, Value val) {
    if (e->var_count < MAX_VARS) {
        e->vars[e->var_count].name = my_strdup(name);
        e->vars[e->var_count].val = value_copy(val);
        e->var_count++;
    }
}

// Updates an existing variable, traversing up the scope chain
static void env_assign(Env *e, const char *name, Value val) {
    Env *cur = e;
    while (cur) {
        for (int i = cur->var_count - 1; i >= 0; i--) {
            if (!strcmp(cur->vars[i].name, name)) {
                value_free(cur->vars[i].val);
                cur->vars[i].val = value_copy(val);
                return;
            }
        }
        cur = cur->parent;
    }
    const char *suggestion = suggest_for_undefined_var(name);
    error_report(ERR_NAME, 0, 0, 
        suggestion ? suggestion : "Variable is not defined", 
        "Declare variables with 'let' before assigning to them");
}

// Defines a function in the current scope
static void env_def_func(Env *e, const char *name, AstNode *def) {
    if (e->func_count < MAX_FUNCS) {
        e->funcs[e->func_count].name = my_strdup(name);
        e->funcs[e->func_count].funcdef = def;
        e->func_count++;
    }
}

// Looks up a function definition
static AstNode *env_get_func(Env *e, const char *name) {
    while (e) {
        for (int i = 0; i < e->func_count; i++) {
            if (!strcmp(e->funcs[i].name, name)) {
                return e->funcs[i].funcdef;
            }
        }
        e = e->parent;
    }
    return NULL;
}

static Value eval_expr(Env *e, AstNode *n);
static Value exec_stmt(Env *e, AstNode *n);

// Recursively finds the actual memory location of a variable or list item
// Used for assigning values to specific list indices (e.g. x[0] = 5)
static Value *get_mutable_value(Env *e, AstNode *n) {
    if (n->kind == NODE_IDENT) {
        return env_get(e, n->ident.name);
    } 
    else if (n->kind == NODE_INDEX) {
        // Recursively get the parent list
        Value *list = get_mutable_value(e, n->index.target);
        if (!list || list->type != VAL_LIST) return NULL;
        
        // Evaluate index
        Value idx = eval_expr(e, n->index.index);
        if (idx.type != VAL_INT) {
            value_free(idx);
            return NULL;
        }
        
        // Check bounds
        if (idx.i < 0 || idx.i >= list->list.count) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Index %lld is out of bounds for list of length %d", 
                     idx.i, list->list.count);
            error_report(ERR_INDEX, 0, 0, msg,
                "Check that your index is between 0 and len(list)-1");
            value_free(idx);
            return NULL;
        }
        
        // Return pointer to the specific item slot
        Value *item_ptr = &list->list.items[idx.i];
        value_free(idx);
        return item_ptr;
    }
    return NULL;
}

// Handles binary operations like +, -, *, /, comparison
static Value eval_binop(BinOpKind op, Value l, Value r) {
    // 1. Handle Pure Integer Operations separately to preserve precision/types
    if (l.type == VAL_INT && r.type == VAL_INT) {
        switch (op) {
            case OP_ADD: return value_int(l.i + r.i);
            case OP_SUB: return value_int(l.i - r.i);
            case OP_MUL: return value_int(l.i * r.i);
            case OP_DIV: 
                if (r.i == 0) return value_int(0); 
                // Return float for division to allow decimals
                return value_float((double)l.i / (double)r.i);
            case OP_MOD: return value_int(l.i % r.i);
            case OP_EQ: return value_bool(l.i == r.i);
            case OP_NEQ: return value_bool(l.i != r.i);
            case OP_LT: return value_bool(l.i < r.i);
            case OP_GT: return value_bool(l.i > r.i);
            case OP_LTE: return value_bool(l.i <= r.i);
            case OP_GTE: return value_bool(l.i >= r.i);
            default: break;
        }
    }

    // 2. Handle Mixed/Float Operations
    if ((l.type == VAL_INT || l.type == VAL_FLOAT) && (r.type == VAL_INT || r.type == VAL_FLOAT)) {
        double dl = (l.type == VAL_INT) ? (double)l.i : l.f;
        double dr = (r.type == VAL_INT) ? (double)r.i : r.f;
        int is_res_float = 1; 
        
        double res = 0;
        
        switch (op) {
            case OP_ADD: res = dl + dr; break;
            case OP_SUB: res = dl - dr; break;
            case OP_MUL: res = dl * dr; break;
            case OP_DIV: res = dr == 0 ? 0 : dl / dr; break;
            case OP_MOD: res = (long long)dl % (long long)dr; is_res_float=0; break;
            
            //Fuzzy Comparison for Floats
            case OP_EQ: return value_bool(fabs(dl - dr) < EPSILON);
            case OP_NEQ: return value_bool(fabs(dl - dr) >= EPSILON);
            case OP_LT: return value_bool(dl < dr);
            case OP_GT: return value_bool(dl > dr);
            case OP_LTE: return value_bool(dl <= dr);
            case OP_GTE: return value_bool(dl >= dr);
            default: break;
        }
        
        if (is_res_float) {
            return value_float(res);
        } else {
            return value_int((long long)res);
        }
    }
    
    // Handle String Equality
    if (l.type == VAL_STRING && r.type == VAL_STRING) {
        if (op == OP_EQ) return value_bool(strcmp(l.s, r.s) == 0);
        if (op == OP_NEQ) return value_bool(strcmp(l.s, r.s) != 0);
    }

    // Handle Boolean and Null Equality
    if (op == OP_EQ) {
        if (l.type == VAL_BOOL && r.type == VAL_BOOL) return value_bool(l.b == r.b);
        if (l.type == VAL_NULL && r.type == VAL_NULL) return value_bool(1);
        if (l.type == VAL_NULL || r.type == VAL_NULL) return value_bool(0);
    }
    if (op == OP_NEQ) {
        if (l.type == VAL_BOOL && r.type == VAL_BOOL) return value_bool(l.b != r.b);
        if (l.type == VAL_NULL && r.type == VAL_NULL) return value_bool(0);
        if (l.type == VAL_NULL || r.type == VAL_NULL) return value_bool(1);
    }

    // Handle Char Equality
    if (l.type == VAL_CHAR && r.type == VAL_CHAR) {
        if (op == OP_EQ) return value_bool(l.c == r.c);
        if (op == OP_NEQ) return value_bool(l.c != r.c);
    }

    // Handle String Concatenation
    if (op == OP_ADD && (l.type == VAL_STRING || r.type == VAL_STRING)) {
        char *sl = value_to_string(l);
        char *sr = value_to_string(r);
        char *comb = malloc(strlen(sl) + strlen(sr) + 1);
        strcpy(comb, sl);
        strcat(comb, sr);
        free(sl);
        free(sr);
        Value v = value_string(comb);
        free(comb);
        return v;
    }
    return value_null();
}

// Evaluates an expression node and returns a Value
static Value eval_expr(Env *e, AstNode *n) {
    if (!n) {
        return value_null();
    }
    switch (n->kind) {
        case NODE_NUMBER: return value_int(n->number.value);
        case NODE_FLOAT: return value_float(n->fnumber.value);
        case NODE_STRING: return value_string(n->string.text);
        case NODE_CHAR: return value_char(n->character.value);
        case NODE_BOOL: return value_bool(n->boolean.value);
        
        // Recursively evaluate items in a list literal
        case NODE_LIST: {
            Value v = value_list();
            for (int i = 0; i < n->list.items.count; i++) {
                Value item = eval_expr(e, n->list.items.items[i]);
                value_list_append(&v, item);
                value_free(item);
            }
            return v;
        }
        
        // Variable lookup
        case NODE_IDENT: {
            Value *v = env_get(e, n->ident.name);
            return v ? value_copy(*v) : value_null();
        }
        
        case NODE_BINOP: {
            // ADDED: Logic Short-circuiting 
            if (n->binop.op == OP_AND) {
                Value l = eval_expr(e, n->binop.left);
                if (!is_truthy(l)) {
                    return l; // Short-circuit: return left (false)
                }
                value_free(l);
                return eval_expr(e, n->binop.right);
            }
            if (n->binop.op == OP_OR) {
                Value l = eval_expr(e, n->binop.left);
                if (is_truthy(l)) {
                    return l; // Short-circuit: return left (true)
                }
                value_free(l);
                return eval_expr(e, n->binop.right);
            }

            Value l = eval_expr(e, n->binop.left);
            Value r = eval_expr(e, n->binop.right);
            Value res = eval_binop(n->binop.op, l, r);
            value_free(l);
            value_free(r);
            return res;
        }
        
        // Unary NOT ---
        case NODE_NOT: {
            Value v = eval_expr(e, n->logic_not.expr);
            Value res = value_bool(!is_truthy(v));
            value_free(v);
            return res;
        }
        // List Indexing: list[index]
        case NODE_INDEX: {
            Value target = eval_expr(e, n->index.target);
            Value idx = eval_expr(e, n->index.index);
            if (target.type == VAL_LIST && idx.type == VAL_INT) {
                if (idx.i >= 0 && idx.i < target.list.count) {
                    Value res = value_copy(target.list.items[idx.i]);
                    value_free(target);
                    value_free(idx);
                    return res;
                }
            }
            value_free(target);
            value_free(idx);
            return value_null();
        }

        //  Increment Operator (++)
        case NODE_INC: {
            Value *v = env_get(e, n->inc.name);
            if (v && v->type == VAL_INT) {
                Value old = value_copy(*v);
                v->i++;
                return old; // Post-increment returns old value
            }
            if (v && v->type == VAL_FLOAT) {
                Value old = value_copy(*v);
                v->f++;
                return old;
            }
            return value_null();
        }
        
        // Decrement Operator (--)
        case NODE_DEC: {
            Value *v = env_get(e, n->dec.name);
            if (v && v->type == VAL_INT) {
                Value old = value_copy(*v);
                v->i--;
                return old; // Post-decrement returns old value
            }
            if (v && v->type == VAL_FLOAT) {
                Value old = value_copy(*v);
                v->f--;
                return old;
            }
            return value_null();
        }

        // Function Calls
        case NODE_CALL: {
            // Built-in: len()
            if (!strcmp(n->call.name, "len")) {
                if (n->call.args.count == 1) {
                    Value v = eval_expr(e, n->call.args.items[0]);
                    int len = 0;
                    if (v.type == VAL_STRING) len = strlen(v.s);
                    if (v.type == VAL_LIST) len = v.list.count;
                    value_free(v);
                    return value_int(len);
                }
            }
            
            // Built-in: append(list, value) 
            if (!strcmp(n->call.name, "append")) {
                if (n->call.args.count != 2) {
                    fprintf(stderr, "Runtime Error: append() takes 2 arguments (list, value)\n");
                    return value_null();
                }
                
                // Get the MUTABLE pointer to the list variable, not a copy!
                Value *list_ptr = get_mutable_value(e, n->call.args.items[0]);
                Value item_val = eval_expr(e, n->call.args.items[1]);
                
                if (list_ptr && list_ptr->type == VAL_LIST) {
                    value_list_append(list_ptr, item_val);
                } else {
                    error_report(ERR_ARGUMENT, 0, 0,
                        "append() expects a list variable as the first argument",
                        "Use append(myList, value) where myList is a list variable");
                }
                
                value_free(item_val);
                return value_null(); // Return null (void)
            }

            // Built-in: type() - Returns type name ("int", "long", "float", etc)
            if (!strcmp(n->call.name, "type")) {
                if (n->call.args.count == 1) {
                    Value v = eval_expr(e, n->call.args.items[0]);
                    char *tname = "unknown";
                    switch (v.type) {
                        case VAL_INT:
                            // Check magnitude to differentiate int vs long for user
                            // Assumes standard 32-bit int limits for "int" label
                            if (v.i > INT_MAX || v.i < INT_MIN) {
                                tname = "long";
                            } else {
                                tname = "int";
                            }
                            break;
                        case VAL_FLOAT: tname = "float"; break;
                        case VAL_STRING: tname = "string"; break;
                        case VAL_CHAR: tname = "char"; break;
                        case VAL_BOOL: tname = "boolean"; break;
                        case VAL_LIST: tname = "list"; break;
                        case VAL_NATIVE: tname = "native_function"; break;
                        case VAL_NULL: tname = "null"; break;
                    }
                    value_free(v);
                    return value_string(tname);
                }
            }

            // Built-in: int()
            if (!strcmp(n->call.name, "int")) {
                if (n->call.args.count == 1) {
                    Value v = eval_expr(e, n->call.args.items[0]);
                    long long res = 0;
                    if (v.type == VAL_STRING) res = atoll(v.s);
                    else if (v.type == VAL_FLOAT) res = (long long)v.f;
                    else if (v.type == VAL_INT) res = v.i;
                    else if (v.type == VAL_BOOL) res = v.b;
                    else if (v.type == VAL_CHAR) res = (long long)v.c;
                    value_free(v);
                    return value_int(res);
                }
            }
            // Built-in: float()
            if (!strcmp(n->call.name, "float")) {
                if (n->call.args.count == 1) {
                    Value v = eval_expr(e, n->call.args.items[0]);
                    double res = 0.0;
                    if (v.type == VAL_STRING) res = atof(v.s);
                    else if (v.type == VAL_INT) res = (double)v.i;
                    else if (v.type == VAL_FLOAT) res = v.f;
                    else if (v.type == VAL_BOOL) res = v.b ? 1.0 : 0.0;
                    value_free(v);
                    return value_float(res);
                }
            }

            // Built-in: assert()
            if (!strcmp(n->call.name, "assert")) {
                if (n->call.args.count != 1) {
                    error_report(ERR_ARGUMENT, 0, 0,
                        "assert() takes exactly 1 argument",
                        "Use assert(condition) to verify an expression is true");
                    exit(1);
                }
                Value v = eval_expr(e, n->call.args.items[0]);
                
                int truthy = is_truthy(v);                
                if (!truthy) {
                    error_report(ERR_ASSERTION, 0, 0,
                        "Assertion failed - condition evaluated to false",
                        "Check that your assertion condition is correct");
                    value_free(v);
                    exit(1); // Non-zero exit code tells the test runner it FAILED
                }
                
                value_free(v);
                return value_bool(1);
            }

            // 1. Check for User defined function
            AstNode *fn = env_get_func(e, n->call.name);
            if (fn) {
                // Create new scope for function execution
                Env *scope = env_create(e);
                
                // Map arguments to parameters
                for (int i = 0; i < fn->funcdef.param_count; i++) {
                    Value v = (i < n->call.args.count) ? eval_expr(e, n->call.args.items[i]) : value_null();
                    env_def(scope, fn->funcdef.params[i], v);
                    value_free(v);
                }
                
                // Execute function body
                for (int i = 0; i < fn->funcdef.body.count; i++) {
                    exec_stmt(scope, fn->funcdef.body.items[i]);
                    if (return_exception.active) break;
                }
                
                // Handle return value
                Value ret = return_exception.active ? value_copy(return_exception.value) : value_null();
                if (return_exception.active) {
                    value_free(return_exception.value);
                    return_exception.active = 0;
                }
                env_free(scope);
                return ret;
            }

            // 2. Check for Native Function (Registered in Variables)
            Value *native_val = env_get(e, n->call.name);
            if (native_val && native_val->type == VAL_NATIVE) {
                // Evaluate Arguments first
                int argc = n->call.args.count;
                Value *argv = malloc(sizeof(Value) * argc);
                for (int i = 0; i < argc; i++) {
                    argv[i] = eval_expr(e, n->call.args.items[i]);
                }

                // Call the C Function Pointer
                Value res = native_val->native(argc, argv);

                // Clean up arguments
                for (int i = 0; i < argc; i++) {
                    value_free(argv[i]);
                }
                free(argv);
                return res;
            }

            return value_null();
        }
        
        case NODE_INPUT: {
            char buf[256];
            if (n->input.prompt) {
                printf("%s", n->input.prompt);
            }
            if (fgets(buf, 256, stdin)) {
                buf[strcspn(buf, "\n")] = 0;
            } else {
                buf[0] = 0;
            }
            return value_string(buf);
        }
        default: return value_null();
    }
}

// Executes a statement node (side effects, control flow)
static Value exec_stmt(Env *e, AstNode *n) {
    if (!n) {
        return value_null();
    }
    switch (n->kind) {
        case NODE_LET: {
            Value v = eval_expr(e, n->let.expr);
            env_def(e, n->let.name, v);
            value_free(v);
            return value_null();
        }
        case NODE_ASSIGN: {
            Value v = eval_expr(e, n->assign.expr);
            env_assign(e, n->assign.name, v);
            value_free(v);
            return value_null();
        }
        case NODE_ASSIGN_INDEX: {
            Value val = eval_expr(e, n->assign_index.value);
            
            // Get pointer to the actual list item in the environment
            Value *target = get_mutable_value(e, n->assign_index.list);
            
            // Verify target is actually a list
            if (!target || target->type != VAL_LIST) {
                error_report(ERR_TYPE, 0, 0,
                    "Cannot assign to non-list target - target must be a list",
                    "Use list indices only on list variables, e.g., myList[0] = value");
                value_free(val);
                return value_null();
            }

            // Evaluate the index
            Value idx = eval_expr(e, n->assign_index.index);
            if (idx.type != VAL_INT) {
                error_report(ERR_TYPE, 0, 0,
                    "List index must be an integer",
                    "Use integer values for list indices, e.g., myList[0] or myList[i]");
                value_free(val);
                value_free(idx);
                return value_null();
            }

            // Bounds Check
            if (idx.i < 0 || idx.i >= target->list.count) {
                char msg[128];
                snprintf(msg, sizeof(msg), "Index %lld is out of bounds for list of length %d",
                         idx.i, target->list.count);
                error_report(ERR_INDEX, 0, 0, msg,
                    "Ensure your index is between 0 and len(list)-1");
                value_free(val);
                value_free(idx);
                return value_null();
            }

            // Assign to the specific slot
            Value *slot = &target->list.items[idx.i];
            value_free(*slot);      // Free the old value in this slot
            *slot = value_copy(val); // Assign the new value
            
            value_free(val);
            value_free(idx);
            return value_null();
        }
        case NODE_PRINT: {
            for (int i = 0; i < n->print.args.count; i++) {
                Value v = eval_expr(e, n->print.args.items[i]);
                char *s = value_to_string(v);
                printf("%s ", s);
                free(s);
                value_free(v);
            }
            printf("\n");
            return value_null();
        }
        case NODE_IF: {
            Value v = eval_expr(e, n->ifstmt.cond);
            int t = is_truthy(v);
            value_free(v);

            NodeList block = t ? n->ifstmt.then_block : n->ifstmt.else_block;
            Env *scope = env_create(e);
            for (int i = 0; i < block.count; i++) {
                exec_stmt(scope, block.items[i]);
                // Stop if a control flow event occurred
                if (return_exception.active || loop_exception.break_active || loop_exception.continue_active) {
                    break;
                }
            }
            env_free(scope);
            return value_null();
        }
        case NODE_WHILE: {
            while (1) {
                Value v = eval_expr(e, n->whilestmt.cond);
                int t = is_truthy(v);
                value_free(v);
                if (!t) {
                    break;
                }

                Env *scope = env_create(e);
                for (int i = 0; i < n->whilestmt.body.count; i++) {
                    exec_stmt(scope, n->whilestmt.body.items[i]);
                    if (return_exception.active || loop_exception.break_active) {
                        break;
                    }
                    if (loop_exception.continue_active) {
                        break;
                    }
                }
                env_free(scope);

                if (return_exception.active) {
                    break;
                }
                if (loop_exception.break_active) {
                    loop_exception.break_active = 0;
                    break;
                }
                if (loop_exception.continue_active) {
                    loop_exception.continue_active = 0;
                    continue;
                }
            }
            return value_null();
        }
        case NODE_FOR: {
            Env *scope = env_create(e); // Create scope for the loop variable (i)
            
            // 1. Run Initializer (once)
            exec_stmt(scope, n->forstmt.init);

            while (1) {
                // 2. Check Condition
                Value c = eval_expr(scope, n->forstmt.cond);
                int truthy = is_truthy(c);
                value_free(c);

                if (!truthy) break; // Exit loop

                // 3. Execute Body
                // We create a generic inner scope for the body to protect the iterator
                Env *inner_scope = env_create(scope); 
                for (int i = 0; i < n->forstmt.body.count; i++) {
                    exec_stmt(inner_scope, n->forstmt.body.items[i]);
                    if (return_exception.active || loop_exception.break_active || loop_exception.continue_active) break;
                }
                env_free(inner_scope);

                if (return_exception.active) break;
                if (loop_exception.break_active) {
                    loop_exception.break_active = 0;
                    break;
                }
                // (Continue is handled implicitly by going to the increment step)
                if (loop_exception.continue_active) {
                    loop_exception.continue_active = 0;
                }

                // 4. Run Increment
                exec_stmt(scope, n->forstmt.incr);
            }
            
            env_free(scope); // Cleanup loop variable 'i'
            return value_null();
        }
        case NODE_SWITCH: {
            Value val = eval_expr(e, n->switchstmt.expr);
            int matched = 0;
            
            // Check all cases
            for (int i = 0; i < n->switchstmt.cases.count; i++) {
                AstNode *c = n->switchstmt.cases.items[i];
                Value cval = eval_expr(e, c->casestmt.value);
                int eq = 0;
                
                // Compare switch value with case value
                if (val.type == cval.type) {
                    if (val.type == VAL_INT) eq = (val.i == cval.i);
                    else if (val.type == VAL_FLOAT) eq = (val.f == cval.f);
                    else if (val.type == VAL_STRING) eq = !strcmp(val.s, cval.s);
                    else if (val.type == VAL_BOOL) eq = (val.b == cval.b);
                    else if (val.type == VAL_CHAR) eq = (val.c == cval.c);
                } else if (val.type == VAL_INT && cval.type == VAL_FLOAT) eq = (val.i == cval.f);
                else if (val.type == VAL_FLOAT && cval.type == VAL_INT) eq = (val.f == cval.i);
                value_free(cval);

                if (eq) {
                    matched = 1;
                    Env *scope = env_create(e);
                    for (int j = 0; j < c->casestmt.body.count; j++) {
                        exec_stmt(scope, c->casestmt.body.items[j]);
                        if (return_exception.active || loop_exception.continue_active) break;
                        if (loop_exception.break_active) break;
                    }
                    env_free(scope);
                    if (loop_exception.break_active) {
                        loop_exception.break_active = 0;
                    }
                    break;
                }
            }
            
            // Execute default block if no match found
            if (!matched && n->switchstmt.default_case.count > 0) {
                Env *scope = env_create(e);
                for (int j = 0; j < n->switchstmt.default_case.count; j++) {
                    exec_stmt(scope, n->switchstmt.default_case.items[j]);
                    if (return_exception.active || loop_exception.continue_active) break;
                    if (loop_exception.break_active) break;
                }
                env_free(scope);
                if (loop_exception.break_active) {
                    loop_exception.break_active = 0;
                }
            }
            value_free(val);
            return value_null();
        }
        case NODE_BLOCK: {
            Env *scope = env_create(e);
            for (int i = 0; i < n->block.items.count; i++) {
                exec_stmt(scope, n->block.items.items[i]);
                if (return_exception.active || loop_exception.break_active || loop_exception.continue_active) {
                    break;
                }
            }
            env_free(scope);
            return value_null();
        }
        case NODE_GROUP: {
            // Execute statements in the CURRENT environment (e)
            for (int i = 0; i < n->block.items.count; i++) {
                exec_stmt(e, n->block.items.items[i]);
                
                // Still need to check for control flow (return/break)
                if (return_exception.active || loop_exception.break_active || loop_exception.continue_active) {
                    break;
                }
            }
            return value_null();
        }
        
        case NODE_FUNC_DEF:
            env_def_func(e, n->funcdef.name, n);
            return value_null();
            
        case NODE_RETURN: {
            // 1. Calculate the return value first
            Value v = eval_expr(e, n->ret.expr);
            // 2. Set the exception flag to stop further execution
            return_exception.active = 1;
            return_exception.value = v;
            return value_null();
        }
        
        case NODE_BREAK:
            loop_exception.break_active = 1;
            return value_null();
            
        case NODE_CONTINUE:
            loop_exception.continue_active = 1;
            return value_null();
            
        default: {
            // Evaluate standalone expressions (e.g., function calls without assignment)
            Value v = eval_expr(e, n);
            value_free(v);
            return value_null();
        }
    }
}

Value interpret(AstNode *prog, Env *env) {
    // Reset global flags to prevent state leaking between tests
    return_exception.active = 0;        
    loop_exception.break_active = 0;    
    loop_exception.continue_active = 0; 

    if (!prog) {
        return value_null();
    }
    
    // Use the passed environment directly
    if (prog->kind == NODE_BLOCK) {
        for (int i = 0; i < prog->block.items.count; i++) {
            exec_stmt(env, prog->block.items.items[i]);
        }
    } else {
        exec_stmt(env, prog);
    }
    return value_null();
}

Env *env_create_global(void) {
    return env_create(NULL);
}

void env_free_global(Env *env) {
    env_free(env);
}

// Registers standard library functions
void env_register_stdlib(Env *env) {
    // Math
    env_def(env, "abs", value_native(lib_math_abs));
    env_def(env, "min", value_native(lib_math_min));
    env_def(env, "max", value_native(lib_math_max));
    env_def(env, "clamp", value_native(lib_math_clamp)); //Is this even useful??
    env_def(env, "sign", value_native(lib_math_sign));
    
    env_def(env, "pow", value_native(lib_math_pow));
    env_def(env, "sqrt", value_native(lib_math_sqrt));
    env_def(env, "cbrt", value_native(lib_math_cbrt));
    env_def(env, "exp", value_native(lib_math_exp));
    env_def(env, "ln", value_native(lib_math_ln));
    env_def(env, "log10", value_native(lib_math_log10));
    
    env_def(env, "sin", value_native(lib_math_sin));
    env_def(env, "cos", value_native(lib_math_cos));
    env_def(env, "tan", value_native(lib_math_tan));
    env_def(env, "asin", value_native(lib_math_asin));
    env_def(env, "acos", value_native(lib_math_acos));
    env_def(env, "atan", value_native(lib_math_atan));
    env_def(env, "atan2", value_native(lib_math_atan2));
    
    env_def(env, "sinh", value_native(lib_math_sinh));
    env_def(env, "cosh", value_native(lib_math_cosh));
    env_def(env, "tanh", value_native(lib_math_tanh));
    
    env_def(env, "floor", value_native(lib_math_floor));
    env_def(env, "ceil", value_native(lib_math_ceil));
    env_def(env, "round", value_native(lib_math_round));
    env_def(env, "trunc", value_native(lib_math_trunc));
    env_def(env, "fract", value_native(lib_math_fract));
    env_def(env, "mod", value_native(lib_math_mod));
    
    env_def(env, "rand", value_native(lib_math_rand));
    env_def(env, "randint", value_native(lib_math_randint));
    env_def(env, "srand", value_native(lib_math_srand));
    
    env_def(env, "deg_to_rad", value_native(lib_math_deg_to_rad));
    env_def(env, "rad_to_deg", value_native(lib_math_rad_to_deg));
    env_def(env, "lerp", value_native(lib_math_lerp));
    
    // String 
    env_def(env, "str_len", value_native(lib_str_len)); // aliased to avoid collision with hardcoded 'len'
    env_def(env, "is_empty", value_native(lib_str_is_empty));
    env_def(env, "concat", value_native(lib_str_concat));
    
    env_def(env, "substring", value_native(lib_str_substring));
    env_def(env, "slice", value_native(lib_str_slice));
    env_def(env, "char_at", value_native(lib_str_char_at));
    
    env_def(env, "index_of", value_native(lib_str_index_of));
    env_def(env, "last_index_of", value_native(lib_str_last_index_of));
    env_def(env, "contains", value_native(lib_str_contains));
    env_def(env, "starts_with", value_native(lib_str_starts_with));
    env_def(env, "ends_with", value_native(lib_str_ends_with));
    
    env_def(env, "to_upper", value_native(lib_str_to_upper));
    env_def(env, "to_lower", value_native(lib_str_to_lower));
    env_def(env, "trim", value_native(lib_str_trim));
    env_def(env, "trim_left", value_native(lib_str_trim_left));
    env_def(env, "trim_right", value_native(lib_str_trim_right));
    env_def(env, "replace", value_native(lib_str_replace));
    env_def(env, "reverse", value_native(lib_str_reverse));
    env_def(env, "repeat", value_native(lib_str_repeat));
    env_def(env, "pad_left", value_native(lib_str_pad_left));
    env_def(env, "pad_right", value_native(lib_str_pad_right));
    
    env_def(env, "split", value_native(lib_str_split));
    env_def(env, "join", value_native(lib_str_join));
    
    env_def(env, "is_digit", value_native(lib_str_is_digit));
    env_def(env, "is_alpha", value_native(lib_str_is_alpha));
    env_def(env, "is_alnum", value_native(lib_str_is_alnum));
    env_def(env, "is_space", value_native(lib_str_is_space));
    
    env_def(env, "to_int", value_native(lib_str_to_int));
    env_def(env, "to_float", value_native(lib_str_to_float));

    // Time Library  
    env_def(env, "clock", value_native(lib_time_clock));
}
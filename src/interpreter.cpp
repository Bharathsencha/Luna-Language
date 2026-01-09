// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h> // Added for fabs()
#include <luna/interpreter.h>
#include <luna/ast.h>
#include <luna/value.h>
#include <luna/mystr.h>
#include <luna/env.h>
#include <luna/library.h>
#include <luna/luna_error.h>
#include <luna/vec_lib.h>

#define EPSILON 0.000001 // Tolerance for float comparison

// Flags to handle 'return' statements across recursive calls
typedef struct
{
    int active;
    Value value;
} ReturnException;

// Flags to handle 'break' and 'continue' inside loops
typedef struct
{
    int break_active;
    int continue_active;
} LoopException;

static ReturnException return_exception = {0};
static LoopException loop_exception = {0};

// Centralized Truthiness Logic ~~~
static int is_truthy(Value v)
{
    switch (v.type)
    {
    case VAL_BOOL:
        return v.b;
    case VAL_INT:
        return v.i != 0;
    case VAL_FLOAT:
        return v.f != 0.0;
    case VAL_STRING:
        return v.s && v.s[0] != '\0'; // Empty strings are false
    case VAL_NULL:
        return 0;
    case VAL_LIST:
        return 1; // Lists are always true (even empty ones, usually)
    case VAL_NATIVE:
        return 1;
    case VAL_CHAR:
        return v.c != 0;
    case VAL_FILE:
        return v.file != NULL; // Files are truthy if open
    default:
        return 0;
    }
}

static Value eval_expr(Env *e, AstNode *n);
static Value exec_stmt(Env *e, AstNode *n);

// Recursively finds the actual memory location of a variable or list item
// Used for assigning values to specific list indices (e.g. x[0] = 5)
static Value *get_mutable_value(Env *e, AstNode *n)
{
    if (n->kind == NODE_IDENT)
    {
        return env_get(e, n->get<IdentNode>().name.c_str());
    }
    else if (n->kind == NODE_INDEX)
    {
        // Recursively get the parent list
        Value *list = get_mutable_value(e, n->get<IndexNode>().target);
        if (!list || list->type != VAL_LIST)
            return nullptr;

        // Evaluate index
         Value idx = eval_expr(e, n->get<IndexNode>().index);
        if (idx.type != VAL_INT)
        {
            value_free(idx);
            return nullptr;
        }

        // Check bounds
        if (idx.i < 0 || idx.i >= list->list.count)
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "Index %lld is out of bounds for list of length %d", idx.i, list->list.count);
            // Updated to use n->line from the AST node
            error_report(ERR_INDEX, n->line, 0, msg,
                         "Check that your index is between 0 and len(list)-1");
            value_free(idx);
            return nullptr;
        }

        // Return pointer to the specific item slot
        Value *item_ptr = &list->list.items[idx.i];
        value_free(idx);
        return item_ptr;
    }
    return nullptr;
}

// Handles binary operations like +, -, *, /, comparison
static Value eval_binop(BinOpKind op, Value l, Value r)
{
    // 1. Handle Pure Integer Operations separately to preserve precision/types
    if (l.type == VAL_INT && r.type == VAL_INT)
    {
        switch (op)
        {
        case OP_ADD:
            return value_int(l.i + r.i);
        case OP_SUB:
            return value_int(l.i - r.i);
        case OP_MUL:
            return value_int(l.i * r.i);
        case OP_DIV:
            if (r.i == 0)
                return value_int(0);

            // Return float for division to allow decimals
            return value_float(static_cast<double>(l.i) / static_cast<double>(r.i));
        case OP_MOD:
            return value_int(l.i % r.i);
        case OP_EQ:
            return value_bool(l.i == r.i);
        case OP_NEQ:
            return value_bool(l.i != r.i);
        case OP_LT:
            return value_bool(l.i < r.i);
        case OP_GT:
            return value_bool(l.i > r.i);
        case OP_LTE:
            return value_bool(l.i <= r.i);
        case OP_GTE:
            return value_bool(l.i >= r.i);
        default:
            break;
        }
    }

    // 2. Handle Mixed/Float Operations
    if ((l.type == VAL_INT || l.type == VAL_FLOAT) && (r.type == VAL_INT || r.type == VAL_FLOAT))
    {
        double dl = (l.type == VAL_INT) ? (double)l.i : l.f;
        double dr = (r.type == VAL_INT) ? (double)r.i : r.f;
        int is_res_float = 1;

        double res = 0;

        switch (op)
        {
        case OP_ADD:
            res = dl + dr;
            break;
        case OP_SUB:
            res = dl - dr;
            break;
        case OP_MUL:
            res = dl * dr;
            break;
        case OP_DIV:
            res = dr == 0 ? 0 : dl / dr;
            break;
        case OP_MOD:
            res = fmod(dl, dr); // Use fmod to avoid int64 -> double conversion warning
            is_res_float = 0;
            break;

        // Fuzzy Comparison for Floats
        case OP_EQ:
            return value_bool(fabs(dl - dr) < EPSILON);
        case OP_NEQ:
            return value_bool(fabs(dl - dr) >= EPSILON);
        case OP_LT:
            return value_bool(dl < dr);
        case OP_GT:
            return value_bool(dl > dr);
        case OP_LTE:
            return value_bool(dl <= dr);
        case OP_GTE:
            return value_bool(dl >= dr);
        default:
            break;
        }

        if (is_res_float)
        {
            return value_float(res);
        }
        else
        {
            return value_int((long long)res);
        }
    }

    // Handle String Equality
    if (l.type == VAL_STRING && r.type == VAL_STRING)
    {
        if (op == OP_EQ)
            return value_bool(strcmp(l.s, r.s) == 0);
        if (op == OP_NEQ)
            return value_bool(strcmp(l.s, r.s) != 0);
    }

    // Handle Boolean and Null Equality
    if (op == OP_EQ)
    {
        if (l.type == VAL_BOOL && r.type == VAL_BOOL)
            return value_bool(l.b == r.b);
        if (l.type == VAL_NULL && r.type == VAL_NULL)
            return value_bool(1);
        if (l.type == VAL_NULL || r.type == VAL_NULL)
            return value_bool(0);
    }
    if (op == OP_NEQ)
    {
        if (l.type == VAL_BOOL && r.type == VAL_BOOL)
            return value_bool(l.b != r.b);
        if (l.type == VAL_NULL && r.type == VAL_NULL)
            return value_bool(0);
        if (l.type == VAL_NULL || r.type == VAL_NULL)
            return value_bool(1);
    }

    // Handle Char Equality
    if (l.type == VAL_CHAR && r.type == VAL_CHAR)
    {
        if (op == OP_EQ)
            return value_bool(l.c == r.c);
        if (op == OP_NEQ)
            return value_bool(l.c != r.c);
    }

    // Handle String Concatenation
    if (op == OP_ADD && (l.type == VAL_STRING || r.type == VAL_STRING))
    {
        char *sl = value_to_string(l);
        char *sr = value_to_string(r);
        char *comb = (char*)malloc(strlen(sl) + strlen(sr) + 1);
        strcpy(comb, sl);
        strcat(comb, sr);
        free(sl);
        free(sr);
        Value v = value_string(comb);
        free(comb);
        return v;
    }
    if (l.type == VAL_LIST && r.type == VAL_LIST)
    {
        switch (op)
        {
        case OP_ADD:
            return vec_add_values(l, r);
        case OP_SUB:
            return vec_sub_values(l, r);
        case OP_MUL:
            return vec_mul_values(l, r);
        case OP_DIV:
            return vec_div_values(l, r);
        default:
            break; // Fall through for other ops (like ==)
        }
    }
    return value_null();
}

// Evaluates an expression node and returns a Value
static Value eval_expr(Env *e, AstNode *n)
{
    if (!n)
    {
        return value_null();
    }
    luna_current_line = n->line;
    switch (n->kind)
    {
    case NODE_NUMBER:
        return value_int(n->get<NumberNode>().value);  
    case NODE_FLOAT:
        return value_float(n->get<FloatNode>().value); 
    case NODE_STRING:
        return value_string(n->get<StringNode>().text.c_str());
    case NODE_CHAR:
        return value_char(n->get<CharNode>().value);
    case NODE_BOOL:
    return value_bool(n->get<BoolNode>().value);  

    // Recursively evaluate items in a list literal
    case NODE_LIST:
    {
        Value v = value_list();
        ListNode& list_node = n->get<ListNode>();
        for (int i = 0; i < list_node.items.count; i++)
        {
            Value item = eval_expr(e, list_node.items.items[i]);
            value_list_append(&v, item);
            value_free(item);
        }
        return v;
    }

    // Variable lookup
    case NODE_IDENT:
    {
        Value *v = env_get(e, n->get<IdentNode>().name.c_str());
        return v ? value_copy(*v) : value_null();
    }

    case NODE_BINOP:
    {
        BinOpNode& binop = n->get<BinOpNode>();  
        // ADDED: Logic Short-circuiting
        if (binop.op == OP_AND)
        {
            Value l = eval_expr(e, binop.left);  
            if (!is_truthy(l))
            {
                return l;
            }
            value_free(l);
            return eval_expr(e, binop.right);  
        }
        if (binop.op == OP_OR)
        {
            Value l = eval_expr(e, binop.left);  
            if (is_truthy(l))
            {
                return l;
            }
            value_free(l);
            return eval_expr(e, binop.right);  
        }

        Value l = eval_expr(e, binop.left);  
        Value r = eval_expr(e, binop.right);  
        Value res = eval_binop(binop.op, l, r);  
        value_free(l);
        value_free(r);
        return res;
    }

    case NODE_NOT:
    {
        Value v = eval_expr(e, n->get<NotNode>().expr);  
        Value res = value_bool(!is_truthy(v));
        value_free(v);
        return res;
    }
    // List Indexing: list[index]
    case NODE_INDEX:
    {
        IndexNode& index_node = n->get<IndexNode>();
        Value target = eval_expr(e, index_node.target);
        Value idx = eval_expr(e, index_node.index);
        if (target.type == VAL_LIST && idx.type == VAL_INT)
        {
            if (idx.i >= 0 && idx.i < target.list.count)
            {
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
    case NODE_INC:
    {
        Value *v = env_get(e, n->get<IncNode>().name.c_str());
        if (v && v->type == VAL_INT)
        {
            Value old = value_copy(*v);
            v->i++;
            return old; // Post-increment returns old value
        }
        if (v && v->type == VAL_FLOAT)
        {
            Value old = value_copy(*v);
            v->f++;
            return old;
        }
        return value_null();
    }

    // Decrement Operator (--)
    case NODE_DEC:
    {
        Value *v = env_get(e, n->get<DecNode>().name.c_str());
        if (v && v->type == VAL_INT)
        {
            Value old = value_copy(*v);
            v->i--;
            return old; // Post-decrement returns old value
        }
        if (v && v->type == VAL_FLOAT)
        {
            Value old = value_copy(*v);
            v->f--;
            return old;
        }
        return value_null();
    }

    // Function Calls
    case NODE_CALL:
    {
        CallNode& call_node = n->get<CallNode>();
        // Built-in: len()
        if (!strcmp(call_node.name.c_str(), "len"))
        {
            if (call_node.args.count == 1)
            {
                Value v = eval_expr(e, call_node.args.items[0]);
                size_t len = 0;
                if (v.type == VAL_STRING)
                {
                    len = strlen(v.s);
                }
                if (v.type == VAL_LIST)
                {
                    len = v.list.count;
                }
                value_free(v);
                return value_int(len);
            }
        }

        // Built-in: append(list, value)
        if (!strcmp(call_node.name.c_str(), "append"))
        {
            if (call_node.args.count != 2)
            {
                fprintf(stderr, "Runtime Error: append() takes 2 arguments (list, value)\n");
                return value_null();
            }

            // Get the MUTABLE pointer to the list variable, not a copy!
            Value *list_ptr = get_mutable_value(e, call_node.args.items[0]);
            Value item_val = eval_expr(e, call_node.args.items[1]);

            if (list_ptr && list_ptr->type == VAL_LIST)
            {
                value_list_append(list_ptr, item_val);
            }
            else
            {
                // Use node line number
                error_report(ERR_ARGUMENT, n->line, 0,
                             "append() expects a list variable as the first argument",
                             "Use append(myList, value) where myList is a list variable");
            }

            value_free(item_val);
            return value_null(); // Return null (void)
        }

        // Built-in: type() - Returns type name ("int", "long", "float", etc)
        if (!strcmp(call_node.name.c_str(), "type"))
        {
            if (call_node.args.count == 1)
            {
                Value v = eval_expr(e, call_node.args.items[0]);
                const char *tname = "unknown";
                switch (v.type)
                {
                case VAL_INT:
                    // Check magnitude to differentiate int vs long for user
                    // Assumes standard 32-bit int limits for "int" label
                    if (v.i > INT_MAX || v.i < INT_MIN)
                    {
                        tname = "long";
                    }
                    else
                    {
                        tname = "int";
                    }
                    break;
                case VAL_FLOAT:
                    tname = "float";
                    break;
                case VAL_STRING:
                    tname = "string";
                    break;
                case VAL_CHAR:
                    tname = "char";
                    break;
                case VAL_BOOL:
                    tname = "boolean";
                    break;
                case VAL_LIST:
                    tname = "list";
                    break;
                case VAL_NATIVE:
                    tname = "native_function";
                    break;
                case VAL_NULL:
                    tname = "null";
                    break;
                }
                value_free(v);
                return value_string(tname);
            }
        }

        // Built-in: int()
        if (!strcmp(call_node.name.c_str(), "int"))
        {
            if (call_node.args.count == 1)
            {
                Value v = eval_expr(e, call_node.args.items[0]);
                long long res = 0;
                if (v.type == VAL_STRING)
                    res = atoll(v.s);
                else if (v.type == VAL_FLOAT)
                    res = (long long)v.f;
                else if (v.type == VAL_INT)
                    res = v.i;
                else if (v.type == VAL_BOOL)
                    res = v.b;
                else if (v.type == VAL_CHAR)
                    res = (long long)v.c;
                value_free(v);
                return value_int(res);
            }
        }
        // Built-in: float()
        if (!strcmp(call_node.name.c_str(), "float"))
        {
            if (call_node.args.count == 1)
            {
                Value v = eval_expr(e, call_node.args.items[0]);
                double res = 0.0;
                if (v.type == VAL_STRING)
                    res = atof(v.s);
                else if (v.type == VAL_INT)
                    res = (double)v.i;
                else if (v.type == VAL_FLOAT)
                    res = v.f;
                else if (v.type == VAL_BOOL)
                    res = v.b ? 1.0 : 0.0;
                value_free(v);
                return value_float(res);
            }
        }

        // 1. Check for User defined function
        AstNode *fn = env_get_func(e, call_node.name.c_str());
        if (fn)
        {
            // Create new scope for function execution
            Env *scope = env_create(e);

            // Map arguments to parameters
            FuncDefNode& funcdef = fn->get<FuncDefNode>();
            for (size_t i = 0; i < funcdef.params.size(); i++)
            {
                Value v = (i < call_node.args.count) ? eval_expr(e, call_node.args.items[i]) : value_null();
                env_def(scope, funcdef.params[i].c_str(), v);
                value_free(v);
            }

            // Execute function body
            for (int i = 0; i < funcdef.body.count; i++)
            {
                exec_stmt(scope, funcdef.body.items[i]);
                if (return_exception.active)
                    break;
            }

            // Handle return value
            Value ret = return_exception.active ? value_copy(return_exception.value) : value_null();
            if (return_exception.active)
            {
                value_free(return_exception.value);
                return_exception.active = 0;
            }
            env_free(scope);
            return ret;
        }

        // 2. Check for Native Function (Registered in Variables)
        Value *native_val = env_get(e, call_node.name.c_str());
        if (native_val && native_val->type == VAL_NATIVE)
        {
            // Evaluate Arguments first
            int argc = call_node.args.count;
            Value *argv = (Value*)malloc(sizeof(Value) * argc);
            for (int i = 0; i < argc; i++)
            {
                // Fixed it, now it Passes list identifiers by reference to allow in-place modification
                if (call_node.args.items[i]->kind == NODE_IDENT)
                {
                    Value *env_ref = env_get(e, call_node.args.items[i]->get<IdentNode>().name.c_str());
                    if (env_ref && env_ref->type == VAL_LIST)
                    {
                        argv[i] = *env_ref; // Pass direct reference
                    }
                    else
                    {
                        argv[i] = eval_expr(e, call_node.args.items[i]);
                    }
                }
                else
                {
                    argv[i] = eval_expr(e, call_node.args.items[i]);
                }
            }

            // Call the C Function Pointer
            Value res = native_val->native(argc, argv);

            // Clean up arguments
            for (int i = 0; i < argc; i++)
            {
                // Only free if it was a deep-copied expression, not an environment reference
                if (call_node.args.items[i]->kind != NODE_IDENT)
                {
                    value_free(argv[i]);
                }
            }
            free(argv);
            return res;
        }

        return value_null();
    }

    case NODE_INPUT:
    {
        InputNode& input_node = n->get<InputNode>();
        char buf[256];
        if (!input_node.prompt.empty())
        {
            printf("%s", input_node.prompt.c_str());
        }
        if (fgets(buf, 256, stdin))
        {
            buf[strcspn(buf, "\n")] = 0;
        }
        else
        {
            buf[0] = 0;
        }
        return value_string(buf);
    }
    default:
        return value_null();
    }
}

// Executes a statement node (side effects, control flow)
static Value exec_stmt(Env *e, AstNode *n)
{
    if (!n)
    {
        return value_null();
    }

    luna_current_line = n->line;

    switch (n->kind)
    {
    case NODE_LET:
    {
        LetNode& let_node = n->get<LetNode>();
        Value v = eval_expr(e, let_node.expr);
        env_def(e, let_node.name.c_str(), v);
        value_free(v);
        return value_null();
    }
    case NODE_ASSIGN:
    {
        AssignNode& assign_node = n->get<AssignNode>();
        Value v = eval_expr(e, assign_node.expr);
        env_assign(e, assign_node.name.c_str(), v);
        value_free(v);
        return value_null();
    }
    case NODE_ASSIGN_INDEX:
    {
        AssignIndexNode& assign_idx = n->get<AssignIndexNode>();
        Value val = eval_expr(e, assign_idx.value);

        // Get pointer to the actual list item in the environment
        Value *target = get_mutable_value(e, assign_idx.list);

        // Verify target is actually a list
        if (!target || target->type != VAL_LIST)
        {
            // Use node line number
            error_report(ERR_TYPE, n->line, 0,
                         "Cannot assign to non-list target - target must be a list",
                         "Use list indices only on list variables, e.g., myList[0] = value");
            value_free(val);
            return value_null();
        }

        // Evaluate the index
        Value idx = eval_expr(e, assign_idx.index);
        if (idx.type != VAL_INT)
        {
            // Use node line number
            error_report(ERR_TYPE, n->line, 0,
                         "List index must be an integer",
                         "Use integer values for list indices, e.g., myList[0] or myList[i]");
            value_free(val);
            value_free(idx);
            return value_null();
        }

        // Bounds Check
        if (idx.i < 0 || idx.i >= target->list.count)
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "Index %lld is out of bounds for list of length %d",
                     idx.i, target->list.count);
            // Use node line number
            error_report(ERR_INDEX, n->line, 0, msg,
                         "Ensure your index is between 0 and len(list)-1");
            value_free(val);
            value_free(idx);
            return value_null();
        }

        // Assign to the specific slot
        Value *slot = &target->list.items[idx.i];
        value_free(*slot);       // Free the old value in this slot
        *slot = value_copy(val); // Assign the new value

        value_free(val);
        value_free(idx);
        return value_null();
    }
    case NODE_PRINT:
    {
        PrintNode& print_node = n->get<PrintNode>();
        for (int i = 0; i < print_node.args.count; i++)
        {
            Value v = eval_expr(e, print_node.args.items[i]);
            char *s = value_to_string(v);
            printf("%s ", s);
            free(s);
            value_free(v);
        }
        printf("\n");
        return value_null();
    }
    case NODE_IF:
    {
        IfNode& if_node = n->get<IfNode>();
        Value v = eval_expr(e, if_node.cond);
        int t = is_truthy(v);
        value_free(v);

        NodeList block = t ? if_node.then_block : if_node.else_block;
        Env *scope = env_create(e);
        for (int i = 0; i < block.count; i++)
        {
            exec_stmt(scope, block.items[i]);
            // Stop if a control flow event occurred
            if (return_exception.active || loop_exception.break_active || loop_exception.continue_active)
            {
                break;
            }
        }
        env_free(scope);
        return value_null();
    }
    case NODE_WHILE:
    {
        WhileNode& while_node = n->get<WhileNode>();
        while (1)
        {
            Value v = eval_expr(e, while_node.cond);
            int t = is_truthy(v);
            value_free(v);
            if (!t)
            {
                break;
            }

            Env *scope = env_create(e);
            for (int i = 0; i < while_node.body.count; i++)
            {
                exec_stmt(scope, while_node.body.items[i]);
                if (return_exception.active || loop_exception.break_active)
                {
                    break;
                }
                if (loop_exception.continue_active)
                {
                    break;
                }
            }
            env_free(scope);

            if (return_exception.active)
            {
                break;
            }
            if (loop_exception.break_active)
            {
                loop_exception.break_active = 0;
                break;
            }
            if (loop_exception.continue_active)
            {
                loop_exception.continue_active = 0;
                continue;
            }
        }
        return value_null();
    }
    case NODE_FOR:
    {
        ForNode& for_node = n->get<ForNode>();
        Env *scope = env_create(e); // Create scope for the loop variable (i)

        // 1. Run Initializer (once)
        exec_stmt(scope, for_node.init);

        while (1)
        {
            // 2. Check Condition
            Value c = eval_expr(scope, for_node.cond);
            int truthy = is_truthy(c);
            value_free(c);

            if (!truthy)
                break; // Exit loop

            // 3. Execute Body
            // We create a generic inner scope for the body to protect the iterator
            Env *inner_scope = env_create(scope);
            for (int i = 0; i < for_node.body.count; i++)
            {
                exec_stmt(inner_scope, for_node.body.items[i]);
                if (return_exception.active || loop_exception.break_active || loop_exception.continue_active)
                    break;
            }
            env_free(inner_scope);

            if (return_exception.active)
                break;
            if (loop_exception.break_active)
            {
                loop_exception.break_active = 0;
                break;
            }
            // (Continue is handled implicitly by going to the increment step)
            if (loop_exception.continue_active)
            {
                loop_exception.continue_active = 0;
            }

            // 4. Run Increment
            exec_stmt(scope, for_node.incr);
        }

        env_free(scope); // Cleanup loop variable 'i'
        return value_null();
    }
    case NODE_SWITCH:
    {
        SwitchNode& switch_node = n->get<SwitchNode>();
        Value val = eval_expr(e, switch_node.expr);
        int matched = 0;

        // Check all cases
        for (int i = 0; i < switch_node.cases.count; i++)
        {
            AstNode *c = switch_node.cases.items[i];
            Value cval = eval_expr(e, c->get<CaseNode>().value);
            int eq = 0;

            // Compare switch value with case value
            if (val.type == cval.type)
            {
                if (val.type == VAL_INT)
                    eq = (val.i == cval.i);
                else if (val.type == VAL_FLOAT)
                    eq = (val.f == cval.f);
                else if (val.type == VAL_STRING)
                    eq = !strcmp(val.s, cval.s);
                else if (val.type == VAL_BOOL)
                    eq = (val.b == cval.b);
                else if (val.type == VAL_CHAR)
                    eq = (val.c == cval.c);
            }
            else if (val.type == VAL_INT && cval.type == VAL_FLOAT)
                eq = (val.i == cval.f);
            else if (val.type == VAL_FLOAT && cval.type == VAL_INT)
                eq = (val.f == cval.i);
            value_free(cval);

            if (eq)
            {
                matched = 1;
                Env *scope = env_create(e);
                for (int j = 0; j < c->get<CaseNode>().body.count; j++)
                {
                    exec_stmt(scope, c->get<CaseNode>().body.items[j]);
                    if (return_exception.active || loop_exception.continue_active)
                        break;
                    if (loop_exception.break_active)
                        break;
                }
                env_free(scope);
                if (loop_exception.break_active)
                {
                    loop_exception.break_active = 0;
                }
                break;
            }
        }

        // Execute default block if no match found
        if (!matched && switch_node.default_case.count > 0)
        {
            Env *scope = env_create(e);
            for (int j = 0; j < switch_node.default_case.count; j++)
            {
                exec_stmt(scope, switch_node.default_case.items[j]);
                if (return_exception.active || loop_exception.continue_active)
                    break;
                if (loop_exception.break_active)
                    break;
            }
            env_free(scope);
            if (loop_exception.break_active)
            {
                loop_exception.break_active = 0;
            }
        }
        value_free(val);
        return value_null();
    }
    case NODE_BLOCK:
    {
        BlockNode& block_node = n->get<BlockNode>();
        Env *scope = env_create(e);
        for (int i = 0; i < block_node.items.count; i++)
        {
            exec_stmt(scope, block_node.items.items[i]);
            if (return_exception.active || loop_exception.break_active || loop_exception.continue_active)
            {
                break;
            }
        }
        env_free(scope);
        return value_null();
    }
    case NODE_GROUP:
    {
        BlockNode& block_node = n->get<BlockNode>();
        // Execute statements in the CURRENT environment (e)
        for (int i = 0; i < block_node.items.count; i++)
        {
            exec_stmt(e, block_node.items.items[i]);

            // Still need to check for control flow (return/break)
            if (return_exception.active || loop_exception.break_active || loop_exception.continue_active)
            {
                break;
            }
        }
        return value_null();
    }

    case NODE_FUNC_DEF:
    {
        FuncDefNode& funcdef = n->get<FuncDefNode>();
        env_def_func(e, funcdef.name.c_str(), n);
        return value_null();
    }

    case NODE_RETURN:
    {
        ReturnNode& ret_node = n->get<ReturnNode>();
        // 1. Calculate the return value first
        Value v = eval_expr(e, ret_node.expr);
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

    default:
    {
        // Evaluate standalone expressions (e.g., function calls without assignment)
        Value v = eval_expr(e, n);
        value_free(v);
        return value_null();
    }
    }
}

Value interpret(AstNode *prog, Env *env)
{
    // Reset global flags to prevent state leaking between tests
    return_exception.active = 0;
    loop_exception.break_active = 0;
    loop_exception.continue_active = 0;

    if (!prog)
    {
        return value_null();
    }

    // Use the passed environment directly
    if (prog->kind == NODE_BLOCK)
    {
        BlockNode& block_node = prog->get<BlockNode>();
        for (int i = 0; i < block_node.items.count; i++)
        {
            exec_stmt(env, block_node.items.items[i]);
        }
    }
    else
    {
        exec_stmt(env, prog);
    }
    return value_null();
}
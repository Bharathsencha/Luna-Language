// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include "gc.h"
#include "value.h"

// Global Tracker
static Obj *vm_objects = NULL; // Head of the linked list of all objects
static Env *vm_roots = NULL;   // Pointer to the global environment

// Forward declaration of the helper we will implement in env.c
extern void env_mark(Env *env);

void gc_init(Env *env) {
    vm_roots = env;
    vm_objects = NULL;
}

// MARK PHASE

void mark_obj(Obj *obj) {
    if (obj == NULL || obj->is_marked) return;
    
    obj->is_marked = true;

    // "Blacken" the object: recursively mark references inside it
    if (obj->type == OBJ_LIST) {
        ObjList *list = (ObjList*)obj;
        for (int i = 0; i < list->count; i++) {
            mark_value(list->items[i]);
        }
    }
    // Strings don't hold references to other objects, so we are done.
}

void mark_value(Value v) {
    if (IS_OBJ(v)) {
        mark_obj(AS_OBJ(v));
    }
}

static void mark_roots(void) {
    if (vm_roots) {
        env_mark(vm_roots);
    }
}

// Sweep
static void sweep(void) {
    Obj **previous = &vm_objects;
    Obj *current = vm_objects;

    while (current != NULL) {
        if (current->is_marked) {
            // Object is reachable. Unmark it for the next run.
            current->is_marked = false;
            previous = &current->next;
            current = current->next;
        } else {
            // Object is garbage. Remove and free.
            Obj *unreached = current;
            current = current->next;
            *previous = current; // Unlink

            // Free the memory
            if (unreached->type == OBJ_STRING) {
                ObjString *s = (ObjString*)unreached;
                free(s->chars);
            } else if (unreached->type == OBJ_LIST) {
                ObjList *l = (ObjList*)unreached;
                free(l->items);
            }
            free(unreached);
        }
    }
}

// PUBLIC INTERFACE

void gc_collect(void) {
    mark_roots();
    sweep();
}

void gc_free_all(void) {
    // Free everything without marking
    Obj *current = vm_objects;
    while (current != NULL) {
        Obj *next = current->next;
        if (current->type == OBJ_STRING) {
            free(((ObjString*)current)->chars);
        } else if (current->type == OBJ_LIST) {
            free(((ObjList*)current)->items);
        }
        free(current);
        current = next;
    }
    vm_objects = NULL;
}

Obj *gc_allocate(size_t size, ObjType type) {
    // Trigger GC here based on memory pressure heuristics
    // For now, we only collect manually to keep it simple.
    
    Obj *object = (Obj*)malloc(size);
    object->type = type;
    object->is_marked = false;
    
    // Insert into the global tracking list
    object->next = vm_objects;
    vm_objects = object;
    
    return object;
}
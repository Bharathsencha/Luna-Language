// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// gui_lib.c — Luna GUI library implementation
// Now uses gl_backend + audio_backend.

#include "gl_backend.h"
#include "audio_backend.h"
#include "gui_lib.h"
#include "../include/intern.h"
#include "../include/value.h"
#include "../include/env.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#pragma GCC diagnostic ignored "-Wunused-parameter"

// PCM Sample Ring Buffer for Audio Visualizer
#define FFT_BUFFER_SIZE 2048

// Static State & Resource Managers
static float layout_cursor_y = 20.0f;
static const float padding = 10.0f;
static const float widget_height = 30.0f;
static const float margin_x = 20.0f;

#define MAX_FONTS 16
static int font_ids[MAX_FONTS];  // GFont handles
static int font_count = 0;

#define MAX_TEXTURES 128
static int texture_ids[MAX_TEXTURES]; // GTexture handles
static int texture_widths[MAX_TEXTURES];
static int texture_heights[MAX_TEXTURES];
static int texture_count = 0;

#define MAX_MUSIC 16
static int music_ids[MAX_MUSIC]; // AMusic handles
static int music_count = 0;

#define MAX_SOUNDS 16
static int sound_ids[MAX_SOUNDS]; // ASound handles
static int sound_count = 0;

#define MAX_IMAGES 16
static int image_ids[MAX_IMAGES]; // GImage handles
static int image_count = 0;

// Internal Helpers

static double val_to_double(Value v) {
    if (v.type == VAL_INT) return (double)v.i;
    if (v.type == VAL_FLOAT) return v.f;
    return 0.0;
}


// Extract GColor from Luna List [r, g, b, a]
static GColor val_to_color(Value v) {
    if (v.type != VAL_LIST || v.list->count < 3) return GCOLOR_WHITE;
    GColor c;
    c.r = (unsigned char)v.list->items[0].i;
    c.g = (unsigned char)v.list->items[1].i;
    c.b = (unsigned char)v.list->items[2].i;
    c.a = (v.list->count > 3) ? (unsigned char)v.list->items[3].i : 255;
    return c;
}

void register_color(Env *env, const char *name, GColor c) {
    Value list = value_list();
    value_list_append(&list, value_int(c.r));
    value_list_append(&list, value_int(c.g));
    value_list_append(&list, value_int(c.b));
    value_list_append(&list, value_int(c.a));
    env_def(env, intern_string(name), list);
}

// Lifecycle & System

Value lib_gui_create_particle_pool(int argc, Value *argv, struct Env *env);

Value lib_gui_init(int argc, Value *argv, Env *env) {
    if (argc < 3) return value_null();
    gl_init_window((int)argv[0].i, (int)argv[1].i, argv[2].string->chars);

    // Register predefined colors
    register_color(env, "RED", (GColor){230, 41, 55, 255});
    register_color(env, "GREEN", (GColor){0, 228, 48, 255});
    register_color(env, "BLUE", (GColor){0, 121, 241, 255});
    register_color(env, "GOLD", (GColor){255, 203, 0, 255});
    register_color(env, "BLACK", (GColor){0, 0, 0, 255});
    register_color(env, "WHITE", (GColor){255, 255, 255, 255});
    register_color(env, "DARKGRAY", (GColor){80, 80, 80, 255});
    register_color(env, "SKYBLUE", (GColor){102, 191, 255, 255});
    
    // Basic Colors
    register_color(env, "YELLOW", (GColor){255, 255, 0, 255});
    register_color(env, "ORANGE", (GColor){255, 165, 0, 255});
    register_color(env, "PURPLE", (GColor){128, 0, 128, 255});
    register_color(env, "CYAN", (GColor){0, 255, 255, 255});
    register_color(env, "MAGENTA", (GColor){255, 0, 255, 255});
    register_color(env, "GRAY", (GColor){128, 128, 128, 255});
    register_color(env, "DARK_GRAY", (GColor){64, 64, 64, 255});
    register_color(env, "LIGHT_GRAY", (GColor){192, 192, 192, 255});
    
    // Extended Colors
    register_color(env, "PINK", (GColor){255, 192, 203, 255});
    register_color(env, "BROWN", (GColor){165, 42, 42, 255});
    register_color(env, "SILVER", (GColor){192, 192, 192, 255});
    register_color(env, "NAVY", (GColor){0, 0, 128, 255});
    register_color(env, "TEAL", (GColor){0, 128, 128, 255});
    register_color(env, "LIME", (GColor){0, 255, 0, 255});
    register_color(env, "MAROON", (GColor){128, 0, 0, 255});
    register_color(env, "OLIVE", (GColor){128, 128, 0, 255});
    register_color(env, "INDIGO", (GColor){75, 0, 130, 255});
    register_color(env, "VIOLET", (GColor){238, 130, 238, 255});
    
    // Shades
    register_color(env, "DARK_RED", (GColor){139, 0, 0, 255});
    register_color(env, "DARK_GREEN", (GColor){0, 100, 0, 255});
    register_color(env, "DARK_BLUE", (GColor){0, 0, 139, 255});
    register_color(env, "LIGHT_RED", (GColor){255, 102, 102, 255});
    register_color(env, "LIGHT_GREEN", (GColor){144, 238, 144, 255});
    register_color(env, "LIGHT_BLUE", (GColor){173, 216, 230, 255});
    
    register_color(env, "TRANSPARENT", (GColor){0, 0, 0, 0});
    
    // Register Key Constants (GLFW key values)
    env_def(env, intern_string("KEY_SPACE"), value_int(GKEY_SPACE));
    env_def(env, intern_string("KEY_ESCAPE"), value_int(GKEY_ESCAPE));
    env_def(env, intern_string("KEY_ENTER"), value_int(GKEY_ENTER));
    env_def(env, intern_string("KEY_TAB"), value_int(GKEY_TAB));
    env_def(env, intern_string("KEY_BACKSPACE"), value_int(GKEY_BACKSPACE));
    
    env_def(env, intern_string("KEY_RIGHT"), value_int(GKEY_RIGHT));
    env_def(env, intern_string("KEY_LEFT"), value_int(GKEY_LEFT));
    env_def(env, intern_string("KEY_DOWN"), value_int(GKEY_DOWN));
    env_def(env, intern_string("KEY_UP"), value_int(GKEY_UP));
    
    // WASD + Common
    env_def(env, intern_string("KEY_W"), value_int(GKEY_W));
    env_def(env, intern_string("KEY_A"), value_int(GKEY_A));
    env_def(env, intern_string("KEY_S"), value_int(GKEY_S));
    env_def(env, intern_string("KEY_D"), value_int(GKEY_D));
    env_def(env, intern_string("KEY_P"), value_int(GKEY_P));
    env_def(env, intern_string("KEY_R"), value_int(GKEY_R));
    env_def(env, intern_string("KEY_M"), value_int(GKEY_M));
    
    // IJKL Navigation
    env_def(env, intern_string("KEY_I"), value_int(GKEY_I));
    env_def(env, intern_string("KEY_J"), value_int(GKEY_J));
    env_def(env, intern_string("KEY_K"), value_int(GKEY_K));
    env_def(env, intern_string("KEY_L"), value_int(GKEY_L));
    
    // Other Common Action Keys
    env_def(env, intern_string("KEY_Q"), value_int(GKEY_Q));
    env_def(env, intern_string("KEY_E"), value_int(GKEY_E));
    env_def(env, intern_string("KEY_Z"), value_int(GKEY_Z));
    env_def(env, intern_string("KEY_C"), value_int(GKEY_C));
    env_def(env, intern_string("KEY_X"), value_int(GKEY_X));
    env_def(env, intern_string("KEY_F"), value_int(GKEY_F));
    env_def(env, intern_string("KEY_H"), value_int(GKEY_H));
    
    // Remaining A-Z
    env_def(env, intern_string("KEY_B"), value_int(GKEY_B));
    env_def(env, intern_string("KEY_G"), value_int(GKEY_G));
    env_def(env, intern_string("KEY_N"), value_int(GKEY_N));
    env_def(env, intern_string("KEY_O"), value_int(GKEY_O));
    env_def(env, intern_string("KEY_T"), value_int(GKEY_T));
    env_def(env, intern_string("KEY_U"), value_int(GKEY_U));
    env_def(env, intern_string("KEY_V"), value_int(GKEY_V));
    env_def(env, intern_string("KEY_Y"), value_int(GKEY_Y));
    
    // Numbers 0-9 (GLFW uses ASCII values for number keys)
    env_def(env, intern_string("KEY_0"), value_int(GKEY_0));
    env_def(env, intern_string("KEY_1"), value_int(GKEY_1));
    env_def(env, intern_string("KEY_2"), value_int(GKEY_2));
    env_def(env, intern_string("KEY_3"), value_int(GKEY_3));
    env_def(env, intern_string("KEY_4"), value_int(GKEY_4));
    env_def(env, intern_string("KEY_5"), value_int(GKEY_5));
    env_def(env, intern_string("KEY_6"), value_int(GKEY_6));
    env_def(env, intern_string("KEY_7"), value_int(GKEY_7));
    env_def(env, intern_string("KEY_8"), value_int(GKEY_8));
    env_def(env, intern_string("KEY_9"), value_int(GKEY_9));
    
    // Mouse Buttons
    env_def(env, intern_string("MOUSE_LEFT_BUTTON"), value_int(GMOUSE_LEFT));
    env_def(env, intern_string("MOUSE_RIGHT_BUTTON"), value_int(GMOUSE_RIGHT));
    env_def(env, intern_string("MOUSE_MIDDLE_BUTTON"), value_int(GMOUSE_MIDDLE));
    
    // Register Particle Pool Helper
    env_def(env, intern_string("create_particle_pool"), value_native(lib_gui_create_particle_pool));

    gl_set_target_fps(60);
    return value_null();
}

Value lib_gui_window_open(int argc, Value *argv, struct Env *env) {
    return value_bool(!gl_window_should_close());
}

Value lib_gui_close_window(int argc, Value *argv, struct Env *env) {
    gl_close_window();
    return value_null();
}

Value lib_gui_set_fps(int argc, Value *argv, Env *env) {
    if (argc > 0) gl_set_target_fps((int)val_to_double(argv[0]));
    return value_null();
}

Value lib_gui_get_delta_time(int argc, Value *argv, Env *env) {
    return value_float(gl_get_frame_time());
}

Value lib_gui_set_opacity(int argc, Value *argv, Env *env) {
    if (argc > 0) gl_set_window_opacity((float)val_to_double(argv[0]));
    return value_null();
}

// Frame Management

Value lib_gui_begin(int argc, Value *argv, Env *env) {
    gl_begin_drawing();
    layout_cursor_y = 20.0f; 
    return value_null();
}

Value lib_gui_end(int argc, Value *argv, Env *env) {
    gl_end_drawing();
    return value_null();
}

Value lib_gui_clear(int argc, Value *argv, Env *env) {
    if (argc >= 4) {
        gl_clear((GColor){
            (unsigned char)val_to_double(argv[0]),
            (unsigned char)val_to_double(argv[1]),
            (unsigned char)val_to_double(argv[2]),
            (unsigned char)val_to_double(argv[3])
        });
    } else if (argc == 1) {
        gl_clear(val_to_color(argv[0]));
    } else {
        gl_clear((GColor){10, 10, 15, 255});
    }
    return value_null();
}

// Unified Shapes

Value lib_gui_draw_rect(int argc, Value *argv, Env *env) {
    if (argc < 4) return value_null();
    GRect rec = { (float)val_to_double(argv[0]), (float)val_to_double(argv[1]), 
                  (float)val_to_double(argv[2]), (float)val_to_double(argv[3]) };
    
    // Check for overloaded signature: (x, y, w, h, Color)
    if (argc == 5 && argv[4].type == VAL_LIST) {
        gl_draw_rect(rec, val_to_color(argv[4]));
        return value_null();
    }

    // Overload: (x, y, w, h, r, g, b, a) — 8 numeric args = solid fill with inline color
    if (argc == 8) {
        GColor col = {
            (unsigned char)val_to_double(argv[4]),
            (unsigned char)val_to_double(argv[5]),
            (unsigned char)val_to_double(argv[6]),
            (unsigned char)val_to_double(argv[7])
        };
        gl_draw_rect(rec, col);
        return value_null();
    }

    float thick = (argc >= 5) ? (float)val_to_double(argv[4]) : -1.0f;
    float radius = (argc >= 6) ? (float)val_to_double(argv[5]) : 0.0f;
    GColor col = (argc >= 7) ? val_to_color(argv[6]) : (GColor){190, 33, 55, 255}; // MAROON

    if (radius > 0) {
        if (thick < 0) gl_draw_rect_rounded(rec, radius, 20, col);
        else gl_draw_rect_rounded_lines(rec, radius, 20, col); 
    } else {
        if (thick < 0) gl_draw_rect(rec, col);
        else gl_draw_rect_lines(rec, thick, col);
    }
    return value_null();
}

Value lib_gui_draw_circle(int argc, Value *argv, Env *env) {
    if (argc < 3) return value_null();
    float x = (float)val_to_double(argv[0]);
    float y = (float)val_to_double(argv[1]);
    float r = (float)val_to_double(argv[2]);
    GColor col = (argc >= 4) ? val_to_color(argv[3]) : GCOLOR_BLUE;
    gl_draw_circle((int)x, (int)y, r, col);
    return value_null();
}

Value lib_gui_draw_line(int argc, Value *argv, Env *env) {
    if (argc < 5) return value_null();
    GVec2 start = { (float)val_to_double(argv[0]), (float)val_to_double(argv[1]) };
    GVec2 end   = { (float)val_to_double(argv[2]), (float)val_to_double(argv[3]) };

    // Overload: (x1, y1, x2, y2, color) — 5 args, 5th is list → default thickness 1
    if (argc == 5 && argv[4].type == VAL_LIST) {
        gl_draw_line(start, end, 1.0f, val_to_color(argv[4]));
        return value_null();
    }

    float thick = (float)val_to_double(argv[4]);
    if (thick < 0.5f) thick = 1.0f;
    GColor col = (argc >= 6) ? val_to_color(argv[5]) : GCOLOR_BLACK;
    gl_draw_line(start, end, thick, col);
    return value_null();
}

Value lib_gui_draw_gradient_v(int argc, Value *argv, struct Env *env) {
    if (argc < 6) return value_null();
    gl_draw_rect_gradient_v((int)val_to_double(argv[0]), (int)val_to_double(argv[1]), 
                            (int)val_to_double(argv[2]), (int)val_to_double(argv[3]), 
                            val_to_color(argv[4]), val_to_color(argv[5]));
    return value_null();
}

Value lib_gui_draw_rectangle_rec(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_null();
    if (argv[0].type != VAL_LIST || argv[0].list->count < 4) return value_null();
    GRect rec = { 
        (float)val_to_double(argv[0].list->items[0]), 
        (float)val_to_double(argv[0].list->items[1]), 
        (float)val_to_double(argv[0].list->items[2]), 
        (float)val_to_double(argv[0].list->items[3]) 
    };
    GColor col = val_to_color(argv[1]);
    gl_draw_rect(rec, col);
    return value_null();
}

Value lib_gui_draw_rectangle_lines(int argc, Value *argv, struct Env *env) {
    if (argc < 5) return value_null();
    GRect rec = {
        (float)val_to_double(argv[0]),
        (float)val_to_double(argv[1]),
        (float)val_to_double(argv[2]),
        (float)val_to_double(argv[3])
    };
    gl_draw_rect_lines(rec, 2.0f, val_to_color(argv[4]));
    return value_null();
}

// Widgets

Value lib_gui_label(int argc, Value *argv, Env *env) {
    if (argc < 1) return value_null();
    gl_draw_text(argv[0].string->chars, (int)margin_x, (int)layout_cursor_y, 20, GCOLOR_DARKGRAY);
    layout_cursor_y += widget_height + padding;
    return value_null();
}

Value lib_gui_button(int argc, Value *argv, Env *env) {
    if (argc < 1) return value_bool(0);
    GRect bounds = { margin_x, layout_cursor_y, 150, widget_height };
    GVec2 mouse = gl_get_mouse_position();
    int hover = gl_check_collision_point_rect(mouse, bounds);
    int clicked = hover && gl_is_mouse_button_pressed(GMOUSE_LEFT);
    gl_draw_rect(bounds, hover ? GCOLOR_LIGHTGRAY : GCOLOR_GRAY);
    gl_draw_text(argv[0].string->chars, (int)bounds.x + 10, (int)bounds.y + 5, 20, GCOLOR_BLACK);
    layout_cursor_y += widget_height + padding;
    return value_bool(clicked);
}

Value lib_gui_slider(int argc, Value *argv, Env *env) {
    if (argc < 4) return value_null();
    const char* var_name = argv[0].string->chars;
    float min = (float)val_to_double(argv[1]);
    float max = (float)val_to_double(argv[2]);
    Value *val = env_get(env, var_name); 
    if (!val) return value_null();
    float current_f = (float)val_to_double(*val);
    GRect bounds = { margin_x, layout_cursor_y, 200, widget_height };
    GVec2 mouse = gl_get_mouse_position();
    if (gl_check_collision_point_rect(mouse, bounds) && gl_is_mouse_button_down(GMOUSE_LEFT)) {
        float pct = (mouse.x - bounds.x) / bounds.w;
        if (pct < 0) pct = 0; 
        if (pct > 1) pct = 1;
        current_f = min + (max - min) * pct;
        if (val->type == VAL_INT) val->i = (long long)current_f;
        else val->f = (double)current_f;
    }
    gl_draw_rect(bounds, GCOLOR_LIGHTGRAY);
    float fill_w = ((current_f - min) / (max - min)) * 200;
    gl_draw_rect_at((int)bounds.x, (int)bounds.y, (int)fill_w, (int)bounds.h, GCOLOR_BLUE);
    gl_draw_text(argv[3].string->chars, (int)(bounds.x + 210), (int)bounds.y + 5, 20, GCOLOR_BLACK);
    layout_cursor_y += widget_height + padding;
    return value_null();
}

// Input

Value lib_gui_get_mouse(int argc, Value *argv, Env *env) {
    GVec2 mouse = gl_get_mouse_position();
    Value list = value_list();
    value_list_append(&list, value_float(mouse.x));
    value_list_append(&list, value_float(mouse.y));
    return list;
}

Value lib_gui_get_mouse_wheel_move(int argc, Value *argv, Env *env) {
    return value_float(gl_get_mouse_wheel());
}

Value lib_gui_is_key_down(int argc, Value *argv, Env *env) {
    if (argc < 1) return value_bool(0);
    return value_bool(gl_is_key_down((int)val_to_double(argv[0])));
}

Value lib_gui_is_key_pressed(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_bool(0);
    return value_bool(gl_is_key_pressed((int)val_to_double(argv[0])));
}

Value lib_gui_is_mouse_button_pressed(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_bool(0);
    return value_bool(gl_is_mouse_button_pressed((int)val_to_double(argv[0])));
}

Value lib_gui_is_mouse_button_down(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_bool(0);
    return value_bool(gl_is_mouse_button_down((int)val_to_double(argv[0])));
}

Value lib_gui_check_collision_point_rec(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_bool(0);
    if (argv[0].type != VAL_LIST || argv[0].list->count < 2) return value_bool(0);
    if (argv[1].type != VAL_LIST || argv[1].list->count < 4) return value_bool(0);

    GVec2 point = { (float)val_to_double(argv[0].list->items[0]), (float)val_to_double(argv[0].list->items[1]) };
    GRect rec = { 
        (float)val_to_double(argv[1].list->items[0]), 
        (float)val_to_double(argv[1].list->items[1]), 
        (float)val_to_double(argv[1].list->items[2]), 
        (float)val_to_double(argv[1].list->items[3]) 
    };

    return value_bool(gl_check_collision_point_rect(point, rec));
}

// Color Utilities

Value lib_gui_rgb(int argc, Value *argv, Env *env) {
    Value list = value_list();
    for (int i = 0; i < 3; i++) value_list_append(&list, (i < argc) ? argv[i] : value_int(0));
    value_list_append(&list, (argc > 3) ? argv[3] : value_int(255));
    return list;
}

Value lib_gui_hsl(int argc, Value *argv, Env *env) {
    if (argc < 3) return value_null();
    GColor c = gl_color_from_hsv((float)val_to_double(argv[0]), (float)val_to_double(argv[1]), (float)val_to_double(argv[2]));
    Value list = value_list();
    value_list_append(&list, value_int(c.r));
    value_list_append(&list, value_int(c.g));
    value_list_append(&list, value_int(c.b));
    value_list_append(&list, value_int(255));
    return list;
}

// Textures & Rotation

Value lib_gui_load_texture(int argc, Value *argv, Env *env) {
    if (argc < 1 || texture_count >= MAX_TEXTURES) return value_int(-1);
    int id = gl_load_texture(argv[0].string->chars);
    if (id < 0) return value_int(-1);
    texture_ids[texture_count] = id;
    texture_widths[texture_count] = gl_get_texture_width(id);
    texture_heights[texture_count] = gl_get_texture_height(id);
    return value_int(texture_count++);
}

Value lib_gui_draw_texture(int argc, Value *argv, Env *env) {
    if (argc < 3) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < texture_count) {
        gl_draw_texture(texture_ids[id], (int)val_to_double(argv[1]), (int)val_to_double(argv[2]), GCOLOR_WHITE);
    }
    return value_null();
}

Value lib_gui_draw_texture_rot(int argc, Value *argv, struct Env *env) {
    if (argc < 4) return value_null();
    int id = (int)argv[0].i;
    if (id < 0 || id >= texture_count) return value_null();
    float x = (float)val_to_double(argv[1]);
    float y = (float)val_to_double(argv[2]);
    float rot = (float)val_to_double(argv[3]);
    int tw = texture_widths[id];
    int th = texture_heights[id];
    // Center origin rotation
    GRect src = {0, 0, (float)tw, (float)th};
    GRect dest = {x, y, (float)tw, (float)th};
    GVec2 origin = {(float)tw/2.0f, (float)th/2.0f};
    gl_draw_texture_pro(texture_ids[id], src, dest, origin, rot, GCOLOR_WHITE);
    return value_null();
}

Value lib_gui_draw_texture_pro(int argc, Value *argv, struct Env *env) {
    if (argc < 6) return value_null();
    int id = (int)argv[0].i; 
    if (id < 0 || id >= texture_count) return value_null();
    
    if (argv[1].type != VAL_LIST || argv[1].list->count < 4) return value_null();
    if (argv[2].type != VAL_LIST || argv[2].list->count < 4) return value_null();
    if (argv[3].type != VAL_LIST || argv[3].list->count < 2) return value_null();

    GRect source = { 
        (float)val_to_double(argv[1].list->items[0]), (float)val_to_double(argv[1].list->items[1]), 
        (float)val_to_double(argv[1].list->items[2]), (float)val_to_double(argv[1].list->items[3]) 
    };
    GRect dest = { 
        (float)val_to_double(argv[2].list->items[0]), (float)val_to_double(argv[2].list->items[1]), 
        (float)val_to_double(argv[2].list->items[2]), (float)val_to_double(argv[2].list->items[3]) 
    };
    GVec2 origin = { 
        (float)val_to_double(argv[3].list->items[0]), (float)val_to_double(argv[3].list->items[1]) 
    };
    float rotation = (float)val_to_double(argv[4]);
    GColor tint = val_to_color(argv[5]);

    gl_draw_texture_pro(texture_ids[id], source, dest, origin, rotation, tint);
    return value_null();
}

Value lib_gui_get_texture_width(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_int(0);
    int id = (int)argv[0].i;
    if (id < 0 || id >= texture_count) return value_int(0);
    return value_int(texture_widths[id]);
}

Value lib_gui_get_texture_height(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_int(0);
    int id = (int)argv[0].i;
    if (id < 0 || id >= texture_count) return value_int(0);
    return value_int(texture_heights[id]);
}

Value lib_gui_unload_texture(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < texture_count) {
        gl_unload_texture(texture_ids[id]);
    }
    return value_null();
}

// Audio System

Value lib_gui_init_audio(int argc, Value *argv, struct Env *env) {
    audio_init();
    return value_null();
}

Value lib_gui_close_audio_device(int argc, Value *argv, struct Env *env) {
    audio_close();
    return value_null();
}

Value lib_gui_load_music(int argc, Value *argv, struct Env *env) {
    if (argc < 1 || music_count >= MAX_MUSIC) return value_int(-1);
    int id = audio_load_music(argv[0].string->chars);
    if (id < 0) return value_int(-1);
    music_ids[music_count] = id;
    return value_int(music_count++);
}

Value lib_gui_unload_music_stream(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < music_count) audio_unload_music(music_ids[id]);
    return value_null();
}

Value lib_gui_load_sound(int argc, Value *argv, struct Env *env) {
    if (argc < 1 || sound_count >= MAX_SOUNDS) return value_int(-1);
    int id = audio_load_sound(argv[0].string->chars);
    if (id < 0) return value_int(-1);
    sound_ids[sound_count] = id;
    return value_int(sound_count++);
}

Value lib_gui_unload_sound(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < sound_count) audio_unload_sound(sound_ids[id]);
    return value_null();
}

Value lib_gui_play_music(int argc, Value *argv, struct Env *env) {
    if (argc > 0) audio_play_music(music_ids[(int)argv[0].i]);
    return value_null();
}

Value lib_gui_stop_music_stream(int argc, Value *argv, struct Env *env) {
    if (argc > 0) audio_stop_music(music_ids[(int)argv[0].i]);
    return value_null();
}

Value lib_gui_pause_music_stream(int argc, Value *argv, struct Env *env) {
    if (argc > 0) audio_pause_music(music_ids[(int)argv[0].i]);
    return value_null();
}

Value lib_gui_resume_music_stream(int argc, Value *argv, struct Env *env) {
    if (argc > 0) audio_resume_music(music_ids[(int)argv[0].i]);
    return value_null();
}

Value lib_gui_update_music(int argc, Value *argv, struct Env *env) {
    if (argc > 0) audio_update_music(music_ids[(int)argv[0].i]);
    return value_null();
}

Value lib_gui_play_sound(int argc, Value *argv, struct Env *env) {
    if (argc > 0) audio_play_sound(sound_ids[(int)argv[0].i]);
    return value_null();
}

Value lib_gui_get_music_fft(int argc, Value *argv, struct Env *env) {
    #define NUM_BANDS 32
    
    // Get PCM data from audio backend
    float samples[FFT_BUFFER_SIZE];
    int buffer_size = 0;
    audio_get_pcm_buffer(samples, &buffer_size);
    if (buffer_size == 0) buffer_size = FFT_BUFFER_SIZE;
    
    // Simple DFT: compute magnitude at NUM_BANDS logarithmically spaced frequencies
    float band_freqs[NUM_BANDS];
    float base_freq = 60.0f;
    float freq_ratio = powf(16000.0f / 60.0f, 1.0f / (NUM_BANDS - 1));
    for (int b = 0; b < NUM_BANDS; b++) {
        band_freqs[b] = base_freq * powf(freq_ratio, (float)b);
    }
    
    float magnitudes[NUM_BANDS];
    float max_mag = 0.0001f;
    int N = 1024;
    float sample_rate = 48000.0f;
    
    for (int b = 0; b < NUM_BANDS; b++) {
        float freq = band_freqs[b];
        float real = 0.0f;
        float imag = 0.0f;
        float omega = 2.0f * 3.14159265f * freq / sample_rate;
        
        int start = buffer_size - N;
        if (start < 0) start = 0;
        for (int n = 0; n < N && (start + n) < buffer_size; n++) {
            float s = samples[start + n];
            real += s * cosf(omega * n);
            imag += s * sinf(omega * n);
        }
        
        magnitudes[b] = sqrtf(real * real + imag * imag) / (float)N;
        if (magnitudes[b] > max_mag) max_mag = magnitudes[b];
    }
    
    Value result = value_list();
    for (int b = 0; b < NUM_BANDS; b++) {
        float normalized = magnitudes[b] / max_mag;
        value_list_append(&result, value_float((double)normalized));
    }
    
    return result;
}

Value lib_gui_get_music_time_length(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_float(0.0f);
    int id = (int)argv[0].i;
    if (id >= 0 && id < music_count) {
        return value_float(audio_get_music_length(music_ids[id]));
    }
    return value_float(0.0f);
}

Value lib_gui_get_music_time_played(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_float(0.0f);
    int id = (int)argv[0].i;
    if (id >= 0 && id < music_count) {
        return value_float(audio_get_music_played(music_ids[id]));
    }
    return value_float(0.0f);
}

Value lib_gui_seek_music_stream(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_null();
    int id = (int)argv[0].i;
    float position = (float)val_to_double(argv[1]);
    if (id >= 0 && id < music_count) {
        audio_seek_music(music_ids[id], position);
    }
    return value_null();
}

// Fonts

Value lib_gui_load_font(int argc, Value *argv, Env *env) {
    if (argc < 2 || font_count >= MAX_FONTS) return value_int(-1);
    int id = gl_load_font(argv[0].string->chars, (int)val_to_double(argv[1]));
    if (id < 0) return value_int(-1);
    font_ids[font_count] = id;
    return value_int(font_count++);
}

Value lib_gui_draw_text(int argc, Value *argv, Env *env) {
    if (argc < 6) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < font_count) {
        GVec2 pos = {(float)val_to_double(argv[2]), (float)val_to_double(argv[3])};
        GColor color = (argc >= 7) ? val_to_color(argv[6]) : GCOLOR_WHITE;
        gl_draw_text_ex(font_ids[id], argv[1].string->chars, pos,
                        (float)val_to_double(argv[4]), (float)val_to_double(argv[5]), color);
    }
    return value_null();
}

Value lib_gui_measure_text(int argc, Value *argv, struct Env *env) {
    if (argc == 2) {
        return value_int(gl_measure_text(argv[0].string->chars, (int)val_to_double(argv[1])));
    }
    else if (argc >= 4) {
        int id = (int)argv[0].i;
        if (id >= 0 && id < font_count) {
            GVec2 size = gl_measure_text_ex(font_ids[id], argv[1].string->chars, 
                                            (float)val_to_double(argv[2]), (float)val_to_double(argv[3]));
            return value_int((int)size.x);
        }
    }
    return value_int(0);
}

Value lib_gui_draw_text_default(int argc, Value *argv, struct Env *env) {
    if (argc < 5) return value_null();
    gl_draw_text(argv[0].string->chars, (int)val_to_double(argv[1]), (int)val_to_double(argv[2]), 
                 (int)val_to_double(argv[3]), val_to_color(argv[4]));
    return value_null();
}

// Camera System

Value lib_gui_begin_mode_2d(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    if (argv[0].type != VAL_LIST || argv[0].list->count < 6) return value_null();
    
    GCamera2D cam;
    cam.offset.x = (float)val_to_double(argv[0].list->items[0]);
    cam.offset.y = (float)val_to_double(argv[0].list->items[1]);
    cam.target.x = (float)val_to_double(argv[0].list->items[2]);
    cam.target.y = (float)val_to_double(argv[0].list->items[3]);
    cam.rotation = (float)val_to_double(argv[0].list->items[4]);
    cam.zoom = (float)val_to_double(argv[0].list->items[5]);
    
    gl_begin_mode_2d(cam);
    return value_null();
}

Value lib_gui_end_mode_2d(int argc, Value *argv, struct Env *env) {
    gl_end_mode_2d();
    return value_null();
}

// Image Manipulation

Value lib_gui_load_image(int argc, Value *argv, struct Env *env) {
    if (argc < 1 || image_count >= MAX_IMAGES) return value_int(-1);
    int id = gl_load_image(argv[0].string->chars);
    if (id < 0) return value_int(-1);
    image_ids[image_count] = id;
    return value_int(image_count++);
}

Value lib_gui_image_rotate_cw(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < image_count) {
        gl_image_rotate_cw(image_ids[id]);
    }
    return value_null();
}

Value lib_gui_load_texture_from_image(int argc, Value *argv, struct Env *env) {
    if (argc < 1 || texture_count >= MAX_TEXTURES) return value_int(-1);
    int img_id = (int)argv[0].i;
    if (img_id < 0 || img_id >= image_count) return value_int(-1);
    
    int tex_id = gl_load_texture_from_image(image_ids[img_id]);
    if (tex_id < 0) return value_int(-1);
    texture_ids[texture_count] = tex_id;
    texture_widths[texture_count] = gl_get_texture_width(tex_id);
    texture_heights[texture_count] = gl_get_texture_height(tex_id);
    return value_int(texture_count++);
}

Value lib_gui_unload_image(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < image_count) {
        gl_unload_image(image_ids[id]);
    }
    return value_null();
}

// Particle System Helper (unchanged)

Value lib_gui_create_particle_pool(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int count = (int)val_to_double(argv[0]);
    
    Value list = value_list();
    list.list->items = malloc(sizeof(Value) * count);
    list.list->capacity = count;
    list.list->count = count;
    
    for (int i = 0; i < count; i++) {
        Value p = value_list();
        value_list_append(&p, value_float(0.0)); // x
        value_list_append(&p, value_float(0.0)); // y
        value_list_append(&p, value_float(0.0)); // angle
        value_list_append(&p, value_float(0.0)); // speed
        value_list_append(&p, value_float(0.0)); // hue
        value_list_append(&p, value_float(0.0)); // size
        value_list_append(&p, value_int(0));     // active
        list.list->items[i] = p;
    }
    
    return list;
}

// Album Art Loader (raw C)

static unsigned int syncsafe_to_int(unsigned char *bytes) {
    return (bytes[0] << 21) | (bytes[1] << 14) | (bytes[2] << 7) | bytes[3];
}

static unsigned int unsynchsafe_to_int(unsigned char *bytes) {
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

Value lib_gui_load_music_cover(int argc, Value *argv, struct Env *env) {
    if (argc < 1 || texture_count >= MAX_TEXTURES) return value_int(-1);
    
    const char *filename = argv[0].string->chars;
    FILE *f = fopen(filename, "rb");
    if (!f) return value_int(-1);
    
    unsigned char header[10];
    if (fread(header, 1, 10, f) != 10) { fclose(f); return value_int(-1); }
    
    if (memcmp(header, "ID3", 3) != 0) { fclose(f); return value_int(-1); }
    
    int version = header[3];
    int size = syncsafe_to_int(header + 6);
    
    if (size > 5 * 1024 * 1024) size = 5 * 1024 * 1024; 
    
    unsigned char *tag_data = (unsigned char*)malloc(size);
    if (!tag_data) { fclose(f); return value_int(-1); }
    
    if (fread(tag_data, 1, size, f) != (size_t)size) {
        free(tag_data);
        fclose(f);
        return value_int(-1);
    }
    fclose(f);
    
    int cursor = 0;
    while (cursor < size - 10) {
        char frame_id[5] = {0};
        memcpy(frame_id, tag_data + cursor, 4);
        
        if (frame_id[0] == 0) break;
        
        unsigned int frame_size = 0;
        if (version == 4) {
            frame_size = syncsafe_to_int(tag_data + cursor + 4);
        } else {
            frame_size = unsynchsafe_to_int(tag_data + cursor + 4);
        }
        
        cursor += 10;
        
        if (strcmp(frame_id, "APIC") == 0) {
            int data_cursor = cursor;
            data_cursor++; // Skip encoding
            
            char mime[64] = {0};
            int i = 0;
            while (data_cursor < size && tag_data[data_cursor] != 0 && i < 63) {
                mime[i++] = tag_data[data_cursor++];
            }
            data_cursor++; // Skip null
            data_cursor++; // Skip Picture Type
            
            int img_start = -1;
            int max_scan = frame_size < 256 ? frame_size : 256;
            
            for (int k = 0; k < max_scan; k++) {
                if (data_cursor + k + 1 < size && tag_data[data_cursor + k] == 0xFF && tag_data[data_cursor + k + 1] == 0xD8) {
                    img_start = data_cursor + k;
                    break;
                }
                if (data_cursor + k + 3 < size && tag_data[data_cursor + k] == 0x89 && tag_data[data_cursor + k + 1] == 0x50 && tag_data[data_cursor + k + 2] == 0x4E) {
                    img_start = data_cursor + k;
                    break;
                }
            }
            
            if (img_start != -1) {
                int img_size = frame_size - (img_start - cursor);
                const char *ext = ".png";
                if (strstr(mime, "jpeg") || strstr(mime, "JPG")) ext = ".jpg";
                
                // Use gl_backend to load texture from memory
                int tex_id = gl_load_texture_from_memory(ext, tag_data + img_start, img_size);
                
                if (tex_id >= 0) {
                    texture_ids[texture_count] = tex_id;
                    texture_widths[texture_count] = gl_get_texture_width(tex_id);
                    texture_heights[texture_count] = gl_get_texture_height(tex_id);
                    free(tag_data);
                    return value_int(texture_count++);
                }
            }
        }
        
        cursor += frame_size;
    }
    
    free(tag_data);
    return value_int(-1);
}

// Advanced Gradient

Value lib_gui_draw_gradient_ex(int argc, Value *argv, struct Env *env) {
    if (argc < 8) return value_null();
    GRect rec = { 
        (float)val_to_double(argv[0]), (float)val_to_double(argv[1]), 
        (float)val_to_double(argv[2]), (float)val_to_double(argv[3]) 
    };
    GColor c1 = val_to_color(argv[4]); // TL
    GColor c2 = val_to_color(argv[5]); // BL
    GColor c3 = val_to_color(argv[6]); // BR
    GColor c4 = val_to_color(argv[7]); // TR
    
    gl_draw_rect_gradient_ex(rec, c1, c2, c3, c4);
    return value_null();
}

// Render Texture System (FBO)

#define MAX_RENDER_TEXTURES 8
static int render_texture_ids[MAX_RENDER_TEXTURES]; // GRenderTex handles
static int render_texture_count = 0;

Value lib_gui_load_render_texture(int argc, Value *argv, struct Env *env) {
    if (argc < 2 || render_texture_count >= MAX_RENDER_TEXTURES) return value_int(-1);
    int w = (int)val_to_double(argv[0]);
    int h = (int)val_to_double(argv[1]);
    int id = gl_load_render_texture(w, h);
    if (id < 0) return value_int(-1);
    render_texture_ids[render_texture_count] = id;
    return value_int(render_texture_count++);
}

Value lib_gui_begin_texture_mode(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < render_texture_count) {
        gl_begin_texture_mode(render_texture_ids[id]);
    }
    return value_null();
}

Value lib_gui_end_texture_mode(int argc, Value *argv, struct Env *env) {
    gl_end_texture_mode();
    return value_null();
}

Value lib_gui_draw_render_texture(int argc, Value *argv, struct Env *env) {
    if (argc < 3) return value_null();
    int id = (int)argv[0].i;
    int x = (int)val_to_double(argv[1]);
    int y = (int)val_to_double(argv[2]);
    
    if (id >= 0 && id < render_texture_count) {
        gl_draw_render_texture(render_texture_ids[id], x, y);
    }
    return value_null();
}

Value lib_gui_unload_render_texture(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < render_texture_count) {
        gl_unload_render_texture(render_texture_ids[id]);
    }
    return value_null();
}

Value lib_gui_take_screenshot(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    gl_take_screenshot(argv[0].string->chars);
    return value_null();
}

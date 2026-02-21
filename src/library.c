// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

// This file connects the raw C library implementations to the Luna environment.
// It maps internal C functions (like lib_math_abs) to Luna function names (like "abs").
// It also defines utility native functions like 'assert'.

#include <stdio.h>
#include <stdlib.h>
#include "library.h"
#include "value.h"
#include "luna_error.h"
#include "math_lib.h"
#include "string_lib.h"
#include "time_lib.h"
#include "vec_lib.h"
#include "file_lib.h" 
#include "list_lib.h" // Added for sort and shuffle
#include "gui_lib.h" // For GUI

// Sand Lib Externs
Value lib_sand_init(int argc, Value *argv, Env *env);
Value lib_sand_set(int argc, Value *argv, Env *env);
Value lib_sand_get(int argc, Value *argv, Env *env);
Value lib_sand_update(int argc, Value *argv, Env *env);

// Helper: Local truthiness check for assert
// (This logic mirrors the interpreter's is_truthy to keep modules decoupled)
static int lib_is_truthy(Value v) {
    switch (v.type) {
        case VAL_BOOL:   return v.b;
        case VAL_INT:    return v.i != 0;
        case VAL_FLOAT:  return v.f != 0.0;
        case VAL_STRING: return v.string && v.string->chars && v.string->chars[0] != '\0';
        case VAL_NULL:   return 0;
        case VAL_LIST:   
        case VAL_DENSE_LIST: return 1; // Updated to include Dense Lists
        case VAL_NATIVE: return 1;
        case VAL_CHAR:   return v.c != 0;
        case VAL_FILE:   return v.file != NULL; // Files are truthy if open
        default:         return 0;
    }
}

// Native implementation of assert()
//  moved this here from interpreter.c to keep the core logic clean.
//  use exit(1) here to fulfill the "Force crash" requirement in test scripts.
static Value lib_assert(int argc, Value *argv, Env *env) {
    if (argc != 1) {
        error_report(ERR_ARGUMENT, 0, 0,
            "assert() takes exactly 1 argument",
            "Use assert(condition) to verify logic.");
        exit(1);
    }
    
    if (!lib_is_truthy(argv[0])) {
        // Passing 0 here is fine; error.c will use luna_current_line
        error_report(ERR_ASSERTION, 0, 0,
            "Assertion failed",
            "The condition evaluated to false.");
        exit(1); // Exit here so that FAILED tests stop the process
    }
    return value_bool(1);
}

void env_register_stdlib(Env *env) {
    env_def(env, "null", value_null());
    
    // Core Utilities 
    env_def(env, "assert", value_native(lib_assert));

    // Math Library
    env_def(env, "abs", value_native(lib_math_abs));
    env_def(env, "min", value_native(lib_math_min));
    env_def(env, "max", value_native(lib_math_max));
    env_def(env, "clamp", value_native(lib_math_clamp));
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
    
    // Unified Random Interface (xoroshiro128++)
    env_def(env, "rand", value_native(lib_math_rand));
    env_def(env, "srand", value_native(lib_math_srand));
    env_def(env, "trand", value_native(lib_math_trand));
    
    env_def(env, "deg_to_rad", value_native(lib_math_deg_to_rad));
    env_def(env, "rad_to_deg", value_native(lib_math_rad_to_deg));
    env_def(env, "lerp", value_native(lib_math_lerp));
    
    // String Library
    // Both 'len' and 'str_len' now point to the polymorphic lib_str_len
    env_def(env, "len", value_native(lib_str_len));
    env_def(env, "str_len", value_native(lib_str_len)); 
    
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
    env_def(env, "to_string", value_native(lib_str_to_string));

    // List Library (Hybrid Sort & Fisher-Yates Shuffle)
    env_def(env, "sort", value_native(lib_list_sort));
    env_def(env, "shuffle", value_native(lib_list_shuffle));
    env_def(env, "list_append", value_native(lib_list_append));
    env_def(env, "dense_list", value_native(lib_dense_list));

    // Time Library
    env_def(env, "clock", value_native(lib_time_clock));
   
    // Vector Math Library
    env_def(env, "vec_add", value_native(lib_vec_add));
    env_def(env, "vec_sub", value_native(lib_vec_sub));
    env_def(env, "vec_mul", value_native(lib_vec_mul));
    env_def(env, "vec_div", value_native(lib_vec_div));
    env_def(env, "mat_mul", value_native(lib_mat_mul)); // New native matrix multiplication

    // File I/O Library
    env_def(env, "open", value_native(lib_file_open));
    env_def(env, "close", value_native(lib_file_close));
    env_def(env, "read", value_native(lib_file_read));
    env_def(env, "read_line", value_native(lib_file_read_line));
    env_def(env, "write", value_native(lib_file_write));
    
    env_def(env, "file_exists", value_native(lib_file_exists));
    env_def(env, "remove_file", value_native(lib_file_remove));
    env_def(env, "flush", value_native(lib_file_flush));

    // GUI Library
    env_def(env, "init_window", value_native(lib_gui_init));
    env_def(env, "window_open", value_native(lib_gui_window_open));
    env_def(env, "set_fps", value_native(lib_gui_set_fps));
    env_def(env, "get_delta_time", value_native(lib_gui_get_delta_time));
    env_def(env, "begin_drawing", value_native(lib_gui_begin));
    env_def(env, "end_drawing", value_native(lib_gui_end));
    env_def(env, "clear_background", value_native(lib_gui_clear));
    env_def(env, "label", value_native(lib_gui_label));
    env_def(env, "button", value_native(lib_gui_button));
    env_def(env, "get_mouse_position", value_native(lib_gui_get_mouse));
    env_def(env, "get_mouse_wheel_move", value_native(lib_gui_get_mouse_wheel_move));
    env_def(env, "slider", value_native(lib_gui_slider));
    env_def(env, "set_opacity", value_native(lib_gui_set_opacity));

    env_def(env, "draw_rectangle", value_native(lib_gui_draw_rect));
    env_def(env, "draw_circle", value_native(lib_gui_draw_circle));
    env_def(env, "draw_line", value_native(lib_gui_draw_line));
    env_def(env, "load_texture", value_native(lib_gui_load_texture));
    env_def(env, "draw_texture", value_native(lib_gui_draw_texture));
    env_def(env, "is_key_down", value_native(lib_gui_is_key_down));
    env_def(env, "load_font", value_native(lib_gui_load_font));
    env_def(env, "draw_text", value_native(lib_gui_draw_text));
    env_def(env, "draw_text_default", value_native(lib_gui_draw_text_default));
    env_def(env, "measure_text", value_native(lib_gui_measure_text));

    // System
    env_def(env, "close_window", value_native(lib_gui_close_window));

    // Audio
    env_def(env, "init_audio_device", value_native(lib_gui_init_audio));
    env_def(env, "close_audio_device", value_native(lib_gui_close_audio_device));
    env_def(env, "load_music_stream", value_native(lib_gui_load_music));
    env_def(env, "unload_music_stream", value_native(lib_gui_unload_music_stream));
    env_def(env, "load_music_cover", value_native(lib_gui_load_music_cover));
    env_def(env, "load_sound", value_native(lib_gui_load_sound));
    env_def(env, "unload_sound", value_native(lib_gui_unload_sound));
    env_def(env, "play_music_stream", value_native(lib_gui_play_music));
    env_def(env, "stop_music_stream", value_native(lib_gui_stop_music_stream));
    env_def(env, "pause_music_stream", value_native(lib_gui_pause_music_stream));
    env_def(env, "resume_music_stream", value_native(lib_gui_resume_music_stream));
    env_def(env, "update_music_stream", value_native(lib_gui_update_music));
    env_def(env, "get_music_time_length", value_native(lib_gui_get_music_time_length));
    env_def(env, "get_music_time_played", value_native(lib_gui_get_music_time_played));
    env_def(env, "seek_music_stream", value_native(lib_gui_seek_music_stream));
    env_def(env, "play_sound", value_native(lib_gui_play_sound));
    env_def(env, "get_music_fft", value_native(lib_gui_get_music_fft));

    // Input & Collision
    env_def(env, "is_mouse_button_pressed", value_native(lib_gui_is_mouse_button_pressed));
    env_def(env, "is_mouse_button_down", value_native(lib_gui_is_mouse_button_down));
    env_def(env, "is_key_pressed", value_native(lib_gui_is_key_pressed));
    env_def(env, "check_collision_point_rec", value_native(lib_gui_check_collision_point_rec));

    // Advanced Graphics
    env_def(env, "draw_rectangle_rec", value_native(lib_gui_draw_rectangle_rec));
    env_def(env, "draw_rectangle_lines", value_native(lib_gui_draw_rectangle_lines));
    env_def(env, "draw_gradient_v", value_native(lib_gui_draw_gradient_v));
    env_def(env, "draw_gradient_ex", value_native(lib_gui_draw_gradient_ex));
    env_def(env, "draw_texture_pro", value_native(lib_gui_draw_texture_pro));
    env_def(env, "get_texture_width", value_native(lib_gui_get_texture_width));
    env_def(env, "get_texture_height", value_native(lib_gui_get_texture_height));
    env_def(env, "unload_texture", value_native(lib_gui_unload_texture));
    
    // Color Utilities
    env_def(env, "rgb", value_native(lib_gui_rgb));
    env_def(env, "hsl", value_native(lib_gui_hsl));
    
    // Image Manipulation
    env_def(env, "load_image", value_native(lib_gui_load_image));
    env_def(env, "image_rotate_cw", value_native(lib_gui_image_rotate_cw));
    env_def(env, "load_texture_from_image", value_native(lib_gui_load_texture_from_image));
    env_def(env, "unload_image", value_native(lib_gui_unload_image));

    // Camera
    env_def(env, "begin_mode_2d", value_native(lib_gui_begin_mode_2d));
    env_def(env, "end_mode_2d", value_native(lib_gui_end_mode_2d));
    // Sand Grid (Native Plugin)
    env_def(env, "sand_init", value_native(lib_sand_init));
    env_def(env, "sand_set", value_native(lib_sand_set));
    env_def(env, "sand_get", value_native(lib_sand_get));
    env_def(env, "sand_update", value_native(lib_sand_update));

    // Render Textures
    env_def(env, "load_render_texture", value_native(lib_gui_load_render_texture));
    env_def(env, "begin_texture_mode", value_native(lib_gui_begin_texture_mode));
    env_def(env, "end_texture_mode", value_native(lib_gui_end_texture_mode));
    env_def(env, "draw_render_texture", value_native(lib_gui_draw_render_texture));
    env_def(env, "unload_render_texture", value_native(lib_gui_unload_render_texture));

    // Screenshot
    env_def(env, "take_screenshot", value_native(lib_gui_take_screenshot));
}
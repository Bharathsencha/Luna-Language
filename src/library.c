// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

// This file connects the raw C library implementations to the Luna environment.
// It maps internal C functions (like lib_math_abs) to Luna function names (like "abs").
// It also defines utility native functions like 'assert'.

#include <stdio.h>
#include <stdlib.h>
#include "library.h"
#include "intern.h"
#include "value.h"
#include "luna_error.h"
#include "math_lib.h"
#include "string_lib.h"
#include "time_lib.h"
#include "vec_lib.h"
#include "file_lib.h" 
#include "list_lib.h"
#include "gui_lib.h" // For GUI
#include "gui_lib_3d.h"

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
        case VAL_BLOC:
        case VAL_BLOC_TYPE:
        case VAL_BOX:
        case VAL_TEMPLATE:
            return 1;
        case VAL_STRING: return v.string && v.string->chars && v.string->chars[0] != '\0';
        case VAL_NULL:   return 0;
        case VAL_LIST:   
        case VAL_DENSE_LIST:
        case VAL_MAP: return 1; // container values are truthy
        case VAL_NATIVE: return 1;
        case VAL_CLOSURE: return 1;
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

static Value lib_map_set(int argc, Value *argv, Env *env) {
    if (argc != 3 || argv[0].type != VAL_MAP || argv[1].type != VAL_STRING || !argv[1].string) {
        error_report(ERR_ARGUMENT, 0, 0,
            "map_set() expects (map, string, value)",
            "Usage: map_set(myMap, \"key\", value)");
        return value_null();
    }
    value_map_set(&argv[0], intern_string(argv[1].string->chars), argv[2]);
    return value_null();
}

static Value lib_map_get(int argc, Value *argv, Env *env) {
    if (argc != 2 || argv[0].type != VAL_MAP || argv[1].type != VAL_STRING || !argv[1].string) {
        error_report(ERR_ARGUMENT, 0, 0,
            "map_get() expects (map, string)",
            "Usage: map_get(myMap, \"key\")");
        return value_null();
    }
    Value *value = value_map_get(&argv[0], intern_string(argv[1].string->chars));
    return value ? value_copy(*value) : value_null();
}

static Value lib_map_has(int argc, Value *argv, Env *env) {
    if (argc != 2 || argv[0].type != VAL_MAP || argv[1].type != VAL_STRING || !argv[1].string) {
        error_report(ERR_ARGUMENT, 0, 0,
            "map_has() expects (map, string)",
            "Usage: map_has(myMap, \"key\")");
        return value_null();
    }
    return value_bool(value_map_has(&argv[0], intern_string(argv[1].string->chars)));
}

static Value lib_map_delete(int argc, Value *argv, Env *env) {
    if (argc != 2 || argv[0].type != VAL_MAP || argv[1].type != VAL_STRING || !argv[1].string) {
        error_report(ERR_ARGUMENT, 0, 0,
            "map_delete() expects (map, string)",
            "Usage: map_delete(myMap, \"key\")");
        return value_null();
    }
    return value_bool(value_map_delete(&argv[0], intern_string(argv[1].string->chars)));
}

static Value lib_map_keys(int argc, Value *argv, Env *env) {
    if (argc != 1 || argv[0].type != VAL_MAP) {
        error_report(ERR_ARGUMENT, 0, 0,
            "map_keys() expects 1 map",
            "Usage: map_keys(myMap)");
        return value_null();
    }
    return value_map_keys(argv[0]);
}

static Value lib_map_values(int argc, Value *argv, Env *env) {
    if (argc != 1 || argv[0].type != VAL_MAP) {
        error_report(ERR_ARGUMENT, 0, 0,
            "map_values() expects 1 map",
            "Usage: map_values(myMap)");
        return value_null();
    }
    return value_map_values(argv[0]);
}

static Value lib_map_items(int argc, Value *argv, Env *env) {
    if (argc != 1 || argv[0].type != VAL_MAP) {
        error_report(ERR_ARGUMENT, 0, 0,
            "map_items() expects 1 map",
            "Usage: map_items(myMap)");
        return value_null();
    }
    return value_map_items(argv[0]);
}

static Value lib_range(int argc, Value *argv, Env *env) {
    long long start = 0;
    long long end = 0;
    long long step = 1;

    if (argc == 1 && argv[0].type == VAL_INT) {
        end = argv[0].i;
    } else if (argc == 2 && argv[0].type == VAL_INT && argv[1].type == VAL_INT) {
        start = argv[0].i;
        end = argv[1].i;
    } else if (argc == 3 && argv[0].type == VAL_INT && argv[1].type == VAL_INT && argv[2].type == VAL_INT) {
        start = argv[0].i;
        end = argv[1].i;
        step = argv[2].i;
    } else {
        error_report(ERR_ARGUMENT, 0, 0,
            "range() expects 1-3 integer arguments",
            "Usage: range(end), range(start, end), or range(start, end, step)");
        return value_null();
    }

    if (step == 0) {
        error_report(ERR_ARGUMENT, 0, 0,
            "range() step cannot be zero",
            "Use a positive or negative step value");
        return value_null();
    }

    Value out = value_list();
    if (step > 0) {
        for (long long i = start; i < end; i += step) {
            value_list_append(&out, value_int(i));
        }
    } else {
        for (long long i = start; i > end; i += step) {
            value_list_append(&out, value_int(i));
        }
    }
    return out;
}

void env_register_stdlib(Env *env) {
    env_def(env, intern_string("null"), value_null());
    
    // Core Utilities 
    env_def(env, intern_string("assert"), value_native(lib_assert));
    env_def(env, intern_string("map_set"), value_native(lib_map_set));
    env_def(env, intern_string("map_get"), value_native(lib_map_get));
    env_def(env, intern_string("map_has"), value_native(lib_map_has));
    env_def(env, intern_string("map_delete"), value_native(lib_map_delete));
    env_def(env, intern_string("map_keys"), value_native(lib_map_keys));
    env_def(env, intern_string("map_values"), value_native(lib_map_values));
    env_def(env, intern_string("map_items"), value_native(lib_map_items));
    env_def(env, intern_string("range"), value_native(lib_range));

    // Math Library
    env_def(env, intern_string("abs"), value_native(lib_math_abs));
    env_def(env, intern_string("min"), value_native(lib_math_min));
    env_def(env, intern_string("max"), value_native(lib_math_max));
    env_def(env, intern_string("clamp"), value_native(lib_math_clamp));
    env_def(env, intern_string("sign"), value_native(lib_math_sign));
    
    env_def(env, intern_string("pow"), value_native(lib_math_pow));
    env_def(env, intern_string("sqrt"), value_native(lib_math_sqrt));
    env_def(env, intern_string("cbrt"), value_native(lib_math_cbrt));
    env_def(env, intern_string("exp"), value_native(lib_math_exp));
    env_def(env, intern_string("ln"), value_native(lib_math_ln));
    env_def(env, intern_string("log10"), value_native(lib_math_log10));
    
    env_def(env, intern_string("sin"), value_native(lib_math_sin));
    env_def(env, intern_string("cos"), value_native(lib_math_cos));
    env_def(env, intern_string("tan"), value_native(lib_math_tan));
    env_def(env, intern_string("asin"), value_native(lib_math_asin));
    env_def(env, intern_string("acos"), value_native(lib_math_acos));
    env_def(env, intern_string("atan"), value_native(lib_math_atan));
    env_def(env, intern_string("atan2"), value_native(lib_math_atan2));
    
    env_def(env, intern_string("sinh"), value_native(lib_math_sinh));
    env_def(env, intern_string("cosh"), value_native(lib_math_cosh));
    env_def(env, intern_string("tanh"), value_native(lib_math_tanh));
    
    env_def(env, intern_string("floor"), value_native(lib_math_floor));
    env_def(env, intern_string("ceil"), value_native(lib_math_ceil));
    env_def(env, intern_string("round"), value_native(lib_math_round));
    env_def(env, intern_string("trunc"), value_native(lib_math_trunc));
    env_def(env, intern_string("fract"), value_native(lib_math_fract));
    env_def(env, intern_string("mod"), value_native(lib_math_mod));
    
    // Unified Random Interface (xoroshiro128++)
    env_def(env, intern_string("rand"), value_native(lib_math_rand));
    env_def(env, intern_string("srand"), value_native(lib_math_srand));
    env_def(env, intern_string("trand"), value_native(lib_math_trand));
    
    env_def(env, intern_string("deg_to_rad"), value_native(lib_math_deg_to_rad));
    env_def(env, intern_string("rad_to_deg"), value_native(lib_math_rad_to_deg));
    env_def(env, intern_string("lerp"), value_native(lib_math_lerp));
    
    // String Library
    // Both 'len' and 'str_len' now point to the polymorphic lib_str_len
    env_def(env, intern_string("len"), value_native(lib_str_len));
    env_def(env, intern_string("str_len"), value_native(lib_str_len)); 
    
    env_def(env, intern_string("is_empty"), value_native(lib_str_is_empty));
    env_def(env, intern_string("concat"), value_native(lib_str_concat));
    
    env_def(env, intern_string("substring"), value_native(lib_str_substring));
    env_def(env, intern_string("slice"), value_native(lib_str_slice));
    env_def(env, intern_string("char_at"), value_native(lib_str_char_at));
    
    env_def(env, intern_string("index_of"), value_native(lib_str_index_of));
    env_def(env, intern_string("last_index_of"), value_native(lib_str_last_index_of));
    env_def(env, intern_string("contains"), value_native(lib_str_contains));
    env_def(env, intern_string("starts_with"), value_native(lib_str_starts_with));
    env_def(env, intern_string("ends_with"), value_native(lib_str_ends_with));
    
    env_def(env, intern_string("to_upper"), value_native(lib_str_to_upper));
    env_def(env, intern_string("to_lower"), value_native(lib_str_to_lower));
    env_def(env, intern_string("trim"), value_native(lib_str_trim));
    env_def(env, intern_string("trim_left"), value_native(lib_str_trim_left));
    env_def(env, intern_string("trim_right"), value_native(lib_str_trim_right));
    env_def(env, intern_string("replace"), value_native(lib_str_replace));
    env_def(env, intern_string("reverse"), value_native(lib_str_reverse));
    env_def(env, intern_string("repeat"), value_native(lib_str_repeat));
    env_def(env, intern_string("pad_left"), value_native(lib_str_pad_left));
    env_def(env, intern_string("pad_right"), value_native(lib_str_pad_right));
    env_def(env, intern_string("format"), value_native(lib_str_format));
    
    env_def(env, intern_string("split"), value_native(lib_str_split));
    env_def(env, intern_string("join"), value_native(lib_str_join));
    
    env_def(env, intern_string("is_digit"), value_native(lib_str_is_digit));
    env_def(env, intern_string("is_alpha"), value_native(lib_str_is_alpha));
    env_def(env, intern_string("is_alnum"), value_native(lib_str_is_alnum));
    env_def(env, intern_string("is_space"), value_native(lib_str_is_space));
    
    env_def(env, intern_string("to_int"), value_native(lib_str_to_int));
    env_def(env, intern_string("to_float"), value_native(lib_str_to_float));
    env_def(env, intern_string("to_string"), value_native(lib_str_to_string));

    // List Library (Hybrid Sort & Fisher-Yates Shuffle)
    env_def(env, intern_string("sort"), value_native(lib_list_sort));
    env_def(env, intern_string("ssort"), value_native(lib_list_ssort));
    env_def(env, intern_string("shuffle"), value_native(lib_list_shuffle));
    env_def(env, intern_string("list_append"), value_native(lib_list_append));
    env_def(env, intern_string("remove"), value_native(lib_list_remove));
    env_def(env, intern_string("find"), value_native(lib_list_find));
    env_def(env, intern_string("map"), value_native(lib_list_map));
    env_def(env, intern_string("filter"), value_native(lib_list_filter));
    env_def(env, intern_string("reduce"), value_native(lib_list_reduce));
    env_def(env, intern_string("dense_list"), value_native(lib_dense_list));

    // Time Library
    env_def(env, intern_string("clock"), value_native(lib_time_clock));
   
    // Vector Math Library
    env_def(env, intern_string("vec_add"), value_native(lib_vec_add));
    env_def(env, intern_string("vec_sub"), value_native(lib_vec_sub));
    env_def(env, intern_string("vec_mul"), value_native(lib_vec_mul));
    env_def(env, intern_string("vec_mul_inline"), value_native(lib_vec_mul_inline));
    env_def(env, intern_string("vec_div"), value_native(lib_vec_div));
    env_def(env, intern_string("mat_mul"), value_native(lib_mat_mul)); // New native matrix multiplication

    // File I/O Library
    env_def(env, intern_string("open"), value_native(lib_file_open));
    env_def(env, intern_string("close"), value_native(lib_file_close));
    env_def(env, intern_string("read"), value_native(lib_file_read));
    env_def(env, intern_string("read_line"), value_native(lib_file_read_line));
    env_def(env, intern_string("write"), value_native(lib_file_write));
    
    env_def(env, intern_string("file_exists"), value_native(lib_file_exists));
    env_def(env, intern_string("remove_file"), value_native(lib_file_remove));
    env_def(env, intern_string("flush"), value_native(lib_file_flush));

    // GUI Library
    env_def(env, intern_string("init_window"), value_native(lib_gui_init));
    env_def(env, intern_string("window_open"), value_native(lib_gui_window_open));
    env_def(env, intern_string("set_fps"), value_native(lib_gui_set_fps));
    env_def(env, intern_string("get_delta_time"), value_native(lib_gui_get_delta_time));
    env_def(env, intern_string("begin_drawing"), value_native(lib_gui_begin));
    env_def(env, intern_string("end_drawing"), value_native(lib_gui_end));
    env_def(env, intern_string("clear_background"), value_native(lib_gui_clear));
    env_def(env, intern_string("label"), value_native(lib_gui_label));
    env_def(env, intern_string("button"), value_native(lib_gui_button));
    env_def(env, intern_string("get_mouse_position"), value_native(lib_gui_get_mouse));
    env_def(env, intern_string("get_mouse_wheel_move"), value_native(lib_gui_get_mouse_wheel_move));
    env_def(env, intern_string("slider"), value_native(lib_gui_slider));
    env_def(env, intern_string("set_opacity"), value_native(lib_gui_set_opacity));

    env_def(env, intern_string("draw_rectangle"), value_native(lib_gui_draw_rect));
    env_def(env, intern_string("draw_circle"), value_native(lib_gui_draw_circle));
    env_def(env, intern_string("draw_line"), value_native(lib_gui_draw_line));
    env_def(env, intern_string("load_texture"), value_native(lib_gui_load_texture));
    env_def(env, intern_string("draw_texture"), value_native(lib_gui_draw_texture));
    env_def(env, intern_string("is_key_down"), value_native(lib_gui_is_key_down));
    env_def(env, intern_string("load_font"), value_native(lib_gui_load_font));
    env_def(env, intern_string("draw_text"), value_native(lib_gui_draw_text));
    env_def(env, intern_string("draw_text_default"), value_native(lib_gui_draw_text_default));
    env_def(env, intern_string("measure_text"), value_native(lib_gui_measure_text));

    // System
    env_def(env, intern_string("close_window"), value_native(lib_gui_close_window));

    // Audio
    env_def(env, intern_string("init_audio_device"), value_native(lib_gui_init_audio));
    env_def(env, intern_string("close_audio_device"), value_native(lib_gui_close_audio_device));
    env_def(env, intern_string("load_music_stream"), value_native(lib_gui_load_music));
    env_def(env, intern_string("unload_music_stream"), value_native(lib_gui_unload_music_stream));
    env_def(env, intern_string("load_music_cover"), value_native(lib_gui_load_music_cover));
    env_def(env, intern_string("load_sound"), value_native(lib_gui_load_sound));
    env_def(env, intern_string("unload_sound"), value_native(lib_gui_unload_sound));
    env_def(env, intern_string("play_music_stream"), value_native(lib_gui_play_music));
    env_def(env, intern_string("stop_music_stream"), value_native(lib_gui_stop_music_stream));
    env_def(env, intern_string("pause_music_stream"), value_native(lib_gui_pause_music_stream));
    env_def(env, intern_string("resume_music_stream"), value_native(lib_gui_resume_music_stream));
    env_def(env, intern_string("update_music_stream"), value_native(lib_gui_update_music));
    env_def(env, intern_string("get_music_time_length"), value_native(lib_gui_get_music_time_length));
    env_def(env, intern_string("get_music_time_played"), value_native(lib_gui_get_music_time_played));
    env_def(env, intern_string("seek_music_stream"), value_native(lib_gui_seek_music_stream));
    env_def(env, intern_string("play_sound"), value_native(lib_gui_play_sound));
    env_def(env, intern_string("get_music_fft"), value_native(lib_gui_get_music_fft));

    // Input & Collision
    env_def(env, intern_string("is_mouse_button_pressed"), value_native(lib_gui_is_mouse_button_pressed));
    env_def(env, intern_string("is_mouse_button_down"), value_native(lib_gui_is_mouse_button_down));
    env_def(env, intern_string("is_key_pressed"), value_native(lib_gui_is_key_pressed));
    env_def(env, intern_string("check_collision_point_rec"), value_native(lib_gui_check_collision_point_rec));

    // Advanced Graphics
    env_def(env, intern_string("draw_rectangle_rec"), value_native(lib_gui_draw_rectangle_rec));
    env_def(env, intern_string("draw_rectangle_lines"), value_native(lib_gui_draw_rectangle_lines));
    env_def(env, intern_string("draw_gradient_v"), value_native(lib_gui_draw_gradient_v));
    env_def(env, intern_string("draw_gradient_ex"), value_native(lib_gui_draw_gradient_ex));
    env_def(env, intern_string("draw_texture_pro"), value_native(lib_gui_draw_texture_pro));

    // --- 3D GUI API ---
    env_def(env, intern_string("create_camera_3d"), value_native(lib_gui_create_camera_3d));
    env_def(env, intern_string("update_camera_3d"), value_native(lib_gui_update_camera_3d));
    env_def(env, intern_string("set_camera_fov"), value_native(lib_gui_set_camera_fov));
    env_def(env, intern_string("begin_mode_3d"), value_native(lib_gui_begin_mode_3d));
    env_def(env, intern_string("end_mode_3d"), value_native(lib_gui_end_mode_3d));
    env_def(env, intern_string("get_camera_forward"), value_native(lib_gui_get_camera_forward));
    
    env_def(env, intern_string("draw_cube"), value_native(lib_gui_draw_cube));
    env_def(env, intern_string("draw_cube_wires"), value_native(lib_gui_draw_cube_wires));
    env_def(env, intern_string("draw_sphere"), value_native(lib_gui_draw_sphere));
    env_def(env, intern_string("draw_plane"), value_native(lib_gui_draw_plane));
    env_def(env, intern_string("draw_cylinder"), value_native(lib_gui_draw_cylinder));
    env_def(env, intern_string("draw_grid"), value_native(lib_gui_draw_grid));
    env_def(env, intern_string("draw_line_3d"), value_native(lib_gui_draw_line_3d));
    env_def(env, intern_string("draw_triangle_3d"), value_native(lib_gui_draw_triangle_3d));
    
    env_def(env, intern_string("create_light"), value_native(lib_gui_create_light));
    env_def(env, intern_string("set_light_enabled"), value_native(lib_gui_set_light_enabled));
    env_def(env, intern_string("set_light_color"), value_native(lib_gui_set_light_color));
    env_def(env, intern_string("set_light_position"), value_native(lib_gui_set_light_position));
    env_def(env, intern_string("set_light_intensity"), value_native(lib_gui_set_light_intensity));
    env_def(env, intern_string("set_ambient_light"), value_native(lib_gui_set_ambient_light));
    
    env_def(env, intern_string("check_collision_boxes"), value_native(lib_gui_check_collision_boxes));
    env_def(env, intern_string("check_collision_spheres"), value_native(lib_gui_check_collision_spheres));
    env_def(env, intern_string("get_texture_width"), value_native(lib_gui_get_texture_width));
    env_def(env, intern_string("get_texture_height"), value_native(lib_gui_get_texture_height));
    env_def(env, intern_string("unload_texture"), value_native(lib_gui_unload_texture));
    
    // Color Utilities
    env_def(env, intern_string("rgb"), value_native(lib_gui_rgb));
    env_def(env, intern_string("hsl"), value_native(lib_gui_hsl));
    
    // Image Manipulation
    env_def(env, intern_string("load_image"), value_native(lib_gui_load_image));
    env_def(env, intern_string("image_rotate_cw"), value_native(lib_gui_image_rotate_cw));
    env_def(env, intern_string("load_texture_from_image"), value_native(lib_gui_load_texture_from_image));
    env_def(env, intern_string("unload_image"), value_native(lib_gui_unload_image));

    // Camera
    env_def(env, intern_string("begin_mode_2d"), value_native(lib_gui_begin_mode_2d));
    env_def(env, intern_string("end_mode_2d"), value_native(lib_gui_end_mode_2d));
    // Sand Grid (Native Plugin)
    env_def(env, intern_string("sand_init"), value_native(lib_sand_init));
    env_def(env, intern_string("sand_set"), value_native(lib_sand_set));
    env_def(env, intern_string("sand_get"), value_native(lib_sand_get));
    env_def(env, intern_string("sand_update"), value_native(lib_sand_update));

    // Render Textures
    env_def(env, intern_string("load_render_texture"), value_native(lib_gui_load_render_texture));
    env_def(env, intern_string("begin_texture_mode"), value_native(lib_gui_begin_texture_mode));
    env_def(env, intern_string("end_texture_mode"), value_native(lib_gui_end_texture_mode));
    env_def(env, intern_string("draw_render_texture"), value_native(lib_gui_draw_render_texture));
    env_def(env, intern_string("unload_render_texture"), value_native(lib_gui_unload_render_texture));

    // Screenshot
    env_def(env, intern_string("take_screenshot"), value_native(lib_gui_take_screenshot));
}
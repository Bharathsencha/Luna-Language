// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
#ifndef GUI_LIB_H
#define GUI_LIB_H
#include "../include/value.h"

// Forward declaration for the Environment
struct Env;

// Lifecycle & System
Value lib_gui_init(int argc, Value *argv, struct Env *env);
Value lib_gui_window_open(int argc, Value *argv, struct Env *env);
Value lib_gui_set_fps(int argc, Value *argv, struct Env *env);
Value lib_gui_get_delta_time(int argc, Value *argv, struct Env *env);
Value lib_gui_set_opacity(int argc, Value *argv, struct Env *env);

// Frame Management
Value lib_gui_begin(int argc, Value *argv, struct Env *env);
Value lib_gui_end(int argc, Value *argv, struct Env *env);
Value lib_gui_clear(int argc, Value *argv, struct Env *env);

// Unified Smart Shapes (x, y, w, h, thickness, border_radius, color_list)
Value lib_gui_draw_rect(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_circle(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_line(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_gradient_v(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_gradient_ex(int argc, Value *argv, struct Env *env);

// Widgets
Value lib_gui_label(int argc, Value *argv, struct Env *env);
Value lib_gui_button(int argc, Value *argv, struct Env *env);
Value lib_gui_slider(int argc, Value *argv, struct Env *env);

// Input
Value lib_gui_get_mouse(int argc, Value *argv, struct Env *env);
Value lib_gui_get_mouse_wheel_move(int argc, Value *argv, struct Env *env);
Value lib_gui_is_key_down(int argc, Value *argv, struct Env *env); 

// Color Utilities (returns [r, g, b, a])
Value lib_gui_rgb(int argc, Value *argv, struct Env *env);
Value lib_gui_hsl(int argc, Value *argv, struct Env *env);

// Image/Texture & Transform
Value lib_gui_load_texture(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_texture(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_texture_rot(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_texture_pro(int argc, Value *argv, struct Env *env);
Value lib_gui_get_texture_width(int argc, Value *argv, struct Env *env);
Value lib_gui_get_texture_height(int argc, Value *argv, struct Env *env);
Value lib_gui_unload_texture(int argc, Value *argv, struct Env *env);

Value lib_gui_close_window(int argc, Value *argv, struct Env *env);

// Audio System
Value lib_gui_init_audio(int argc, Value *argv, struct Env *env);
Value lib_gui_close_audio_device(int argc, Value *argv, struct Env *env);
Value lib_gui_load_music(int argc, Value *argv, struct Env *env);
Value lib_gui_unload_music_stream(int argc, Value *argv, struct Env *env);
Value lib_gui_load_sound(int argc, Value *argv, struct Env *env);
Value lib_gui_unload_sound(int argc, Value *argv, struct Env *env);
Value lib_gui_play_music(int argc, Value *argv, struct Env *env);
Value lib_gui_load_music_cover(int argc, Value *argv, struct Env *env);
Value lib_gui_stop_music_stream(int argc, Value *argv, struct Env *env);
Value lib_gui_pause_music_stream(int argc, Value *argv, struct Env *env);
Value lib_gui_resume_music_stream(int argc, Value *argv, struct Env *env);
Value lib_gui_update_music(int argc, Value *argv, struct Env *env);
Value lib_gui_get_music_time_length(int argc, Value *argv, struct Env *env);
Value lib_gui_get_music_time_played(int argc, Value *argv, struct Env *env);
Value lib_gui_seek_music_stream(int argc, Value *argv, struct Env *env);
Value lib_gui_play_sound(int argc, Value *argv, struct Env *env);
Value lib_gui_get_music_fft(int argc, Value *argv, struct Env *env);

// Input & Collision
Value lib_gui_is_mouse_button_pressed(int argc, Value *argv, struct Env *env);
Value lib_gui_is_mouse_button_down(int argc, Value *argv, struct Env *env);
Value lib_gui_is_key_pressed(int argc, Value *argv, struct Env *env);
Value lib_gui_check_collision_point_rec(int argc, Value *argv, struct Env *env);

// Text Structures
Value lib_gui_measure_text(int argc, Value *argv, struct Env *env);

// Advanced Shapes
Value lib_gui_draw_rectangle_rec(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_rectangle_lines(int argc, Value *argv, struct Env *env);

// Camera System (2D)
Value lib_gui_begin_mode_2d(int argc, Value *argv, struct Env *env);
Value lib_gui_end_mode_2d(int argc, Value *argv, struct Env *env);

// Image Manipulation
Value lib_gui_load_image(int argc, Value *argv, struct Env *env);
Value lib_gui_image_rotate_cw(int argc, Value *argv, struct Env *env);
Value lib_gui_load_texture_from_image(int argc, Value *argv, struct Env *env);
Value lib_gui_unload_image(int argc, Value *argv, struct Env *env);

// Fonts
Value lib_gui_load_font(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_text(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_text_default(int argc, Value *argv, struct Env *env);

// Render Textures
Value lib_gui_load_render_texture(int argc, Value *argv, struct Env *env);
Value lib_gui_begin_texture_mode(int argc, Value *argv, struct Env *env);
Value lib_gui_end_texture_mode(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_render_texture(int argc, Value *argv, struct Env *env);
Value lib_gui_unload_render_texture(int argc, Value *argv, struct Env *env);

// Screenshot
Value lib_gui_take_screenshot(int argc, Value *argv, struct Env *env);

#endif
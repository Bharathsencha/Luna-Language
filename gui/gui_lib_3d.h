// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// gui_lib_3d.h — Luna 3D GUI script bindings
// Exposes 3D camera, primitives, lighting, and collision to Luna scripts.

#ifndef GUI_LIB_3D_H
#define GUI_LIB_3D_H

#include "../include/value.h"

struct Env;

// Camera System
Value lib_gui_create_camera_3d(int argc, Value *argv, struct Env *env);
Value lib_gui_update_camera_3d(int argc, Value *argv, struct Env *env);
Value lib_gui_update_camera_free(int argc, Value *argv, struct Env *env);
Value lib_gui_set_camera_fov(int argc, Value *argv, struct Env *env);
Value lib_gui_capture_cursor(int argc, Value *argv, struct Env *env);
Value lib_gui_begin_mode_3d(int argc, Value *argv, struct Env *env);
Value lib_gui_end_mode_3d(int argc, Value *argv, struct Env *env);
Value lib_gui_get_camera_forward(int argc, Value *argv, struct Env *env);

// 3D Primitives
Value lib_gui_draw_cube(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_cube_wires(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_sphere(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_plane(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_cylinder(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_grid(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_line_3d(int argc, Value *argv, struct Env *env);
Value lib_gui_draw_triangle_3d(int argc, Value *argv, struct Env *env);

// Lighting
Value lib_gui_create_light(int argc, Value *argv, struct Env *env);
Value lib_gui_set_light_enabled(int argc, Value *argv, struct Env *env);
Value lib_gui_set_light_color(int argc, Value *argv, struct Env *env);
Value lib_gui_set_light_position(int argc, Value *argv, struct Env *env);
Value lib_gui_set_light_intensity(int argc, Value *argv, struct Env *env);
Value lib_gui_set_ambient_light(int argc, Value *argv, struct Env *env);

// Collision
Value lib_gui_check_collision_boxes(int argc, Value *argv, struct Env *env);
Value lib_gui_check_collision_spheres(int argc, Value *argv, struct Env *env);

#endif // GUI_LIB_3D_H

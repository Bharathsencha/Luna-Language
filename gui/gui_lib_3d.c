// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// gui_lib_3d.c — Luna 3D GUI script binding implementations
// Bridges Luna scripts to gl_backend_3d functions.

#include "gl_backend_3d.h"
#include "gui_lib_3d.h"
#include "model_backend.h"
#include "../include/intern.h"
#include "../include/value.h"
#include "../include/env.h"
#include "../include/luna_error.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#pragma GCC diagnostic ignored "-Wunused-parameter"


static double val3d_to_double(Value v) {
    if (v.type == VAL_INT) return (double)v.i;
    if (v.type == VAL_FLOAT) return v.f;
    return 0.0;
}

static GVec3 val_to_vec3(Value v) {
    GVec3 r = {0, 0, 0};
    if (v.type != VAL_LIST || v.list->count < 3) return r;
    r.x = (float)val3d_to_double(v.list->items[0]);
    r.y = (float)val3d_to_double(v.list->items[1]);
    r.z = (float)val3d_to_double(v.list->items[2]);
    return r;
}

static GColor val3d_to_color(Value v) {
    if (v.type != VAL_LIST || v.list->count < 3) return (GColor){255, 255, 255, 255};
    GColor c;
    c.r = (unsigned char)v.list->items[0].i;
    c.g = (unsigned char)v.list->items[1].i;
    c.b = (unsigned char)v.list->items[2].i;
    c.a = (v.list->count > 3) ? (unsigned char)v.list->items[3].i : 255;
    return c;
}

static Value vec3_to_value(GVec3 v) {
    Value list = value_list();
    value_list_append(&list, value_float(v.x));
    value_list_append(&list, value_float(v.y));
    value_list_append(&list, value_float(v.z));
    return list;
}


#define MAX_CAMERAS_3D 8
static GCamera3D cameras_3d[MAX_CAMERAS_3D];
static int camera_3d_count = 0;


static void report_bad_camera(int id) {
    char msg[160];
    snprintf(msg, sizeof(msg),
             "camera id %d does not exist (valid ids are 0..%d)", id, MAX_CAMERAS_3D - 1);
    error_report(ERR_INDEX, 0, 0, msg,
                 "Use the value returned by create_camera_3d()");
}

static int cam_index_or_error(Value v) {
    if (v.type != VAL_INT && v.type != VAL_FLOAT) return -1;
    int id = (int)val3d_to_double(v);
    if (id < 0 || id >= camera_3d_count) {
        report_bad_camera(id);
        return -1;
    }
    return id;
}


// create_camera_3d(pos, target, up, fov) → cam_id
Value lib_gui_create_camera_3d(int argc, Value *argv, struct Env *env) {
    if (argc < 4 || camera_3d_count >= MAX_CAMERAS_3D) return value_int(-1);

    int id = camera_3d_count++;
    cameras_3d[id].position = val_to_vec3(argv[0]);
    cameras_3d[id].target = val_to_vec3(argv[1]);
    cameras_3d[id].up = val_to_vec3(argv[2]);
    cameras_3d[id].fov = (float)val3d_to_double(argv[3]);
    cameras_3d[id].projection = 0; // perspective
    return value_int(id);
}

// update_camera_3d(cam_id, pos, target)
Value lib_gui_update_camera_3d(int argc, Value *argv, struct Env *env) {
    if (argc < 3) return value_null();
    int id = cam_index_or_error(argv[0]);
    if (id < 0) return value_null();
    cameras_3d[id].position = val_to_vec3(argv[1]);
    cameras_3d[id].target = val_to_vec3(argv[2]);
    return value_null();
}

// update_camera_orbit(cam_id, center, dist, yaw_deg, pitch_deg)
Value lib_gui_update_camera_orbit(int argc, Value *argv, struct Env *env) {
    if (argc < 5) return value_null();
    int id = cam_index_or_error(argv[0]);
    if (id < 0) return value_null();
    gl3d_update_camera_orbit(&cameras_3d[id], val_to_vec3(argv[1]),
                             (float)val3d_to_double(argv[2]),
                             (float)val3d_to_double(argv[3]),
                             (float)val3d_to_double(argv[4]));
    return value_null();
}

// get_camera_position(cam_id) → [x, y, z]
Value lib_gui_get_camera_position(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = cam_index_or_error(argv[0]);
    if (id < 0) return value_null();
    return vec3_to_value(cameras_3d[id].position);
}

// set_camera_projection(cam_id, mode) — 0 perspective, 1 orthographic
Value lib_gui_set_camera_projection(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_null();
    int id = cam_index_or_error(argv[0]);
    if (id < 0) return value_null();
    cameras_3d[id].projection = ((int)val3d_to_double(argv[1]) != 0) ? 1 : 0;
    return value_null();
}

// update_camera_free(cam_id, speed=4.0, sensitivity=0.003)
Value lib_gui_update_camera_free(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = cam_index_or_error(argv[0]);
    if (id < 0) return value_null();

    float speed = (argc >= 2) ? (float)val3d_to_double(argv[1]) : 4.0f;
    float sensitivity = (argc >= 3) ? (float)val3d_to_double(argv[2]) : 0.003f;
    gl3d_update_camera_free(&cameras_3d[id], speed, sensitivity);
    return value_null();
}

// set_camera_fov(cam_id, fov)
Value lib_gui_set_camera_fov(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_null();
    int id = cam_index_or_error(argv[0]);
    if (id < 0) return value_null();
    cameras_3d[id].fov = (float)val3d_to_double(argv[1]);
    return value_null();
}

// capture_cursor(captured)
Value lib_gui_capture_cursor(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    gl_set_cursor_captured((int)val3d_to_double(argv[0]) != 0);
    gl3d_reset_free_camera_mouse();
    return value_null();
}

// begin_mode_3d(cam_id)
Value lib_gui_begin_mode_3d(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = cam_index_or_error(argv[0]);
    if (id < 0) return value_null();
    gl3d_begin_mode_3d(cameras_3d[id]);
    return value_null();
}

// end_mode_3d()
Value lib_gui_end_mode_3d(int argc, Value *argv, struct Env *env) {
    gl3d_end_mode_3d();
    return value_null();
}

// get_camera_forward(cam_id) → [x, y, z]
Value lib_gui_get_camera_forward(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = cam_index_or_error(argv[0]);
    if (id < 0) return value_null();
    GVec3 fwd = vec3_normalize(vec3_sub(cameras_3d[id].target, cameras_3d[id].position));
    return vec3_to_value(fwd);
}


// draw_cube(pos, size, color)
Value lib_gui_draw_cube(int argc, Value *argv, struct Env *env) {
    if (argc < 3) return value_null();
    gl3d_draw_cube(val_to_vec3(argv[0]), val_to_vec3(argv[1]), val3d_to_color(argv[2]));
    return value_null();
}

// draw_cube_wires(pos, size, color)
Value lib_gui_draw_cube_wires(int argc, Value *argv, struct Env *env) {
    if (argc < 3) return value_null();
    gl3d_draw_cube_wires(val_to_vec3(argv[0]), val_to_vec3(argv[1]), val3d_to_color(argv[2]));
    return value_null();
}

// draw_sphere(center, radius, color)
Value lib_gui_draw_sphere(int argc, Value *argv, struct Env *env) {
    if (argc < 3) return value_null();
    int rings = (argc >= 4) ? (int)val3d_to_double(argv[3]) : 16;
    int slices = (argc >= 5) ? (int)val3d_to_double(argv[4]) : 16;
    gl3d_draw_sphere(val_to_vec3(argv[0]), (float)val3d_to_double(argv[1]),
                     rings, slices, val3d_to_color(argv[2]));
    return value_null();
}

// draw_plane(center, size, color)
Value lib_gui_draw_plane(int argc, Value *argv, struct Env *env) {
    if (argc < 3) return value_null();
    GVec3 center = val_to_vec3(argv[0]);
    GVec2 size;
    if (argv[1].type == VAL_LIST && argv[1].list->count >= 2) {
        size.x = (float)val3d_to_double(argv[1].list->items[0]);
        size.y = (float)val3d_to_double(argv[1].list->items[1]);
    } else {
        float s = (float)val3d_to_double(argv[1]);
        size.x = s; size.y = s;
    }
    gl3d_draw_plane(center, size, val3d_to_color(argv[2]));
    return value_null();
}

// draw_cylinder(pos, rtop, rbot, height, slices, color)
Value lib_gui_draw_cylinder(int argc, Value *argv, struct Env *env) {
    if (argc < 6) return value_null();
    gl3d_draw_cylinder(val_to_vec3(argv[0]),
                       (float)val3d_to_double(argv[1]),
                       (float)val3d_to_double(argv[2]),
                       (float)val3d_to_double(argv[3]),
                       (int)val3d_to_double(argv[4]),
                       val3d_to_color(argv[5]));
    return value_null();
}

// draw_grid(slices, spacing)
Value lib_gui_draw_grid(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_null();
    gl3d_draw_grid((int)val3d_to_double(argv[0]), (float)val3d_to_double(argv[1]));
    return value_null();
}

// draw_line_3d(start, end, color)
Value lib_gui_draw_line_3d(int argc, Value *argv, struct Env *env) {
    if (argc < 3) return value_null();
    gl3d_draw_line_3d(val_to_vec3(argv[0]), val_to_vec3(argv[1]), val3d_to_color(argv[2]));
    return value_null();
}

// draw_triangle_3d(v1, v2, v3, color)
Value lib_gui_draw_triangle_3d(int argc, Value *argv, struct Env *env) {
    if (argc < 4) return value_null();
    gl3d_draw_triangle_3d(val_to_vec3(argv[0]), val_to_vec3(argv[1]),
                          val_to_vec3(argv[2]), val3d_to_color(argv[3]));
    return value_null();
}


// TRANSFORM STACK

// push_matrix() / pop_matrix() / reset_matrix()
Value lib_gui_push_matrix(int argc, Value *argv, struct Env *env) {
    gl3d_push_matrix();
    return value_null();
}

Value lib_gui_pop_matrix(int argc, Value *argv, struct Env *env) {
    gl3d_pop_matrix();
    return value_null();
}

Value lib_gui_reset_matrix(int argc, Value *argv, struct Env *env) {
    gl3d_load_identity_matrix();
    return value_null();
}

// translate_3d(v)
Value lib_gui_translate_3d(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    gl3d_translate(val_to_vec3(argv[0]));
    return value_null();
}

// rotate_3d(axis, degrees)
Value lib_gui_rotate_3d(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_null();
    gl3d_rotate(val_to_vec3(argv[0]), (float)val3d_to_double(argv[1]));
    return value_null();
}

// scale_3d(s) — list [x,y,z] or scalar
Value lib_gui_scale_3d(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    GVec3 s;
    if (argv[0].type == VAL_LIST && argv[0].list->count >= 3) {
        s = val_to_vec3(argv[0]);
    } else {
        float u = (float)val3d_to_double(argv[0]);
        s = (GVec3){u, u, u};
    }
    gl3d_scale(s);
    return value_null();
}


// create_light(type, position, target, color) → light_id
Value lib_gui_create_light(int argc, Value *argv, struct Env *env) {
    if (argc < 4) return value_int(-1);
    int type = (int)val3d_to_double(argv[0]);
    return value_int(gl3d_create_light(type, val_to_vec3(argv[1]),
                                       val_to_vec3(argv[2]), val3d_to_color(argv[3])));
}

// set_light_enabled(light_id, enabled)
Value lib_gui_set_light_enabled(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_null();
    gl3d_set_light_enabled((int)argv[0].i, (int)val3d_to_double(argv[1]));
    return value_null();
}

// set_light_color(light_id, color)
Value lib_gui_set_light_color(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_null();
    gl3d_set_light_color((int)argv[0].i, val3d_to_color(argv[1]));
    return value_null();
}

// set_light_position(light_id, pos)
Value lib_gui_set_light_position(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_null();
    gl3d_set_light_position((int)argv[0].i, val_to_vec3(argv[1]));
    return value_null();
}

// set_light_intensity(light_id, value)
Value lib_gui_set_light_intensity(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_null();
    gl3d_set_light_intensity((int)argv[0].i, (float)val3d_to_double(argv[1]));
    return value_null();
}

// set_ambient_light(color)
Value lib_gui_set_ambient_light(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    gl3d_set_ambient_light(val3d_to_color(argv[0]));
    return value_null();
}


// check_collision_boxes(box1, box2) → bool
// box = [min_x, min_y, min_z, max_x, max_y, max_z]
Value lib_gui_check_collision_boxes(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_bool(0);
    if (argv[0].type != VAL_LIST || argv[0].list->count < 6) return value_bool(0);
    if (argv[1].type != VAL_LIST || argv[1].list->count < 6) return value_bool(0);

    GBoundingBox a, b;
    a.min.x = (float)val3d_to_double(argv[0].list->items[0]);
    a.min.y = (float)val3d_to_double(argv[0].list->items[1]);
    a.min.z = (float)val3d_to_double(argv[0].list->items[2]);
    a.max.x = (float)val3d_to_double(argv[0].list->items[3]);
    a.max.y = (float)val3d_to_double(argv[0].list->items[4]);
    a.max.z = (float)val3d_to_double(argv[0].list->items[5]);

    b.min.x = (float)val3d_to_double(argv[1].list->items[0]);
    b.min.y = (float)val3d_to_double(argv[1].list->items[1]);
    b.min.z = (float)val3d_to_double(argv[1].list->items[2]);
    b.max.x = (float)val3d_to_double(argv[1].list->items[3]);
    b.max.y = (float)val3d_to_double(argv[1].list->items[4]);
    b.max.z = (float)val3d_to_double(argv[1].list->items[5]);

    return value_bool(gl3d_check_collision_boxes(a, b));
}

// check_collision_spheres(center1, radius1, center2, radius2) → bool
Value lib_gui_check_collision_spheres(int argc, Value *argv, struct Env *env) {
    if (argc < 4) return value_bool(0);
    return value_bool(gl3d_check_collision_spheres(
        val_to_vec3(argv[0]), (float)val3d_to_double(argv[1]),
        val_to_vec3(argv[2]), (float)val3d_to_double(argv[3])));
}


// ROTATED (_PRO) PRIMITIVES — trailing lit arg defaults to true

// draw_cube_pro(pos, size, rot, color, lit=1)
Value lib_gui_draw_cube_pro(int argc, Value *argv, struct Env *env) {
    if (argc < 4) return value_null();
    int lit = (argc >= 5) ? ((int)val3d_to_double(argv[4]) != 0) : 1;
    gl3d_draw_cube_pro(val_to_vec3(argv[0]), val_to_vec3(argv[1]),
                       val_to_vec3(argv[2]), val3d_to_color(argv[3]), lit);
    return value_null();
}

// draw_sphere_pro(pos, radius, rot, color, lit=1)
Value lib_gui_draw_sphere_pro(int argc, Value *argv, struct Env *env) {
    if (argc < 4) return value_null();
    int lit = (argc >= 5) ? ((int)val3d_to_double(argv[4]) != 0) : 1;
    gl3d_draw_sphere_pro(val_to_vec3(argv[0]), (float)val3d_to_double(argv[1]),
                         val_to_vec3(argv[2]), val3d_to_color(argv[3]), lit);
    return value_null();
}

// draw_cylinder_pro(pos, rtop, rbot, height, rot, color, lit=1)
Value lib_gui_draw_cylinder_pro(int argc, Value *argv, struct Env *env) {
    if (argc < 6) return value_null();
    int lit = (argc >= 7) ? ((int)val3d_to_double(argv[6]) != 0) : 1;
    gl3d_draw_cylinder_pro(val_to_vec3(argv[0]),
                           (float)val3d_to_double(argv[1]),
                           (float)val3d_to_double(argv[2]),
                           (float)val3d_to_double(argv[3]),
                           val_to_vec3(argv[4]), val3d_to_color(argv[5]), lit);
    return value_null();
}

// draw_plane_pro(center, size, rot, color, lit=1) — size is [w,d] or scalar
Value lib_gui_draw_plane_pro(int argc, Value *argv, struct Env *env) {
    if (argc < 4) return value_null();
    GVec2 size;
    if (argv[1].type == VAL_LIST && argv[1].list->count >= 2) {
        size.x = (float)val3d_to_double(argv[1].list->items[0]);
        size.y = (float)val3d_to_double(argv[1].list->items[1]);
    } else {
        float s = (float)val3d_to_double(argv[1]);
        size.x = s; size.y = s;
    }
    int lit = (argc >= 5) ? ((int)val3d_to_double(argv[4]) != 0) : 1;
    gl3d_draw_plane_pro(val_to_vec3(argv[0]), size,
                        val_to_vec3(argv[2]), val3d_to_color(argv[3]), lit);
    return value_null();
}

// draw_particle_3d(pos, size, color) — camera-facing unlit billboard
Value lib_gui_draw_particle_3d(int argc, Value *argv, struct Env *env) {
    if (argc < 3) return value_null();
    gl3d_draw_particle_3d(val_to_vec3(argv[0]),
                          (float)val3d_to_double(argv[1]),
                          val3d_to_color(argv[2]));
    return value_null();
}


// LIGHT EXTRAS

// set_light_target(light_id, pos)
Value lib_gui_set_light_target(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_null();
    gl3d_set_light_target((int)argv[0].i, val_to_vec3(argv[1]));
    return value_null();
}

// set_light_cone(light_id, inner_deg, outer_deg)
Value lib_gui_set_light_cone(int argc, Value *argv, struct Env *env) {
    if (argc < 3) return value_null();
    gl3d_set_light_cone((int)argv[0].i,
                        (float)val3d_to_double(argv[1]),
                        (float)val3d_to_double(argv[2]));
    return value_null();
}


// MATERIAL / ATMOSPHERE

// set_material(shininess, spec_strength)
Value lib_gui_set_material(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_null();
    gl3d_set_material((float)val3d_to_double(argv[0]),
                      (float)val3d_to_double(argv[1]));
    return value_null();
}

// set_fog(color, density) — density 0 disables
Value lib_gui_set_fog(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_null();
    gl3d_set_fog(val3d_to_color(argv[0]), (float)val3d_to_double(argv[1]));
    return value_null();
}

// set_gamma(enabled)
Value lib_gui_set_gamma(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    gl3d_set_gamma((int)val3d_to_double(argv[0]) != 0);
    return value_null();
}


// PICKING

// get_mouse_ray(cam_id) → [ox, oy, oz, dx, dy, dz]
Value lib_gui_get_mouse_ray(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = cam_index_or_error(argv[0]);
    if (id < 0) return value_null();

    GRay3D ray = gl3d_get_mouse_ray(cameras_3d[id]);
    Value list = value_list();
    value_list_append(&list, value_float(ray.origin.x));
    value_list_append(&list, value_float(ray.origin.y));
    value_list_append(&list, value_float(ray.origin.z));
    value_list_append(&list, value_float(ray.direction.x));
    value_list_append(&list, value_float(ray.direction.y));
    value_list_append(&list, value_float(ray.direction.z));
    return list;
}

static GRay3D list_to_ray(Value v) {
    GRay3D r = {{0}, {0, 0, -1}};
    if (v.type != VAL_LIST || v.list->count < 6) return r;
    r.origin = val_to_vec3(v);
    GVec3 dir;
    dir.x = (float)val3d_to_double(v.list->items[3]);
    dir.y = (float)val3d_to_double(v.list->items[4]);
    dir.z = (float)val3d_to_double(v.list->items[5]);
    r.direction = dir;
    return r;
}

// ray_hits_box(ray6, box6) → bool — both flat lists
Value lib_gui_ray_hits_box(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_bool(0);
    if (argv[0].type != VAL_LIST || argv[0].list->count < 6) return value_bool(0);
    if (argv[1].type != VAL_LIST || argv[1].list->count < 6) return value_bool(0);

    GRay3D ray = list_to_ray(argv[0]);
    GBoundingBox box;
    box.min.x = (float)val3d_to_double(argv[1].list->items[0]);
    box.min.y = (float)val3d_to_double(argv[1].list->items[1]);
    box.min.z = (float)val3d_to_double(argv[1].list->items[2]);
    box.max.x = (float)val3d_to_double(argv[1].list->items[3]);
    box.max.y = (float)val3d_to_double(argv[1].list->items[4]);
    box.max.z = (float)val3d_to_double(argv[1].list->items[5]);

    return value_bool(gl3d_ray_hits_box(ray, box));
}

// ray_plane_distance(ray6, point3, normal3) → float (-1 = no hit)
Value lib_gui_ray_plane_distance(int argc, Value *argv, struct Env *env) {
    if (argc < 3) return value_float(-1.0);
    if (argv[0].type != VAL_LIST || argv[0].list->count < 6) return value_float(-1.0);
    GRay3D ray = list_to_ray(argv[0]);
    return value_float(gl3d_ray_plane_distance(ray, val_to_vec3(argv[1]), val_to_vec3(argv[2])));
}


// LOADED MODELS (OBJ)

// load_model(path) → id or -1
Value lib_gui_load_model(int argc, Value *argv, struct Env *env) {
    if (argc < 1 || argv[0].type != VAL_STRING || !argv[0].string) return value_int(-1);
    return value_int(model_load(argv[0].string->chars));
}

// draw_model(id, pos, size, rot, tint, lit=1)
// size is [sx,sy,sz] or scalar; tint is [r,g,b] / [r,g,b,a]
Value lib_gui_draw_model(int argc, Value *argv, struct Env *env) {
    if (argc < 5) return value_null();
    int id = (int)val3d_to_double(argv[0]);

    GVec3 size;
    if (argv[2].type == VAL_LIST && argv[2].list->count >= 3) {
        size = val_to_vec3(argv[2]);
    } else {
        float u = (float)val3d_to_double(argv[2]);
        size = (GVec3){u, u, u};
    }

    GMat4 model = mat4_translate(val_to_vec3(argv[1]));
    model = mat4_multiply(model, mat4_rotate((GVec3){1, 0, 0}, val_to_vec3(argv[3]).x));
    model = mat4_multiply(model, mat4_rotate((GVec3){0, 1, 0}, val_to_vec3(argv[3]).y));
    model = mat4_multiply(model, mat4_rotate((GVec3){0, 0, 1}, val_to_vec3(argv[3]).z));
    model = mat4_multiply(model, mat4_scale(size));

    int lit = (argc >= 6) ? ((int)val3d_to_double(argv[5]) != 0) : 1;
    model_draw(id, model, val3d_to_color(argv[4]), lit);
    return value_null();
}

// unload_model(id)
Value lib_gui_unload_model(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    model_unload((int)val3d_to_double(argv[0]));
    return value_null();
}


// Called on window close so a fresh window starts without stale cameras/models
void lib_gui_3d_reset_runtime(void) {
    camera_3d_count = 0;
    memset(cameras_3d, 0, sizeof(cameras_3d));
    gl3d_reset_runtime();
    model_unload_all();
}

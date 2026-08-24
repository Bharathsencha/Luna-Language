// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// gl_backend_3d.h — OpenGL 3.3 3D rendering backend for Luna GUI
// Extends the 2D gl_backend with perspective cameras, 3D primitives, and lighting.

#ifndef GL_BACKEND_3D_H
#define GL_BACKEND_3D_H

#include "gl_backend.h"


typedef struct { float x, y, z; } GVec3;

typedef struct { float m[16]; } GMat4;  // column-major

typedef struct {
    GVec3 position;    // eye position
    GVec3 target;      // look-at point
    GVec3 up;          // up vector (typically 0,1,0)
    float fov;         // field of view in degrees
    int   projection;  // 0 = perspective, 1 = orthographic
} GCamera3D;

typedef struct {
    GVec3 position;
    GVec3 target;     // direction for directional/spot
    GColor color;
    float intensity;
    int   type;       // 0 = directional, 1 = point, 2 = spot
    int   enabled;
    float inner_deg;  // spot cone inner angle (fully lit)
    float outer_deg;  // spot cone outer angle (fades to zero)
} GLight3D;

typedef struct {
    GVec3 origin;
    GVec3 direction;
} GRay3D;


int  gl3d_init(void);                // compile 3D shaders, setup VAOs
void gl3d_shutdown(void);            // cleanup
void gl3d_reset_runtime(void);       // reset lights/material/fog/stack state


void gl3d_begin_mode_3d(GCamera3D cam);  // enable depth, set projection/view
void gl3d_end_mode_3d(void);             // restore 2D state
void gl3d_update_camera_free(GCamera3D *cam, float move_speed, float mouse_sensitivity);
void gl3d_update_camera_orbit(GCamera3D *cam, GVec3 center, float dist,
                              float yaw_deg, float pitch_deg);
void gl3d_reset_free_camera_mouse(void);


void gl3d_draw_cube(GVec3 pos, GVec3 size, GColor color);
void gl3d_draw_cube_pro(GVec3 pos, GVec3 size, GVec3 rot_deg, GColor color, int lit);
void gl3d_draw_cube_wires(GVec3 pos, GVec3 size, GColor color);
void gl3d_draw_sphere(GVec3 center, float radius, int rings, int slices, GColor color);
void gl3d_draw_sphere_pro(GVec3 center, float radius, GVec3 rot_deg, GColor color, int lit);
void gl3d_draw_plane(GVec3 center, GVec2 size, GColor color);
void gl3d_draw_plane_pro(GVec3 center, GVec2 size, GVec3 rot_deg, GColor color, int lit);
void gl3d_draw_cylinder(GVec3 pos, float rtop, float rbot, float height, int slices, GColor color);
void gl3d_draw_cylinder_pro(GVec3 pos, float rtop, float rbot, float height,
                            GVec3 rot_deg, GColor color, int lit);
void gl3d_draw_particle_3d(GVec3 pos, float size, GColor color); // camera-facing, unlit
void gl3d_draw_grid(int slices, float spacing);
void gl3d_draw_line_3d(GVec3 start, GVec3 end, GColor color);
void gl3d_draw_triangle_3d(GVec3 v1, GVec3 v2, GVec3 v3, GColor color);

// Loaded meshes: interleaved x,y,z,nx,ny,nz,r,g,b,a triangles
int        gl3d_upload_mesh(const float *verts, int vert_count,
                            int *vao_out, int *vbo_out);
void       gl3d_free_mesh(int vao, int vbo);
void       gl3d_draw_mesh(int vao, int vert_count, GMat4 local_model,
                          GColor tint, int lit);


#define GL3D_MAX_LIGHTS 8

int  gl3d_create_light(int type, GVec3 position, GVec3 target, GColor color);
void gl3d_set_light_enabled(int id, int enabled);
void gl3d_set_light_color(int id, GColor color);
void gl3d_set_light_position(int id, GVec3 pos);
void gl3d_set_light_target(int id, GVec3 target);
void gl3d_set_light_intensity(int id, float intensity);
void gl3d_set_light_cone(int id, float inner_deg, float outer_deg);
void gl3d_set_ambient_light(GColor color);
void gl3d_reset_lights(void);


GMat4 mat4_identity(void);
GMat4 mat4_perspective(float fov_deg, float aspect, float near, float far);
GMat4 mat4_ortho(float left, float right, float bottom, float top,
                 float near, float far);
GMat4 mat4_look_at(GVec3 eye, GVec3 target, GVec3 up);
GMat4 mat4_multiply(GMat4 a, GMat4 b);
GMat4 mat4_translate(GVec3 v);
GMat4 mat4_rotate(GVec3 axis, float angle_deg);
GMat4 mat4_scale(GVec3 v);
GMat4 mat4_transpose(GMat4 m);
GMat4 mat4_inverse(GMat4 m);

GVec3 vec3_normalize(GVec3 v);
GVec3 vec3_cross(GVec3 a, GVec3 b);
float vec3_dot(GVec3 a, GVec3 b);
GVec3 vec3_sub(GVec3 a, GVec3 b);
GVec3 vec3_add(GVec3 a, GVec3 b);
GVec3 vec3_scale(GVec3 v, float s);


void  gl3d_push_matrix(void);          // duplicate current matrix onto stack
void  gl3d_pop_matrix(void);           // restore previous matrix
void  gl3d_load_identity_matrix(void); // reset current matrix
void  gl3d_translate(GVec3 v);         // current = current * T(v)
void  gl3d_rotate(GVec3 axis, float angle_deg);
void  gl3d_scale(GVec3 s);
GMat4 gl3d_current_model(void);


void gl3d_set_material(float shininess, float spec_strength);
void gl3d_set_fog(GColor color, float density);   // density 0 disables fog
void gl3d_set_gamma(int enabled);


typedef struct { GVec3 min; GVec3 max; } GBoundingBox;

int gl3d_check_collision_boxes(GBoundingBox a, GBoundingBox b);
int gl3d_check_collision_spheres(GVec3 c1, float r1, GVec3 c2, float r2);

GRay3D gl3d_get_mouse_ray(GCamera3D cam);
int    gl3d_ray_hits_box(GRay3D ray, GBoundingBox box);
float  gl3d_ray_plane_distance(GRay3D ray, GVec3 point, GVec3 normal); // -1 = no hit
GVec3  gl3d_ray_at(GRay3D ray, float dist);


int  gl3d_get_win_w(void);
int  gl3d_get_win_h(void);

#endif // GL_BACKEND_3D_H

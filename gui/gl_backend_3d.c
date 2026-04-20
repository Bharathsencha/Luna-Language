// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// gl_backend_3d.c — OpenGL 3.3 3D rendering backend for Luna GUI
// 3D shaders, math, camera, primitives, lighting.

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include "gl_backend_3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#pragma GCC diagnostic ignored "-Wunused-parameter"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


static GLuint g3d_shader = 0;
static GLuint g3d_vao = 0, g3d_vbo = 0;
static GLint  g3d_u_mvp = -1;
static GLint  g3d_u_model = -1;
static GLint  g3d_u_use_lighting = -1;
static GLint  g3d_u_ambient = -1;
static GLint  g3d_u_view_pos = -1;
static GLint  g3d_u_light_count = -1;
static GLint  g3d_u_light_type[GL3D_MAX_LIGHTS];
static GLint  g3d_u_light_pos[GL3D_MAX_LIGHTS];
static GLint  g3d_u_light_dir[GL3D_MAX_LIGHTS];
static GLint  g3d_u_light_color[GL3D_MAX_LIGHTS];
static GLint  g3d_u_light_intensity[GL3D_MAX_LIGHTS];
static GLint  g3d_u_light_enabled[GL3D_MAX_LIGHTS];

static int g3d_initialized = 0;
static int g3d_mode_active = 0;

static GMat4 g3d_projection;
static GMat4 g3d_view;
static GCamera3D g3d_current_cam;
static int g3d_free_camera_mouse_initialized = 0;
static double g3d_free_camera_last_mouse_x = 0.0;
static double g3d_free_camera_last_mouse_y = 0.0;

// Lighting state
static GLight3D g3d_lights[GL3D_MAX_LIGHTS];
static int g3d_light_count = 0;
static GColor g3d_ambient = {40, 40, 60, 255};

// Batch buffer for 3D: x, y, z, nx, ny, nz, r, g, b, a = 10 floats per vert
#define VERT3D_STRIDE 10
#define MAX_BATCH3D_VERTS 65536
static float g3d_batch[MAX_BATCH3D_VERTS * VERT3D_STRIDE];
static int   g3d_batch_count = 0;

// Window size access (from gl_backend.c — we need these externs)
extern int g_win_w, g_win_h;

static float clampf(float value, float lo, float hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}


static const char *vs_3d_src =
    "#version 330 core\n"
    "layout(location=0) in vec3 aPos;\n"
    "layout(location=1) in vec3 aNormal;\n"
    "layout(location=2) in vec4 aColor;\n"
    "uniform mat4 uMVP;\n"
    "uniform mat4 uModel;\n"
    "out vec3 vNormal;\n"
    "out vec4 vColor;\n"
    "out vec3 vFragPos;\n"
    "void main() {\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "    vFragPos = vec3(uModel * vec4(aPos, 1.0));\n"
    "    vNormal = mat3(transpose(inverse(uModel))) * aNormal;\n"
    "    vColor = aColor;\n"
    "}\n";

static const char *fs_3d_src =
    "#version 330 core\n"
    "in vec3 vNormal;\n"
    "in vec4 vColor;\n"
    "in vec3 vFragPos;\n"
    "uniform int uUseLighting;\n"
    "uniform vec3 uAmbient;\n"
    "uniform vec3 uViewPos;\n"
    // Lights: type, position, direction, color, intensity, enabled
    "uniform int  uLightCount;\n"
    "uniform int  uLightType[8];\n"
    "uniform vec3 uLightPos[8];\n"
    "uniform vec3 uLightDir[8];\n"
    "uniform vec3 uLightColor[8];\n"
    "uniform float uLightIntensity[8];\n"
    "uniform int  uLightEnabled[8];\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    if (uUseLighting == 0) {\n"
    "        FragColor = vColor;\n"
    "        return;\n"
    "    }\n"
    "    vec3 norm = normalize(vNormal);\n"
    "    vec3 result = uAmbient * vColor.rgb;\n"
    "    for (int i = 0; i < uLightCount; i++) {\n"
    "        if (uLightEnabled[i] == 0) continue;\n"
    "        vec3 lightDir;\n"
    "        float attenuation = 1.0;\n"
    "        if (uLightType[i] == 0) {\n"  // directional
    "            lightDir = normalize(-uLightDir[i]);\n"
    "        } else {\n"  // point
    "            lightDir = normalize(uLightPos[i] - vFragPos);\n"
    "            float dist = length(uLightPos[i] - vFragPos);\n"
    "            attenuation = 1.0 / (1.0 + 0.09*dist + 0.032*dist*dist);\n"
    "        }\n"
    "        float diff = max(dot(norm, lightDir), 0.0);\n"
    "        vec3 diffuse = diff * uLightColor[i] * uLightIntensity[i] * attenuation;\n"
    "        vec3 viewDir = normalize(uViewPos - vFragPos);\n"
    "        vec3 halfDir = normalize(lightDir + viewDir);\n"
    "        float spec = pow(max(dot(norm, halfDir), 0.0), 32.0);\n"
    "        vec3 specular = spec * uLightColor[i] * 0.3 * attenuation;\n"
    "        result += (diffuse + specular) * vColor.rgb;\n"
    "    }\n"
    "    FragColor = vec4(result, vColor.a);\n"
    "}\n";


static GLuint compile_shader_3d(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    int ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, NULL, log);
        fprintf(stderr, "[GL3D] Shader compile error: %s\n", log);
    }
    return s;
}

static GLuint create_program_3d(const char *vs, const char *fs) {
    GLuint v = compile_shader_3d(GL_VERTEX_SHADER, vs);
    GLuint f = compile_shader_3d(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    int ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, 512, NULL, log);
        fprintf(stderr, "[GL3D] Program link error: %s\n", log);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}


GMat4 mat4_identity(void) {
    GMat4 m = {{0}};
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    return m;
}

GMat4 mat4_perspective(float fov_deg, float aspect, float near, float far) {
    GMat4 m = {{0}};
    float f = 1.0f / tanf(fov_deg * (float)M_PI / 360.0f);
    m.m[0]  = f / aspect;
    m.m[5]  = f;
    m.m[10] = (far + near) / (near - far);
    m.m[11] = -1.0f;
    m.m[14] = (2.0f * far * near) / (near - far);
    return m;
}

GMat4 mat4_look_at(GVec3 eye, GVec3 target, GVec3 up) {
    GVec3 f = vec3_normalize(vec3_sub(target, eye));
    GVec3 s = vec3_normalize(vec3_cross(f, up));
    GVec3 u = vec3_cross(s, f);

    GMat4 m = mat4_identity();
    m.m[0]  = s.x;  m.m[4]  = s.y;  m.m[8]  = s.z;
    m.m[1]  = u.x;  m.m[5]  = u.y;  m.m[9]  = u.z;
    m.m[2]  = -f.x; m.m[6]  = -f.y; m.m[10] = -f.z;
    m.m[12] = -vec3_dot(s, eye);
    m.m[13] = -vec3_dot(u, eye);
    m.m[14] =  vec3_dot(f, eye);
    return m;
}

GMat4 mat4_multiply(GMat4 a, GMat4 b) {
    GMat4 r = {{0}};
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
            for (int k = 0; k < 4; k++)
                r.m[col * 4 + row] += a.m[k * 4 + row] * b.m[col * 4 + k];
    return r;
}

GMat4 mat4_translate(GVec3 v) {
    GMat4 m = mat4_identity();
    m.m[12] = v.x; m.m[13] = v.y; m.m[14] = v.z;
    return m;
}

GMat4 mat4_rotate(GVec3 axis, float angle_deg) {
    float a = angle_deg * (float)M_PI / 180.0f;
    float c = cosf(a), s = sinf(a), t = 1.0f - c;
    GVec3 n = vec3_normalize(axis);

    GMat4 m = mat4_identity();
    m.m[0]  = t*n.x*n.x + c;      m.m[1]  = t*n.x*n.y + s*n.z;  m.m[2]  = t*n.x*n.z - s*n.y;
    m.m[4]  = t*n.x*n.y - s*n.z;  m.m[5]  = t*n.y*n.y + c;      m.m[6]  = t*n.y*n.z + s*n.x;
    m.m[8]  = t*n.x*n.z + s*n.y;  m.m[9]  = t*n.y*n.z - s*n.x;  m.m[10] = t*n.z*n.z + c;
    return m;
}

GMat4 mat4_scale(GVec3 v) {
    GMat4 m = mat4_identity();
    m.m[0] = v.x; m.m[5] = v.y; m.m[10] = v.z;
    return m;
}

GVec3 vec3_normalize(GVec3 v) {
    float len = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len < 0.00001f) return (GVec3){0, 0, 0};
    return (GVec3){v.x/len, v.y/len, v.z/len};
}

GVec3 vec3_cross(GVec3 a, GVec3 b) {
    return (GVec3){
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    };
}

float vec3_dot(GVec3 a, GVec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

GVec3 vec3_sub(GVec3 a, GVec3 b) {
    return (GVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

GVec3 vec3_add(GVec3 a, GVec3 b) {
    return (GVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

GVec3 vec3_scale(GVec3 v, float s) {
    return (GVec3){v.x * s, v.y * s, v.z * s};
}


static void flush_batch_3d(void) {
    if (g3d_batch_count == 0) return;

    glUseProgram(g3d_shader);
    glBindVertexArray(g3d_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g3d_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    g3d_batch_count * VERT3D_STRIDE * sizeof(float), g3d_batch);
    glDrawArrays(GL_TRIANGLES, 0, g3d_batch_count);

    g3d_batch_count = 0;
}

static inline void push_vert_3d(float x, float y, float z,
                                  float nx, float ny, float nz,
                                  float r, float g, float b, float a) {
    if (g3d_batch_count >= MAX_BATCH3D_VERTS) flush_batch_3d();
    int i = g3d_batch_count * VERT3D_STRIDE;
    g3d_batch[i+0] = x;  g3d_batch[i+1] = y;  g3d_batch[i+2] = z;
    g3d_batch[i+3] = nx; g3d_batch[i+4] = ny; g3d_batch[i+5] = nz;
    g3d_batch[i+6] = r;  g3d_batch[i+7] = g;  g3d_batch[i+8] = b; g3d_batch[i+9] = a;
    g3d_batch_count++;
}

// Push a 3D triangle with flat normal
static void push_tri_3d(GVec3 v1, GVec3 v2, GVec3 v3, GVec3 normal, GColor col) {
    float r = col.r/255.0f, g = col.g/255.0f, b = col.b/255.0f, a = col.a/255.0f;
    push_vert_3d(v1.x, v1.y, v1.z, normal.x, normal.y, normal.z, r, g, b, a);
    push_vert_3d(v2.x, v2.y, v2.z, normal.x, normal.y, normal.z, r, g, b, a);
    push_vert_3d(v3.x, v3.y, v3.z, normal.x, normal.y, normal.z, r, g, b, a);
}

// Push a 3D quad (two triangles) with flat normal
static void push_quad_3d(GVec3 v1, GVec3 v2, GVec3 v3, GVec3 v4, GVec3 normal, GColor col) {
    push_tri_3d(v1, v2, v3, normal, col);
    push_tri_3d(v1, v3, v4, normal, col);
}

// Set uniforms for model matrix and lighting
static void set_model_uniforms(GMat4 model) {
    GMat4 mvp = mat4_multiply(g3d_projection, mat4_multiply(g3d_view, model));
    glUseProgram(g3d_shader);
    glUniformMatrix4fv(g3d_u_mvp, 1, GL_FALSE, mvp.m);
    glUniformMatrix4fv(g3d_u_model, 1, GL_FALSE, model.m);
}

static void upload_lighting_uniforms(void) {
    glUseProgram(g3d_shader);

    int has_lights = (g3d_light_count > 0) ? 1 : 0;
    glUniform1i(g3d_u_use_lighting, has_lights);

    float amb[3] = {g3d_ambient.r/255.0f, g3d_ambient.g/255.0f, g3d_ambient.b/255.0f};
    glUniform3fv(g3d_u_ambient, 1, amb);

    float vp[3] = {g3d_current_cam.position.x, g3d_current_cam.position.y, g3d_current_cam.position.z};
    glUniform3fv(g3d_u_view_pos, 1, vp);

    glUniform1i(g3d_u_light_count, g3d_light_count);

    for (int i = 0; i < g3d_light_count; i++) {
        glUniform1i(g3d_u_light_type[i], g3d_lights[i].type);

        float lp[3] = {g3d_lights[i].position.x, g3d_lights[i].position.y, g3d_lights[i].position.z};
        glUniform3fv(g3d_u_light_pos[i], 1, lp);

        GVec3 dir = vec3_normalize(vec3_sub(g3d_lights[i].target, g3d_lights[i].position));
        float ld[3] = {dir.x, dir.y, dir.z};
        glUniform3fv(g3d_u_light_dir[i], 1, ld);

        float lc[3] = {g3d_lights[i].color.r/255.0f, g3d_lights[i].color.g/255.0f, g3d_lights[i].color.b/255.0f};
        glUniform3fv(g3d_u_light_color[i], 1, lc);

        glUniform1f(g3d_u_light_intensity[i], g3d_lights[i].intensity);

        glUniform1i(g3d_u_light_enabled[i], g3d_lights[i].enabled);
    }
}


int gl3d_init(void) {
    if (g3d_initialized) return 0;

    g3d_shader = create_program_3d(vs_3d_src, fs_3d_src);
    g3d_u_mvp = glGetUniformLocation(g3d_shader, "uMVP");
    g3d_u_model = glGetUniformLocation(g3d_shader, "uModel");
    g3d_u_use_lighting = glGetUniformLocation(g3d_shader, "uUseLighting");
    g3d_u_ambient = glGetUniformLocation(g3d_shader, "uAmbient");
    g3d_u_view_pos = glGetUniformLocation(g3d_shader, "uViewPos");
    g3d_u_light_count = glGetUniformLocation(g3d_shader, "uLightCount");
    for (int i = 0; i < GL3D_MAX_LIGHTS; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "uLightType[%d]", i);
        g3d_u_light_type[i] = glGetUniformLocation(g3d_shader, buf);
        snprintf(buf, sizeof(buf), "uLightPos[%d]", i);
        g3d_u_light_pos[i] = glGetUniformLocation(g3d_shader, buf);
        snprintf(buf, sizeof(buf), "uLightDir[%d]", i);
        g3d_u_light_dir[i] = glGetUniformLocation(g3d_shader, buf);
        snprintf(buf, sizeof(buf), "uLightColor[%d]", i);
        g3d_u_light_color[i] = glGetUniformLocation(g3d_shader, buf);
        snprintf(buf, sizeof(buf), "uLightIntensity[%d]", i);
        g3d_u_light_intensity[i] = glGetUniformLocation(g3d_shader, buf);
        snprintf(buf, sizeof(buf), "uLightEnabled[%d]", i);
        g3d_u_light_enabled[i] = glGetUniformLocation(g3d_shader, buf);
    }

    // Create 3D VAO/VBO
    glGenVertexArrays(1, &g3d_vao);
    glGenBuffers(1, &g3d_vbo);
    glBindVertexArray(g3d_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g3d_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g3d_batch), NULL, GL_DYNAMIC_DRAW);

    // Position (location 0): 3 floats
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, VERT3D_STRIDE * sizeof(float), (void*)0);
    // Normal (location 1): 3 floats
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, VERT3D_STRIDE * sizeof(float), (void*)(3 * sizeof(float)));
    // Color (location 2): 4 floats
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, VERT3D_STRIDE * sizeof(float), (void*)(6 * sizeof(float)));

    glBindVertexArray(0);

    g3d_light_count = 0;
    g3d_initialized = 1;
    return 0;
}

void gl3d_shutdown(void) {
    if (!g3d_initialized) return;
    if (g3d_vao) glDeleteVertexArrays(1, &g3d_vao);
    if (g3d_vbo) glDeleteBuffers(1, &g3d_vbo);
    if (g3d_shader) glDeleteProgram(g3d_shader);
    g3d_initialized = 0;
}

void gl3d_reset_free_camera_mouse(void) {
    g3d_free_camera_mouse_initialized = 0;
}

void gl3d_update_camera_free(GCamera3D *cam, float move_speed, float mouse_sensitivity) {
    if (!cam) return;

    GVec2 mouse = gl_get_mouse_position();
    if (!g3d_free_camera_mouse_initialized) {
        g3d_free_camera_last_mouse_x = mouse.x;
        g3d_free_camera_last_mouse_y = mouse.y;
        g3d_free_camera_mouse_initialized = 1;
    }

    float dx = (float)(mouse.x - g3d_free_camera_last_mouse_x);
    float dy = (float)(mouse.y - g3d_free_camera_last_mouse_y);
    g3d_free_camera_last_mouse_x = mouse.x;
    g3d_free_camera_last_mouse_y = mouse.y;

    GVec3 up = vec3_normalize(cam->up);
    if (fabsf(up.x) < 0.0001f && fabsf(up.y) < 0.0001f && fabsf(up.z) < 0.0001f) {
        up = (GVec3){0.0f, 1.0f, 0.0f};
    }

    GVec3 forward = vec3_normalize(vec3_sub(cam->target, cam->position));
    if (fabsf(forward.x) < 0.0001f && fabsf(forward.y) < 0.0001f && fabsf(forward.z) < 0.0001f) {
        forward = (GVec3){0.0f, 0.0f, -1.0f};
    }

    float yaw = atan2f(forward.z, forward.x);
    float pitch = asinf(clampf(forward.y, -1.0f, 1.0f));
    yaw += dx * mouse_sensitivity;
    pitch -= dy * mouse_sensitivity;
    pitch = clampf(pitch, -1.5533f, 1.5533f);

    GVec3 look = {
        cosf(pitch) * cosf(yaw),
        sinf(pitch),
        cosf(pitch) * sinf(yaw)
    };
    look = vec3_normalize(look);

    GVec3 right = vec3_normalize(vec3_cross(look, up));
    if (fabsf(right.x) < 0.0001f && fabsf(right.y) < 0.0001f && fabsf(right.z) < 0.0001f) {
        right = (GVec3){1.0f, 0.0f, 0.0f};
    }

    float frame_time = gl_get_frame_time();
    if (frame_time <= 0.0f) frame_time = 1.0f / 60.0f;
    float step = move_speed * frame_time;
    GVec3 move = {0};

    if (gl_is_key_down(GKEY_W)) move = vec3_add(move, vec3_scale(look, step));
    if (gl_is_key_down(GKEY_S)) move = vec3_add(move, vec3_scale(look, -step));
    if (gl_is_key_down(GKEY_A)) move = vec3_add(move, vec3_scale(right, -step));
    if (gl_is_key_down(GKEY_D)) move = vec3_add(move, vec3_scale(right, step));

    cam->position = vec3_add(cam->position, move);
    cam->target = vec3_add(cam->position, look);
}


void gl3d_begin_mode_3d(GCamera3D cam) {
    if (!g3d_initialized) gl3d_init();

    g3d_mode_active = 1;
    g3d_current_cam = cam;
    g3d_batch_count = 0;

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Build projection matrix
    float aspect = (float)g_win_w / (float)g_win_h;
    if (cam.projection == 0) {
        g3d_projection = mat4_perspective(cam.fov, aspect, 0.1f, 1000.0f);
    } else {
        // Orthographic 3D — not yet implemented, use perspective fallback
        g3d_projection = mat4_perspective(cam.fov, aspect, 0.1f, 1000.0f);
    }

    // Build view matrix
    g3d_view = mat4_look_at(cam.position, cam.target, cam.up);

    // Upload default uniforms
    GMat4 model = mat4_identity();
    set_model_uniforms(model);
    upload_lighting_uniforms();
}

void gl3d_end_mode_3d(void) {
    flush_batch_3d();
    glDisable(GL_DEPTH_TEST);
    g3d_mode_active = 0;
}


void gl3d_draw_cube(GVec3 pos, GVec3 size, GColor color) {
    GMat4 model = mat4_multiply(mat4_translate(pos), mat4_scale(size));
    set_model_uniforms(model);
    flush_batch_3d();

    // Unit cube vertices centered at origin (-0.5 to 0.5)
    float s = 0.5f;
    GVec3 v[8] = {
        {-s, -s, -s}, { s, -s, -s}, { s,  s, -s}, {-s,  s, -s},
        {-s, -s,  s}, { s, -s,  s}, { s,  s,  s}, {-s,  s,  s}
    };

    // Front  (+Z)
    push_quad_3d(v[4], v[5], v[6], v[7], (GVec3){0, 0, 1}, color);
    // Back   (-Z)
    push_quad_3d(v[1], v[0], v[3], v[2], (GVec3){0, 0, -1}, color);
    // Right  (+X)
    push_quad_3d(v[5], v[1], v[2], v[6], (GVec3){1, 0, 0}, color);
    // Left   (-X)
    push_quad_3d(v[0], v[4], v[7], v[3], (GVec3){-1, 0, 0}, color);
    // Top    (+Y)
    push_quad_3d(v[7], v[6], v[2], v[3], (GVec3){0, 1, 0}, color);
    // Bottom (-Y)
    push_quad_3d(v[0], v[1], v[5], v[4], (GVec3){0, -1, 0}, color);

    flush_batch_3d();
    set_model_uniforms(mat4_identity());
}

void gl3d_draw_cube_wires(GVec3 pos, GVec3 size, GColor color) {
    float hw = size.x * 0.5f, hh = size.y * 0.5f, hd = size.z * 0.5f;
    GVec3 v[8] = {
        {pos.x-hw, pos.y-hh, pos.z-hd}, {pos.x+hw, pos.y-hh, pos.z-hd},
        {pos.x+hw, pos.y+hh, pos.z-hd}, {pos.x-hw, pos.y+hh, pos.z-hd},
        {pos.x-hw, pos.y-hh, pos.z+hd}, {pos.x+hw, pos.y-hh, pos.z+hd},
        {pos.x+hw, pos.y+hh, pos.z+hd}, {pos.x-hw, pos.y+hh, pos.z+hd}
    };

    // 12 edges
    int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    for (int i = 0; i < 12; i++) {
        gl3d_draw_line_3d(v[edges[i][0]], v[edges[i][1]], color);
    }
}

void gl3d_draw_sphere(GVec3 center, float radius, int rings, int slices, GColor color) {
    if (rings < 4) rings = 16;
    if (slices < 4) slices = 16;

    GMat4 model = mat4_translate(center);
    set_model_uniforms(model);
    flush_batch_3d();

    for (int i = 0; i < rings; i++) {
        float lat0 = (float)M_PI * (-0.5f + (float)i / rings);
        float lat1 = (float)M_PI * (-0.5f + (float)(i + 1) / rings);
        float y0 = sinf(lat0) * radius, y1 = sinf(lat1) * radius;
        float r0 = cosf(lat0) * radius, r1 = cosf(lat1) * radius;

        for (int j = 0; j < slices; j++) {
            float lon0 = 2.0f * (float)M_PI * (float)j / slices;
            float lon1 = 2.0f * (float)M_PI * (float)(j + 1) / slices;

            GVec3 p00 = {r0*cosf(lon0), y0, r0*sinf(lon0)};
            GVec3 p10 = {r1*cosf(lon0), y1, r1*sinf(lon0)};
            GVec3 p01 = {r0*cosf(lon1), y0, r0*sinf(lon1)};
            GVec3 p11 = {r1*cosf(lon1), y1, r1*sinf(lon1)};

            // Normals point outward from center (which is origin in model space)
            GVec3 n00 = vec3_normalize(p00);
            GVec3 n10 = vec3_normalize(p10);
            GVec3 n01 = vec3_normalize(p01);
            GVec3 n11 = vec3_normalize(p11);

            float r = color.r/255.0f, g = color.g/255.0f, b = color.b/255.0f, a = color.a/255.0f;

            // Triangle 1
            push_vert_3d(p00.x, p00.y, p00.z, n00.x, n00.y, n00.z, r, g, b, a);
            push_vert_3d(p10.x, p10.y, p10.z, n10.x, n10.y, n10.z, r, g, b, a);
            push_vert_3d(p11.x, p11.y, p11.z, n11.x, n11.y, n11.z, r, g, b, a);

            // Triangle 2
            push_vert_3d(p00.x, p00.y, p00.z, n00.x, n00.y, n00.z, r, g, b, a);
            push_vert_3d(p11.x, p11.y, p11.z, n11.x, n11.y, n11.z, r, g, b, a);
            push_vert_3d(p01.x, p01.y, p01.z, n01.x, n01.y, n01.z, r, g, b, a);
        }
    }

    flush_batch_3d();
    set_model_uniforms(mat4_identity());
}

void gl3d_draw_plane(GVec3 center, GVec2 size, GColor color) {
    float hw = size.x * 0.5f, hh = size.y * 0.5f;
    GVec3 v1 = {center.x - hw, center.y, center.z - hh};
    GVec3 v2 = {center.x + hw, center.y, center.z - hh};
    GVec3 v3 = {center.x + hw, center.y, center.z + hh};
    GVec3 v4 = {center.x - hw, center.y, center.z + hh};
    push_quad_3d(v1, v2, v3, v4, (GVec3){0, 1, 0}, color);
}

void gl3d_draw_cylinder(GVec3 pos, float rtop, float rbot, float height, int slices, GColor color) {
    if (slices < 4) slices = 16;

    GMat4 model = mat4_translate(pos);
    set_model_uniforms(model);
    flush_batch_3d();

    float half_h = height * 0.5f;

    for (int i = 0; i < slices; i++) {
        float a0 = 2.0f * (float)M_PI * i / slices;
        float a1 = 2.0f * (float)M_PI * (i + 1) / slices;

        float cos0 = cosf(a0), sin0 = sinf(a0);
        float cos1 = cosf(a1), sin1 = sinf(a1);

        // Side quad
        GVec3 p0 = {rbot * cos0, -half_h, rbot * sin0};
        GVec3 p1 = {rbot * cos1, -half_h, rbot * sin1};
        GVec3 p2 = {rtop * cos1,  half_h, rtop * sin1};
        GVec3 p3 = {rtop * cos0,  half_h, rtop * sin0};

        GVec3 n = vec3_normalize((GVec3){cos0 + cos1, 0, sin0 + sin1});
        push_quad_3d(p0, p1, p2, p3, n, color);

        // Top cap
        if (rtop > 0.001f) {
            GVec3 tc = {0, half_h, 0};
            push_tri_3d(tc, p3, p2, (GVec3){0, 1, 0}, color);
        }

        // Bottom cap
        if (rbot > 0.001f) {
            GVec3 bc = {0, -half_h, 0};
            push_tri_3d(bc, p1, p0, (GVec3){0, -1, 0}, color);
        }
    }

    flush_batch_3d();
    set_model_uniforms(mat4_identity());
}

void gl3d_draw_grid(int slices, float spacing) {
    GMat4 model = mat4_identity();
    set_model_uniforms(model);

    // Disable lighting for grid
    glUniform1i(g3d_u_use_lighting, 0);

    float half = slices * spacing * 0.5f;
    GColor grid_color = {100, 100, 100, 255};

    for (int i = 0; i <= slices; i++) {
        float pos = -half + i * spacing;
        // Lines parallel to Z
        gl3d_draw_line_3d((GVec3){pos, 0, -half}, (GVec3){pos, 0, half}, grid_color);
        // Lines parallel to X
        gl3d_draw_line_3d((GVec3){-half, 0, pos}, (GVec3){half, 0, pos}, grid_color);
    }

    // Restore lighting state
    upload_lighting_uniforms();
}

void gl3d_draw_line_3d(GVec3 start, GVec3 end, GColor color) {
    // Draw line as thin triangulated quad
    GVec3 dir = vec3_sub(end, start);
    float len = sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    if (len < 0.0001f) return;

    // Create a very thin quad oriented toward the camera
    GVec3 cam_dir = vec3_sub(g3d_current_cam.position, vec3_scale(vec3_add(start, end), 0.5f));
    GVec3 side = vec3_normalize(vec3_cross(dir, cam_dir));
    float thickness = 0.02f;
    GVec3 offset = vec3_scale(side, thickness);

    GVec3 v1 = vec3_sub(start, offset);
    GVec3 v2 = vec3_add(start, offset);
    GVec3 v3 = vec3_add(end, offset);
    GVec3 v4 = vec3_sub(end, offset);

    GVec3 normal = vec3_normalize(cam_dir);
    float r = color.r/255.0f, g = color.g/255.0f, b = color.b/255.0f, a = color.a/255.0f;

    push_vert_3d(v1.x, v1.y, v1.z, normal.x, normal.y, normal.z, r, g, b, a);
    push_vert_3d(v2.x, v2.y, v2.z, normal.x, normal.y, normal.z, r, g, b, a);
    push_vert_3d(v3.x, v3.y, v3.z, normal.x, normal.y, normal.z, r, g, b, a);

    push_vert_3d(v1.x, v1.y, v1.z, normal.x, normal.y, normal.z, r, g, b, a);
    push_vert_3d(v3.x, v3.y, v3.z, normal.x, normal.y, normal.z, r, g, b, a);
    push_vert_3d(v4.x, v4.y, v4.z, normal.x, normal.y, normal.z, r, g, b, a);
}

void gl3d_draw_triangle_3d(GVec3 v1, GVec3 v2, GVec3 v3, GColor color) {
    GVec3 edge1 = vec3_sub(v2, v1);
    GVec3 edge2 = vec3_sub(v3, v1);
    GVec3 normal = vec3_normalize(vec3_cross(edge1, edge2));
    push_tri_3d(v1, v2, v3, normal, color);
}


int gl3d_create_light(int type, GVec3 position, GVec3 target, GColor color) {
    if (g3d_light_count >= GL3D_MAX_LIGHTS) return -1;
    int id = g3d_light_count++;
    g3d_lights[id].type = type;
    g3d_lights[id].position = position;
    g3d_lights[id].target = target;
    g3d_lights[id].color = color;
    g3d_lights[id].intensity = 1.0f;
    g3d_lights[id].enabled = 1;
    return id;
}

void gl3d_set_light_enabled(int id, int enabled) {
    if (id >= 0 && id < g3d_light_count) g3d_lights[id].enabled = enabled;
}

void gl3d_set_light_color(int id, GColor color) {
    if (id >= 0 && id < g3d_light_count) g3d_lights[id].color = color;
}

void gl3d_set_light_position(int id, GVec3 pos) {
    if (id >= 0 && id < g3d_light_count) g3d_lights[id].position = pos;
}

void gl3d_set_light_intensity(int id, float intensity) {
    if (id >= 0 && id < g3d_light_count) g3d_lights[id].intensity = intensity;
}

void gl3d_set_ambient_light(GColor color) {
    g3d_ambient = color;
}


int gl3d_check_collision_boxes(GBoundingBox a, GBoundingBox b) {
    return (a.max.x >= b.min.x && a.min.x <= b.max.x) &&
           (a.max.y >= b.min.y && a.min.y <= b.max.y) &&
           (a.max.z >= b.min.z && a.min.z <= b.max.z);
}

int gl3d_check_collision_spheres(GVec3 c1, float r1, GVec3 c2, float r2) {
    GVec3 d = vec3_sub(c1, c2);
    float dist2 = vec3_dot(d, d);
    float rsum = r1 + r2;
    return dist2 <= rsum * rsum;
}


int gl3d_get_win_w(void) { return g_win_w; }
int gl3d_get_win_h(void) { return g_win_h; }

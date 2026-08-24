// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// gl_backend_3d.c — OpenGL 3.3 3D rendering backend for Luna GUI
//
// Two rendering paths:
//   1. Cached templates — unit cube/sphere/cylinder/plane live in GPU VAOs,
//      generated once at init. Drawing = model-matrix uniforms + one
//      glDrawArrays call. No per-frame CPU vertex generation.
//   2. Immediate batch — lines, wireframes, triangles, grid. Dynamic batch
//      buffer flushed as needed (also used by loaded models' debug draws).
//
// Lighting: Blinn-Phong with directional/point/spot lights, exponential-squared
// fog, gamma correction, CPU-computed normal matrices.

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
static GLint  g3d_u_normal_mat = -1;
static GLint  g3d_u_use_lighting = -1;
static GLint  g3d_u_ambient = -1;
static GLint  g3d_u_view_pos = -1;
static GLint  g3d_u_color_mul = -1;
static GLint  g3d_u_shininess = -1;
static GLint  g3d_u_spec_strength = -1;
static GLint  g3d_u_fog_color = -1;
static GLint  g3d_u_fog_density = -1;
static GLint  g3d_u_gamma = -1;
static GLint  g3d_u_light_count = -1;
static GLint  g3d_u_light_type[GL3D_MAX_LIGHTS];
static GLint  g3d_u_light_pos[GL3D_MAX_LIGHTS];
static GLint  g3d_u_light_dir[GL3D_MAX_LIGHTS];
static GLint  g3d_u_light_color[GL3D_MAX_LIGHTS];
static GLint  g3d_u_light_intensity[GL3D_MAX_LIGHTS];
static GLint  g3d_u_light_enabled[GL3D_MAX_LIGHTS];
static GLint  g3d_u_light_inner[GL3D_MAX_LIGHTS];
static GLint  g3d_u_light_outer[GL3D_MAX_LIGHTS];

static int g3d_initialized = 0;

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

// Material / atmosphere state
static float g3d_shininess = 32.0f;
static float g3d_spec_strength = 0.3f;
static GColor g3d_fog_color = {15, 15, 25, 255};
static float g3d_fog_density = 0.0f;   // 0 = disabled
static int   g3d_gamma = 0;

// Transform stack: top entry is the parent model matrix every draw composes with
#define MATRIX_STACK_MAX 16
static GMat4 g3d_matrix_stack[MATRIX_STACK_MAX];
static int   g3d_matrix_depth = 0; // index of current top

// Batch buffer for 3D: x, y, z, nx, ny, nz, r, g, b, a = 10 floats per vert
#define VERT3D_STRIDE 10
#define MAX_BATCH3D_VERTS 65536
static float g3d_batch[MAX_BATCH3D_VERTS * VERT3D_STRIDE];
static int   g3d_batch_count = 0;

// Window size access (from gl_backend.c)
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
    "uniform mat4 uNormalMat;\n"
    "uniform vec4 uColorMul;\n"
    "out vec3 vNormal;\n"
    "out vec4 vColor;\n"
    "out vec3 vFragPos;\n"
    "void main() {\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "    vFragPos = vec3(uModel * vec4(aPos, 1.0));\n"
    "    vNormal = mat3(uNormalMat) * aNormal;\n"
    "    vColor = aColor * uColorMul;\n"
    "}\n";

static const char *fs_3d_src =
    "#version 330 core\n"
    "in vec3 vNormal;\n"
    "in vec4 vColor;\n"
    "in vec3 vFragPos;\n"
    "uniform int uUseLighting;\n"
    "uniform vec3 uAmbient;\n"
    "uniform vec3 uViewPos;\n"
    "uniform float uShininess;\n"
    "uniform float uSpecStrength;\n"
    "uniform vec3 uFogColor;\n"
    "uniform float uFogDensity;\n"
    "uniform int uGamma;\n"
    // Lights: type, position, direction, color, intensity, enabled, cone angles
    "uniform int  uLightCount;\n"
    "uniform int  uLightType[8];\n"
    "uniform vec3 uLightPos[8];\n"
    "uniform vec3 uLightDir[8];\n"
    "uniform vec3 uLightColor[8];\n"
    "uniform float uLightIntensity[8];\n"
    "uniform int  uLightEnabled[8];\n"
    "uniform float uLightInner[8];\n"
    "uniform float uLightOuter[8];\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    vec4 lit = vColor;\n"
    "    if (uUseLighting == 1) {\n"
    "        vec3 norm = normalize(vNormal);\n"
    "        vec3 result = uAmbient * vColor.rgb;\n"
    "        for (int i = 0; i < uLightCount; i++) {\n"
    "            if (uLightEnabled[i] == 0) continue;\n"
    "            vec3 lightDir;\n"
    "            float attenuation = 1.0;\n"
    "            float spot = 1.0;\n"
    "            if (uLightType[i] == 0) {\n"  // directional
    "                lightDir = normalize(-uLightDir[i]);\n"
    "            } else {\n"  // point and spot share position falloff
    "                vec3 toLight = uLightPos[i] - vFragPos;\n"
    "                float dist = length(toLight);\n"
    "                lightDir = toLight / max(dist, 0.0001);\n"
    "                attenuation = 1.0 / (1.0 + 0.09*dist + 0.032*dist*dist);\n"
    "                if (uLightType[i] == 2) {\n"  // spot cone
    "                    vec3 spotDir = normalize(uLightDir[i]);\n"
    "                    float theta = dot(lightDir, spotDir);\n"
    "                    spot = smoothstep(uLightOuter[i], uLightInner[i], theta);\n"
    "                }\n"
    "            }\n"
    "            float diff = max(dot(norm, lightDir), 0.0);\n"
    "            vec3 diffuse = diff * uLightColor[i] * uLightIntensity[i] * attenuation * spot;\n"
    "            vec3 viewDir = normalize(uViewPos - vFragPos);\n"
    "            vec3 halfDir = normalize(lightDir + viewDir);\n"
    "            float spec = pow(max(dot(norm, halfDir), 0.0), uShininess);\n"
    "            vec3 specular = spec * uSpecStrength * uLightColor[i] * attenuation * spot;\n"
    "            result += (diffuse * vColor.rgb) + specular;\n"
    "        }\n"
    "        lit = vec4(result, vColor.a);\n"
    "    }\n"
    "    if (uFogDensity > 0.0) {\n"
    "        float dist = length(uViewPos - vFragPos);\n"
    "        float fogFactor = 1.0 - exp(-uFogDensity * uFogDensity * dist * dist);\n"
    "        lit.rgb = mix(lit.rgb, uFogColor, clamp(fogFactor, 0.0, 1.0));\n"
    "    }\n"
    "    if (uGamma == 1) {\n"
    "        lit.rgb = pow(max(lit.rgb, vec3(0.0)), vec3(1.0 / 2.2));\n"
    "    }\n"
    "    FragColor = lit;\n"
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

GMat4 mat4_ortho(float left, float right, float bottom, float top,
                 float near, float far) {
    GMat4 m = {{0}};
    m.m[0]  = 2.0f / (right - left);
    m.m[5]  = 2.0f / (top - bottom);
    m.m[10] = -2.0f / (far - near);
    m.m[12] = -(right + left) / (right - left);
    m.m[13] = -(top + bottom) / (top - bottom);
    m.m[14] = -(far + near) / (far - near);
    m.m[15] = 1.0f;
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

GMat4 mat4_transpose(GMat4 m) {
    GMat4 r;
    for (int c = 0; c < 4; c++)
        for (int rr = 0; rr < 4; rr++)
            r.m[c * 4 + rr] = m.m[rr * 4 + c];
    return r;
}

// General 4x4 inversion via adjugate / cofactor expansion
GMat4 mat4_inverse(GMat4 m) {
    float inv[16];

    inv[0] = m.m[5]*m.m[10]*m.m[15] - m.m[5]*m.m[11]*m.m[14] - m.m[9]*m.m[6]*m.m[15] +
             m.m[9]*m.m[7]*m.m[14] + m.m[13]*m.m[6]*m.m[11] - m.m[13]*m.m[7]*m.m[10];
    inv[4] = -m.m[4]*m.m[10]*m.m[15] + m.m[4]*m.m[11]*m.m[14] + m.m[8]*m.m[6]*m.m[15] -
             m.m[8]*m.m[7]*m.m[14] - m.m[12]*m.m[6]*m.m[11] + m.m[12]*m.m[7]*m.m[10];
    inv[8] = m.m[4]*m.m[9]*m.m[15] - m.m[4]*m.m[11]*m.m[13] - m.m[8]*m.m[5]*m.m[15] +
             m.m[8]*m.m[7]*m.m[13] + m.m[12]*m.m[5]*m.m[11] - m.m[12]*m.m[7]*m.m[9];
    inv[12] = -m.m[4]*m.m[9]*m.m[14] + m.m[4]*m.m[10]*m.m[13] + m.m[8]*m.m[5]*m.m[14] -
              m.m[8]*m.m[6]*m.m[13] - m.m[12]*m.m[5]*m.m[10] + m.m[12]*m.m[6]*m.m[9];
    inv[1] = -m.m[1]*m.m[10]*m.m[15] + m.m[1]*m.m[11]*m.m[14] + m.m[9]*m.m[2]*m.m[15] -
             m.m[9]*m.m[3]*m.m[14] - m.m[13]*m.m[2]*m.m[11] + m.m[13]*m.m[3]*m.m[10];
    inv[5] = m.m[0]*m.m[10]*m.m[15] - m.m[0]*m.m[11]*m.m[14] - m.m[8]*m.m[2]*m.m[15] +
             m.m[8]*m.m[3]*m.m[14] + m.m[12]*m.m[2]*m.m[11] - m.m[12]*m.m[3]*m.m[10];
    inv[9] = -m.m[0]*m.m[9]*m.m[15] + m.m[0]*m.m[11]*m.m[13] + m.m[8]*m.m[1]*m.m[15] -
             m.m[8]*m.m[3]*m.m[13] - m.m[12]*m.m[1]*m.m[11] + m.m[12]*m.m[3]*m.m[9];
    inv[13] = m.m[0]*m.m[9]*m.m[14] - m.m[0]*m.m[10]*m.m[13] - m.m[8]*m.m[1]*m.m[14] +
              m.m[8]*m.m[2]*m.m[13] + m.m[12]*m.m[1]*m.m[10] - m.m[12]*m.m[2]*m.m[9];
    inv[2] = m.m[1]*m.m[6]*m.m[15] - m.m[1]*m.m[7]*m.m[14] - m.m[5]*m.m[2]*m.m[15] +
             m.m[5]*m.m[3]*m.m[14] + m.m[13]*m.m[2]*m.m[7] - m.m[13]*m.m[3]*m.m[6];
    inv[6] = -m.m[0]*m.m[6]*m.m[15] + m.m[0]*m.m[7]*m.m[14] + m.m[4]*m.m[2]*m.m[15] -
             m.m[4]*m.m[3]*m.m[14] - m.m[12]*m.m[2]*m.m[7] + m.m[12]*m.m[3]*m.m[6];
    inv[10] = m.m[0]*m.m[5]*m.m[15] - m.m[0]*m.m[7]*m.m[13] - m.m[4]*m.m[1]*m.m[15] +
              m.m[4]*m.m[3]*m.m[13] + m.m[12]*m.m[1]*m.m[7] - m.m[12]*m.m[3]*m.m[5];
    inv[14] = -m.m[0]*m.m[5]*m.m[14] + m.m[0]*m.m[6]*m.m[13] + m.m[4]*m.m[1]*m.m[14] -
              m.m[4]*m.m[2]*m.m[13] - m.m[12]*m.m[1]*m.m[6] + m.m[12]*m.m[2]*m.m[5];
    inv[3] = -m.m[1]*m.m[6]*m.m[11] + m.m[1]*m.m[7]*m.m[10] + m.m[5]*m.m[2]*m.m[11] -
             m.m[5]*m.m[3]*m.m[10] - m.m[9]*m.m[2]*m.m[7] + m.m[9]*m.m[3]*m.m[6];
    inv[7] = m.m[0]*m.m[6]*m.m[11] - m.m[0]*m.m[7]*m.m[10] - m.m[4]*m.m[2]*m.m[11] +
             m.m[4]*m.m[3]*m.m[10] + m.m[8]*m.m[2]*m.m[7] - m.m[8]*m.m[3]*m.m[6];
    inv[11] = -m.m[0]*m.m[5]*m.m[11] + m.m[0]*m.m[7]*m.m[9] + m.m[4]*m.m[1]*m.m[11] -
              m.m[4]*m.m[3]*m.m[9] - m.m[8]*m.m[1]*m.m[7] + m.m[8]*m.m[3]*m.m[5];
    inv[15] = m.m[0]*m.m[5]*m.m[10] - m.m[0]*m.m[6]*m.m[9] - m.m[4]*m.m[1]*m.m[10] +
              m.m[4]*m.m[2]*m.m[9] + m.m[8]*m.m[1]*m.m[6] - m.m[8]*m.m[2]*m.m[5];

    float det = m.m[0]*inv[0] + m.m[1]*inv[4] + m.m[2]*inv[8] + m.m[3]*inv[12];
    if (det == 0.0f) return mat4_identity();

    float inv_det = 1.0f / det;
    GMat4 out;
    for (int i = 0; i < 16; i++) out.m[i] = inv[i] * inv_det;
    return out;
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


// TRANSFORM STACK

GMat4 gl3d_current_model(void) {
    return g3d_matrix_stack[g3d_matrix_depth];
}

void gl3d_push_matrix(void) {
    if (g3d_matrix_depth >= MATRIX_STACK_MAX - 1) return;
    g3d_matrix_stack[g3d_matrix_depth + 1] = g3d_matrix_stack[g3d_matrix_depth];
    g3d_matrix_depth++;
}

void gl3d_pop_matrix(void) {
    if (g3d_matrix_depth > 0) g3d_matrix_depth--;
}

void gl3d_load_identity_matrix(void) {
    g3d_matrix_stack[g3d_matrix_depth] = mat4_identity();
}

void gl3d_translate(GVec3 v) {
    g3d_matrix_stack[g3d_matrix_depth] =
        mat4_multiply(g3d_matrix_stack[g3d_matrix_depth], mat4_translate(v));
}

void gl3d_rotate(GVec3 axis, float angle_deg) {
    g3d_matrix_stack[g3d_matrix_depth] =
        mat4_multiply(g3d_matrix_stack[g3d_matrix_depth], mat4_rotate(axis, angle_deg));
}

void gl3d_scale(GVec3 s) {
    g3d_matrix_stack[g3d_matrix_depth] =
        mat4_multiply(g3d_matrix_stack[g3d_matrix_depth], mat4_scale(s));
}


// CACHED GEOMETRY TEMPLATES

typedef struct {
    GLuint vao, vbo;
    int vert_count;
} GeoTemplate;

enum {
    TMPL_CUBE = 0,
    TMPL_SPHERE,
    TMPL_CYLINDER,
    TMPL_PLANE,
    TMPL_QUAD,
    TMPL_COUNT
};

static GeoTemplate g3d_templates[TMPL_COUNT];

static void bind_geo_attribs(void) {
    // Position (location 0): 3 floats
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, VERT3D_STRIDE * sizeof(float), (void*)0);
    // Normal (location 1): 3 floats
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, VERT3D_STRIDE * sizeof(float), (void*)(3 * sizeof(float)));
    // Color (location 2): 4 floats
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, VERT3D_STRIDE * sizeof(float), (void*)(6 * sizeof(float)));
}

static void make_template(GeoTemplate *t, const float *verts, int vert_count) {
    glGenVertexArrays(1, &t->vao);
    glGenBuffers(1, &t->vbo);
    glBindVertexArray(t->vao);
    glBindBuffer(GL_ARRAY_BUFFER, t->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vert_count * VERT3D_STRIDE * sizeof(float),
                 verts, GL_STATIC_DRAW);
    bind_geo_attribs();
    glBindVertexArray(0);
    t->vert_count = vert_count;
}

static void emit_quad(float *out, int *n, GVec3 a, GVec3 b, GVec3 c, GVec3 d,
                      GVec3 normal) {
    // a-b-c / a-c-d wound CCW, flat normal, white vertex color
    GVec3 v[6] = {a, b, c, a, c, d};
    for (int i = 0; i < 6; i++) {
        int o = (*n) * VERT3D_STRIDE;
        out[o+0] = v[i].x; out[o+1] = v[i].y; out[o+2] = v[i].z;
        out[o+3] = normal.x; out[o+4] = normal.y; out[o+5] = normal.z;
        out[o+6] = 1.0f; out[o+7] = 1.0f; out[o+8] = 1.0f; out[o+9] = 1.0f;
        (*n)++;
    }
}

static void build_cube_template(void) {
    static float verts[36 * VERT3D_STRIDE];
    int n = 0;
    float h = 0.5f;
    GVec3 p[8] = {
        {-h,-h,-h},{ h,-h,-h},{ h, h,-h},{-h, h,-h},
        {-h,-h, h},{ h,-h, h},{ h, h, h},{-h, h, h}
    };
    emit_quad(verts, &n, p[4], p[5], p[6], p[7], (GVec3){0,0, 1}); // front +Z
    emit_quad(verts, &n, p[1], p[0], p[3], p[2], (GVec3){0,0,-1}); // back -Z
    emit_quad(verts, &n, p[5], p[1], p[2], p[6], (GVec3){ 1,0,0}); // right +X
    emit_quad(verts, &n, p[0], p[4], p[7], p[3], (GVec3){-1,0,0}); // left -X
    emit_quad(verts, &n, p[7], p[6], p[2], p[3], (GVec3){0, 1,0}); // top +Y
    emit_quad(verts, &n, p[0], p[1], p[5], p[4], (GVec3){0,-1,0}); // bottom -Y
    make_template(&g3d_templates[TMPL_CUBE], verts, n);
}

static void build_sphere_template(int rings, int slices) {
    int quad_count = rings * slices;
    float *verts = malloc((size_t)quad_count * 6 * VERT3D_STRIDE * sizeof(float));
    if (!verts) return;
    int n = 0;
    for (int i = 0; i < rings; i++) {
        float lat0 = (float)M_PI * (-0.5f + (float)i / rings);
        float lat1 = (float)M_PI * (-0.5f + (float)(i + 1) / rings);
        float y0 = sinf(lat0), y1 = sinf(lat1);
        float r0 = cosf(lat0), r1 = cosf(lat1);
        for (int j = 0; j < slices; j++) {
            float lon0 = 2.0f * (float)M_PI * (float)j / slices;
            float lon1 = 2.0f * (float)M_PI * (float)(j + 1) / slices;
            GVec3 p00 = {r0*cosf(lon0), y0, r0*sinf(lon0)};
            GVec3 p10 = {r1*cosf(lon0), y1, r1*sinf(lon0)};
            GVec3 p01 = {r0*cosf(lon1), y0, r0*sinf(lon1)};
            GVec3 p11 = {r1*cosf(lon1), y1, r1*sinf(lon1)};
            GVec3 n00 = vec3_normalize(p00);
            GVec3 n10 = vec3_normalize(p10);
            GVec3 n01 = vec3_normalize(p01);
            GVec3 n11 = vec3_normalize(p11);
            GVec3 tri[6] = {p00, p10, p11, p00, p11, p01};
            GVec3 nor[6] = {n00, n10, n11, n00, n11, n01};
            for (int k = 0; k < 6; k++) {
                int o = n * VERT3D_STRIDE;
                verts[o+0]=tri[k].x; verts[o+1]=tri[k].y; verts[o+2]=tri[k].z;
                verts[o+3]=nor[k].x; verts[o+4]=nor[k].y; verts[o+5]=nor[k].z;
                verts[o+6]=1.0f; verts[o+7]=1.0f; verts[o+8]=1.0f; verts[o+9]=1.0f;
                n++;
            }
        }
    }
    make_template(&g3d_templates[TMPL_SPHERE], verts, n);
    free(verts);
}

static void build_cylinder_template(int slices) {
    int cap_tris = slices * 2; // top + bottom fans
    int total_verts = slices * 6 + cap_tris * 3;
    float *verts = malloc((size_t)total_verts * VERT3D_STRIDE * sizeof(float));
    if (!verts) return;
    int n = 0;
    float half_h = 0.5f, r = 0.5f;
    for (int i = 0; i < slices; i++) {
        float a0 = 2.0f * (float)M_PI * i / slices;
        float a1 = 2.0f * (float)M_PI * (i + 1) / slices;
        float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
        GVec3 p0 = {r*c0, -half_h, r*s0};
        GVec3 p1 = {r*c1, -half_h, r*s1};
        GVec3 p2 = {r*c1,  half_h, r*s1};
        GVec3 p3 = {r*c0,  half_h, r*s0};
        GVec3 sn = vec3_normalize((GVec3){c0 + c1, 0, s0 + s1});
        GVec3 side[6] = {p0, p1, p2, p0, p2, p3};
        for (int k = 0; k < 6; k++) {
            int o = n * VERT3D_STRIDE;
            verts[o+0]=side[k].x; verts[o+1]=side[k].y; verts[o+2]=side[k].z;
            verts[o+3]=sn.x; verts[o+4]=sn.y; verts[o+5]=sn.z;
            verts[o+6]=1.0f; verts[o+7]=1.0f; verts[o+8]=1.0f; verts[o+9]=1.0f;
            n++;
        }
        // Top fan
        GVec3 tc = {0, half_h, 0};
        GVec3 top[3] = {tc, p3, p2};
        for (int k = 0; k < 3; k++) {
            int o = n * VERT3D_STRIDE;
            verts[o+0]=top[k].x; verts[o+1]=top[k].y; verts[o+2]=top[k].z;
            verts[o+3]=0.0f; verts[o+4]=1.0f; verts[o+5]=0.0f;
            verts[o+6]=1.0f; verts[o+7]=1.0f; verts[o+8]=1.0f; verts[o+9]=1.0f;
            n++;
        }
        // Bottom fan
        GVec3 bc = {0, -half_h, 0};
        GVec3 bot[3] = {bc, p1, p0};
        for (int k = 0; k < 3; k++) {
            int o = n * VERT3D_STRIDE;
            verts[o+0]=bot[k].x; verts[o+1]=bot[k].y; verts[o+2]=bot[k].z;
            verts[o+3]=0.0f; verts[o+4]=-1.0f; verts[o+5]=0.0f;
            verts[o+6]=1.0f; verts[o+7]=1.0f; verts[o+8]=1.0f; verts[o+9]=1.0f;
            n++;
        }
    }
    make_template(&g3d_templates[TMPL_CYLINDER], verts, n);
    free(verts);
}

static void build_plane_template(void) {
    static float verts[6 * VERT3D_STRIDE];
    int n = 0;
    float h = 0.5f;
    emit_quad(verts, &n,
              (GVec3){-h, 0, -h}, (GVec3){ h, 0, -h},
              (GVec3){ h, 0,  h}, (GVec3){-h, 0,  h},
              (GVec3){0, 1, 0});
    make_template(&g3d_templates[TMPL_PLANE], verts, n);
}

static void build_quad_template(void) {
    static float verts[6 * VERT3D_STRIDE];
    int n = 0;
    float h = 0.5f;
    emit_quad(verts, &n,
              (GVec3){-h,-h, 0}, (GVec3){-h, h, 0},
              (GVec3){ h, h, 0}, (GVec3){ h,-h, 0},
              (GVec3){0, 0, 1});
    make_template(&g3d_templates[TMPL_QUAD], verts, n);
}


// IMMEDIATE BATCH (lines, wires, triangles, grid)

static void flush_batch_3d(void) {
    if (g3d_batch_count == 0) return;

    glUseProgram(g3d_shader);
    glBindVertexArray(g3d_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g3d_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)g3d_batch_count * VERT3D_STRIDE * sizeof(float), g3d_batch);
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


// MODEL-MATRIX DRAW PATH

// Compute and upload MVP/model/normal-matrix for a composed model matrix
static void apply_model_uniforms(GMat4 model) {
    GMat4 mvp = mat4_multiply(g3d_projection, mat4_multiply(g3d_view, model));
    GMat4 normal_mat = mat4_transpose(mat4_inverse(model));

    glUniformMatrix4fv(g3d_u_mvp, 1, GL_FALSE, mvp.m);
    glUniformMatrix4fv(g3d_u_model, 1, GL_FALSE, model.m);
    glUniformMatrix4fv(g3d_u_normal_mat, 1, GL_FALSE, normal_mat.m);
}

// Translucent draw queue: deferred so alpha objects render back-to-front
// after everything opaque, with depth writes off.
typedef struct {
    GLuint vao;
    int vert_count;
    GMat4 model;
    GColor color;
    int unlit;
} TransEntry;

#define TRANS_QUEUE_MAX 2048
static TransEntry g3d_trans_queue[TRANS_QUEUE_MAX];
static int g3d_trans_count = 0;

static void draw_vao_now(GLuint vao, int vert_count, GMat4 model,
                         GColor color, int unlit) {
    flush_batch_3d(); // preserve draw order vs immediate path
    glUseProgram(g3d_shader);
    glUniform1i(g3d_u_use_lighting, unlit ? 0 : (g3d_light_count > 0 ? 1 : 0));
    apply_model_uniforms(model);
    glUniform4f(g3d_u_color_mul, color.r/255.0f, color.g/255.0f,
                color.b/255.0f, color.a/255.0f);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vert_count);
}

static void draw_template_opaque_now(GeoTemplate *t, GMat4 model, GColor color, int unlit) {
    draw_vao_now(t->vao, t->vert_count, model, color, unlit);
}

static void draw_template(GeoTemplate *t, GMat4 local_model, GColor color, int unlit) {
    if (color.a < 250 && g3d_trans_count < TRANS_QUEUE_MAX) {
        TransEntry *e = &g3d_trans_queue[g3d_trans_count++];
        e->vao = t->vao;
        e->vert_count = t->vert_count;
        e->model = mat4_multiply(gl3d_current_model(), local_model);
        e->color = color;
        e->unlit = unlit;
        return;
    }
    GMat4 full = mat4_multiply(gl3d_current_model(), local_model);
    draw_template_opaque_now(t, full, color, unlit);
}

static GVec3 trans_centroid(const TransEntry *e) {
    return (GVec3){e->model.m[12], e->model.m[13], e->model.m[14]};
}

static int trans_cmp(const void *pa, const void *pb) {
    const TransEntry *a = pa, *b = pb;
    GVec3 ca = trans_centroid(a), cb = trans_centroid(b);
    GVec3 eye = g3d_current_cam.position;
    float da = (ca.x-eye.x)*(ca.x-eye.x) + (ca.y-eye.y)*(ca.y-eye.y) + (ca.z-eye.z)*(ca.z-eye.z);
    float db = (cb.x-eye.x)*(cb.x-eye.x) + (cb.y-eye.y)*(cb.y-eye.y) + (cb.z-eye.z)*(cb.z-eye.z);
    if (da > db) return -1; // farthest first
    if (da < db) return 1;
    return 0;
}

static void flush_translucent_pass(void) {
    if (g3d_trans_count == 0) return;

    qsort(g3d_trans_queue, (size_t)g3d_trans_count, sizeof(TransEntry), trans_cmp);

    flush_batch_3d();
    glDepthMask(GL_FALSE);
    for (int i = 0; i < g3d_trans_count; i++) {
        TransEntry *e = &g3d_trans_queue[i];
        glUseProgram(g3d_shader);
        glUniform1i(g3d_u_use_lighting, e->unlit ? 0 : (g3d_light_count > 0 ? 1 : 0));
        apply_model_uniforms(e->model);
        glUniform4f(g3d_u_color_mul, e->color.r/255.0f, e->color.g/255.0f,
                    e->color.b/255.0f, e->color.a/255.0f);
        glBindVertexArray(e->vao);
        glDrawArrays(GL_TRIANGLES, 0, e->vert_count);
    }
    glDepthMask(GL_TRUE);
    g3d_trans_count = 0;
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

        // Spot cone angles in degrees -> cosines
        float inner_cos = cosf(g3d_lights[i].inner_deg * (float)M_PI / 180.0f);
        float outer_cos = cosf(g3d_lights[i].outer_deg * (float)M_PI / 180.0f);
        glUniform1f(g3d_u_light_inner[i], inner_cos);
        glUniform1f(g3d_u_light_outer[i], outer_cos);
    }
}

static void upload_atmosphere_uniforms(void) {
    glUseProgram(g3d_shader);
    glUniform1f(g3d_u_shininess, g3d_shininess);
    glUniform1f(g3d_u_spec_strength, g3d_spec_strength);
    float fc[3] = {g3d_fog_color.r/255.0f, g3d_fog_color.g/255.0f, g3d_fog_color.b/255.0f};
    glUniform3fv(g3d_u_fog_color, 1, fc);
    glUniform1f(g3d_u_fog_density, g3d_fog_density);
    glUniform1i(g3d_u_gamma, g3d_gamma);
    glUniform4f(g3d_u_color_mul, 1.0f, 1.0f, 1.0f, 1.0f);
}


// LIFECYCLE

int gl3d_init(void) {
    if (g3d_initialized) return 0;

    g3d_shader = create_program_3d(vs_3d_src, fs_3d_src);
    g3d_u_mvp = glGetUniformLocation(g3d_shader, "uMVP");
    g3d_u_model = glGetUniformLocation(g3d_shader, "uModel");
    g3d_u_normal_mat = glGetUniformLocation(g3d_shader, "uNormalMat");
    g3d_u_use_lighting = glGetUniformLocation(g3d_shader, "uUseLighting");
    g3d_u_ambient = glGetUniformLocation(g3d_shader, "uAmbient");
    g3d_u_view_pos = glGetUniformLocation(g3d_shader, "uViewPos");
    g3d_u_color_mul = glGetUniformLocation(g3d_shader, "uColorMul");
    g3d_u_shininess = glGetUniformLocation(g3d_shader, "uShininess");
    g3d_u_spec_strength = glGetUniformLocation(g3d_shader, "uSpecStrength");
    g3d_u_fog_color = glGetUniformLocation(g3d_shader, "uFogColor");
    g3d_u_fog_density = glGetUniformLocation(g3d_shader, "uFogDensity");
    g3d_u_gamma = glGetUniformLocation(g3d_shader, "uGamma");
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
        snprintf(buf, sizeof(buf), "uLightInner[%d]", i);
        g3d_u_light_inner[i] = glGetUniformLocation(g3d_shader, buf);
        snprintf(buf, sizeof(buf), "uLightOuter[%d]", i);
        g3d_u_light_outer[i] = glGetUniformLocation(g3d_shader, buf);
    }

    // Immediate-mode batch VAO/VBO
    glGenVertexArrays(1, &g3d_vao);
    glGenBuffers(1, &g3d_vbo);
    glBindVertexArray(g3d_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g3d_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g3d_batch), NULL, GL_DYNAMIC_DRAW);
    bind_geo_attribs();
    glBindVertexArray(0);

    // Cached unit templates (white vertex colors; tinted per draw via uColorMul)
    build_cube_template();
    build_sphere_template(18, 26);
    build_cylinder_template(32);
    build_plane_template();
    build_quad_template();

    gl3d_reset_runtime();
    g3d_initialized = 1;
    return 0;
}

void gl3d_reset_runtime(void) {
    g3d_light_count = 0;
    memset(g3d_lights, 0, sizeof(g3d_lights));
    g3d_ambient = (GColor){40, 40, 60, 255};
    g3d_shininess = 32.0f;
    g3d_spec_strength = 0.3f;
    g3d_fog_color = (GColor){15, 15, 25, 255};
    g3d_fog_density = 0.0f;
    g3d_gamma = 0;
    g3d_free_camera_mouse_initialized = 0;
    g3d_matrix_depth = 0;
    g3d_matrix_stack[0] = mat4_identity();
    g3d_batch_count = 0;
    g3d_trans_count = 0;
}

void gl3d_shutdown(void) {
    if (!g3d_initialized) return;
    for (int i = 0; i < TMPL_COUNT; i++) {
        if (g3d_templates[i].vao) glDeleteVertexArrays(1, &g3d_templates[i].vao);
        if (g3d_templates[i].vbo) glDeleteBuffers(1, &g3d_templates[i].vbo);
        g3d_templates[i] = (GeoTemplate){0};
    }
    if (g3d_vao) glDeleteVertexArrays(1, &g3d_vao);
    if (g3d_vbo) glDeleteBuffers(1, &g3d_vbo);
    if (g3d_shader) glDeleteProgram(g3d_shader);
    g3d_vao = g3d_vbo = g3d_shader = 0;
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
    if (gl_is_key_down(GKEY_LEFT_SHIFT)) step *= 2.5f;
    GVec3 move = {0};

    if (gl_is_key_down(GKEY_W)) move = vec3_add(move, vec3_scale(look, step));
    if (gl_is_key_down(GKEY_S)) move = vec3_add(move, vec3_scale(look, -step));
    if (gl_is_key_down(GKEY_A)) move = vec3_add(move, vec3_scale(right, -step));
    if (gl_is_key_down(GKEY_D)) move = vec3_add(move, vec3_scale(right, step));
    if (gl_is_key_down(GKEY_E)) move = vec3_add(move, vec3_scale(up, step));
    if (gl_is_key_down(GKEY_Q)) move = vec3_add(move, vec3_scale(up, -step));

    cam->position = vec3_add(cam->position, move);
    cam->target = vec3_add(cam->position, look);
}

void gl3d_update_camera_orbit(GCamera3D *cam, GVec3 center, float dist,
                              float yaw_deg, float pitch_deg) {
    if (!cam) return;
    float pitch = clampf(pitch_deg * (float)M_PI / 180.0f, -1.5533f, 1.5533f);
    float yaw = yaw_deg * (float)M_PI / 180.0f;

    GVec3 offset = {
        cosf(pitch) * cosf(yaw),
        sinf(pitch),
        cosf(pitch) * sinf(yaw)
    };

    cam->target = center;
    cam->position = vec3_add(center, vec3_scale(offset, dist));
}


void gl3d_begin_mode_3d(GCamera3D cam) {
    if (!g3d_initialized) gl3d_init();

    // Flush anything pending from an earlier 3D block instead of discarding it
    flush_batch_3d();

    g3d_current_cam = cam;

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Build projection matrix
    float aspect = (float)g_win_w / (float)g_win_h;
    if (cam.projection == 0) {
        g3d_projection = mat4_perspective(cam.fov, aspect, 0.1f, 1000.0f);
    } else {
        g3d_projection = mat4_ortho(-aspect * cam.fov, aspect * cam.fov,
                                    -cam.fov, cam.fov, 0.1f, 1000.0f);
    }

    // Build view matrix
    g3d_view = mat4_look_at(cam.position, cam.target, cam.up);

    // Reset transform stack to identity
    g3d_matrix_depth = 0;
    g3d_matrix_stack[0] = mat4_identity();

    // Upload all per-frame uniforms
    apply_model_uniforms(g3d_matrix_stack[0]);
    upload_lighting_uniforms();
    upload_atmosphere_uniforms();
}

void gl3d_end_mode_3d(void) {
    flush_batch_3d();
    flush_translucent_pass();
    glDisable(GL_DEPTH_TEST);
    glUseProgram(g3d_shader);
    glUniform4f(g3d_u_color_mul, 1.0f, 1.0f, 1.0f, 1.0f);
}


// PRIMITIVES (cached-template path)

void gl3d_draw_cube(GVec3 pos, GVec3 size, GColor color) {
    GMat4 model = mat4_multiply(mat4_translate(pos), mat4_scale(size));
    draw_template(&g3d_templates[TMPL_CUBE], model, color, 0);
}

void gl3d_draw_cube_pro(GVec3 pos, GVec3 size, GVec3 rot_deg, GColor color, int lit) {
    GMat4 model = mat4_translate(pos);
    model = mat4_multiply(model, mat4_rotate((GVec3){1, 0, 0}, rot_deg.x));
    model = mat4_multiply(model, mat4_rotate((GVec3){0, 1, 0}, rot_deg.y));
    model = mat4_multiply(model, mat4_rotate((GVec3){0, 0, 1}, rot_deg.z));
    model = mat4_multiply(model, mat4_scale(size));
    draw_template(&g3d_templates[TMPL_CUBE], model, color, !lit);
}

void gl3d_draw_sphere(GVec3 center, float radius, int rings, int slices, GColor color) {
    (void)rings; (void)slices; // single cached LOD
    GMat4 model = mat4_multiply(mat4_translate(center), mat4_scale((GVec3){radius, radius, radius}));
    draw_template(&g3d_templates[TMPL_SPHERE], model, color, 0);
}

void gl3d_draw_sphere_pro(GVec3 center, float radius, GVec3 rot_deg, GColor color, int lit) {
    GMat4 model = mat4_translate(center);
    model = mat4_multiply(model, mat4_rotate((GVec3){1, 0, 0}, rot_deg.x));
    model = mat4_multiply(model, mat4_rotate((GVec3){0, 1, 0}, rot_deg.y));
    model = mat4_multiply(model, mat4_rotate((GVec3){0, 0, 1}, rot_deg.z));
    model = mat4_multiply(model, mat4_scale((GVec3){radius, radius, radius}));
    draw_template(&g3d_templates[TMPL_SPHERE], model, color, !lit);
}

void gl3d_draw_cylinder(GVec3 pos, float rtop, float rbot, float height, int slices, GColor color) {
    (void)slices; // single cached LOD
    // Non-uniform radii unsupported by the shared template; approximate by
    // scaling X/Z by average radius. Cone-like shapes still read correctly.
    float ravg = (rtop + rbot) * 0.5f;
    GMat4 model = mat4_multiply(mat4_translate(pos),
                                mat4_scale((GVec3){ravg * 2.0f, height, ravg * 2.0f}));
    draw_template(&g3d_templates[TMPL_CYLINDER], model, color, 0);
}

void gl3d_draw_cylinder_pro(GVec3 pos, float rtop, float rbot, float height,
                            GVec3 rot_deg, GColor color, int lit) {
    float ravg = (rtop + rbot) * 0.5f;
    GMat4 model = mat4_translate(pos);
    model = mat4_multiply(model, mat4_rotate((GVec3){1, 0, 0}, rot_deg.x));
    model = mat4_multiply(model, mat4_rotate((GVec3){0, 1, 0}, rot_deg.y));
    model = mat4_multiply(model, mat4_rotate((GVec3){0, 0, 1}, rot_deg.z));
    model = mat4_multiply(model, mat4_scale((GVec3){ravg * 2.0f, height, ravg * 2.0f}));
    draw_template(&g3d_templates[TMPL_CYLINDER], model, color, !lit);
}

void gl3d_draw_plane(GVec3 center, GVec2 size, GColor color) {
    GMat4 model = mat4_multiply(mat4_translate(center),
                                mat4_scale((GVec3){size.x, 1.0f, size.y}));
    draw_template(&g3d_templates[TMPL_PLANE], model, color, 0);
}

void gl3d_draw_plane_pro(GVec3 center, GVec2 size, GVec3 rot_deg, GColor color, int lit) {
    GMat4 model = mat4_translate(center);
    model = mat4_multiply(model, mat4_rotate((GVec3){1, 0, 0}, rot_deg.x));
    model = mat4_multiply(model, mat4_rotate((GVec3){0, 1, 0}, rot_deg.y));
    model = mat4_multiply(model, mat4_rotate((GVec3){0, 0, 1}, rot_deg.z));
    model = mat4_multiply(model, mat4_scale((GVec3){size.x, 1.0f, size.y}));
    draw_template(&g3d_templates[TMPL_PLANE], model, color, !lit);
}

// Camera-facing billboard quad (unlit by design — particles/markers)
void gl3d_draw_particle_3d(GVec3 pos, float size, GColor color) {
    GVec3 to_cam = vec3_normalize(vec3_sub(g3d_current_cam.position, pos));
    if (to_cam.x == 0 && to_cam.y == 0 && to_cam.z == 0) return;

    GVec3 up_world = {0, 1, 0};
    if (fabsf(to_cam.y) > 0.999f) up_world = (GVec3){1, 0, 0};
    GVec3 right = vec3_normalize(vec3_cross(up_world, to_cam));
    GVec3 up = vec3_cross(to_cam, right);

    GMat4 model;
    model.m[0] = right.x * size; model.m[1] = right.y * size; model.m[2]  = right.z * size; model.m[3]  = 0;
    model.m[4] = up.x * size;    model.m[5] = up.y * size;    model.m[6]  = up.z * size;    model.m[7]  = 0;
    model.m[8] = to_cam.x;       model.m[9] = to_cam.y;       model.m[10] = to_cam.z;       model.m[11] = 0;
    model.m[12] = pos.x;         model.m[13] = pos.y;         model.m[14] = pos.z;          model.m[15] = 1;

    draw_template(&g3d_templates[TMPL_QUAD], model, color, 1);
}


// EXTERNAL MESHES (loaded models)

// Upload an interleaved vertex buffer (x,y,z,nx,ny,nz,r,g,b,a per vertex,
// triangles). Returns 0 on success.
int gl3d_upload_mesh(const float *verts, int vert_count, int *vao_out, int *vbo_out) {
    if (!verts || vert_count <= 0 || !vao_out || !vbo_out) return -1;

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vert_count * VERT3D_STRIDE * sizeof(float),
                 verts, GL_STATIC_DRAW);
    bind_geo_attribs();
    glBindVertexArray(0);
    *vao_out = (int)vao;
    *vbo_out = (int)vbo;
    return 0;
}

void gl3d_free_mesh(int vao_h, int vbo_h) {
    GLuint vao = (GLuint)vao_h, vbo = (GLuint)vbo_h;
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
}

// Draw a loaded mesh composed with the current transform stack.
// Tint multiplies the baked vertex colors.
void gl3d_draw_mesh(int vao_h, int vert_count, GMat4 local_model,
                    GColor tint, int lit) {
    GLuint vao = (GLuint)vao_h;
    if (!vao || vert_count <= 0) return;

    GMat4 full = mat4_multiply(gl3d_current_model(), local_model);
    if (tint.a < 250 && g3d_trans_count < TRANS_QUEUE_MAX) {
        TransEntry *e = &g3d_trans_queue[g3d_trans_count++];
        e->vao = vao;
        e->vert_count = vert_count;
        e->model = full;
        e->color = tint;
        e->unlit = !lit;
        return;
    }
    draw_vao_now(vao, vert_count, full, tint, lit);
}

void gl3d_draw_grid(int slices, float spacing) {
    // Grid stays on the immediate path: thin camera-facing quads per line.
    glUseProgram(g3d_shader);
    glUniform1i(g3d_u_use_lighting, 0);

    float half = slices * spacing * 0.5f;
    GColor grid_color = {100, 100, 100, 255};

    for (int i = 0; i <= slices; i++) {
        float pos = -half + i * spacing;
        gl3d_draw_line_3d((GVec3){pos, 0, -half}, (GVec3){pos, 0, half}, grid_color);
        gl3d_draw_line_3d((GVec3){-half, 0, pos}, (GVec3){half, 0, pos}, grid_color);
    }

    // Restore lighting state
    upload_lighting_uniforms();
}

void gl3d_draw_cube_wires(GVec3 pos, GVec3 size, GColor color) {
    float hw = size.x * 0.5f, hh = size.y * 0.5f, hd = size.z * 0.5f;
    GVec3 v[8] = {
        {pos.x-hw, pos.y-hh, pos.z-hd}, {pos.x+hw, pos.y-hh, pos.z-hd},
        {pos.x+hw, pos.y+hh, pos.z-hd}, {pos.x-hw, pos.y+hh, pos.z-hd},
        {pos.x-hw, pos.y-hh, pos.z+hd}, {pos.x+hw, pos.y-hh, pos.z+hd},
        {pos.x+hw, pos.y+hh, pos.z+hd}, {pos.x-hw, pos.y+hh, pos.z+hd}
    };

    int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    for (int i = 0; i < 12; i++) {
        gl3d_draw_line_3d(v[edges[i][0]], v[edges[i][1]], color);
    }
}

void gl3d_draw_line_3d(GVec3 start, GVec3 end, GColor color) {
    // Draw line as thin triangulated quad
    GVec3 dir = vec3_sub(end, start);
    float len = sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    if (len < 0.0001f) return;

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


// LIGHTS

int gl3d_create_light(int type, GVec3 position, GVec3 target, GColor color) {
    if (g3d_light_count >= GL3D_MAX_LIGHTS) return -1;
    int id = g3d_light_count++;
    g3d_lights[id].type = type;
    g3d_lights[id].position = position;
    g3d_lights[id].target = target;
    g3d_lights[id].color = color;
    g3d_lights[id].intensity = 1.0f;
    g3d_lights[id].enabled = 1;
    g3d_lights[id].inner_deg = 12.5f;
    g3d_lights[id].outer_deg = 20.0f;
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

void gl3d_set_light_target(int id, GVec3 target) {
    if (id >= 0 && id < g3d_light_count) g3d_lights[id].target = target;
}

void gl3d_set_light_cone(int id, float inner_deg, float outer_deg) {
    if (id >= 0 && id < g3d_light_count) {
        g3d_lights[id].inner_deg = clampf(inner_deg, 0.0f, 89.0f);
        g3d_lights[id].outer_deg = clampf(outer_deg, g3d_lights[id].inner_deg, 89.9f);
    }
}

void gl3d_set_light_intensity(int id, float intensity) {
    if (id >= 0 && id < g3d_light_count) g3d_lights[id].intensity = intensity;
}

void gl3d_set_ambient_light(GColor color) {
    g3d_ambient = color;
}

void gl3d_reset_lights(void) {
    g3d_light_count = 0;
    memset(g3d_lights, 0, sizeof(g3d_lights));
}


// MATERIAL / ATMOSPHERE

void gl3d_set_material(float shininess, float spec_strength) {
    g3d_shininess = clampf(shininess, 1.0f, 256.0f);
    g3d_spec_strength = clampf(spec_strength, 0.0f, 4.0f);
}

void gl3d_set_fog(GColor color, float density) {
    g3d_fog_color = color;
    g3d_fog_density = (density < 0.0f) ? 0.0f : density;
}

void gl3d_set_gamma(int enabled) {
    g3d_gamma = enabled ? 1 : 0;
}


// COLLISION

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


// PICKING

// Build a world-space ray through the mouse cursor for the given camera
GRay3D gl3d_get_mouse_ray(GCamera3D cam) {
    GRay3D ray = {{0}, {0, 0, -1}};

    GMat4 projection;
    float aspect = (float)g_win_w / (float)g_win_h;
    if (cam.projection == 0) {
        projection = mat4_perspective(cam.fov, aspect, 0.1f, 1000.0f);
    } else {
        projection = mat4_ortho(-aspect * cam.fov, aspect * cam.fov,
                                -cam.fov, cam.fov, 0.1f, 1000.0f);
    }
    GMat4 view = mat4_look_at(cam.position, cam.target, cam.up);
    GMat4 vp_inv = mat4_inverse(mat4_multiply(projection, view));

    GVec2 mouse = gl_get_mouse_position();
    float ndc_x = (mouse.x / (float)g_win_w) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (mouse.y / (float)g_win_h) * 2.0f;

    // Unproject near and far clip points to world space
    GVec3 p_near = {
        vp_inv.m[0]*ndc_x + vp_inv.m[4]*ndc_y + vp_inv.m[8]*(-1.0f) + vp_inv.m[12],
        vp_inv.m[1]*ndc_x + vp_inv.m[5]*ndc_y + vp_inv.m[9]*(-1.0f) + vp_inv.m[13],
        vp_inv.m[2]*ndc_x + vp_inv.m[6]*ndc_y + vp_inv.m[10]*(-1.0f) + vp_inv.m[14]
    };
    float w_near = vp_inv.m[3]*ndc_x + vp_inv.m[7]*ndc_y + vp_inv.m[11]*(-1.0f) + vp_inv.m[15];
    GVec3 p_far = {
        vp_inv.m[0]*ndc_x + vp_inv.m[4]*ndc_y + vp_inv.m[8] + vp_inv.m[12],
        vp_inv.m[1]*ndc_x + vp_inv.m[5]*ndc_y + vp_inv.m[9] + vp_inv.m[13],
        vp_inv.m[2]*ndc_x + vp_inv.m[6]*ndc_y + vp_inv.m[10] + vp_inv.m[14]
    };
    float w_far = vp_inv.m[3]*ndc_x + vp_inv.m[7]*ndc_y + vp_inv.m[11] + vp_inv.m[15];

    if (w_near == 0 || w_far == 0) return ray;
    p_near = vec3_scale(p_near, 1.0f / w_near);
    p_far  = vec3_scale(p_far,  1.0f / w_far);

    ray.origin = p_near;
    ray.direction = vec3_normalize(vec3_sub(p_far, p_near));
    return ray;
}

// Slab-method ray/AABB test
int gl3d_ray_hits_box(GRay3D ray, GBoundingBox box) {
    float tmin = -1e9f, tmax = 1e9f;
    float *ro = &ray.origin.x;
    float *rd = &ray.direction.x;
    float *bmin = &box.min.x;
    float *bmax = &box.max.x;

    for (int i = 0; i < 3; i++) {
        if (fabsf(rd[i]) < 1e-8f) {
            if (ro[i] < bmin[i] || ro[i] > bmax[i]) return 0;
        } else {
            float t1 = (bmin[i] - ro[i]) / rd[i];
            float t2 = (bmax[i] - ro[i]) / rd[i];
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            tmin = (t1 > tmin) ? t1 : tmin;
            tmax = (t2 < tmax) ? t2 : tmax;
            if (tmin > tmax) return 0;
        }
    }
    return tmax >= 0.0f;
}

// Distance along the ray where it crosses the plane, or -1
float gl3d_ray_plane_distance(GRay3D ray, GVec3 point, GVec3 normal) {
    GVec3 n = vec3_normalize(normal);
    float denom = vec3_dot(n, ray.direction);
    if (fabsf(denom) < 1e-6f) return -1.0f;
    float t = vec3_dot(vec3_sub(point, ray.origin), n) / denom;
    return (t >= 0.0f) ? t : -1.0f;
}

// Point on a ray at a given distance
GVec3 gl3d_ray_at(GRay3D ray, float dist) {
    return vec3_add(ray.origin, vec3_scale(ray.direction, dist));
}


int gl3d_get_win_w(void) { return g_win_w; }
int gl3d_get_win_h(void) { return g_win_h; }

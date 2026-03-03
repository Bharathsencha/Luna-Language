// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// gl_backend.c — OpenGL 3.3 Core Profile + GLFW backend for Luna GUI
// OpenGL 3.3 backend. Uses stb_image for textures, stb_truetype for fonts.

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include "gl_backend.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// INTERNAL STATE

static GLFWwindow *g_window = NULL;
static int g_win_w = 800, g_win_h = 600;
static int g_target_fps = 60;
static double g_frame_time = 0.0;
static double g_last_time = 0.0;

#define MAX_KEYS 512
static int g_key_current[MAX_KEYS];
static int g_key_previous[MAX_KEYS];
static int g_mouse_current[8];
static int g_mouse_previous[8];
static double g_mouse_x, g_mouse_y;
static float g_mouse_wheel;

static GLuint g_shader_quad = 0;     // textured/colored quads
static GLuint g_vao_quad = 0, g_vbo_quad = 0;
static GLint  g_u_projection = -1;
static GLint  g_u_use_texture = -1;

// Each vertex: x, y, u, v, r, g, b, a  (8 floats)
#define VERT_STRIDE 8
#define MAX_BATCH_VERTS (65536)
static float g_batch[MAX_BATCH_VERTS * VERT_STRIDE];
static int   g_batch_count = 0;
static GLuint g_batch_tex = 0; // 0 = no texture (solid color)

// 1x1 white pixel texture for solid shapes
static GLuint g_white_tex = 0;

#define MAX_TEX 128
typedef struct {
    GLuint id;
    int w, h;
    int active;
} TexSlot;
static TexSlot g_textures[MAX_TEX];
static int g_tex_count = 0;

#define MAX_FONTS_CACHE 16
#define FONT_ATLAS_W 1024
#define FONT_ATLAS_H 1024
#define FONT_FIRST_CHAR 32
#define FONT_NUM_CHARS 96

typedef struct {
    GLuint atlas_tex;
    stbtt_bakedchar cdata[FONT_NUM_CHARS];
    float pixel_size;
    int active;
} FontSlot;
static FontSlot g_fonts[MAX_FONTS_CACHE];
static int g_font_count = 0;

static int g_default_font_loaded = 0;
static FontSlot g_default_font;

#define MAX_IMAGES_CACHE 16
typedef struct {
    unsigned char *data;
    int w, h, channels;
    int active;
} ImageSlot;
static ImageSlot g_images[MAX_IMAGES_CACHE];
static int g_image_count = 0;

#define MAX_RT 8
typedef struct {
    GLuint fbo, tex, rbo;
    int w, h;
    int active;
} RTSlot;
static RTSlot g_render_textures[MAX_RT];
static int g_rt_count = 0;

static int g_active_fbo = 0; // 0 = default framebuffer

static int g_camera_active = 0;
static GCamera2D g_camera;

// SHADER SOURCES

static const char *vs_quad_src =
    "#version 330 core\n"
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec2 aUV;\n"
    "layout(location=2) in vec4 aColor;\n"
    "uniform mat4 uProjection;\n"
    "out vec2 vUV;\n"
    "out vec4 vColor;\n"
    "void main() {\n"
    "    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);\n"
    "    vUV = aUV;\n"
    "    vColor = aColor;\n"
    "}\n";

static const char *fs_quad_src =
    "#version 330 core\n"
    "in vec2 vUV;\n"
    "in vec4 vColor;\n"
    "uniform sampler2D uTexture;\n"
    "uniform int uUseTexture;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    if (uUseTexture == 1) {\n"
    "        FragColor = texture(uTexture, vUV) * vColor;\n"
    "    } else {\n"
    "        FragColor = vColor;\n"
    "    }\n"
    "}\n";

// SHADER HELPERS

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    int ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, NULL, log);
        fprintf(stderr, "[GL Backend] Shader compile error: %s\n", log);
    }
    return s;
}

static GLuint create_program(const char *vs, const char *fs) {
    GLuint v = compile_shader(GL_VERTEX_SHADER, vs);
    GLuint f = compile_shader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    int ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, 512, NULL, log);
        fprintf(stderr, "[GL Backend] Program link error: %s\n", log);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

// PROJECTION MATRIX

static void set_ortho(float l, float r, float b, float t) {
    float m[16] = {0};
    m[0]  = 2.0f / (r - l);
    m[5]  = 2.0f / (t - b);
    m[10] = -1.0f;
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[15] = 1.0f;

    // Apply camera transform if active
    if (g_camera_active) {
        float cx = g_camera.target.x;
        float cy = g_camera.target.y;
        float ox = g_camera.offset.x;
        float oy = g_camera.offset.y;
        float zoom = g_camera.zoom;
        float rot = g_camera.rotation * 3.14159265f / 180.0f;
        float cosR = cosf(rot);
        float sinR = sinf(rot);

        // Camera transformation: translate to offset, scale, rotate around target
        // Build camera matrix: T(offset) * R(rot) * S(zoom) * T(-target)
        float cam[16] = {0};
        cam[0]  =  cosR * zoom;
        cam[1]  =  sinR * zoom;
        cam[4]  = -sinR * zoom;
        cam[5]  =  cosR * zoom;
        cam[10] = 1.0f;
        cam[12] = ox - cx * cosR * zoom + cy * sinR * zoom;
        cam[13] = oy - cx * sinR * zoom - cy * cosR * zoom;
        cam[15] = 1.0f;

        // result = ortho * cam (column-major multiplication)
        float res[16] = {0};
        for (int col = 0; col < 4; col++)
            for (int row = 0; row < 4; row++)
                for (int k = 0; k < 4; k++)
                    res[col * 4 + row] += m[k * 4 + row] * cam[col * 4 + k];
        memcpy(m, res, sizeof(m));
    }

    glUseProgram(g_shader_quad);
    glUniformMatrix4fv(g_u_projection, 1, GL_FALSE, m);
}

// BATCH RENDERER

static void flush_batch(void) {
    if (g_batch_count == 0) return;

    glUseProgram(g_shader_quad);
    glBindVertexArray(g_vao_quad);

    if (g_batch_tex) {
        glUniform1i(g_u_use_texture, 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_batch_tex);
    } else {
        glUniform1i(g_u_use_texture, 0);
        // Bind white texture for solid color mode
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_white_tex);
    }

    glBindBuffer(GL_ARRAY_BUFFER, g_vbo_quad);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    g_batch_count * VERT_STRIDE * sizeof(float), g_batch);
    glDrawArrays(GL_TRIANGLES, 0, g_batch_count);

    g_batch_count = 0;
    g_batch_tex = 0;
}

static void ensure_batch(GLuint tex, int verts_needed) {
    if (g_batch_count + verts_needed > MAX_BATCH_VERTS || g_batch_tex != tex) {
        flush_batch();
        g_batch_tex = tex;
    }
}

// Push a single vertex into the batch
static inline void push_vert(float x, float y, float u, float v,
                              float r, float g, float b, float a) {
    int i = g_batch_count * VERT_STRIDE;
    g_batch[i + 0] = x;
    g_batch[i + 1] = y;
    g_batch[i + 2] = u;
    g_batch[i + 3] = v;
    g_batch[i + 4] = r;
    g_batch[i + 5] = g;
    g_batch[i + 6] = b;
    g_batch[i + 7] = a;
    g_batch_count++;
}

// Push a colored quad (two triangles) - no texture
static void push_quad_color(float x, float y, float w, float h, GColor c) {
    ensure_batch(0, 6);
    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f, a = c.a / 255.0f;
    // Triangle 1
    push_vert(x,     y,     0, 0, r, g, b, a);
    push_vert(x + w, y,     0, 0, r, g, b, a);
    push_vert(x + w, y + h, 0, 0, r, g, b, a);
    // Triangle 2
    push_vert(x,     y,     0, 0, r, g, b, a);
    push_vert(x + w, y + h, 0, 0, r, g, b, a);
    push_vert(x,     y + h, 0, 0, r, g, b, a);
}

// Push a textured quad with tint
static void push_quad_tex(GLuint tex, float x, float y, float w, float h,
                           float u0, float v0, float u1, float v1, GColor tint) {
    ensure_batch(tex, 6);
    float r = tint.r / 255.0f, g = tint.g / 255.0f, b = tint.b / 255.0f, a = tint.a / 255.0f;
    push_vert(x,     y,     u0, v0, r, g, b, a);
    push_vert(x + w, y,     u1, v0, r, g, b, a);
    push_vert(x + w, y + h, u1, v1, r, g, b, a);
    push_vert(x,     y,     u0, v0, r, g, b, a);
    push_vert(x + w, y + h, u1, v1, r, g, b, a);
    push_vert(x,     y + h, u0, v1, r, g, b, a);
}

// Push a textured quad with rotation (for DrawTexturePro equivalent)
static void push_quad_tex_rotated(GLuint tex,
                                   GRect src, GRect dest,
                                   GVec2 origin, float rot_deg,
                                   float tex_w, float tex_h, GColor tint) {
    ensure_batch(tex, 6);
    float r = tint.r / 255.0f, g = tint.g / 255.0f, b = tint.b / 255.0f, a = tint.a / 255.0f;

    // UV coordinates from source rect (use fabsf so negative dims flip correctly)
    float u0 = src.x / tex_w;
    float v0 = src.y / tex_h;
    float u1 = (src.x + fabsf(src.w)) / tex_w;
    float v1 = (src.y + fabsf(src.h)) / tex_h;

    // Handle negative src dimensions (used for flipping)
    if (src.w < 0) { float t = u0; u0 = u1; u1 = t; }
    if (src.h < 0) { float t = v0; v0 = v1; v1 = t; }

    float rot = rot_deg * 3.14159265f / 180.0f;
    float cosR = cosf(rot), sinR = sinf(rot);

    // 4 corners relative to origin
    float dx = dest.x, dy = dest.y;
    float ox = origin.x, oy = origin.y;

    float corners[4][2] = {
        {-ox,          -oy},
        {dest.w - ox,  -oy},
        {dest.w - ox,  dest.h - oy},
        {-ox,          dest.h - oy},
    };

    float tx[4], ty[4];
    for (int i = 0; i < 4; i++) {
        tx[i] = dx + corners[i][0] * cosR - corners[i][1] * sinR;
        ty[i] = dy + corners[i][0] * sinR + corners[i][1] * cosR;
    }

    float uvs[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};

    push_vert(tx[0], ty[0], uvs[0][0], uvs[0][1], r, g, b, a);
    push_vert(tx[1], ty[1], uvs[1][0], uvs[1][1], r, g, b, a);
    push_vert(tx[2], ty[2], uvs[2][0], uvs[2][1], r, g, b, a);
    push_vert(tx[0], ty[0], uvs[0][0], uvs[0][1], r, g, b, a);
    push_vert(tx[2], ty[2], uvs[2][0], uvs[2][1], r, g, b, a);
    push_vert(tx[3], ty[3], uvs[3][0], uvs[3][1], r, g, b, a);
}

// Push a quad with per-vertex colors (gradient)
static void push_quad_gradient(float x, float y, float w, float h,
                                GColor tl, GColor tr, GColor bl, GColor br) {
    ensure_batch(0, 6);
    float tl_r = tl.r/255.0f, tl_g = tl.g/255.0f, tl_b = tl.b/255.0f, tl_a = tl.a/255.0f;
    float tr_r = tr.r/255.0f, tr_g = tr.g/255.0f, tr_b = tr.b/255.0f, tr_a = tr.a/255.0f;
    float bl_r = bl.r/255.0f, bl_g = bl.g/255.0f, bl_b = bl.b/255.0f, bl_a = bl.a/255.0f;
    float br_r = br.r/255.0f, br_g = br.g/255.0f, br_b = br.b/255.0f, br_a = br.a/255.0f;
    // TL, TR, BR
    push_vert(x,     y,     0, 0, tl_r, tl_g, tl_b, tl_a);
    push_vert(x + w, y,     0, 0, tr_r, tr_g, tr_b, tr_a);
    push_vert(x + w, y + h, 0, 0, br_r, br_g, br_b, br_a);
    // TL, BR, BL
    push_vert(x,     y,     0, 0, tl_r, tl_g, tl_b, tl_a);
    push_vert(x + w, y + h, 0, 0, br_r, br_g, br_b, br_a);
    push_vert(x,     y + h, 0, 0, bl_r, bl_g, bl_b, bl_a);
}

// DEFAULT FONT

static GLuint upload_font_atlas_rgba(unsigned char *alpha_data, int w, int h);

// We try to find a system monospace font for the default
static void load_default_font(void) {
    if (g_default_font_loaded) return;

    // Search common locations for a default font
    const char *candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
        NULL
    };

    unsigned char *ttf_buffer = NULL;
    for (int i = 0; candidates[i]; i++) {
        FILE *f = fopen(candidates[i], "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            ttf_buffer = (unsigned char *)malloc(sz);
            if (ttf_buffer) {
                if (fread(ttf_buffer, 1, sz, f) != (size_t)sz) {
                    free(ttf_buffer);
                    ttf_buffer = NULL;
                }
            }
            fclose(f);
            break;
        }
    }

    if (!ttf_buffer) {
        fprintf(stderr, "[GL Backend] Warning: No system font found for default\n");
        g_default_font_loaded = 1;
        return;
    }

    unsigned char *atlas = (unsigned char *)malloc(FONT_ATLAS_W * FONT_ATLAS_H);
    stbtt_BakeFontBitmap(ttf_buffer, 0, 64.0f, atlas,
                         FONT_ATLAS_W, FONT_ATLAS_H,
                         FONT_FIRST_CHAR, FONT_NUM_CHARS,
                         g_default_font.cdata);
    free(ttf_buffer);

    // Upload atlas via the same swizzled path as custom fonts
    g_default_font.atlas_tex = upload_font_atlas_rgba(atlas, FONT_ATLAS_W, FONT_ATLAS_H);
    free(atlas);

    g_default_font.pixel_size = 64.0f;
    g_default_font.active = 1;
    g_default_font_loaded = 1;
}

// FONT RENDERING (stb_truetype glyph-by-glyph)

// We use a special shader mode for font rendering: the font atlas is single-channel (red).
// We'll use the existing shader but bind the atlas as a texture and use:
//   FragColor = vec4(vColor.rgb, vColor.a * texture(uTexture, vUV).r)
// To support this without a second shader, we upload font atlases as RGBA with the alpha channel.
// This is slightly wasteful but keeps the pipeline simple.

// Actually, let's use a swizzle mask approach: GL_TEXTURE_SWIZZLE
// Set swizzle: R->1, G->1, B->1, A->R  so sampling gives (1,1,1,alpha)
// Then multiply by vertex color gives (color.rgb, color.a * alpha) — perfect!

static GLuint upload_font_atlas_rgba(unsigned char *alpha_data, int w, int h) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, alpha_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Swizzle: sample R channel as alpha, and fill RGB with 1.0
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
    return tex;
}

static void draw_text_internal(FontSlot *fnt, const char *text,
                                float x, float y, float scale, GColor color) {
    if (!fnt || !fnt->active || !text) return;

    // Scale factor = desired size / baked size
    float s = scale / fnt->pixel_size;
    float start_x = x;
    (void)start_x;

    GLuint tex = fnt->atlas_tex;

    for (const char *p = text; *p; p++) {
        int ch = (unsigned char)*p;
        if (ch == '\n') {
            x = start_x;
            y += scale;
            continue;
        }
        if (ch < FONT_FIRST_CHAR || ch >= FONT_FIRST_CHAR + FONT_NUM_CHARS) continue;

        stbtt_bakedchar *b = &fnt->cdata[ch - FONT_FIRST_CHAR];

        float cx = x + b->xoff * s;
        float cy = y + b->yoff * s + scale; // baseline offset
        float w = (b->x1 - b->x0) * s;
        float h = (b->y1 - b->y0) * s;
        float u0 = b->x0 / (float)FONT_ATLAS_W;
        float v0 = b->y0 / (float)FONT_ATLAS_H;
        float u1 = b->x1 / (float)FONT_ATLAS_W;
        float v1 = b->y1 / (float)FONT_ATLAS_H;

        push_quad_tex(tex, cx, cy, w, h, u0, v0, u1, v1, color);

        x += b->xadvance * s;
    }
}

static float measure_text_internal(FontSlot *fnt, const char *text, float scale) {
    if (!fnt || !fnt->active || !text) return 0;
    float s = scale / fnt->pixel_size;
    float w = 0;
    for (const char *p = text; *p; p++) {
        int ch = (unsigned char)*p;
        if (ch < FONT_FIRST_CHAR || ch >= FONT_FIRST_CHAR + FONT_NUM_CHARS) continue;
        w += fnt->cdata[ch - FONT_FIRST_CHAR].xadvance * s;
    }
    return w;
}

static float measure_text_height_internal(FontSlot *fnt, const char *text, float scale) {
    (void)text;
    return scale; // Simple approximation
}

// CALLBACKS

static void scroll_callback(GLFWwindow *win, double xoff, double yoff) {
    (void)win; (void)xoff;
    g_mouse_wheel = (float)yoff;
}

static void framebuffer_size_callback(GLFWwindow *win, int w, int h) {
    (void)win;
    g_win_w = w;
    g_win_h = h;
    glViewport(0, 0, w, h);
}

// LIFECYCLE

int gl_init_window(int w, int h, const char *title) {
    if (!glfwInit()) {
        fprintf(stderr, "[GL Backend] Failed to init GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    g_window = glfwCreateWindow(w, h, title, NULL, NULL);
    if (!g_window) {
        fprintf(stderr, "[GL Backend] Failed to create window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);
    glfwSetScrollCallback(g_window, scroll_callback);
    glfwSetFramebufferSizeCallback(g_window, framebuffer_size_callback);

    g_win_w = w;
    g_win_h = h;

    // Create shader program
    g_shader_quad = create_program(vs_quad_src, fs_quad_src);
    g_u_projection = glGetUniformLocation(g_shader_quad, "uProjection");
    g_u_use_texture = glGetUniformLocation(g_shader_quad, "uUseTexture");

    // Create VAO/VBO
    glGenVertexArrays(1, &g_vao_quad);
    glGenBuffers(1, &g_vbo_quad);
    glBindVertexArray(g_vao_quad);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo_quad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_batch), NULL, GL_DYNAMIC_DRAW);

    // Position (location 0): 2 floats
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, VERT_STRIDE * sizeof(float), (void *)0);
    // UV (location 1): 2 floats
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, VERT_STRIDE * sizeof(float), (void *)(2 * sizeof(float)));
    // Color (location 2): 4 floats
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, VERT_STRIDE * sizeof(float), (void *)(4 * sizeof(float)));

    glBindVertexArray(0);

    // Create 1x1 white texture
    unsigned char white[4] = {255, 255, 255, 255};
    glGenTextures(1, &g_white_tex);
    glBindTexture(GL_TEXTURE_2D, g_white_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Set texture sampler uniform to unit 0
    glUseProgram(g_shader_quad);
    glUniform1i(glGetUniformLocation(g_shader_quad, "uTexture"), 0);

    // Zero key state
    memset(g_key_current, 0, sizeof(g_key_current));
    memset(g_key_previous, 0, sizeof(g_key_previous));
    memset(g_mouse_current, 0, sizeof(g_mouse_current));
    memset(g_mouse_previous, 0, sizeof(g_mouse_previous));

    g_last_time = glfwGetTime();
    g_target_fps = 60;

    // Load default font
    load_default_font();

    return 0;
}

int gl_window_should_close(void) {
    return g_window ? glfwWindowShouldClose(g_window) : 1;
}

void gl_close_window(void) {
    // Clean up textures
    for (int i = 0; i < g_tex_count; i++) {
        if (g_textures[i].active) glDeleteTextures(1, &g_textures[i].id);
    }
    // Clean up fonts
    for (int i = 0; i < g_font_count; i++) {
        if (g_fonts[i].active) glDeleteTextures(1, &g_fonts[i].atlas_tex);
    }
    if (g_default_font.active) glDeleteTextures(1, &g_default_font.atlas_tex);
    // Clean up render textures
    for (int i = 0; i < g_rt_count; i++) {
        if (g_render_textures[i].active) {
            glDeleteFramebuffers(1, &g_render_textures[i].fbo);
            glDeleteTextures(1, &g_render_textures[i].tex);
            glDeleteRenderbuffers(1, &g_render_textures[i].rbo);
        }
    }
    // Clean up images
    for (int i = 0; i < g_image_count; i++) {
        if (g_images[i].active && g_images[i].data) stbi_image_free(g_images[i].data);
    }

    if (g_white_tex) glDeleteTextures(1, &g_white_tex);
    if (g_vao_quad) glDeleteVertexArrays(1, &g_vao_quad);
    if (g_vbo_quad) glDeleteBuffers(1, &g_vbo_quad);
    if (g_shader_quad) glDeleteProgram(g_shader_quad);

    if (g_window) {
        glfwDestroyWindow(g_window);
        g_window = NULL;
    }
    glfwTerminate();

    // Reset state
    g_tex_count = 0;
    g_font_count = 0;
    g_rt_count = 0;
    g_image_count = 0;
    g_default_font_loaded = 0;
    g_default_font.active = 0;
}

void gl_set_target_fps(int fps) {
    g_target_fps = fps;
    if (g_window) {
        glfwSwapInterval(fps > 0 ? 1 : 0);
    }
}

float gl_get_frame_time(void) {
    return (float)g_frame_time;
}

void gl_set_window_opacity(float opacity) {
    if (g_window) glfwSetWindowOpacity(g_window, opacity);
}

// FRAME

void gl_begin_drawing(void) {
    // Update input state
    memcpy(g_key_previous, g_key_current, sizeof(g_key_current));
    memcpy(g_mouse_previous, g_mouse_current, sizeof(g_mouse_current));
    g_mouse_wheel = 0;

    glfwPollEvents();

    // Update key states
    for (int k = 32; k < MAX_KEYS; k++) {
        g_key_current[k] = glfwGetKey(g_window, k) == GLFW_PRESS;
    }
    // Mouse buttons
    for (int b = 0; b < 3; b++) {
        g_mouse_current[b] = glfwGetMouseButton(g_window, b) == GLFW_PRESS;
    }
    glfwGetCursorPos(g_window, &g_mouse_x, &g_mouse_y);

    // Timing
    double now = glfwGetTime();
    g_frame_time = now - g_last_time;
    g_last_time = now;

    // Reset batch
    g_batch_count = 0;
    g_batch_tex = 0;

    // Set projection for current window size
    glfwGetFramebufferSize(g_window, &g_win_w, &g_win_h);
    glViewport(0, 0, g_win_w, g_win_h);
    set_ortho(0, (float)g_win_w, (float)g_win_h, 0);
}

void gl_end_drawing(void) {
    flush_batch();
    glfwSwapBuffers(g_window);

    // Frame rate limiting
    if (g_target_fps > 0) {
        double target_time = 1.0 / (double)g_target_fps;
        double elapsed = glfwGetTime() - g_last_time;
        while (elapsed < target_time) {
            // Busy wait for precision (could use nanosleep for part of it)
            double remaining = target_time - elapsed;
            if (remaining > 0.002) {
                struct timespec ts;
                ts.tv_sec = 0;
                ts.tv_nsec = (long)((remaining - 0.001) * 1e9);
                nanosleep(&ts, NULL);
            }
            elapsed = glfwGetTime() - g_last_time;
        }
    }
}

void gl_clear(GColor color) {
    flush_batch(); // Ensure pending draws are flushed before clear
    glClearColor(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

// 2D SHAPES

void gl_draw_rect(GRect rect, GColor color) {
    push_quad_color(rect.x, rect.y, rect.w, rect.h, color);
}

void gl_draw_rect_lines(GRect rect, float thick, GColor color) {
    // Top
    push_quad_color(rect.x, rect.y, rect.w, thick, color);
    // Bottom
    push_quad_color(rect.x, rect.y + rect.h - thick, rect.w, thick, color);
    // Left
    push_quad_color(rect.x, rect.y + thick, thick, rect.h - 2 * thick, color);
    // Right
    push_quad_color(rect.x + rect.w - thick, rect.y + thick, thick, rect.h - 2 * thick, color);
}

void gl_draw_rect_rounded(GRect rect, float radius, int segments, GColor color) {
    if (segments < 4) segments = 4;

    float r = radius;
    // Clamp radius to half of smaller dimension
    if (r > rect.w * 0.5f) r = rect.w * 0.5f;
    if (r > rect.h * 0.5f) r = rect.h * 0.5f;

    // Center cross (horizontal bar)
    push_quad_color(rect.x + r, rect.y, rect.w - 2 * r, rect.h, color);
    // Left bar
    push_quad_color(rect.x, rect.y + r, r, rect.h - 2 * r, color);
    // Right bar
    push_quad_color(rect.x + rect.w - r, rect.y + r, r, rect.h - 2 * r, color);

    // Corner fan approximation (draw as triangle fans)
    float cx[4] = {rect.x + r, rect.x + rect.w - r, rect.x + rect.w - r, rect.x + r};
    float cy[4] = {rect.y + r, rect.y + r, rect.y + rect.h - r, rect.y + rect.h - r};
    float start_angles[4] = {3.14159265f, 3.14159265f * 1.5f, 0.0f, 3.14159265f * 0.5f};

    float cr = color.r / 255.0f, cg = color.g / 255.0f, cb = color.b / 255.0f, ca = color.a / 255.0f;

    for (int c = 0; c < 4; c++) {
        float angle_step = (3.14159265f * 0.5f) / segments;
        for (int i = 0; i < segments; i++) {
            float a0 = start_angles[c] + i * angle_step;
            float a1 = a0 + angle_step;
            float x0 = cx[c] + cosf(a0) * r;
            float y0 = cy[c] + sinf(a0) * r;
            float x1 = cx[c] + cosf(a1) * r;
            float y1 = cy[c] + sinf(a1) * r;
            ensure_batch(0, 3);
            push_vert(cx[c], cy[c], 0, 0, cr, cg, cb, ca);
            push_vert(x0, y0, 0, 0, cr, cg, cb, ca);
            push_vert(x1, y1, 0, 0, cr, cg, cb, ca);
        }
    }
}

void gl_draw_rect_rounded_lines(GRect rect, float radius, int segments, GColor color) {
    if (segments < 4) segments = 4;
    float line_w = 1.5f; // Default line width for outline

    float r = radius;
    if (r > rect.w * 0.5f) r = rect.w * 0.5f;
    if (r > rect.h * 0.5f) r = rect.h * 0.5f;

    // Straight edges
    // Top
    push_quad_color(rect.x + r, rect.y, rect.w - 2 * r, line_w, color);
    // Bottom
    push_quad_color(rect.x + r, rect.y + rect.h - line_w, rect.w - 2 * r, line_w, color);
    // Left
    push_quad_color(rect.x, rect.y + r, line_w, rect.h - 2 * r, color);
    // Right
    push_quad_color(rect.x + rect.w - line_w, rect.y + r, line_w, rect.h - 2 * r, color);

    // Corner arcs
    float cx[4] = {rect.x + r, rect.x + rect.w - r, rect.x + rect.w - r, rect.x + r};
    float cy[4] = {rect.y + r, rect.y + r, rect.y + rect.h - r, rect.y + rect.h - r};
    float start_angles[4] = {3.14159265f, 3.14159265f * 1.5f, 0.0f, 3.14159265f * 0.5f};

    for (int c = 0; c < 4; c++) {
        float angle_step = (3.14159265f * 0.5f) / segments;
        for (int i = 0; i < segments; i++) {
            float a0 = start_angles[c] + i * angle_step;
            float a1 = a0 + angle_step;
            float x0 = cx[c] + cosf(a0) * r;
            float y0 = cy[c] + sinf(a0) * r;
            float x1 = cx[c] + cosf(a1) * r;
            float y1 = cy[c] + sinf(a1) * r;
            // Draw line segment as thin quad
            float dx = x1 - x0, dy = y1 - y0;
            float len = sqrtf(dx * dx + dy * dy);
            if (len < 0.001f) continue;
            float nx = -dy / len * line_w * 0.5f;
            float ny =  dx / len * line_w * 0.5f;
            ensure_batch(0, 6);
            float cr = color.r/255.0f, cg = color.g/255.0f, cb = color.b/255.0f, ca = color.a/255.0f;
            push_vert(x0 + nx, y0 + ny, 0, 0, cr, cg, cb, ca);
            push_vert(x0 - nx, y0 - ny, 0, 0, cr, cg, cb, ca);
            push_vert(x1 - nx, y1 - ny, 0, 0, cr, cg, cb, ca);
            push_vert(x0 + nx, y0 + ny, 0, 0, cr, cg, cb, ca);
            push_vert(x1 - nx, y1 - ny, 0, 0, cr, cg, cb, ca);
            push_vert(x1 + nx, y1 + ny, 0, 0, cr, cg, cb, ca);
        }
    }
}

void gl_draw_rect_gradient_v(int x, int y, int w, int h, GColor top, GColor bottom) {
    push_quad_gradient((float)x, (float)y, (float)w, (float)h,
                       top, top, bottom, bottom);
}

void gl_draw_rect_gradient_ex(GRect rect, GColor tl, GColor bl, GColor br, GColor tr) {
    push_quad_gradient(rect.x, rect.y, rect.w, rect.h, tl, tr, bl, br);
}

void gl_draw_circle(int cx, int cy, float radius, GColor color) {
    // Approximate circle with a triangle fan
    int segments = (int)(radius * 2);
    if (segments < 12) segments = 12;
    if (segments > 64) segments = 64;

    float cr = color.r / 255.0f, cg = color.g / 255.0f, cb = color.b / 255.0f, ca = color.a / 255.0f;
    float step = 2.0f * 3.14159265f / segments;

    for (int i = 0; i < segments; i++) {
        float a0 = i * step;
        float a1 = (i + 1) * step;
        ensure_batch(0, 3);
        push_vert((float)cx, (float)cy, 0, 0, cr, cg, cb, ca);
        push_vert((float)cx + cosf(a0) * radius, (float)cy + sinf(a0) * radius, 0, 0, cr, cg, cb, ca);
        push_vert((float)cx + cosf(a1) * radius, (float)cy + sinf(a1) * radius, 0, 0, cr, cg, cb, ca);
    }
}

void gl_draw_line(GVec2 start, GVec2 end, float thick, GColor color) {
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;

    float nx = -dy / len * thick * 0.5f;
    float ny =  dx / len * thick * 0.5f;

    ensure_batch(0, 6);
    float r = color.r/255.0f, g = color.g/255.0f, b = color.b/255.0f, a = color.a/255.0f;
    push_vert(start.x + nx, start.y + ny, 0, 0, r, g, b, a);
    push_vert(start.x - nx, start.y - ny, 0, 0, r, g, b, a);
    push_vert(end.x   - nx, end.y   - ny, 0, 0, r, g, b, a);
    push_vert(start.x + nx, start.y + ny, 0, 0, r, g, b, a);
    push_vert(end.x   - nx, end.y   - ny, 0, 0, r, g, b, a);
    push_vert(end.x   + nx, end.y   + ny, 0, 0, r, g, b, a);
}

void gl_draw_rect_at(int x, int y, int w, int h, GColor color) {
    push_quad_color((float)x, (float)y, (float)w, (float)h, color);
}

// TEXTURES

GTexture gl_load_texture(const char *path) {
    if (g_tex_count >= MAX_TEX) return -1;

    int w, h, ch;
    stbi_set_flip_vertically_on_load(0);
    unsigned char *data = stbi_load(path, &w, &h, &ch, 4);
    if (!data) {
        fprintf(stderr, "[GL Backend] Failed to load texture: %s\n", path);
        return -1;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(data);

    int idx = g_tex_count++;
    g_textures[idx].id = tex;
    g_textures[idx].w = w;
    g_textures[idx].h = h;
    g_textures[idx].active = 1;
    return idx;
}

void gl_draw_texture(GTexture tex, int x, int y, GColor tint) {
    if (tex < 0 || tex >= g_tex_count || !g_textures[tex].active) return;
    push_quad_tex(g_textures[tex].id, (float)x, (float)y,
                  (float)g_textures[tex].w, (float)g_textures[tex].h,
                  0, 0, 1, 1, tint);
}

void gl_draw_texture_pro(GTexture tex, GRect src, GRect dest,
                          GVec2 origin, float rot, GColor tint) {
    if (tex < 0 || tex >= g_tex_count || !g_textures[tex].active) return;
    push_quad_tex_rotated(g_textures[tex].id, src, dest, origin, rot,
                          (float)g_textures[tex].w, (float)g_textures[tex].h, tint);
}

int gl_get_texture_width(GTexture tex) {
    if (tex < 0 || tex >= g_tex_count) return 0;
    return g_textures[tex].w;
}

int gl_get_texture_height(GTexture tex) {
    if (tex < 0 || tex >= g_tex_count) return 0;
    return g_textures[tex].h;
}

void gl_unload_texture(GTexture tex) {
    if (tex < 0 || tex >= g_tex_count || !g_textures[tex].active) return;
    glDeleteTextures(1, &g_textures[tex].id);
    g_textures[tex].active = 0;
}

GTexture gl_load_texture_from_memory(const char *ext, const unsigned char *data, int size) {
    if (g_tex_count >= MAX_TEX) return -1;
    (void)ext;

    int w, h, ch;
    stbi_set_flip_vertically_on_load(0);
    unsigned char *pixels = stbi_load_from_memory(data, size, &w, &h, &ch, 4);
    if (!pixels) return -1;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(pixels);

    int idx = g_tex_count++;
    g_textures[idx].id = tex;
    g_textures[idx].w = w;
    g_textures[idx].h = h;
    g_textures[idx].active = 1;
    return idx;
}

// FONTS

GFont gl_load_font(const char *path, int size) {
    if (g_font_count >= MAX_FONTS_CACHE) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[GL Backend] Failed to load font: %s\n", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *ttf = (unsigned char *)malloc(sz);
    if (fread(ttf, 1, sz, f) != (size_t)sz) {
        free(ttf);
        fclose(f);
        return -1;
    }
    fclose(f);

    unsigned char *atlas = (unsigned char *)malloc(FONT_ATLAS_W * FONT_ATLAS_H);
    int idx = g_font_count;

    stbtt_BakeFontBitmap(ttf, 0, (float)size, atlas,
                         FONT_ATLAS_W, FONT_ATLAS_H,
                         FONT_FIRST_CHAR, FONT_NUM_CHARS,
                         g_fonts[idx].cdata);
    free(ttf);

    g_fonts[idx].atlas_tex = upload_font_atlas_rgba(atlas, FONT_ATLAS_W, FONT_ATLAS_H);
    free(atlas);

    g_fonts[idx].pixel_size = (float)size;
    g_fonts[idx].active = 1;
    g_font_count++;
    return idx;
}

void gl_draw_text_ex(GFont font, const char *text, GVec2 pos,
                      float size, float spacing, GColor color) {
    (void)spacing; // We handle spacing in the font metrics
    if (font < 0 || font >= g_font_count || !g_fonts[font].active) return;
    draw_text_internal(&g_fonts[font], text, pos.x, pos.y, size, color);
}

void gl_draw_text(const char *text, int x, int y, int size, GColor color) {
    if (!g_default_font_loaded || !g_default_font.active) return;
    draw_text_internal(&g_default_font, text, (float)x, (float)y, (float)size, color);
}

int gl_measure_text(const char *text, int size) {
    if (!g_default_font_loaded || !g_default_font.active) return (int)(strlen(text) * size * 0.6f);
    return (int)measure_text_internal(&g_default_font, text, (float)size);
}

GVec2 gl_measure_text_ex(GFont font, const char *text, float size, float spacing) {
    (void)spacing;
    GVec2 result = {0, 0};
    if (font < 0 || font >= g_font_count || !g_fonts[font].active) {
        result.x = strlen(text) * size * 0.6f;
        result.y = size;
        return result;
    }
    result.x = measure_text_internal(&g_fonts[font], text, size);
    result.y = measure_text_height_internal(&g_fonts[font], text, size);
    return result;
}

// INPUT

GVec2 gl_get_mouse_position(void) {
    return (GVec2){(float)g_mouse_x, (float)g_mouse_y};
}

float gl_get_mouse_wheel(void) {
    return g_mouse_wheel;
}

int gl_is_key_down(int key) {
    if (key < 0 || key >= MAX_KEYS) return 0;
    return g_key_current[key];
}

int gl_is_key_pressed(int key) {
    if (key < 0 || key >= MAX_KEYS) return 0;
    return g_key_current[key] && !g_key_previous[key];
}

int gl_is_mouse_button_pressed(int button) {
    if (button < 0 || button >= 8) return 0;
    return g_mouse_current[button] && !g_mouse_previous[button];
}

int gl_is_mouse_button_down(int button) {
    if (button < 0 || button >= 8) return 0;
    return g_mouse_current[button];
}

int gl_check_collision_point_rect(GVec2 point, GRect rect) {
    return point.x >= rect.x && point.x <= rect.x + rect.w &&
           point.y >= rect.y && point.y <= rect.y + rect.h;
}

// CAMERA 2D

void gl_begin_mode_2d(GCamera2D cam) {
    flush_batch();
    g_camera_active = 1;
    g_camera = cam;
    set_ortho(0, (float)g_win_w, (float)g_win_h, 0);
}

void gl_end_mode_2d(void) {
    flush_batch();
    g_camera_active = 0;
    set_ortho(0, (float)g_win_w, (float)g_win_h, 0);
}

// RENDER TEXTURES (FBO)

GRenderTex gl_load_render_texture(int w, int h) {
    if (g_rt_count >= MAX_RT) return -1;

    int idx = g_rt_count;
    RTSlot *rt = &g_render_textures[idx];

    glGenFramebuffers(1, &rt->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

    glGenTextures(1, &rt->tex);
    glBindTexture(GL_TEXTURE_2D, rt->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->tex, 0);

    // Optional depth/stencil renderbuffer (not strictly needed for 2D but good practice)
    glGenRenderbuffers(1, &rt->rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rt->rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt->rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "[GL Backend] Framebuffer not complete!\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, g_active_fbo);

    rt->w = w;
    rt->h = h;
    rt->active = 1;
    g_rt_count++;
    return idx;
}

void gl_begin_texture_mode(GRenderTex rt) {
    if (rt < 0 || rt >= g_rt_count || !g_render_textures[rt].active) return;
    flush_batch();
    g_active_fbo = g_render_textures[rt].fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, g_active_fbo);
    glViewport(0, 0, g_render_textures[rt].w, g_render_textures[rt].h);
    // Set projection for render texture dimensions (flipped Y for FBO)
    set_ortho(0, (float)g_render_textures[rt].w, (float)g_render_textures[rt].h, 0);
}

void gl_end_texture_mode(void) {
    flush_batch();
    g_active_fbo = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_win_w, g_win_h);
    set_ortho(0, (float)g_win_w, (float)g_win_h, 0);
}

void gl_draw_render_texture(GRenderTex rt, int x, int y) {
    if (rt < 0 || rt >= g_rt_count || !g_render_textures[rt].active) return;
    // FBO textures are Y-flipped, so we draw with flipped V coordinates
    push_quad_tex(g_render_textures[rt].tex,
                  (float)x, (float)y,
                  (float)g_render_textures[rt].w, (float)g_render_textures[rt].h,
                  0, 1, 1, 0, GCOLOR_WHITE);
}

void gl_unload_render_texture(GRenderTex rt) {
    if (rt < 0 || rt >= g_rt_count || !g_render_textures[rt].active) return;
    glDeleteFramebuffers(1, &g_render_textures[rt].fbo);
    glDeleteTextures(1, &g_render_textures[rt].tex);
    glDeleteRenderbuffers(1, &g_render_textures[rt].rbo);
    g_render_textures[rt].active = 0;
}

// IMAGE MANIPULATION (CPU-side)

GImage gl_load_image(const char *path) {
    if (g_image_count >= MAX_IMAGES_CACHE) return -1;
    int w, h, ch;
    unsigned char *data = stbi_load(path, &w, &h, &ch, 4);
    if (!data) return -1;

    int idx = g_image_count++;
    g_images[idx].data = data;
    g_images[idx].w = w;
    g_images[idx].h = h;
    g_images[idx].channels = 4;
    g_images[idx].active = 1;
    return idx;
}

void gl_image_rotate_cw(GImage img) {
    if (img < 0 || img >= g_image_count || !g_images[img].active) return;
    ImageSlot *s = &g_images[img];
    int w = s->w, h = s->h;
    unsigned char *rotated = (unsigned char *)malloc(w * h * 4);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int src_idx = (y * w + x) * 4;
            int dst_idx = (x * h + (h - 1 - y)) * 4;
            memcpy(rotated + dst_idx, s->data + src_idx, 4);
        }
    }

    stbi_image_free(s->data);
    s->data = rotated;
    s->w = h;
    s->h = w;
}

GTexture gl_load_texture_from_image(GImage img) {
    if (img < 0 || img >= g_image_count || !g_images[img].active) return -1;
    if (g_tex_count >= MAX_TEX) return -1;

    ImageSlot *s = &g_images[img];
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s->w, s->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, s->data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int idx = g_tex_count++;
    g_textures[idx].id = tex;
    g_textures[idx].w = s->w;
    g_textures[idx].h = s->h;
    g_textures[idx].active = 1;
    return idx;
}

void gl_unload_image(GImage img) {
    if (img < 0 || img >= g_image_count || !g_images[img].active) return;
    if (g_images[img].data) stbi_image_free(g_images[img].data);
    g_images[img].data = NULL;
    g_images[img].active = 0;
}

// UTILITIES

GColor gl_color_from_hsv(float h, float s, float v) {
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r, g, b;

    if      (h < 60)  { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }

    return (GColor){
        (unsigned char)((r + m) * 255.0f),
        (unsigned char)((g + m) * 255.0f),
        (unsigned char)((b + m) * 255.0f),
        255
    };
}

void gl_take_screenshot(const char *path) {
    flush_batch();

    int w = g_win_w, h = g_win_h;
    unsigned char *pixels = (unsigned char *)malloc(w * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // Flip vertically (OpenGL reads bottom-up)
    unsigned char *flipped = (unsigned char *)malloc(w * h * 4);
    for (int y = 0; y < h; y++) {
        memcpy(flipped + y * w * 4, pixels + (h - 1 - y) * w * 4, w * 4);
    }
    free(pixels);

    stbi_write_png(path, w, h, 4, flipped, w * 4);
    free(flipped);
}

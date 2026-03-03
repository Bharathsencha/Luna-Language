// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// gl_backend.h — OpenGL 3.3 + GLFW rendering backend for Luna GUI
// OpenGL 3.3 + GLFW rendering backend. Zero external runtime dependencies.

#ifndef GL_BACKEND_H
#define GL_BACKEND_H

#include <stdbool.h>
#include <stddef.h>

// TYPES

typedef struct { unsigned char r, g, b, a; } GColor;
typedef struct { float x, y, w, h; }         GRect;
typedef struct { float x, y; }               GVec2;

typedef struct {
    GVec2 offset;
    GVec2 target;
    float rotation;
    float zoom;
} GCamera2D;

// Opaque handles (index into internal caches)
typedef int GTexture;
typedef int GFont;
typedef int GRenderTex;
typedef int GImage;

// WINDOW / LIFECYCLE

int   gl_init_window(int w, int h, const char *title);
int   gl_window_should_close(void);
void  gl_close_window(void);
void  gl_set_target_fps(int fps);
float gl_get_frame_time(void);
void  gl_set_window_opacity(float opacity);

// FRAME

void  gl_begin_drawing(void);
void  gl_end_drawing(void);
void  gl_clear(GColor color);

// 2D SHAPES

void  gl_draw_rect(GRect rect, GColor color);
void  gl_draw_rect_lines(GRect rect, float thick, GColor color);
void  gl_draw_rect_rounded(GRect rect, float radius, int segments, GColor color);
void  gl_draw_rect_rounded_lines(GRect rect, float radius, int segments, GColor color);
void  gl_draw_rect_gradient_v(int x, int y, int w, int h, GColor top, GColor bottom);
void  gl_draw_rect_gradient_ex(GRect rect, GColor tl, GColor bl, GColor br, GColor tr);
void  gl_draw_circle(int cx, int cy, float radius, GColor color);
void  gl_draw_line(GVec2 start, GVec2 end, float thick, GColor color);
void  gl_draw_rect_at(int x, int y, int w, int h, GColor color);

// TEXTURES

GTexture gl_load_texture(const char *path);
void     gl_draw_texture(GTexture tex, int x, int y, GColor tint);
void     gl_draw_texture_pro(GTexture tex, GRect src, GRect dest,
                             GVec2 origin, float rot, GColor tint);
int      gl_get_texture_width(GTexture tex);
int      gl_get_texture_height(GTexture tex);
void     gl_unload_texture(GTexture tex);
GTexture gl_load_texture_from_memory(const char *ext, const unsigned char *data, int size);

// FONTS

GFont  gl_load_font(const char *path, int size);
void   gl_draw_text_ex(GFont font, const char *text, GVec2 pos,
                       float size, float spacing, GColor color);
void   gl_draw_text(const char *text, int x, int y, int size, GColor color);
int    gl_measure_text(const char *text, int size);
GVec2  gl_measure_text_ex(GFont font, const char *text, float size, float spacing);

// INPUT

GVec2 gl_get_mouse_position(void);
float gl_get_mouse_wheel(void);
int   gl_is_key_down(int key);
int   gl_is_key_pressed(int key);
int   gl_is_mouse_button_pressed(int button);
int   gl_is_mouse_button_down(int button);
int   gl_check_collision_point_rect(GVec2 point, GRect rect);

// Key codes (matching GLFW values)
#define GKEY_SPACE           32
#define GKEY_ESCAPE          256
#define GKEY_ENTER           257
#define GKEY_TAB             258
#define GKEY_BACKSPACE       259
#define GKEY_RIGHT           262
#define GKEY_LEFT            263
#define GKEY_DOWN            264
#define GKEY_UP              265

// A-Z keys (GLFW uses ASCII for letters)
#define GKEY_A               65
#define GKEY_B               66
#define GKEY_C               67
#define GKEY_D               68
#define GKEY_E               69
#define GKEY_F               70
#define GKEY_G               71
#define GKEY_H               72
#define GKEY_I               73
#define GKEY_J               74
#define GKEY_K               75
#define GKEY_L               76
#define GKEY_M               77
#define GKEY_N               78
#define GKEY_O               79
#define GKEY_P               80
#define GKEY_Q               81
#define GKEY_R               82
#define GKEY_S               83
#define GKEY_T               84
#define GKEY_U               85
#define GKEY_V               86
#define GKEY_W               87
#define GKEY_X               88
#define GKEY_Y               89
#define GKEY_Z               90

// Number keys
#define GKEY_0               48
#define GKEY_1               49
#define GKEY_2               50
#define GKEY_3               51
#define GKEY_4               52
#define GKEY_5               53
#define GKEY_6               54
#define GKEY_7               55
#define GKEY_8               56
#define GKEY_9               57

// Mouse buttons
#define GMOUSE_LEFT          0
#define GMOUSE_RIGHT         1
#define GMOUSE_MIDDLE        2

// CAMERA 2D

void  gl_begin_mode_2d(GCamera2D cam);
void  gl_end_mode_2d(void);

// RENDER TEXTURES (FBO)

GRenderTex gl_load_render_texture(int w, int h);
void       gl_begin_texture_mode(GRenderTex rt);
void       gl_end_texture_mode(void);
void       gl_draw_render_texture(GRenderTex rt, int x, int y);
void       gl_unload_render_texture(GRenderTex rt);

// IMAGE MANIPULATION (CPU-side)

GImage   gl_load_image(const char *path);
void     gl_image_rotate_cw(GImage img);
GTexture gl_load_texture_from_image(GImage img);
void     gl_unload_image(GImage img);

// UTILITIES

GColor gl_color_from_hsv(float h, float s, float v);
void   gl_take_screenshot(const char *path);

// Predefined colors
#define GCOLOR_WHITE    (GColor){255, 255, 255, 255}
#define GCOLOR_BLACK    (GColor){0, 0, 0, 255}
#define GCOLOR_RED      (GColor){230, 41, 55, 255}
#define GCOLOR_GREEN    (GColor){0, 228, 48, 255}
#define GCOLOR_BLUE     (GColor){0, 121, 241, 255}
#define GCOLOR_GOLD     (GColor){255, 203, 0, 255}
#define GCOLOR_DARKGRAY (GColor){80, 80, 80, 255}
#define GCOLOR_LIGHTGRAY (GColor){200, 200, 200, 255}
#define GCOLOR_GRAY     (GColor){130, 130, 130, 255}
#define GCOLOR_SKYBLUE  (GColor){102, 191, 255, 255}
#define GCOLOR_MAROON   (GColor){190, 33, 55, 255}

#endif // GL_BACKEND_H

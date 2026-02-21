// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include "raylib.h"
#include "gui_lib.h"
#include "../include/value.h"
#include "../include/env.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#pragma GCC diagnostic ignored "-Wunused-parameter"

// PCM Sample Ring Buffer for Audio Visualizer
#define FFT_BUFFER_SIZE 2048
static float fft_buffer[FFT_BUFFER_SIZE];
static int fft_write_cursor = 0;

// Audio stream processor callback - captures PCM samples from the playing music
static void audio_processor_callback(void *bufferData, unsigned int frames) {
    float *samples = (float *)bufferData;
    for (unsigned int i = 0; i < frames; i++) {
        // Average left and right channels to mono
        float mono = (samples[i * 2] + samples[i * 2 + 1]) * 0.5f;
        fft_buffer[fft_write_cursor] = mono;
        fft_write_cursor = (fft_write_cursor + 1) % FFT_BUFFER_SIZE;
    }
}

// Static State & Resource Managers
static float layout_cursor_y = 20.0f;
static const float padding = 10.0f;
static const float widget_height = 30.0f;
static const float margin_x = 20.0f;

#define MAX_FONTS 16
static Font font_cache[MAX_FONTS];
static int font_count = 0;

#define MAX_TEXTURES 128
static Texture2D texture_cache[MAX_TEXTURES];
static int texture_count = 0;

#define MAX_MUSIC 16
static Music music_cache[MAX_MUSIC];
static int music_count = 0;

#define MAX_SOUNDS 16
static Sound sound_cache[MAX_SOUNDS];
static int sound_count = 0;

#define MAX_IMAGES 16
static Image image_cache[MAX_IMAGES];
static int image_count = 0;

// Internal Helpers

static double val_to_double(Value v) {
    if (v.type == VAL_INT) return (double)v.i;
    if (v.type == VAL_FLOAT) return v.f;
    return 0.0;
}

// Extract Color from Luna List [r, g, b, a]
static Color val_to_color(Value v) {
    if (v.type != VAL_LIST || v.list->count < 3) return WHITE;
    Color c;
    c.r = (unsigned char)v.list->items[0].i;
    c.g = (unsigned char)v.list->items[1].i;
    c.b = (unsigned char)v.list->items[2].i;
    c.a = (v.list->count > 3) ? (unsigned char)v.list->items[3].i : 255;
    return c;
}

void register_color(Env *env, const char *name, Color c) {
    Value list = value_list();
    value_list_append(&list, value_int(c.r));
    value_list_append(&list, value_int(c.g));
    value_list_append(&list, value_int(c.b));
    value_list_append(&list, value_int(c.a));
    env_def(env, name, list);
}

// Lifecycle & System

Value lib_gui_create_particle_pool(int argc, Value *argv, struct Env *env);

Value lib_gui_init(int argc, Value *argv, Env *env) {
    if (argc < 3) return value_null();
    InitWindow((int)argv[0].i, (int)argv[1].i, argv[2].string->chars);
    SetExitKey(0); // Disable default ESC exit behavior
    
    register_color(env, "RED", RED);
    register_color(env, "GREEN", GREEN);
    register_color(env, "BLUE", BLUE);
    register_color(env, "GOLD", GOLD);
    register_color(env, "BLACK", BLACK);
    register_color(env, "WHITE", WHITE);
    register_color(env, "DARKGRAY", DARKGRAY);
    register_color(env, "DARKGRAY", DARKGRAY);
    register_color(env, "SKYBLUE", SKYBLUE);
    
    // Basic Colors
    register_color(env, "YELLOW", (Color){255, 255, 0, 255});
    register_color(env, "ORANGE", (Color){255, 165, 0, 255});
    register_color(env, "PURPLE", (Color){128, 0, 128, 255});
    register_color(env, "CYAN", (Color){0, 255, 255, 255});
    register_color(env, "MAGENTA", (Color){255, 0, 255, 255});
    register_color(env, "GRAY", (Color){128, 128, 128, 255});
    register_color(env, "DARK_GRAY", (Color){64, 64, 64, 255});
    register_color(env, "LIGHT_GRAY", (Color){192, 192, 192, 255});
    
    // Extended Colors
    register_color(env, "PINK", (Color){255, 192, 203, 255});
    register_color(env, "BROWN", (Color){165, 42, 42, 255});
    register_color(env, "SILVER", (Color){192, 192, 192, 255});
    register_color(env, "NAVY", (Color){0, 0, 128, 255});
    register_color(env, "TEAL", (Color){0, 128, 128, 255});
    register_color(env, "LIME", (Color){0, 255, 0, 255});
    register_color(env, "MAROON", (Color){128, 0, 0, 255});
    register_color(env, "OLIVE", (Color){128, 128, 0, 255});
    register_color(env, "INDIGO", (Color){75, 0, 130, 255});
    register_color(env, "VIOLET", (Color){238, 130, 238, 255});
    
    // Shades
    register_color(env, "DARK_RED", (Color){139, 0, 0, 255});
    register_color(env, "DARK_GREEN", (Color){0, 100, 0, 255});
    register_color(env, "DARK_BLUE", (Color){0, 0, 139, 255});
    register_color(env, "LIGHT_RED", (Color){255, 102, 102, 255});
    register_color(env, "LIGHT_GREEN", (Color){144, 238, 144, 255});
    register_color(env, "LIGHT_BLUE", (Color){173, 216, 230, 255});
    
    register_color(env, "TRANSPARENT", (Color){0, 0, 0, 0});
    
    // Register Key Constants
    env_def(env, "KEY_SPACE", value_int(KEY_SPACE));
    env_def(env, "KEY_ESCAPE", value_int(KEY_ESCAPE));
    env_def(env, "KEY_ENTER", value_int(KEY_ENTER));
    env_def(env, "KEY_TAB", value_int(KEY_TAB));
    env_def(env, "KEY_BACKSPACE", value_int(KEY_BACKSPACE));
    
    env_def(env, "KEY_RIGHT", value_int(KEY_RIGHT));
    env_def(env, "KEY_LEFT", value_int(KEY_LEFT));
    env_def(env, "KEY_DOWN", value_int(KEY_DOWN));
    env_def(env, "KEY_UP", value_int(KEY_UP));
    
    // WASD + Common
    env_def(env, "KEY_W", value_int(KEY_W));
    env_def(env, "KEY_A", value_int(KEY_A));
    env_def(env, "KEY_S", value_int(KEY_S));
    env_def(env, "KEY_D", value_int(KEY_D));
    env_def(env, "KEY_P", value_int(KEY_P));
    env_def(env, "KEY_R", value_int(KEY_R));
    env_def(env, "KEY_M", value_int(KEY_M));
    
    // IJKL Navigation
    env_def(env, "KEY_I", value_int(KEY_I));
    env_def(env, "KEY_J", value_int(KEY_J));
    env_def(env, "KEY_K", value_int(KEY_K));
    env_def(env, "KEY_L", value_int(KEY_L));
    
    // Other Common Action Keys
    env_def(env, "KEY_Q", value_int(KEY_Q));
    env_def(env, "KEY_E", value_int(KEY_E));
    env_def(env, "KEY_Z", value_int(KEY_Z));
    env_def(env, "KEY_C", value_int(KEY_C));
    env_def(env, "KEY_X", value_int(KEY_X));
    env_def(env, "KEY_F", value_int(KEY_F));
    env_def(env, "KEY_H", value_int(KEY_H));
    
    // Remaining A-Z
    env_def(env, "KEY_B", value_int(KEY_B));
    env_def(env, "KEY_G", value_int(KEY_G));
    env_def(env, "KEY_N", value_int(KEY_N));
    env_def(env, "KEY_O", value_int(KEY_O));
    env_def(env, "KEY_T", value_int(KEY_T));
    env_def(env, "KEY_U", value_int(KEY_U));
    env_def(env, "KEY_V", value_int(KEY_V));
    env_def(env, "KEY_Y", value_int(KEY_Y));
    
    // Numbers 0-9
    env_def(env, "KEY_0", value_int(KEY_ZERO));
    env_def(env, "KEY_1", value_int(KEY_ONE));
    env_def(env, "KEY_2", value_int(KEY_TWO));
    env_def(env, "KEY_3", value_int(KEY_THREE));
    env_def(env, "KEY_4", value_int(KEY_FOUR));
    env_def(env, "KEY_5", value_int(KEY_FIVE));
    env_def(env, "KEY_6", value_int(KEY_SIX));
    env_def(env, "KEY_7", value_int(KEY_SEVEN));
    env_def(env, "KEY_8", value_int(KEY_EIGHT));
    env_def(env, "KEY_9", value_int(KEY_NINE));
    
    // Mouse Buttons
    env_def(env, "MOUSE_LEFT_BUTTON", value_int(MOUSE_LEFT_BUTTON));
    env_def(env, "MOUSE_RIGHT_BUTTON", value_int(MOUSE_RIGHT_BUTTON));
    env_def(env, "MOUSE_MIDDLE_BUTTON", value_int(MOUSE_MIDDLE_BUTTON));
    
    // Register Particle Pool Helper
    env_def(env, "create_particle_pool", value_native(lib_gui_create_particle_pool));

    SetTargetFPS(60);
    return value_null();
}

Value lib_gui_window_open(int argc, Value *argv, struct Env *env) {
    return value_bool(!WindowShouldClose());
}

Value lib_gui_close_window(int argc, Value *argv, struct Env *env) {
    CloseWindow();
    return value_null();
}

Value lib_gui_set_fps(int argc, Value *argv, Env *env) {
    if (argc > 0) SetTargetFPS((int)val_to_double(argv[0]));
    return value_null();
}

Value lib_gui_get_delta_time(int argc, Value *argv, Env *env) {
    return value_float(GetFrameTime());
}

Value lib_gui_set_opacity(int argc, Value *argv, Env *env) {
    if (argc > 0) SetWindowOpacity((float)val_to_double(argv[0]));
    return value_null();
}

// Frame Management

Value lib_gui_begin(int argc, Value *argv, Env *env) {
    BeginDrawing();
    layout_cursor_y = 20.0f; 
    return value_null();
}

Value lib_gui_end(int argc, Value *argv, Env *env) {
    EndDrawing();
    return value_null();
}

Value lib_gui_clear(int argc, Value *argv, Env *env) {
    if (argc >= 4) {
        ClearBackground((Color){
            (unsigned char)val_to_double(argv[0]),
            (unsigned char)val_to_double(argv[1]),
            (unsigned char)val_to_double(argv[2]),
            (unsigned char)val_to_double(argv[3])
        });
    } else if (argc == 1) {
        ClearBackground(val_to_color(argv[0]));
    } else {
        ClearBackground((Color){10, 10, 15, 255});
    }
    return value_null();
}

// Unified Shapes

Value lib_gui_draw_rect(int argc, Value *argv, Env *env) {
    if (argc < 4) return value_null();
    Rectangle rec = { (float)val_to_double(argv[0]), (float)val_to_double(argv[1]), 
                      (float)val_to_double(argv[2]), (float)val_to_double(argv[3]) };
    
    // Check for overloaded signature: (x, y, w, h, Color)
    if (argc == 5 && argv[4].type == VAL_LIST) {
        DrawRectangleRec(rec, val_to_color(argv[4]));
        return value_null();
    }

    float thick = (argc >= 5) ? (float)val_to_double(argv[4]) : -1.0f;
    float radius = (argc >= 6) ? (float)val_to_double(argv[5]) : 0.0f;
    Color col = (argc >= 7) ? val_to_color(argv[6]) : MAROON;

    if (radius > 0) {
        if (thick < 0) DrawRectangleRounded(rec, radius, 20, col);
        else DrawRectangleRoundedLines(rec, radius, 20, col); 
    } else {
        if (thick < 0) DrawRectangleRec(rec, col);
        else DrawRectangleLinesEx(rec, thick, col);
    }
    return value_null();
}

Value lib_gui_draw_circle(int argc, Value *argv, Env *env) {
    if (argc < 3) return value_null();
    float x = (float)val_to_double(argv[0]);
    float y = (float)val_to_double(argv[1]);
    float r = (float)val_to_double(argv[2]);
    Color col = (argc >= 4) ? val_to_color(argv[3]) : BLUE;
    DrawCircle((int)x, (int)y, r, col);
    return value_null();
}

Value lib_gui_draw_line(int argc, Value *argv, Env *env) {
    if (argc < 5) return value_null();
    Vector2 start = { (float)val_to_double(argv[0]), (float)val_to_double(argv[1]) };
    Vector2 end = { (float)val_to_double(argv[2]), (float)val_to_double(argv[3]) };
    float thick = (float)val_to_double(argv[4]);
    Color col = (argc >= 6) ? val_to_color(argv[5]) : BLACK;
    DrawLineEx(start, end, thick, col);
    return value_null();
}

Value lib_gui_draw_gradient_v(int argc, Value *argv, struct Env *env) {
    if (argc < 6) return value_null();
    DrawRectangleGradientV((int)val_to_double(argv[0]), (int)val_to_double(argv[1]), 
                           (int)val_to_double(argv[2]), (int)val_to_double(argv[3]), 
                           val_to_color(argv[4]), val_to_color(argv[5]));
    return value_null();
}

Value lib_gui_draw_rectangle_rec(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_null();
    if (argv[0].type != VAL_LIST || argv[0].list->count < 4) return value_null();
    Rectangle rec = { 
        (float)val_to_double(argv[0].list->items[0]), 
        (float)val_to_double(argv[0].list->items[1]), 
        (float)val_to_double(argv[0].list->items[2]), 
        (float)val_to_double(argv[0].list->items[3]) 
    };
    Color col = val_to_color(argv[1]);
    DrawRectangleRec(rec, col);
    return value_null();
}

Value lib_gui_draw_rectangle_lines(int argc, Value *argv, struct Env *env) {
    if (argc < 5) return value_null();
    DrawRectangleLines(
        (int)val_to_double(argv[0]), 
        (int)val_to_double(argv[1]), 
        (int)val_to_double(argv[2]), 
        (int)val_to_double(argv[3]), 
        val_to_color(argv[4])
    );
    return value_null();
}

// Widgets

Value lib_gui_label(int argc, Value *argv, Env *env) {
    if (argc < 1) return value_null();
    DrawText(argv[0].string->chars, (int)margin_x, (int)layout_cursor_y, 20, DARKGRAY);
    layout_cursor_y += widget_height + padding;
    return value_null();
}

Value lib_gui_button(int argc, Value *argv, Env *env) {
    if (argc < 1) return value_bool(0);
    Rectangle bounds = { margin_x, layout_cursor_y, 150, widget_height };
    bool hover = CheckCollisionPointRec(GetMousePosition(), bounds);
    bool clicked = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    DrawRectangleRec(bounds, hover ? LIGHTGRAY : GRAY);
    DrawText(argv[0].string->chars, (int)bounds.x + 10, (int)bounds.y + 5, 20, BLACK);
    layout_cursor_y += widget_height + padding;
    return value_bool(clicked);
}

Value lib_gui_slider(int argc, Value *argv, Env *env) {
    if (argc < 4) return value_null();
    const char* var_name = argv[0].string->chars;
    float min = (float)val_to_double(argv[1]);
    float max = (float)val_to_double(argv[2]);
    Value *val = env_get(env, var_name); 
    if (!val) return value_null();
    float current_f = (float)val_to_double(*val);
    Rectangle bounds = { margin_x, layout_cursor_y, 200, widget_height };
    if (CheckCollisionPointRec(GetMousePosition(), bounds) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float pct = (GetMousePosition().x - bounds.x) / bounds.width;
        if (pct < 0) pct = 0; 
        if (pct > 1) pct = 1;
        current_f = min + (max - min) * pct;
        if (val->type == VAL_INT) val->i = (long long)current_f;
        else val->f = (double)current_f;
    }
    DrawRectangleRec(bounds, LIGHTGRAY);
    float fill_w = ((current_f - min) / (max - min)) * 200;
    DrawRectangle((int)bounds.x, (int)bounds.y, (int)fill_w, (int)bounds.height, BLUE);
    DrawText(argv[3].string->chars, (int)(bounds.x + 210), (int)bounds.y + 5, 20, BLACK);
    layout_cursor_y += widget_height + padding;
    return value_null();
}

// Input

Value lib_gui_get_mouse(int argc, Value *argv, Env *env) {
    Vector2 mouse = GetMousePosition();
    Value list = value_list();
    value_list_append(&list, value_float(mouse.x));
    value_list_append(&list, value_float(mouse.y));
    return list;
}

Value lib_gui_get_mouse_wheel_move(int argc, Value *argv, Env *env) {
    return value_float(GetMouseWheelMove());
}

Value lib_gui_is_key_down(int argc, Value *argv, Env *env) {
    if (argc < 1) return value_bool(false);
    return value_bool(IsKeyDown((int)val_to_double(argv[0])));
}

Value lib_gui_is_key_pressed(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_bool(false);
    return value_bool(IsKeyPressed((int)val_to_double(argv[0])));
}

Value lib_gui_is_mouse_button_pressed(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_bool(false);
    return value_bool(IsMouseButtonPressed((int)val_to_double(argv[0])));
}

Value lib_gui_is_mouse_button_down(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_bool(false);
    return value_bool(IsMouseButtonDown((int)val_to_double(argv[0])));
}

Value lib_gui_check_collision_point_rec(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_bool(false);
    // Arg 0: Point [x, y]
    // Arg 1: Rec [x, y, w, h]
    if (argv[0].type != VAL_LIST || argv[0].list->count < 2) return value_bool(0);
    if (argv[1].type != VAL_LIST || argv[1].list->count < 4) return value_bool(0);

    Vector2 point = { (float)val_to_double(argv[0].list->items[0]), (float)val_to_double(argv[0].list->items[1]) };
    Rectangle rec = { 
        (float)val_to_double(argv[1].list->items[0]), 
        (float)val_to_double(argv[1].list->items[1]), 
        (float)val_to_double(argv[1].list->items[2]), 
        (float)val_to_double(argv[1].list->items[3]) 
    };

    return value_bool(CheckCollisionPointRec(point, rec));
}

// Color Utilities

Value lib_gui_rgb(int argc, Value *argv, Env *env) {
    Value list = value_list();
    for (int i = 0; i < 3; i++) value_list_append(&list, (i < argc) ? argv[i] : value_int(0));
    value_list_append(&list, (argc > 3) ? argv[3] : value_int(255));
    return list;
}

Value lib_gui_hsl(int argc, Value *argv, Env *env) {
    if (argc < 3) return value_null();
    Color c = ColorFromHSV((float)val_to_double(argv[0]), (float)val_to_double(argv[1]), (float)val_to_double(argv[2]));
    Value list = value_list();
    value_list_append(&list, value_int(c.r));
    value_list_append(&list, value_int(c.g));
    value_list_append(&list, value_int(c.b));
    value_list_append(&list, value_int(255));
    return list;
}

// Textures & Rotation

Value lib_gui_load_texture(int argc, Value *argv, Env *env) {
    if (argc < 1 || texture_count >= MAX_TEXTURES) return value_int(-1);
    texture_cache[texture_count] = LoadTexture(argv[0].string->chars);
    return value_int(texture_count++);
}

Value lib_gui_draw_texture(int argc, Value *argv, Env *env) {
    if (argc < 3) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < texture_count) {
        DrawTexture(texture_cache[id], (int)val_to_double(argv[1]), (int)val_to_double(argv[2]), WHITE);
    }
    return value_null();
}

Value lib_gui_draw_texture_rot(int argc, Value *argv, struct Env *env) {
    if (argc < 4) return value_null();
    int id = (int)argv[0].i;
    if (id < 0 || id >= texture_count) return value_null();
    Texture2D tex = texture_cache[id];
    float x = (float)val_to_double(argv[1]);
    float y = (float)val_to_double(argv[2]);
    float rot = (float)val_to_double(argv[3]);
    // Center origin rotation
    DrawTexturePro(tex, (Rectangle){0, 0, (float)tex.width, (float)tex.height}, 
                        (Rectangle){x, y, (float)tex.width, (float)tex.height}, 
                        (Vector2){(float)tex.width/2.0f, (float)tex.height/2.0f}, rot, WHITE);
    return value_null();
}

Value lib_gui_draw_texture_pro(int argc, Value *argv, struct Env *env) {
    if (argc < 6) return value_null();
    int id = (int)argv[0].i; 
    if (id < 0 || id >= texture_count) return value_null();
    
    // Arg 1: Source Rec [x, y, w, h]
    // Arg 2: Dest Rec [x, y, w, h]
    // Arg 3: Origin [x, y]
    // Arg 4: Rotation
    // Arg 5: Tint
    
    if (argv[1].type != VAL_LIST || argv[1].list->count < 4) return value_null();
    if (argv[2].type != VAL_LIST || argv[2].list->count < 4) return value_null();
    if (argv[3].type != VAL_LIST || argv[3].list->count < 2) return value_null();

    Rectangle source = { 
        (float)val_to_double(argv[1].list->items[0]), (float)val_to_double(argv[1].list->items[1]), 
        (float)val_to_double(argv[1].list->items[2]), (float)val_to_double(argv[1].list->items[3]) 
    };
    Rectangle dest = { 
        (float)val_to_double(argv[2].list->items[0]), (float)val_to_double(argv[2].list->items[1]), 
        (float)val_to_double(argv[2].list->items[2]), (float)val_to_double(argv[2].list->items[3]) 
    };
    Vector2 origin = { 
        (float)val_to_double(argv[3].list->items[0]), (float)val_to_double(argv[3].list->items[1]) 
    };
    float rotation = (float)val_to_double(argv[4]);
    Color tint = val_to_color(argv[5]);

    DrawTexturePro(texture_cache[id], source, dest, origin, rotation, tint);
    return value_null();
}

Value lib_gui_get_texture_width(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_int(0);
    int id = (int)argv[0].i;
    if (id < 0 || id >= texture_count) return value_int(0);
    return value_int(texture_cache[id].width);
}

Value lib_gui_get_texture_height(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_int(0);
    int id = (int)argv[0].i;
    if (id < 0 || id >= texture_count) return value_int(0);
    return value_int(texture_cache[id].height);
}

Value lib_gui_unload_texture(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < texture_count) {
        UnloadTexture(texture_cache[id]);
    }
    return value_null();
}

// Audio System

Value lib_gui_init_audio(int argc, Value *argv, struct Env *env) {
    InitAudioDevice();
    return value_null();
}

Value lib_gui_close_audio_device(int argc, Value *argv, struct Env *env) {
    CloseAudioDevice();
    return value_null();
}

Value lib_gui_load_music(int argc, Value *argv, struct Env *env) {
    if (argc < 1 || music_count >= MAX_MUSIC) return value_int(-1);
    music_cache[music_count] = LoadMusicStream(argv[0].string->chars);
    // Attach audio processor for visualizer PCM capture
    AttachAudioStreamProcessor(music_cache[music_count].stream, audio_processor_callback);
    return value_int(music_count++);
}

Value lib_gui_unload_music_stream(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < music_count) UnloadMusicStream(music_cache[id]);
    return value_null();
}

Value lib_gui_load_sound(int argc, Value *argv, struct Env *env) {
    if (argc < 1 || sound_count >= MAX_SOUNDS) return value_int(-1);
    sound_cache[sound_count] = LoadSound(argv[0].string->chars);
    return value_int(sound_count++);
}

Value lib_gui_unload_sound(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < sound_count) UnloadSound(sound_cache[id]);
    return value_null();
}

Value lib_gui_play_music(int argc, Value *argv, struct Env *env) {
    if (argc > 0) PlayMusicStream(music_cache[(int)argv[0].i]);
    return value_null();
}

Value lib_gui_stop_music_stream(int argc, Value *argv, struct Env *env) {
    if (argc > 0) StopMusicStream(music_cache[(int)argv[0].i]);
    return value_null();
}

Value lib_gui_pause_music_stream(int argc, Value *argv, struct Env *env) {
    if (argc > 0) PauseMusicStream(music_cache[(int)argv[0].i]);
    return value_null();
}

Value lib_gui_resume_music_stream(int argc, Value *argv, struct Env *env) {
    if (argc > 0) ResumeMusicStream(music_cache[(int)argv[0].i]);
    return value_null();
}

Value lib_gui_update_music(int argc, Value *argv, struct Env *env) {
    if (argc > 0) UpdateMusicStream(music_cache[(int)argv[0].i]);
    return value_null();
}

Value lib_gui_play_sound(int argc, Value *argv, struct Env *env) {
    if (argc > 0) PlaySound(sound_cache[(int)argv[0].i]);
    return value_null();
}

Value lib_gui_get_music_fft(int argc, Value *argv, struct Env *env) {
    // Returns a list of 16 frequency band magnitudes (0.0 - 1.0)
    #define NUM_BANDS 32
    
    // Copy the ring buffer so we have a stable snapshot
    float samples[FFT_BUFFER_SIZE];
    int cursor = fft_write_cursor; // snapshot cursor
    for (int i = 0; i < FFT_BUFFER_SIZE; i++) {
        samples[i] = fft_buffer[(cursor + i) % FFT_BUFFER_SIZE];
    }
    
    // Simple DFT: compute magnitude at NUM_BANDS logarithmically spaced frequencies
    // Frequency bands roughly span: 60Hz, 100Hz, 170Hz, 280Hz, 470Hz, ... up to ~16kHz
    // Assuming 48000 Hz sample rate
    float band_freqs[NUM_BANDS];
    float base_freq = 60.0f;
    float freq_ratio = powf(16000.0f / 60.0f, 1.0f / (NUM_BANDS - 1));
    for (int b = 0; b < NUM_BANDS; b++) {
        band_freqs[b] = base_freq * powf(freq_ratio, (float)b);
    }
    
    float magnitudes[NUM_BANDS];
    float max_mag = 0.0001f; // avoid div by zero
    int N = 1024; // Use last 1024 samples for better performance
    float sample_rate = 48000.0f;
    
    for (int b = 0; b < NUM_BANDS; b++) {
        float freq = band_freqs[b];
        float real = 0.0f;
        float imag = 0.0f;
        float omega = 2.0f * 3.14159265f * freq / sample_rate;
        
        int start = FFT_BUFFER_SIZE - N;
        for (int n = 0; n < N; n++) {
            float s = samples[start + n];
            real += s * cosf(omega * n);
            imag += s * sinf(omega * n);
        }
        
        magnitudes[b] = sqrtf(real * real + imag * imag) / (float)N;
        if (magnitudes[b] > max_mag) max_mag = magnitudes[b];
    }
    
    // Normalize to 0.0 - 1.0 and return as Luna list
    Value result = value_list();
    for (int b = 0; b < NUM_BANDS; b++) {
        float normalized = magnitudes[b] / max_mag;
        value_list_append(&result, value_float((double)normalized));
    }
    
    return result;
}

Value lib_gui_get_music_time_length(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_float(0.0f);
    int id = (int)argv[0].i;
    if (id >= 0 && id < music_count) {
        return value_float(GetMusicTimeLength(music_cache[id]));
    }
    return value_float(0.0f);
}

Value lib_gui_get_music_time_played(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_float(0.0f);
    int id = (int)argv[0].i;
    if (id >= 0 && id < music_count) {
        return value_float(GetMusicTimePlayed(music_cache[id]));
    }
    return value_float(0.0f);
}

Value lib_gui_seek_music_stream(int argc, Value *argv, struct Env *env) {
    if (argc < 2) return value_null();
    int id = (int)argv[0].i;
    float position = (float)val_to_double(argv[1]);
    if (id >= 0 && id < music_count) {
        SeekMusicStream(music_cache[id], position);
    }
    return value_null();
}

// Fonts

Value lib_gui_load_font(int argc, Value *argv, Env *env) {
    if (argc < 2 || font_count >= MAX_FONTS) return value_int(-1);
    font_cache[font_count] = LoadFontEx(argv[0].string->chars, (int)val_to_double(argv[1]), NULL, 0);
    SetTextureFilter(font_cache[font_count].texture, TEXTURE_FILTER_BILINEAR);
    return value_int(font_count++);
}

Value lib_gui_draw_text(int argc, Value *argv, Env *env) {
    if (argc < 6) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < font_count) {
        DrawTextEx(font_cache[id], argv[1].string->chars, (Vector2){(float)val_to_double(argv[2]), (float)val_to_double(argv[3])}, 
                   (float)val_to_double(argv[4]), (float)val_to_double(argv[5]), WHITE);
    }
    return value_null();
}

Value lib_gui_measure_text(int argc, Value *argv, struct Env *env) {
    // 2 args: string, fontSize (Default Font)
    // 4 args: fontID, text, fontSize, spacing (Custom Font)
    
    if (argc == 2) {
         return value_int(MeasureText(argv[0].string->chars, (int)val_to_double(argv[1])));
    }
    else if (argc >= 4) { // fontID, text, fontSize, spacing
        int id = (int)argv[0].i;
        if (id >= 0 && id < font_count) {
            Vector2 size = MeasureTextEx(font_cache[id], argv[1].string->chars, (float)val_to_double(argv[2]), (float)val_to_double(argv[3]));
            return value_int((int)size.x);
        }
    }
    return value_int(0);
}

Value lib_gui_draw_text_default(int argc, Value *argv, struct Env *env) {
    if (argc < 5) return value_null();
    // Arg 0: Text
    // Arg 1: x
    // Arg 2: y
    // Arg 3: FontSize
    // Arg 4: Color
    DrawText(argv[0].string->chars, (int)val_to_double(argv[1]), (int)val_to_double(argv[2]), 
             (int)val_to_double(argv[3]), val_to_color(argv[4]));
    return value_null();
}

// Camera System
Value lib_gui_begin_mode_2d(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    // Arg 0: Camera list [offset_x, offset_y, target_x, target_y, rotation, zoom]
    if (argv[0].type != VAL_LIST || argv[0].list->count < 6) return value_null();
    
    Camera2D cam = { 0 };
    cam.offset.x = (float)val_to_double(argv[0].list->items[0]);
    cam.offset.y = (float)val_to_double(argv[0].list->items[1]);
    cam.target.x = (float)val_to_double(argv[0].list->items[2]);
    cam.target.y = (float)val_to_double(argv[0].list->items[3]);
    cam.rotation = (float)val_to_double(argv[0].list->items[4]);
    cam.zoom = (float)val_to_double(argv[0].list->items[5]);
    
    BeginMode2D(cam);
    return value_null();
}

Value lib_gui_end_mode_2d(int argc, Value *argv, struct Env *env) {
    EndMode2D();
    return value_null();
}

// Image Manipulation
Value lib_gui_load_image(int argc, Value *argv, struct Env *env) {
    if (argc < 1 || image_count >= MAX_IMAGES) return value_int(-1);
    image_cache[image_count] = LoadImage(argv[0].string->chars);
    return value_int(image_count++);
}

Value lib_gui_image_rotate_cw(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < image_count) {
        ImageRotateCW(&image_cache[id]);
    }
    return value_null();
}

Value lib_gui_load_texture_from_image(int argc, Value *argv, struct Env *env) {
    if (argc < 1 || texture_count >= MAX_TEXTURES) return value_int(-1);
    int img_id = (int)argv[0].i;
    if (img_id < 0 || img_id >= image_count) return value_int(-1);
    
    texture_cache[texture_count] = LoadTextureFromImage(image_cache[img_id]);
    return value_int(texture_count++);
}

Value lib_gui_unload_image(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < image_count) {
        UnloadImage(image_cache[id]);
    }
    return value_null();
}

// Particle System Helper (Workaround for List Bugs)
Value lib_gui_create_particle_pool(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int count = (int)val_to_double(argv[0]);
    
    Value list = value_list();
    // Pre-allocate
    list.list->items = malloc(sizeof(Value) * count);
    list.list->capacity = count;
    list.list->count = count;
    
    for (int i = 0; i < count; i++) {
        // Create a default particle struct [x, y, angle, speed, hue, size, active]
        Value p = value_list();
        value_list_append(&p, value_float(0.0)); // x
        value_list_append(&p, value_float(0.0)); // y
        value_list_append(&p, value_float(0.0)); // angle
        value_list_append(&p, value_float(0.0)); // speed
        value_list_append(&p, value_float(0.0)); // hue
        value_list_append(&p, value_float(0.0)); // size
        value_list_append(&p, value_int(0));     // active

        // Construct pool with direct assignment for performance
        list.list->items[i] = p;
    }
    
    return list;
}
static unsigned int syncsafe_to_int(unsigned char *bytes) {
    return (bytes[0] << 21) | (bytes[1] << 14) | (bytes[2] << 7) | bytes[3];
}

static unsigned int unsynchsafe_to_int(unsigned char *bytes) {
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

// Load Album Art from MP3
Value lib_gui_load_music_cover(int argc, Value *argv, struct Env *env) {
    if (argc < 1 || texture_count >= MAX_TEXTURES) return value_int(-1);
    
    const char *filename = argv[0].string->chars;
    FILE *f = fopen(filename, "rb");
    if (!f) return value_int(-1);
    
    unsigned char header[10];
    if (fread(header, 1, 10, f) != 10) { fclose(f); return value_int(-1); }
    
    if (memcmp(header, "ID3", 3) != 0) { fclose(f); return value_int(-1); }
    
    int version = header[3];
    int size = syncsafe_to_int(header + 6);
    
    // Safety limit
    if (size > 5 * 1024 * 1024) size = 5 * 1024 * 1024; 
    
    unsigned char *tag_data = (unsigned char*)malloc(size);
    if (!tag_data) { fclose(f); return value_int(-1); }
    
    if (fread(tag_data, 1, size, f) != (size_t)size) {
        free(tag_data);
        fclose(f);
        return value_int(-1);
    }
    fclose(f); // Done reading
    
    int cursor = 0;
    while (cursor < size - 10) {
        // Frame Header
        // ID: 4 bytes
        // Size: 4 bytes
        // Flags: 2 bytes
        
        char frame_id[5] = {0};
        memcpy(frame_id, tag_data + cursor, 4);
        
        // Check for padding (null bytes)
        if (frame_id[0] == 0) break;
        
        unsigned int frame_size = 0;
        if (version == 4) {
            frame_size = syncsafe_to_int(tag_data + cursor + 4);
        } else {
            frame_size = unsynchsafe_to_int(tag_data + cursor + 4);
        }
        
        cursor += 10; // Skip header
        
        if (strcmp(frame_id, "APIC") == 0) {
            // Found APIC frame (Attached Picture)
            // Structure: [Text Encoding] [MIME Type] [Picture Type] [Description] [Picture Data]
            
            int data_cursor = cursor;
            // Skip encoding
            data_cursor++; 
            
            // Read MIME
            char mime[64] = {0};
            int i = 0;
            while (data_cursor < size && tag_data[data_cursor] != 0 && i < 63) {
                mime[i++] = tag_data[data_cursor++];
            }
            data_cursor++; // Skip null
            
            // Skip Picture Type
            data_cursor++;
            
            // Skip Description (variable length, null-terminated)
            // We search for image signatures (JPEG/PNG) to locate the start of data.
            int img_start = -1;
            int max_scan = frame_size < 256 ? frame_size : 256;
            
            for (int k = 0; k < max_scan; k++) {
                // Check for JPEG signature (FF D8)
                if (data_cursor + k + 1 < size && tag_data[data_cursor + k] == 0xFF && tag_data[data_cursor + k + 1] == 0xD8) {
                    img_start = data_cursor + k;
                    break;
                }
                // Check for PNG signature (89 50 4E 47)
                if (data_cursor + k + 3 < size && tag_data[data_cursor + k] == 0x89 && tag_data[data_cursor + k + 1] == 0x50 && tag_data[data_cursor + k + 2] == 0x4E) {
                    img_start = data_cursor + k;
                    break;
                }
            }
            
            if (img_start != -1) {
                int img_size = frame_size - (img_start - cursor);
                // Determine format extension from MIME type
                const char *ext = ".png";
                if (strstr(mime, "jpeg") || strstr(mime, "JPG")) ext = ".jpg";
                
                Image img = LoadImageFromMemory(ext, tag_data + img_start, img_size);
                
                if (img.data) {
                    // Success!
                    texture_cache[texture_count] = LoadTextureFromImage(img);
                    UnloadImage(img);
                    free(tag_data);
                    return value_int(texture_count++);
                }
            }
        }
        
        cursor += frame_size;
    }
    
    free(tag_data);
    return value_int(-1);
}

// Advanced Gradient
Value lib_gui_draw_gradient_ex(int argc, Value *argv, struct Env *env) {
    if (argc < 8) return value_null();
    Rectangle rec = { 
        (float)val_to_double(argv[0]), (float)val_to_double(argv[1]), 
        (float)val_to_double(argv[2]), (float)val_to_double(argv[3]) 
    };
    Color c1 = val_to_color(argv[4]); // TL
    Color c2 = val_to_color(argv[5]); // BL
    Color c3 = val_to_color(argv[6]); // BR
    Color c4 = val_to_color(argv[7]); // TR
    
    DrawRectangleGradientEx(rec, c1, c2, c3, c4);
    return value_null();
}

// Render Texture System (For Persistent Canvas)
#define MAX_RENDER_TEXTURES 8
static RenderTexture2D render_textures[MAX_RENDER_TEXTURES];
static int render_texture_count = 0;

Value lib_gui_load_render_texture(int argc, Value *argv, struct Env *env) {
    if (argc < 2 || render_texture_count >= MAX_RENDER_TEXTURES) return value_int(-1);
    int w = (int)val_to_double(argv[0]);
    int h = (int)val_to_double(argv[1]);
    render_textures[render_texture_count] = LoadRenderTexture(w, h);
    return value_int(render_texture_count++);
}

Value lib_gui_begin_texture_mode(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < render_texture_count) {
        BeginTextureMode(render_textures[id]);
    }
    return value_null();
}

Value lib_gui_end_texture_mode(int argc, Value *argv, struct Env *env) {
    EndTextureMode();
    return value_null();
}

Value lib_gui_draw_render_texture(int argc, Value *argv, struct Env *env) {
    // Draw the texture of the render texture (usually flipped vertically in OpenGL)
    if (argc < 3) return value_null();
    int id = (int)argv[0].i;
    int x = (int)val_to_double(argv[1]);
    int y = (int)val_to_double(argv[2]);
    
    if (id >= 0 && id < render_texture_count) {
        Rectangle src = {0, 0, (float)render_textures[id].texture.width, (float)-render_textures[id].texture.height};
        Rectangle dest = {(float)x, (float)y, (float)render_textures[id].texture.width, (float)render_textures[id].texture.height};
        Vector2 origin = {0, 0};
        DrawTexturePro(render_textures[id].texture, src, dest, origin, 0.0f, WHITE);
    }
    return value_null();
}

Value lib_gui_unload_render_texture(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    int id = (int)argv[0].i;
    if (id >= 0 && id < render_texture_count) {
        UnloadRenderTexture(render_textures[id]);
    }
    return value_null();
}

Value lib_gui_take_screenshot(int argc, Value *argv, struct Env *env) {
    if (argc < 1) return value_null();
    TakeScreenshot(argv[0].string->chars);
    return value_null();
}
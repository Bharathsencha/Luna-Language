# Luna GUI Documentation

The Luna GUI module provides immediate-mode 2D graphics, audio, and input handling. It is powered by Luna's own custom backends:

- **Rendering**: OpenGL 3.3 Core Profile + GLFW 3.3.10 (static) — implemented in `gui/gl_backend.c`
- **Audio**: miniaudio — implemented in `gui/audio_backend.c`
- **Image loading**: stb_image — **Font rendering**: stb_truetype — **Screenshots**: stb_image_write

> **History:** Luna originally used [Raylib](https://github.com/raysan5/raylib) as its graphics and audio backend. The Raylib dependency has been fully replaced with these custom backends. The Luna scripting API remains the same — all existing scripts continue to work without modification.

## How It Works

1. **Window & Context**: `init_window()` calls GLFW to create a window and OpenGL 3.3 core context, then compiles embedded GLSL 330 shaders and sets up a batch quad renderer (VBO + VAO, up to 65,536 vertices).
2. **Drawing**: Shape functions (`draw_rectangle`, `draw_circle`, etc.) push colored/textured quads into a vertex batch. At `end_drawing()`, the batch is flushed to the GPU in a single draw call (or a few, if textures change).
3. **Textures**: `load_texture()` uses stb_image to decode image files into RGBA pixels, then uploads them as OpenGL textures.
4. **Fonts**: `load_font()` uses stb_truetype to bake a glyph atlas into a single GL texture. Text drawing samples glyphs from this atlas.
5. **Render Textures**: `load_render_texture()` creates an OpenGL Framebuffer Object (FBO) for off-screen rendering.
6. **Audio**: `init_audio_device()` initializes a miniaudio engine. Music is streamed (`MA_SOUND_FLAG_STREAM`), sounds are decoded fully into memory. A PCM ring-buffer capture feeds `get_music_fft()` for real-time frequency analysis.
7. **Input**: Keyboard and mouse state is tracked via GLFW callbacks with per-frame "pressed" vs "down" detection.

## Core System

| Luna Function | C Backend Call | Description |
| :--- | :--- | :--- |
| `init_window(w, h, title)` | `gl_init_window` | Initialize window and OpenGL context. |
| `window_open()` | `gl_window_should_close` | Returns true while window should remain open. |
| `close_window()` | `gl_close_window` | Close window and release OpenGL resources. |
| `begin_drawing()` | `gl_begin_drawing` | Clear framebuffer and begin a new frame. |
| `end_drawing()` | `gl_end_drawing` | Flush batch renderer and swap buffers. |
| `clear_background(color)` | `gl_clear_background` | Set background color (using Color list). |
| `set_opacity(opacity)` | `gl_set_window_opacity` | Set window opacity (0.0 to 1.0). |
| `get_delta_time()` | `gl_get_frame_time` | Get time in seconds for last frame drawn (delta time). |
| `set_fps(fps)` | `gl_set_target_fps` | Set target FPS (maximum). |

## Colors

The following colors are built-in and can be used directly:

| Color | Description |
| :--- | :--- |
| **Basic** | `RED`, `GREEN`, `BLUE`, `WHITE`, `BLACK`, `YELLOW`, `ORANGE`, `PURPLE`, `CYAN`, `MAGENTA` |
| **Grayscale** | `GRAY`, `DARK_GRAY`, `LIGHT_GRAY`, `NOIR` |
| **Extended** | `PINK`, `BROWN`, `SILVER`, `NAVY`, `TEAL`, `LIME`, `MAROON`, `OLIVE`, `INDIGO`, `VIOLET` |
| **Shades** | `DARK_RED`, `DARK_GREEN`, `DARK_BLUE`, `LIGHT_RED`, `LIGHT_GREEN`, `LIGHT_BLUE` |
| **Special** | `TRANSPARENT`, `GOLD`, `SKYBLUE` |

## Shapes

| Luna Function | C Backend Call | Description |
| :--- | :--- | :--- |
| `draw_rectangle(x, y, w, h, color)` or `(x, y, w, h, r, g, b, a)` or `(x, y, w, h, thick, radius, color)` | `gl_draw_rect` / `gl_draw_rect_rounded` | Draw a rectangle. Accepts color as list, inline RGBA, or with style args. |
| `draw_rectangle_rec(rec, color)` | `gl_draw_rectangle` | Draw a rectangle using `[x,y,w,h]` list. |
| `draw_rectangle_lines(x, y, w, h, color)` | `gl_draw_rectangle_lines` | Draw rectangle outline. |
| `draw_circle(x, y, radius, [color])` | `gl_draw_circle` | Draw a color-filled circle. |
| `draw_line(x1, y1, x2, y2, thick, [color])` | `gl_draw_line` | Draw a line defined by start and end points. |
| `draw_gradient_v(x, y, w, h, color1, color2)` | `gl_draw_gradient_v` | Draw a vertical-gradient-filled rectangle. |
| `draw_gradient_ex(x, y, w, h, tl, bl, br, tr)` | `gl_draw_gradient_ex` | Draw a rectangle with 4-corner gradient colors. |
| `hsl(h, s, l)` | (computed in gui_lib.c) | Convert HSL to RGB. Returns `[r, g, b]` list. |

## Render Textures (Canvas)

| Luna Function | C Backend Call | Description |
| :--- | :--- | :--- |
| `load_render_texture(w, h)` | `gl_load_render_texture` | Create a persistent off-screen FBO canvas. Returns ID. |
| `unload_render_texture(id)` | `gl_unload_render_texture` | Unload render texture from GPU. |
| `begin_texture_mode(id)` | `gl_begin_texture_mode` | Start drawing onto the render texture (binds FBO). |
| `end_texture_mode()` | `gl_end_texture_mode` | Stop drawing to texture, return to default framebuffer. |
| `draw_render_texture(id, x, y)` | `gl_draw_render_texture` | Draw the render texture to the screen (handles Y-flip). |

## Screenshot

| Luna Function | C Backend Call | Description |
| :--- | :--- | :--- |
| `take_screenshot(filename)` | `gl_take_screenshot` | Save the current screen to a PNG file using stb_image_write. |

## Textures & Images

| Luna Function | C Backend Call | Description |
| :--- | :--- | :--- |
| `load_texture(path)` | `gl_load_texture` | Load texture from file (via stb_image) into GPU. Returns ID. |
| `draw_texture(id, x, y)` | `gl_draw_texture` | Draw a texture. |
| `draw_texture_rot(id, x, y, rot)` | `gl_draw_texture_rot` | Draw texture with rotation (centered). |
| `draw_texture_pro(id, source, dest, origin, rot, tint)` | `gl_draw_texture_pro` | Draw a part of a texture defined by a rect. |
| `get_texture_width(id)` | `gl_get_texture_width` | Get texture width. |
| `get_texture_height(id)` | `gl_get_texture_height` | Get texture height. |
| `unload_texture(id)` | `gl_unload_texture` | Unload texture from GPU memory. |
| `load_image(path)` | `gl_load_image` | Load image from file into CPU memory (stb_image). Returns ID. |
| `image_rotate_cw(id)` | `gl_image_rotate_cw` | Rotate image clockwise 90deg. |
| `load_texture_from_image(id)` | `gl_load_texture_from_image` | Upload image to GPU as texture. Returns Texture ID. |
| `unload_image(id)` | `gl_unload_image` | Unload image from CPU memory. |

## Text

| Luna Function | C Backend Call | Description |
| :--- | :--- | :--- |
| `load_font(path, size)` | `gl_load_font` | Load font from file (via stb_truetype). Returns ID. |
| `draw_text(id, text, x, y, size, spacing)` | `gl_draw_text` | Draw text using a loaded font. |
| `draw_text_default(text, x, y, size, color)` | `gl_draw_text_default` | Draw text using the system default font. |
| `measure_text(text, size)` | `gl_measure_text` | Measure string width for default font. |
| `measure_text(fontID, text, size, spacing)` | `gl_measure_text_ex` | Measure string width for custom font. |
| `to_string(value)` | (computed in gui_lib.c) | Convert any value to string (useful for UI). |
| `label(text)` | (computed in gui_lib.c) | Draw a simple label widget. |
| `button(text)` | (computed in gui_lib.c) | Draw a button widget. |
| `slider(var_name, min, max, label)` | (computed in gui_lib.c) | Draw a slider widget bound to a variable. |

## Input

| Luna Function | C Backend Call | Description |
| :--- | :--- | :--- |
| `get_mouse_position()` | `gl_get_mouse_position` | Returns `[x, y]` list. |
| `is_mouse_button_pressed(button)` | `gl_is_mouse_button_pressed` | Check if a mouse button has been pressed once. |
| `is_mouse_button_down(button)` | `gl_is_mouse_button_down` | Check if a mouse button is being held down. |
| `is_key_down(key)` | `gl_is_key_down` | Check if a key is being pressed (GLFW callback). |
| `is_key_pressed(key)` | `gl_is_key_pressed` | Check if a key has been pressed once (edge-detected). |
| `check_collision_point_rec(point, rec)` | `gl_check_collision_point_rec` | Check if point is inside rectangle. |

### Controls & Input

#### 1. Keyboard Input

Use `is_key_down(KEY)` for continuous actions (like movement) and `is_key_pressed(KEY)` for single triggers (like jumping or pausing).

| Key Group | Constants |
| :--- | :--- |
| **Arrows** | `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT` |
| **Alphabet** | `KEY_A`, `KEY_B`, `KEY_C` ... `KEY_Z` (All A-Z available) |
| **Numbers** | `KEY_0`, `KEY_1` ... `KEY_9` |
| **Action** | `KEY_SPACE`, `KEY_ENTER`, `KEY_ESCAPE`, `KEY_TAB`, `KEY_BACKSPACE` |

**Example:**
```javascript
if (is_key_down(KEY_RIGHT)) {
    # Move Right
}
if (is_key_pressed(KEY_SPACE)) {
    
    # Jump
}
```

#### 2. Mouse Input

**Mouse Buttons:**
Use `is_mouse_button_pressed(BUTTON)` to detect clicks (once) and `is_mouse_button_down(BUTTON)` for continuous holding (like painting).

-   `MOUSE_LEFT_BUTTON`
-   `MOUSE_RIGHT_BUTTON`
-   `MOUSE_MIDDLE_BUTTON`

**Mouse Position:**
```python
let mouse = get_mouse_position() # Returns [x, y]
```

**Mouse Scroll:**
Use `get_mouse_wheel_move()` to detect scrolling. It returns a value:
-   **Positive (> 0)**: Scrolled UP
-   **Negative (< 0)**: Scrolled DOWN
-   **0**: No scroll

**Example:**
```javascript
# Check for Left Click
if (is_mouse_button_pressed(MOUSE_LEFT_BUTTON)) {
    # Shoot / Select
}

# Check Scroll
let scroll = get_mouse_wheel_move()
if (scroll > 0) {
    # Zoom In
} else if (scroll < 0) {
    # Zoom Out
}
```

## Audio

Audio is powered by [miniaudio](https://github.com/mackron/miniaudio), a single-header C library. Music is streamed directly from disk; sounds are fully decoded into memory for low-latency playback.

| Luna Function | C Backend Call | Description |
| :--- | :--- | :--- |
| `init_audio_device()` | `audio_init` | Initialize miniaudio engine. |
| `close_audio_device()` | `audio_close` | Shut down miniaudio engine. |
| `load_music_stream(path)` | `audio_load_music` | Load music stream from file. Returns ID. |
| `load_music_cover(path)` | (ID3 parser in gui_lib.c) | Load album art from MP3 ID3 tags. Returns Texture ID. |
| `play_music_stream(id)` | `audio_play_music` | Start music playing. |
| `update_music_stream(id)` | `audio_update_music` | Updates buffers for music streaming. |
| `stop_music_stream(id)` | `audio_stop_music` | Stop music playing (resets position to start). |
| `pause_music_stream(id)` | `audio_pause_music` | Pause music playing (preserves position). |
| `resume_music_stream(id)` | `audio_resume_music` | Resume paused music stream. |
| `unload_music_stream(id)` | `audio_unload_music` | Unload music stream buffers from RAM. |
| `get_music_time_length(id)` | `audio_get_music_time_length` | Get music duration in seconds. |
| `get_music_time_played(id)` | `audio_get_music_time_played` | Get current playback time in seconds. |
| `seek_music_stream(id, pos)` | `audio_seek_music` | Seek music to a specific time position (seconds). |
| `get_music_fft(id)` | `audio_get_pcm_buffer` + FFT | Get 32-band frequency magnitudes (0.0-1.0) for visualizer. Returns list. |
| `load_sound(path)` | `audio_load_sound` | Load sound from file (fully decoded). Returns ID. |
| `play_sound(id)` | `audio_play_sound` | Play a sound. |
| `unload_sound(id)` | `audio_unload_sound` | Unload sound. |

## Camera (2D)

| Luna Function | C Backend Call | Description |
| :--- | :--- | :--- |
| `begin_mode_2d(camera)` | `gl_begin_mode_2d` | Initialize 2D mode with custom camera (offset, target, rotation, zoom). |
| `end_mode_2d()` | `gl_end_mode_2d` | End 2D camera mode and restore identity transform. |

### Data Structures (Lists)

- **Rectangle**: `[x, y, width, height]`
- **Vector2/Point**: `[x, y]`
- **Color**: `[r, g, b, a]` (or use builtin colors like `RED`, `msg`)
- **Camera2D**: `[offset_x, offset_y, target_x, target_y, rotation, zoom]`

## Demos

### Car Game
A complete example of a 2D racing game logic, physics, and tile-based rendering.

You can run the built-in demos using:
-   **Car Game**: `make run-cargame`
-   **Music Player**: `make run-music`

## Examples

Here are 5 examples ranging from a minimal window to a complex animation.

### 1. Minimal Window
The absolute minimum code required to open a window.

```javascript
func main() {
    init_window(800, 600, "Window Tile")

    while (window_open()) {
        begin_drawing()
        clear_background(255, 255, 255, 255) # White
        end_drawing()
    }

    close_window()
}
```

### 2. Hello World
Drawing text on the screen.

```javascript
func main() {
    init_window(800, 600, "Hello World")
    set_fps(60)

    while (window_open()) {
        begin_drawing()
        clear_background(30, 30, 30, 255) # Dark Gray
        
        draw_text_default("Hello, Luna!", 100, 100, 40, [255, 255, 255, 255])
        
        end_drawing()
    }

    close_window()
}
```

### 3. Moving Square (Input)
Handling keyboard input to move a shape.

```javascript
func main() {
    init_window(800, 600, "Input Demo")
    set_fps(60)

    let x = 400
    let y = 300
    let speed = 5

    while (window_open()) {
        # Input Logic
        if (is_key_down(KEY_RIGHT)) { x = x + speed }
        if (is_key_down(KEY_LEFT))  { x = x - speed }
        if (is_key_down(KEY_UP))    { y = y - speed }
        if (is_key_down(KEY_DOWN))  { y = y + speed }

        begin_drawing()
        clear_background(0, 0, 0, 255)
        
        # Draw Player
        draw_rectangle(x, y, 50, 50, 0, 0, 255, 255) # Blue Square
        
        end_drawing()
    }

    close_window()
}
```

### 4. Textures & Images
Loading and drawing an image.

```javascript
func main() {
    init_window(800, 600, "Texture Demo")
    set_fps(60)

    # Load Texture
    let img_id = load_texture("assets/logo.png")
    let w = get_texture_width(img_id)
    let h = get_texture_height(img_id)

    while (window_open()) {
        begin_drawing()
        clear_background(200, 200, 200, 255)
        
        # Draw Texture at (50, 50)
        draw_texture(img_id, 50, 50)
        
        # Draw Info
        draw_text_default("Image Size: " + to_string(w) + "x" + to_string(h), 50, 10, 20, [0, 0, 0, 255])
        
        end_drawing()
    }

    unload_texture(img_id)
    close_window()
}
```

### 5. Complex Animation (Geometric Dreams)
A creative coding example combining math, shapes, and transparency.

```javascript
let time = 0.0
let my_font = -1

func main() {
    init_window(1024, 768, "Luna - Geometric Dreams")
    
    my_font = load_font("assets/ShadeBlue-2OozX.ttf", 128)
    
    while(window_open()) {
        time = time + 0.02
        
        let m = get_mouse_position()
        let mx = m[0]
        let my = m[1]
        
        begin_drawing()
        clear_background()
        
        let offset = sin(time * 0.5) * 50
        draw_rectangle(100 + offset, 0, 50, 768)
        draw_rectangle(800 - offset, 0, 50, 768)
        
        let bar_h = 100 + sin(time * 2) * 50
        draw_rectangle(0, 384 - (bar_h / 2), 1024, bar_h)

        let o1_x = mx + cos(time * 3) * 80
        let o1_y = my + sin(time * 3) * 80
        draw_circle(o1_x, o1_y, 20)

        let o2_x = mx + sin(time * 1.5) * 150
        let o2_y = my + cos(time * 1.5) * 150
        draw_circle(o2_x, o2_y, 35)

        let pulse_r = 40 + sin(time * 5) * 10
        draw_circle(mx, my, pulse_r)

        if (my_font != -1) {
            let text_y = 100 + sin(time) * 20
            draw_text(my_font, "geometric dreams", 55, text_y + 5, 100, 2)
            draw_text(my_font, "geometric dreams", 50, text_y, 100, 2)
        }
        
        end_drawing()
    }
}
```

### 6. Color Palette
Using the built-in color constants.

```javascript
func main() {
    init_window(800, 600, "Luna Colors")
    set_fps(60)

    while (window_open()) {
        begin_drawing()
        clear_background(240, 240, 240, 255) # Light Gray BG
        
        draw_text_default("Built-in Colors", 20, 20, 30, DARK_GRAY)
        
        # Row 1
        draw_rectangle(50, 80, 100, 100, RED)
        draw_rectangle(160, 80, 100, 100, ORANGE)
        draw_rectangle(270, 80, 100, 100, YELLOW)
        draw_rectangle(380, 80, 100, 100, LIME)
        draw_rectangle(490, 80, 100, 100, GREEN)
        draw_rectangle(600, 80, 100, 100, TEAL)
        
        # Row 2
        draw_rectangle(50, 200, 100, 100, BLUE)
        draw_rectangle(160, 200, 100, 100, NAVY)
        draw_rectangle(270, 200, 100, 100, PURPLE)
        draw_rectangle(380, 200, 100, 100, VIOLET)
        draw_rectangle(490, 200, 100, 100, MAGENTA)
        draw_rectangle(600, 200, 100, 100, PINK)
        
        # Row 3
        draw_rectangle(50, 320, 100, 100, BROWN)
        draw_rectangle(160, 320, 100, 100, GOLD)
        draw_rectangle(270, 320, 100, 100, SILVER)
        draw_rectangle(380, 320, 100, 100, BLACK)
        draw_rectangle(490, 320, 100, 100, GRAY)
        
        end_drawing()
    }

    close_window()
}
```

# Luna GUI & Raylib Documentation

The Luna GUI module is a binding to the Raylib C library, providing immediate-mode 2D graphics, audio, and input handling.

## Core System

| Luna Function | Raylib Equivalent | Description |
| :--- | :--- | :--- |
| `init_window(w, h, title)` | `InitWindow` | Initialize window and OpenGL context. |
| `window_open()` | `!WindowShouldClose` | Returns true while window should remain open. |
| `close_window()` | `CloseWindow` | Close window and unload OpenGL context. |
| `begin_drawing()` | `BeginDrawing` | Setup canvas (framebuffer) to start drawing. |
| `end_drawing()` | `EndDrawing` | End canvas drawing and swap buffers (double buffering). |
| `clear_background(color)` | `ClearBackground` | Set background color (using Color list). |
| `set_opacity(opacity)` | `SetWindowOpacity` | Set window opacity (0.0 to 1.0). |
| `get_delta_time()` | `GetFrameTime` | Get time in seconds for last frame drawn (delta time). |
| `set_fps(fps)` | `SetTargetFPS` | Set target FPS (maximum). |

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

| Luna Function | Raylib Equivalent | Description |
| :--- | :--- | :--- |
| `draw_rectangle(x, y, w, h, [thick, radius, color])` | `DrawRectangle*` | Draw a rectangle. Optional args control style. |
| `draw_rectangle_rec(rec, color)` | `DrawRectangleRec` | Draw a rectangle using `[x,y,w,h]` list. |
| `draw_rectangle_lines(x, y, w, h, color)` | `DrawRectangleLines` | Draw rectangle outline. |
| `draw_circle(x, y, radius, [color])` | `DrawCircle` | Draw a color-filled circle. |
| `draw_line(x1, y1, x2, y2, thick, [color])` | `DrawLineEx` | Draw a line defined by start and end points. |
| `draw_gradient_v(x, y, w, h, color1, color2)` | `DrawRectangleGradientV` | Draw a vertical-gradient-filled rectangle. |
| `draw_gradient_ex(x, y, w, h, tl, bl, br, tr)` | `DrawRectangleGradientEx` | Draw a rectangle with 4-corner gradient colors. |
| `hsl(h, s, l)` | (Custom) | Convert HSL to RGB. Returns `[r, g, b]` list. |

## Render Textures (Canvas)

| Luna Function | Raylib Equivalent | Description |
| :--- | :--- | :--- |
| `load_render_texture(w, h)` | `LoadRenderTexture` | Create a persistent off-screen canvas. Returns ID. |
| `unload_render_texture(id)` | `UnloadRenderTexture` | Unload render texture from GPU. |
| `begin_texture_mode(id)` | `BeginTextureMode` | Start drawing onto the render texture. |
| `end_texture_mode()` | `EndTextureMode` | Stop drawing to texture, return to screen drawing. |
| `draw_render_texture(id, x, y)` | (Custom) | Draw the render texture to the screen (handles Y-flip). |

## Screenshot

| Luna Function | Raylib Equivalent | Description |
| :--- | :--- | :--- |
| `take_screenshot(filename)` | `TakeScreenshot` | Save the current screen to a file (e.g., "art.png"). |

## Textures & Images

| Luna Function | Raylib Equivalent | Description |
| :--- | :--- | :--- |
| `load_texture(path)` | `LoadTexture` | Load texture from file into GPU memory. Returns ID. |
| `draw_texture(id, x, y)` | `DrawTexture` | Draw a Texture2D. |
| `draw_texture_rot(id, x, y, rot)` | `DrawTexturePro` | Draw texture with rotation (centered). |
| `draw_texture_pro(id, source, dest, origin, rot, tint)` | `DrawTexturePro` | Draw a part of a texture defined by a rect. |
| `get_texture_width(id)` | `tex.width` | Get texture width. |
| `get_texture_height(id)` | `tex.height` | Get texture height. |
| `unload_texture(id)` | `UnloadTexture` | Unload texture from GPU memory. |
| `load_image(path)` | `LoadImage` | Load image from file into CPU memory. Returns ID. |
| `image_rotate_cw(id)` | `ImageRotateCW` | Rotate image clockwise 90deg. |
| `load_texture_from_image(id)` | `LoadTextureFromImage` | Create texture from image. Returns Texture ID. |
| `unload_image(id)` | `UnloadImage` | Unload image from CPU memory. |

## Text

| Luna Function | Raylib Equivalent | Description |
| :--- | :--- | :--- |
| `load_font(path, size)` | `LoadFontEx` | Load font from file. Returns ID. |
| `draw_text(id, text, x, y, size, spacing)` | `DrawTextEx` | Draw text using font. |
| `draw_text_default(text, x, y, size, color)` | `DrawText` | Draw text using default font. |
| `measure_text(text, size)` | `MeasureText` | Measure string width for default font. |
| `measure_text(fontID, text, size, spacing)` | `MeasureTextEx` | Measure string width for custom font. |
| `to_string(value)` | (Custom) | Convert any value to string (useful for UI). |
| `label(text)` | (Custom) | Draw a simple label widget. |
| `button(text)` | (Custom) | Draw a button widget. |
| `slider(var_name, min, max, label)` | (Custom) | Draw a slider widget bound to a variable. |

## Input

| Luna Function | Raylib Equivalent | Description |
| :--- | :--- | :--- |
| `get_mouse_position()` | `GetMousePosition` | Returns `[x, y]` list. |
| `is_mouse_button_pressed(button)` | `IsMouseButtonPressed` | Check if a mouse button has been pressed once. |
| `is_mouse_button_down(button)` | `IsMouseButtonDown` | Check if a mouse button is being held down. |
| `is_key_down(key)` | `IsKeyDown` | Check if a key is being pressed. |
| `is_key_pressed(key)` | `IsKeyPressed` | Check if a key has been pressed once. |
| `check_collision_point_rec(point, rec)` | `CheckCollisionPointRec` | Check if point is inside rectangle. |

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

| Luna Function | Raylib Equivalent | Description |
| :--- | :--- | :--- |
| `init_audio_device()` | `InitAudioDevice` | Initialize audio device and context. |
| `close_audio_device()` | `CloseAudioDevice` | Close the audio device and context. |
| `load_music_stream(path)` | `LoadMusicStream` | Load music stream from file. Returns ID. |
| `load_music_cover(path)` | (Custom) | Load album art from MP3 ID3 tags. Returns Texture ID. |
| `play_music_stream(id)` | `PlayMusicStream` | Start music playing. |
| `update_music_stream(id)` | `UpdateMusicStream` | Updates buffers for music streaming. |
| `stop_music_stream(id)` | `StopMusicStream` | Stop music playing (resets position to start). |
| `pause_music_stream(id)` | `PauseMusicStream` | Pause music playing (preserves position). |
| `resume_music_stream(id)` | `ResumeMusicStream` | Resume paused music stream. |
| `unload_music_stream(id)` | `UnloadMusicStream` | Unload music stream buffers from RAM. |
| `get_music_time_length(id)` | `GetMusicTimeLength` | Get music duration in seconds. |
| `get_music_time_played(id)` | `GetMusicTimePlayed` | Get current playback time in seconds. |
| `seek_music_stream(id, pos)` | `SeekMusicStream` | Seek music to a specific time position (seconds). |
| `get_music_fft(id)` | (Custom) | Get 32-band frequency magnitudes (0.0-1.0) for visualizer. Returns list. |
| `load_sound(path)` | `LoadSound` | Load sound from file. Returns ID. |
| `play_sound(id)` | `PlaySound` | Play a sound. |
| `unload_sound(id)` | `UnloadSound` | Unload sound. |

## Camera (2D)

| Luna Function | Raylib Equivalent | Description |
| :--- | :--- | :--- |
| `begin_mode_2d(camera)` | `BeginMode2D` | Initialize 2D mode with custom camera (Cam2D). |
| `end_mode_2d()` | `EndMode2D` | Ends 2D mode with custom camera. |

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

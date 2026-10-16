# Graphics and Audio

2D and 3D rendering on OpenGL 3.3 core (GLFW, linked statically) and audio on miniaudio. The
implementation is in `gui/`; the builtins are registered in `src/library.c`.

Conventions:

- **Colors** are lists `[r, g, b]` or `[r, g, b, a]` (0-255). `rgb(r, g, b, a = 255)` builds
  one. Named colors are predefined (see [Constants](#constants)).
- **Handles.** Loaders return an integer handle, or `-1` on failure. Always check for `-1`
  before drawing.
- **Rectangles** are `[x, y, w, h]`. **3D vectors** are `[x, y, z]`.
- Paths are relative to the working directory. Run scripts from the repository root.

## Window and frame

```javascript
init_window(800, 600, "Luna")
set_fps(60)

while (window_open()) {
    let dt = get_delta_time()
    begin_drawing()
    clear_background(rgb(20, 20, 30))
    # draw here
    end_drawing()
}

close_window()
```

| Function | Effect |
|---|---|
| `init_window(w, h, title)` | Open the window (integer width and height) |
| `window_open()` | `false` once the user closes the window |
| `set_fps(n)` | Target frame rate |
| `get_delta_time()` | Seconds since the last frame |
| `begin_drawing()`, `end_drawing()` | Bracket each frame |
| `clear_background(color)` | Fill the frame |
| `set_opacity(f)` | Window opacity, 0.0-1.0 |
| `take_screenshot(path)` | Save the framebuffer as a PNG |
| `close_window()` | Close the window and release GPU resources |

## 2D shapes

| Function | Arguments |
|---|---|
| `draw_rectangle(x, y, w, h, color)` | Filled rectangle |
| `draw_rectangle_rec(rect, color)` | Same, with `rect = [x, y, w, h]` |
| `draw_rectangle_lines(x, y, w, h, color)` | 2 px outline |
| `draw_circle(cx, cy, r, color)` | Filled circle (the color defaults to blue) |
| `draw_line(x1, y1, x2, y2, thick, color)` | Line; `draw_line(x1, y1, x2, y2, color)` uses 1 px |
| `draw_gradient_v(x, y, w, h, top, bottom)` | Vertical gradient |
| `draw_gradient_ex(x, y, w, h, tl, bl, br, tr)` | Four-corner gradient |
| `hsl(h, s, v)` | Color from hue (0-360), saturation and value (0-1). Despite the name, this is an HSV conversion |

## Text

| Function | Arguments |
|---|---|
| `load_font(path, size)` | TrueType font baked at `size`; returns a handle |
| `draw_text(font, text, x, y, size, spacing, color = WHITE)` | Text in a loaded font |
| `draw_text_default(text, x, y, size, color)` | Text in the built-in font |
| `measure_text(text, size)` | Width in pixels, built-in font |
| `measure_text(font, text, size, spacing)` | Width in pixels, loaded font |

## Textures and images

| Function | Arguments |
|---|---|
| `load_texture(path)` | Upload an image file as a texture |
| `draw_texture(tex, x, y)` | Draw at its natural size |
| `draw_texture_pro(tex, src_rect, dst_rect, origin, rotation, tint)` | Source and destination rectangles, `origin = [ox, oy]`, rotation in degrees |
| `get_texture_width(tex)`, `get_texture_height(tex)` | Size in pixels |
| `unload_texture(tex)` | Free it |
| `load_image(path)` | CPU-side image, for editing before upload |
| `image_rotate_cw(img)` | Rotate the image 90 degrees clockwise |
| `load_texture_from_image(img)` | Upload an image as a texture |
| `unload_image(img)` | Free the CPU image |

## Render textures

```javascript
let rt = load_render_texture(800, 600)
begin_texture_mode(rt)
    # draw into rt
end_texture_mode()
draw_render_texture(rt, 0, 0)
unload_render_texture(rt)
```

## 2D camera

`begin_mode_2d(cam)` takes `cam = [offset_x, offset_y, target_x, target_y, rotation, zoom]`.
Drawing between it and `end_mode_2d()` is transformed by the camera.

## Input

| Function | Returns |
|---|---|
| `is_key_down(key)`, `is_key_pressed(key)` | Held, or pressed this frame |
| `is_mouse_button_down(btn)`, `is_mouse_button_pressed(btn)` | Same for mouse buttons |
| `get_mouse_position()` | `[x, y]` |
| `get_mouse_wheel_move()` | Wheel delta this frame |
| `check_collision_point_rec(point, rect)` | `true` if `[x, y]` is inside `rect` |

## Immediate-mode widgets

Simple widgets laid out top to bottom from a fixed margin; each call moves the layout cursor
down.

| Function | Effect |
|---|---|
| `label(text)` | A line of text |
| `button(text)` | Draw a button; returns `true` on the frame it is clicked |
| `slider("var", min, max, text)` | Slider bound by name to the variable `var`, which it updates in place |

## 3D

A single Blinn-Phong shader drives all 3D drawing. It supports up to 8 lights (directional,
point, spot), exponential-squared fog, gamma correction, a 16-level transform stack,
camera-facing billboards, and sorted translucency. Geometry is cached in GPU vertex arrays.

```javascript
let cam = create_camera_3d([0, 5, -10], [0, 0, 0], [0, 1, 0], 45.0)
let sun = create_light(LIGHT_DIRECTIONAL, [10, 20, 10], [-0.4, -1, -0.2], rgb(255, 250, 240))
set_ambient_light([30, 30, 50])

while (window_open()) {
    begin_drawing()
    clear_background(rgb(15, 15, 25))
    begin_mode_3d(cam)
        draw_cube([0, 0.5, 0], [1, 1, 1], rgb(100, 180, 255))
        draw_grid(10, 1)
    end_mode_3d()
    end_drawing()
}
```

### Camera

| Function | Arguments |
|---|---|
| `create_camera_3d(pos, target, up, fov)` | Returns a camera handle |
| `update_camera_3d(cam, pos, target)` | Move the camera |
| `update_camera_orbit(cam, center, radius, yaw_deg, pitch_deg)` | Orbit around `center` |
| `update_camera_free(cam, speed = 4.0, sensitivity = 0.003)` | WASD + mouse fly camera; pair with `capture_cursor(1)` |
| `capture_cursor(on)` | Lock and hide the cursor (1), or release it (0) |
| `set_camera_fov(cam, fov)` | Field of view in degrees |
| `set_camera_projection(cam, mode)` | `PROJECTION_PERSPECTIVE` or `PROJECTION_ORTHOGRAPHIC` |
| `get_camera_position(cam)`, `get_camera_forward(cam)` | `[x, y, z]` |
| `begin_mode_3d(cam)`, `end_mode_3d()` | Bracket 3D drawing |

### Transform stack

```javascript
push_matrix()
translate_3d([2.0, 0.0, 0.0])
rotate_3d([0.0, yaw, 0.0])     # Euler XYZ, degrees
scale_3d(2.0)                  # scalar or [x, y, z]
draw_cube([0, 0, 0], [1, 1, 1], rgb(200, 60, 50))
pop_matrix()
reset_matrix()
```

### Primitives

Each `_pro` variant takes an Euler rotation. A trailing `lit` argument (default 1) draws the
shape unlit when 0. Colors with alpha below 255 are drawn far-to-near after all opaque
geometry.

| Function | Arguments |
|---|---|
| `draw_cube` / `draw_cube_pro` | `(pos, size, color)` / `(pos, size, rot, color, lit = 1)` |
| `draw_cube_wires` | `(pos, size, color)` |
| `draw_sphere` / `draw_sphere_pro` | `(center, radius, color, rings = 16, slices = 16)` / `(center, radius, rot, color, lit = 1)` |
| `draw_plane` / `draw_plane_pro` | `(center, size, color)` / `(center, size, rot, color, lit = 1)`; `size` is `[w, d]` or a scalar |
| `draw_cylinder` / `draw_cylinder_pro` | `(pos, r_top, r_bottom, height, slices, color)` / `(pos, r_top, r_bottom, height, rot, color, lit = 1)` |
| `draw_particle_3d` | `(pos, size, color)`: unlit, camera-facing billboard |
| `draw_grid` | `(slices, spacing)` |
| `draw_line_3d` | `(a, b, color)` |
| `draw_triangle_3d` | `(a, b, c, color)` |

### Models

`load_model(path)` reads a triangulated Wavefront `.obj` and its `.mtl` file. Material diffuse
colors (`Kd`) are baked into vertex colors, so a model is one draw call through the same
shader as the primitives. Missing materials default to gray, and missing normals fall back to
flat face normals.

```javascript
let car = load_model("assets/models/test/cube.obj")
if (car >= 0) {
    draw_model(car, [0, 0.5, 0], 1.0, [0, 45, 0], rgb(255, 255, 255))   # id, pos, size, rot, tint, lit = 1
    unload_model(car)
}
```

### Lights, fog, materials

| Function | Arguments |
|---|---|
| `create_light(type, pos, target, color)` | `LIGHT_DIRECTIONAL`, `LIGHT_POINT` or `LIGHT_SPOT`; returns a handle |
| `set_light_position(l, pos)`, `set_light_target(l, target)` | Move or aim |
| `set_light_color(l, color)`, `set_light_intensity(l, f)`, `set_light_enabled(l, on)` | |
| `set_light_cone(l, inner_deg, outer_deg)` | Spot cone |
| `set_ambient_light([r, g, b])` | Global ambient light |
| `set_fog(color, density)` | Exponential-squared fog; density 0 disables it |
| `set_gamma(on)` | 1 enables sRGB gamma correction on output, 0 disables it |
| `set_material(shininess, spec_strength)` | Specular parameters |

### Picking and collision

| Function | Returns |
|---|---|
| `get_mouse_ray(cam)` | `[ox, oy, oz, dx, dy, dz]` through the cursor |
| `ray_hits_box(ray, [minx, miny, minz, maxx, maxy, maxz])` | `true` on hit |
| `ray_plane_distance(ray, point, normal)` | Distance along the ray to the plane |
| `check_collision_boxes(a, b)` | Boxes as six-element min/max lists |
| `check_collision_spheres(c1, r1, c2, r2)` | `true` if the spheres overlap |

## Audio

```javascript
init_audio_device()
let bgm = load_music_stream("song.mp3")
play_music_stream(bgm)

while (window_open()) {
    update_music_stream(bgm)    # required every frame
    # ...
}

unload_music_stream(bgm)
close_audio_device()
```

| Function | Effect |
|---|---|
| `init_audio_device()`, `close_audio_device()` | Start or stop audio. Call `init_audio_device()` before loading |
| `load_music_stream(path)` | Streamed track (MP3, WAV, FLAC) |
| `play_music_stream`, `pause_music_stream`, `resume_music_stream`, `stop_music_stream` | `(m)` |
| `update_music_stream(m)` | Refill stream buffers; call every frame |
| `seek_music_stream(m, seconds)` | Jump to a position |
| `get_music_time_length(m)`, `get_music_time_played(m)` | Seconds |
| `get_music_fft(m)` | 32 floats (0-1), spectrum from 60 Hz to 16 kHz, log-spaced |
| `load_music_cover(path)` | Album art from MP3 ID3v2 tags, as a texture, or `-1` |
| `unload_music_stream(m)` | Free a stream |
| `load_sound(path)`, `play_sound(s)`, `unload_sound(s)` | Short effects, fully decoded in memory |

## Falling-sand plugin

A native cellular-automaton grid of 200x150 cells, used by `docs/examples/fun/sand.lu`. Cell
types: 0 empty, 1 sand, 2 water, 3 stone, 4 acid, 5 fire.

| Function | Effect |
|---|---|
| `sand_init()` | Clear the grid |
| `sand_set(x, y, type)`, `sand_get(x, y)` | Write or read one cell |
| `sand_brush(x, y, radius, type, percent = 100)` | Paint a disc; `percent` sets the spray density |
| `sand_update()` | Advance the simulation one step |
| `sand_draw(cell_px = 4, offset_x = 0, offset_y = 0)` | Draw the grid |

## Constants

- **Colors:** `RED GREEN BLUE GOLD BLACK WHITE DARKGRAY SKYBLUE YELLOW ORANGE PURPLE CYAN
  MAGENTA GRAY DARK_GRAY LIGHT_GRAY PINK BROWN SILVER NAVY TEAL LIME MAROON OLIVE INDIGO VIOLET
  DARK_RED DARK_GREEN DARK_BLUE LIGHT_RED LIGHT_GREEN LIGHT_BLUE TRANSPARENT`
- **Keys:** `KEY_A` to `KEY_Z` (most letters), `KEY_0` to `KEY_9`, `KEY_SPACE KEY_ESCAPE
  KEY_ENTER KEY_TAB KEY_BACKSPACE KEY_UP KEY_DOWN KEY_LEFT KEY_RIGHT`. Raw GLFW key codes also
  work (`is_key_down(265)` is Up).
- **Mouse:** `MOUSE_LEFT_BUTTON MOUSE_RIGHT_BUTTON MOUSE_MIDDLE_BUTTON`
- **3D:** `LIGHT_DIRECTIONAL LIGHT_POINT LIGHT_SPOT PROJECTION_PERSPECTIVE PROJECTION_ORTHOGRAPHIC`

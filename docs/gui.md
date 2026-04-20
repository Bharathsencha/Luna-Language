# Luna GUI API Reference

Luna features a built-in GUI library powered by OpenGL 3.3 Core and miniaudio. It provides hardware-accelerated 2D and 3D rendering alongside spatial audio capabilities.

## Initialization & Window Management

```luna
// Initialize the window with resolution and title
init_window(800, 600, "Luna App")

// Set target frames per second
set_fps(60)

// Returns true if the window should close (e.g., user clicked X)
window_open()

// Explicitly close the window
close_window()

// Get frame delta time in seconds
get_delta_time()
```

## Basic 2D Drawing

```luna
// Begin/End a render pass. All draw calls MUST be between these
begin_drawing()
end_drawing()

// Clear background with a color
clear_background(rgb(30, 30, 30))

// Primitive shapes
draw_rectangle(x, y, w, h, rgb(255, 0, 0))
draw_circle(cx, cy, radius, rgb(0, 255, 0))
draw_line([x1, y1], [x2, y2], thickness, rgb(0, 0, 255))
```

## 2D Modes & Textures

```luna
// Load and draw textures
let tex = load_texture("image.png")
draw_texture(tex, x, y, tint)

// Render targets (FBOs)
let rt = load_render_texture(w, h)
begin_texture_mode(rt)
    // Draw into the texture
end_texture_mode()
draw_render_texture(rt, x, y)
```

## Input Handling

```luna
// Mouse position [x, y]
let pos = get_mouse_position()

// Check if mouse button is held/clicked
// 0 = Left, 1 = Right, 2 = Middle
is_mouse_button_down(0)
is_mouse_button_pressed(0)

// Check if keyboard key is held/pressed
// e.g., Space = 32
is_key_down(32)
is_key_pressed(32)
```

## 3D Rendering Pipeline

The 3D functionality utilizes a Blinn-Phong lighting system with support for directional and point lights.

### Camera

```luna
// Create a perspective camera
// params: eye_position, target_position, up_vector, fov
let cam = create_camera_3d([0, 5, -10], [0, 0, 0], [0, 1, 0], 45.0)

// Update camera position and target
update_camera_3d(cam, new_pos, new_target)

// 3D Rendering Block
// All 3D primitives must be drawn inside this block
begin_mode_3d(cam)
    draw_cube([0,0,0], [1,1,1], rgb(255, 0, 0))
end_mode_3d()
```

### 3D Primitives

* **draw_cube**: `(pos, size, color)`
* **draw_cube_wires**: `(pos, size, color)`
* **draw_sphere**: `(center, radius, rings, slices, color)` 
* **draw_plane**: `(center, size, color)`
* **draw_cylinder**: `(pos, rtop, rbot, height, slices, color)`
* **draw_grid**: `(slices, spacing)`
* **draw_line_3d**: `(start_point, end_point, color)`

### Lighting 

```luna
// Set global ambient light color
set_ambient_light([40, 40, 60])

// Create a light source
// type: 0 = Directional, 1 = Point
// params: type, position, target, color
let sun = create_light(0, [10, 20, 10], [0, 0, 0], rgb(255, 250, 240))

// Light modification
set_light_position(sun, [x, y, z])
set_light_intensity(sun, 1.5)
set_light_enabled(sun, 1)
set_light_color(sun, rgb(255, 0, 0))
```

### Audio

```luna
init_audio_device()

// Music streaming (Ogg / MP3 / Wav)
let bgm = load_music_stream("song.ogg")
play_music_stream(bgm)
update_music_stream(bgm) // Must be called every frame

// SFX (loaded into memory entirely)
let sfx = load_sound("jump.wav")
play_sound(sfx)
```

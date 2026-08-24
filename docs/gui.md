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

A single Blinn-Phong shader drives everything: up to 8 simultaneous lights
(directional, point, and smooth-cone spot), exp² fog, specular controls,
gamma correction, a transform stack, camera-facing billboards, translucent
sorting, and OBJ model loading with material colors baked into vertices.
Geometry is cached in GPU VAOs — primitives are templates, models are
uploaded once at load time.

### Camera

```luna
// Create a perspective camera
// params: eye_position, target_position, up_vector, fov
let cam = create_camera_3d([0, 5, -10], [0, 0, 0], [0, 1, 0], 45.0)

// Update camera position and target
update_camera_3d(cam, new_pos, new_target)

// Orbit helper: spins around a center point (degrees)
update_camera_orbit(cam, [0, 1, 0], 6.0, time * 40.0, 18.0)

get_camera_position(cam)      // -> [x, y, z]
set_camera_projection(cam, PROJECTION_ORTHOGRAPHIC) // or PROJECTION_PERSPECTIVE

// 3D Rendering Block
// All 3D drawing must happen inside this block
begin_mode_3d(cam)
    draw_cube([0,0,0], [1,1,1], rgb(255, 0, 0))
end_mode_3d()
```

### Transform Stack

Compose hierarchies (car body → wheels → …) without hand-multiplying
matrices. The stack is 16 levels deep; drawing composes with the current
matrix automatically.

```luna
push_matrix()
translate_3d([2.0, 0.0, 0.0])
rotate_3d([0.0, yaw, 0.0])       // euler XYZ, degrees
scale_3d(2.0)                    // scalar or [x, y, z]
draw_cube([0, 0, 0], [1, 1, 1], rgb(200, 60, 50))
pop_matrix()
reset_matrix()                   // back to identity
```

### 3D Primitives

Every primitive has a `_pro` variant taking an euler rotation; the trailing
`lit` argument (default 1) renders unlit when 0.

* **draw_cube**: `(pos, size, color)` / **draw_cube_pro**: `(pos, size, rot, color, lit=1)`
* **draw_sphere**: `(center, radius, rings, slices, color)` / **draw_sphere_pro**: `(center, radius, rot, color, lit=1)`
* **draw_plane**: `(center, size, color)` / **draw_plane_pro**: `(center, size, rot, color, lit=1)` — size is `[w, d]` or scalar
* **draw_cylinder**: `(pos, rtop, rbot, height, slices, color)` / **draw_cylinder_pro**: `(pos, rtop, rbot, height, rot, color, lit=1)`
* **draw_particle_3d**: `(pos, size, color)` — camera-facing unlit billboard (fog/smoke/snow/petals)
* **draw_grid**: `(slices, spacing)`
* **draw_line_3d**: `(start_point, end_point, color)`
* **draw_triangle_3d**: `(a, b, c, color)`

Alpha < 255 enters the sorted translucent queue automatically (drawn
far-to-near after all opaque geometry).

### Models (OBJ)

`load_model` parses a triangulated Wavefront .obj plus its .mtl library.
Material diffuse colors (`Kd`) are baked into per-vertex colors, so models
flow through the same one-shader pipeline as primitives — no texture units,
one draw call per model. Missing materials default to gray; missing normals
fall back to flat face normals. `mtllib` paths resolve relative to the .obj.

```luna
let car = load_model("assets/car.obj")   // handle >= 0, -1 on failure
if (car >= 0) {
    draw_model(car, [0, 0.5, 0], 1.0, [0, 45, 0], rgb(255, 255, 255))
    unload_model(car)
}
```

**draw_model**: `(id, pos, size, rot, tint, lit=1)` — size is scalar or
`[x, y, z]`; tint multiplies the baked vertex colors. All models unload when
the window closes.

### Lighting

```luna
// Set global ambient light color
set_ambient_light([40, 40, 60])

// Create a light source
// type: LIGHT_DIRECTIONAL | LIGHT_POINT | LIGHT_SPOT
// params: type, position, direction/target, color
let sun = create_light(LIGHT_DIRECTIONAL, [10, 20, 10], [-0.4, -1, -0.2], rgb(255, 250, 240))

// Light modification
set_light_position(sun, [x, y, z])
set_light_target(sun, [0, 0, 0])   // recompute direction from position
set_light_intensity(sun, 1.5)
set_light_enabled(sun, 1)
set_light_color(sun, rgb(255, 0, 0))
set_light_cone(sun, 22.5, 30.0)    // inner/outer cutoffs (spot lights, degrees)
```

### Atmosphere & Picking

```luna
// Fog: mode 0 disables, 1 enables exponential-squared fog
set_fog(1, [180, 200, 230], 0.035)

// Gamma correction on the final output (2.2 = standard sRGB)
set_gamma(2.2)

// Specular defaults; tune per-scene if needed
set_material(32.0, 0.5)   // shininess, spec strength

// Mouse picking: flat ray [ox, oy, oz, dx, dy, dz] through the cursor
let ray = get_mouse_ray(cam)
let hit = ray_hits_box(ray, [minx, miny, minz, maxx, maxy, maxz])  // box corners
let t = ray_plane_distance(ray, [0.0, 0.0, 0.0], [0.0, 1.0, 0.0])  // hit dist to ground plane
let pt = [ray[0] + ray[3] * t, 0.0, ray[2] + ray[5] * t]
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

# Luna `fun/` — How Everything Works

A deep walkthrough of every file in the `fun/` folder.

---

## The Technical Foundations

### 1. The Stack Everything Runs On
Luna does not talk to OpenGL or the OS directly. Under the hood, everything goes through **Raylib**, a lean C library that handles window creation, the OpenGL context, input, audio streaming, texture loading, and drawing. Every Luna function like `draw_circle`, `load_texture`, or `play_music_stream` is a thin wrapper around a corresponding Raylib call, wired up in `gui/gui_lib.c`.

The binding layer works as follows:
1. Luna parses and evaluates a script.
2. When it hits a function like `draw_rectangle(...)`, the interpreter looks it up in the environment hash table.
3. It finds a native C function pointer registered by `library.c` and calls it with arguments as a `Value*` array.
4. The C function unpacks those Values, calls the actual Raylib function, and returns a result Value back to the interpreter.

```
Luna script → interpreter eval_expr → env hash lookup → lib_gui_draw_circle() in gui_lib.c → DrawCircleV() in Raylib → OpenGL
```

### 2. The Math Library (`math_lib.c`)
The math functions Luna scripts use (`sin`, `cos`, `sqrt`, `pow`, `lerp`, `rand`) are implemented in `math_lib.c`.
- **High-Quality PRNG**: The random number generator is not the standard C `rand()`. It uses **xoroshiro128++**, a modern PRNG that is much faster and statistically superior. The state is two 64-bit integers, and each call performs bit rotations and XORs to advance it.
- **True Entropy**: Seeding reads from `/dev/urandom` for real entropy instead of just using the system clock.
- **Zero Dependencies**: `math_lib.c` is a pure computation library with no Raylib dependency, ensuring math is available even in non-GUI contexts.

---

## Table of Contents

1. [How We Load Music](#1-how-we-load-music)
2. [aurora.lu — The Northern Lights (Vertical Ray Style)](#2-auroralu--the-northern-lights-vertical-ray-style)
3. [aurora_wave.lu — The Northern Lights (Horizontal Wave Style)](#3-aurora_wavelu--the-northern-lights-horizontal-wave-style)
4. [How Mountains Work (Both Aurora Files)](#4-how-mountains-work-both-aurora-files)
5. [How Stars Twinkle (Both Aurora Files)](#5-how-stars-twinkle-both-aurora-files)
6. [meadow.lu — The Sunlit Meadow](#6-meadowlu--the-sunlit-meadow)
7. [eye.lu — The All-Seeing Eye](#7-eyelu--the-all-seeing-eye)
8. [paint.lu — The Drawing App](#8-paintlu--the-drawing-app)
9. [sand.lu — The Falling Sand Simulation](#9-sandlu--the-falling-sand-simulation)
10. [galaxy.lu — The Milky Way](#10-galaxylu--the-milky-way)
11. [ripples.lu — Hypnotic Ripples](#11-rippleslu--hypnotic-ripples)
12. [car_retro.lu — Neon Retro Racer](#12-car_retrolu--neon-retro-racer)
13. [music.lu — The Audio Visualizer](#13-musiclu--the-audio-visualizer)
14. [boobs.lu — Soft Body Physics](#14-boobslu--soft-body-physics)
15. [vortex.lu & blackhole.lu — Cosmic Physics](#15-vortexlu--blackholelu--cosmic-physics)

---

## 1. How We Load Music

Every demo that uses audio follows the exact same three-step pattern using Luna's Raylib audio bindings.

```lua
init_audio_device()
let music = load_music_stream("fun/music/somefile.mp3")
play_music_stream(music)
```

**Why `init_audio_device()` first?**
Raylib requires the audio subsystem to be initialized before any sound object can be created. Under the hood this opens the OS audio driver, sets up an audio context (using miniaudio internally), and prepares mixing buffers. If you skip this call, `load_music_stream` silently fails and returns -1.

**`load_music_stream` vs `load_sound`:**
Music files are *streamed* — meaning Raylib reads them chunk-by-chunk from disk while they play. This is how you play a 4-minute MP3 without loading the whole file into RAM at once. `load_sound` would load the entire file into memory, which is fine for short sound effects but wasteful for long music tracks.

**The update call inside the loop:**
```lua
update_music_stream(music)
```
This is called every single frame inside the `while (window_open())` loop. This is mandatory. Raylib's streaming works by refilling audio buffers as they drain. If you forget `update_music_stream`, the music plays for about a second and then cuts out because the buffer runs dry and never gets refilled.

**Cleanup:**
```lua
close_audio_device()
```
Called after the loop ends. This flushes any remaining audio, closes the OS driver, and frees resources. Not calling it on exit isn't catastrophic but is bad practice.

**Music files used:**
- `aurora.lu` → `はじまりの曲-(146k).mp3` (a soft Japanese ambient track matching the serene aurora mood)
- `aurora_wave.lu` → same track
- `eye.lu` → `Pogo - Forget [Slowed down]-(129k).mp3` (a slowed, eerie edit that matches the unsettling eye theme)
- `meadow.lu` → `グッド・ドクター-(143k).mp3` (a warm, gentle track fitting the sunlit meadow)
- `car_retro.lu` → `kavinsky.mp3` (a synthwave track perfect for the neon racer aesthetic)

---

## 2. `aurora.lu` — The Northern Lights (Vertical Ray Style)

This file renders a full Northern Lights scene with a night sky, stars, moon, three layers of aurora curtains rendered as vertical rays, a two-layer mountain silhouette, a treeline, and a dark ground strip.

### The Time Variable

```lua
let time = 0.0
...
time = time + 0.008
```

`time` is the heartbeat of every animation in this file. It advances by 0.008 every frame. At 60 FPS that means time grows by `0.008 * 60 = 0.48` per second. Every `sin()` and `cos()` call in the file uses `time` as its argument (or a scaled version of it), which causes those functions to smoothly oscillate back and forth, driving all movement.

### The Night Sky Gradient

```lua
let sky_top = hsl(230.0, 0.6, 0.03)
let sky_bot = hsl(240.0, 0.4, 0.12)
draw_gradient_ex(0, 0, 1024, 768, sky_top, sky_bot, sky_bot, sky_top)
```

`hsl()` converts Hue-Saturation-Lightness to RGB. HSL was chosen here instead of raw RGB because it makes it trivially easy to express "a very dark, slightly blue/navy color" without guessing at R/G/B values.

- `hsl(230, 0.6, 0.03)` → hue 230° is blue-indigo, saturation 0.6 means quite colorful, lightness 0.03 means almost black. This gives the deep-space top of the sky.
- `hsl(240, 0.4, 0.12)` → hue 240° is slightly more blue, lightness 0.12 gives a barely-lit navy at the horizon.

`draw_gradient_ex` takes four corner colors: top-left, bottom-left, bottom-right, top-right. We pass `sky_top` for both top corners and `sky_bot` for both bottom corners, producing a smooth vertical gradient from deep black-blue at the top to slightly lighter navy at the bottom, exactly mimicking a real night sky.

### The Aurora Curtains (Vertical Ray Technique)

Aurora in real life hangs from the sky like glowing curtains of light — tall vertical rays that sway. We replicate this by iterating **horizontally** across the screen, and for each column drawing a tall vertical strip downward.

**Curtain 1 — Green:**
```lua
let ax = 50
while (ax < 900) {
    let top_y = 60 + sin(time * 1.0 + ax * 0.02) * 40 + cos(time * 0.6 + ax * 0.01) * 25
    let ray_h = 200 + sin(time * 0.8 + ax * 0.03) * 80 + sin(ax * 0.05) * 60
    let shimmer = 0.6 + sin(time * 3.0 + ax * 0.1) * 0.4
    let aurora_h = 130.0 + sin(time * 0.5 + ax * 0.015) * 15
    let a_col = hsl(aurora_h, 0.9, 0.5 * shimmer)
    let ry = 0
    while (ry < ray_h) {
        let fade = 1.0 - (ry / ray_h)
        let a_alpha = to_int(fade * fade * 90 * shimmer)
        if (a_alpha > 1) {
            draw_rectangle(ax, to_int(top_y + ry), 5, 6, [..., a_alpha])
        }
        ry = ry + 5
    }
    ax = ax + 8
}
```

**How `top_y` produces the wavy top edge:**

The top of each ray is not a flat horizontal line — it undulates. We compute it as:

```
top_y = 60 + sin(time * 1.0 + ax * 0.02) * 40 + cos(time * 0.6 + ax * 0.01) * 25
```

Breaking this down:
- `60` — base height, about 60px from the top of the screen
- `sin(time * 1.0 + ax * 0.02) * 40` — a sine wave that oscillates 40px up and down. The `ax * 0.02` term means each column is at a *different phase* of the wave, so adjacent columns have slightly different heights, creating a smooth horizontal wave shape. The `time * 1.0` makes this wave move in time.
- `cos(time * 0.6 + ax * 0.01) * 25` — a *second* wave added on top at a different frequency (0.6) and different spatial phase (0.01). Adding two sine waves of different frequencies creates an *irregular, organic* wave that never exactly repeats — much more natural than a single perfect sine.

This compound-sine pattern is sometimes called **additive synthesis** — the same principle used in audio synthesis and in noise functions like Perlin noise.

**How `ray_h` varies per column:**

```
ray_h = 200 + sin(time * 0.8 + ax * 0.03) * 80 + sin(ax * 0.05) * 60
```

Again two sine terms: one time-varying (makes the rays grow and shrink as they animate) and one static based on position (`sin(ax * 0.05) * 60`). The static one means some columns are permanently taller than others, giving the curtain its jagged, uneven bottom edge.

**How `shimmer` works:**

```
shimmer = 0.6 + sin(time * 3.0 + ax * 0.1) * 0.4
```

This oscillates between `0.2` and `1.0` at a fast rate (3.0 * time means ~3 full cycles per second at our time step). It modulates both the lightness (`0.5 * shimmer`) and the alpha (`90 * shimmer`), making each column independently brighten and dim. Because the phase is offset by `ax * 0.1`, adjacent columns shimmer out of sync — creating that characteristic shimmering, flickering aurora effect.

**How the color shifts per column:**

```
aurora_h = 130.0 + sin(time * 0.5 + ax * 0.015) * 15
```

Hue 130° is bright green. Adding a slow sine offset means the hue slowly drifts between ~115° (yellow-green) and ~145° (teal-green) both over time and across x position. This gives the aurora its subtle color variation rather than a flat uniform green.

**The vertical fade (quadratic):**

```
fade = 1.0 - (ry / ray_h)
a_alpha = to_int(fade * fade * 90 * shimmer)
```

`fade` goes from 1.0 at the top of the ray down to 0.0 at the bottom. We square it (`fade * fade`) instead of using it linearly. Why? A linear fade would look like the aurora has a hard bright top and fades uniformly. Squaring it means the alpha drops off quickly near the bottom and stays bright longer near the top — much closer to how real aurora rays look: bright where they originate and softly dissolving into the dark sky below.

**Three curtain layers:**

- Curtain 1 (green, hue ~130°): columns spaced 8px apart, covering x=50 to x=900
- Curtain 2 (cyan, hue ~175°): columns spaced 9px, x=200 to x=1000, different wave frequencies
- Curtain 3 (blue, hue ~200°): columns spaced 10px, x=0 to x=700, different wave frequencies

Each curtain uses different time multipliers, spatial frequencies, and positional offsets. The three overlap and blend on screen to create the rich multicolor layered aurora.

---

## 3. `aurora_wave.lu` — The Northern Lights (Horizontal Wave Style)

This file is a different approach to the same aurora scene. The sky, stars, moon, and mountains are identical to `aurora.lu`. What changes is **how the aurora is drawn**: instead of vertical rays, it uses horizontal strips with a sinusoidal X offset — making the aurora look like flowing horizontal bands that wave side to side.

### How the Wave Pattern Works

```lua
let ay = 60
while (ay < 480) {
    let wave1 = sin(time * 1.2 + ay * 0.015) * 140
    let wave2 = sin(time * 0.7 + ay * 0.008 + 1.5) * 90
    let x_off = wave1 + wave2
    ...
    draw_rectangle(to_int(250 + x_off), ay, to_int(a_width), 4, [...])
    ay = ay + 4
}
```

Here we iterate **vertically** — each iteration draws a horizontal strip 4px tall. The key is `x_off`, which shifts each strip horizontally by a different amount. The offset is the sum of two sine waves:

- `wave1 = sin(time * 1.2 + ay * 0.015) * 140` — 140px amplitude, changes based on both time and vertical position
- `wave2 = sin(time * 0.7 + ay * 0.008 + 1.5) * 90` — 90px amplitude, different frequency and phase offset of 1.5 radians

Because `ay * 0.015` shifts the phase per row, each horizontal strip is at a different point in the sine wave. The result is that if you look at all strips together, they form an S-curve or flowing wave shape. As time advances, the whole wave appears to sway left and right.

This is the fundamental trick: **using the loop variable as a phase offset in a sine function** to turn a simple oscillation into a spatial wave pattern. The same math works for water ripples, flags waving, tentacles moving, or anything that needs a traveling wave shape.

### Quadratic Alpha Fade

```lua
let pct = (ay - 60.0) / 420.0
let a_alpha = to_int((1.0 - pct * pct) * 120)
```

`pct` goes from 0 (top of aurora) to 1 (bottom). `1.0 - pct * pct` is a downward parabola — bright at top, fading at bottom. Squaring makes the fade non-linear: the aurora stays bright through most of its height and only drops to transparent near the very bottom. Maximum alpha is 120 (semi-transparent), keeping the aurora ethereal rather than opaque.

### The Lake Reflection (Unique to aurora_wave.lu)

```lua
let ly = 650
while (ly < 768) {
    let ref_pct = (ly - 650.0) / 118.0
    let ref_wave = sin(time * 2.0 + ly * 0.1) * 5
    let ref_hue = 140.0 + sin(time * 0.5 + ly * 0.02) * 40
    let ref_col = hsl(ref_hue, 0.7, 0.2)
    let ref_alpha = to_int((1.0 - ref_pct) * 60)
    draw_rectangle(0 + to_int(ref_wave), ly, 1024, 3, [...])
    ly = ly + 3
}
```

The bottom of the screen (y=650 to 768) simulates a lake reflecting the aurora. Each 3px-tall strip is shifted horizontally by `ref_wave = sin(time * 2.0 + ly * 0.1) * 5` — a tiny 5-pixel ripple oscillation. The phase `ly * 0.1` means each row is shifted by a different amount, creating a gentle ripple pattern across the water. The hue slowly drifts (140° to 180°, green to cyan) following the aurora colors. Alpha fades from 60 (top of lake) down to 0 (bottom), making the reflection feel like it's gently dissolving into the shore.

---

## 4. How Mountains Work (Both Aurora Files)

Both `aurora.lu` and `aurora_wave.lu` use the exact same mountain technique. Mountains are drawn as **column-by-column rectangle strips**, where the height of each strip is determined by sine waves.

### Layer 1 — Far Mountains (Lighter Color, Higher Up)

```lua
let mx = 0
while (mx < 1024) {
    let peak1 = sin(mx * 0.008 + 1.0) * 100 + cos(mx * 0.015) * 50
    let mtn_y = 520 - peak1
    let mtn_h = 768 - mtn_y
    draw_rectangle(mx, to_int(mtn_y), 4, to_int(mtn_h), [15, 18, 30, 255])
    mx = mx + 3
}
```

For every x position, we compute a `peak1` value using two trig functions:
- `sin(mx * 0.008 + 1.0) * 100` — a slow wave (0.008 = very low spatial frequency, meaning the wave crests are ~785px apart) with 100px amplitude. The `+ 1.0` is a phase shift to avoid starting at zero.
- `cos(mx * 0.015) * 50` — a faster wave (crests ~418px apart) with 50px amplitude.

Adding them creates an irregular ridgeline that rises and falls across the screen without being a simple repetitive wave. The base elevation is 520px (`mtn_y = 520 - peak1`), and the rectangle fills from `mtn_y` all the way down to the bottom of the screen (height = `768 - mtn_y`). Color is `[15, 18, 30]` — a very dark blue-grey, giving the impression of distant, misty mountains.

### Layer 2 — Near Mountains (Darker Color, Lower, Different Shape)

```lua
let peak2 = sin(mx * 0.006 + 0.5) * 80 + sin(mx * 0.02 + 3.0) * 40
let mtn_y = 570 - peak2
draw_rectangle(mx, to_int(mtn_y), 4, to_int(mtn_h), [8, 10, 18, 255])
```

Different spatial frequencies (0.006 and 0.02) and different phase offsets (0.5, 3.0) mean this layer has a completely different profile from layer 1 — they won't mirror each other. The base elevation is 570px instead of 520px, so this layer is **lower on screen and thus appears closer** (following perspective: things closer to you sit lower). The color `[8, 10, 18]` is darker — nearly pure black — reinforcing that these are nearer, more silhouetted mountains blocking the sky.

**Why two layers?** Atmospheric perspective: distant mountains appear lighter and more blue-grey because light scatters through the atmosphere. Near mountains are dark silhouettes. Two layers with different shades and different profiles creates convincing depth.

### The Treeline

```lua
while (tx < 1024) {
    let tree_base = 570 - sin(tx * 0.006 + 0.5) * 80 - sin(tx * 0.02 + 3.0) * 40
    let tree_h = 10 + sin(tx * 0.3 + tx * 0.07) * 8
    if (tree_h > 3) {
        let tw = 4 + to_int(sin(tx * 0.5) * 2)
        draw_rectangle(tx, to_int(tree_base - tree_h), tw, to_int(tree_h), [5, 8, 12, 255])
    }
    tx = tx + 7
}
```

The treeline sits *exactly on top of layer 2* — `tree_base` uses the same formula as `mtn_y` in layer 2. Each "tree" is a tiny rectangle whose height varies with `sin(tx * 0.3 + tx * 0.07) * 8` — a fast-oscillating sine that gives each tree a different height (6 to 18px), simulating the irregular tops of conifer silhouettes. Width `tw` also varies slightly (4 to 6px). Color is even darker than the mountains (`[5, 8, 12]`), making the treeline the darkest element, sitting in front of everything.

---

## 5. How Stars Twinkle (Both Aurora Files)

### The Deterministic Position Trick

```lua
let sx = (s * 137 + 53) % 1024
let sy = (s * 97 + 29) % 500
```

We need 80 stars at fixed positions — but we have no random seed available at this stage, and we want the positions to be deterministic (same every run). So instead of random numbers, we use **linear congruential hashing**: multiplying the star index `s` by a prime number (137, 97) and adding an offset, then modding by the screen dimensions. The primes ensure the distribution doesn't cluster — they're coprime to 1024 and 500, so the values spread evenly across the range. This is a classic trick for seeding fake randomness with no PRNG needed.

### The Twinkling Formula

```lua
let twinkle = sin(time * (2.0 + s * 0.3) + s * 1.7)
let star_alpha = to_int(120 + twinkle * 130)
let star_r = 1.5 + twinkle * 0.8
```

Each star has a unique twinkling frequency: `time * (2.0 + s * 0.3)`. Star 0 oscillates at speed 2.0, star 1 at speed 2.3, star 10 at speed 5.0, and so on. The `+ s * 1.7` term adds a unique phase offset per star, so they don't all pulse in sync. The result is 80 stars all independently brightening and dimming at different rates — exactly like real stars.

Alpha ranges from 120 - 130 = -10 (clamped to 30) to 120 + 130 = 250, clamped to 255. Radius ranges from 1.5 - 0.8 = 0.7 (clamped to 0.8) to 1.5 + 0.8 = 2.3. The star literally grows and shrinks with its brightness, which is more visually convincing than just alpha alone.

### Bright Stars with Glow

```lua
draw_circle(bx, by, 8, [200, 220, 255, glow_alpha])  # soft glow
draw_circle(bx, by, 3, [255, 255, 255, to_int(200 + glow * 50)])  # bright core
```

Each of the 6 bright stars is two concentric circles: a large dim blue circle (radius 8, low alpha) for the soft atmospheric glow, and a small white circle (radius 3, high alpha) for the sharp bright core. This two-circle technique is a simple substitute for a proper bloom/glow shader.

---

## 6. `meadow.lu` — The Sunlit Meadow

### Sky

```lua
let sky_top = hsl(210.0, 0.7, 0.75)
let sky_mid = hsl(200.0, 0.6, 0.85)
draw_gradient_ex(0, 0, 1024, 768, sky_top, sky_mid, sky_mid, sky_top)
```

Hue 210° is sky blue, lightness 0.75/0.85 makes it bright and airy. This is a daytime sky — light at the top, slightly lighter near the horizon (inverted from the night sky where the horizon is lighter for a different reason). The `draw_gradient_ex` creates the corner-blended effect.

### The Sun

```lua
draw_circle(sun_x, sun_y, 120, [255, 245, 200, 15])
draw_circle(sun_x, sun_y, 90, [255, 240, 180, 25])
draw_circle(sun_x, sun_y, 65, [255, 235, 160, 40])
draw_circle(sun_x, sun_y, to_int(45 * sun_pulse), [255, 250, 220, 255])
draw_circle(sun_x, sun_y, to_int(40 * sun_pulse), [255, 255, 240, 255])
```

The sun is built from **5 concentric circles** of decreasing radius and increasing alpha. The outer three are large and nearly transparent — they simulate the glow/haze around a bright sun in the sky. The inner two are the bright solid sun disc. `sun_pulse = 0.95 + sin(time * 0.8) * 0.05` makes the disc subtly breathe (±5% size) at a slow rate.

The 12 rays are lines drawn from radius 50 outward to `50 + r_len`, where `r_len = 70 + sin(time * 2 + ray * 1.3) * 20`. Each ray has a unique length that oscillates independently. The angle rotates slowly: `r_angle = ray * 0.524 + time * 0.3` (0.524 radians = 30°, so 12 rays evenly spaced, slowly spinning).

### Clouds

```lua
while (c < 5) {
    let cx = ((c * 253 + 80) % 1200) - 100 + sin(time * 0.3 + c * 2) * 30
    let cy = 60 + c * 55 + sin(time * 0.5 + c) * 10
    draw_circle(cx, cy, 35, [255, 255, 255, c_alpha])
    draw_circle(cx + 30, cy - 8, 30, ...)
    draw_circle(cx + 55, cy, 28, ...)
    draw_circle(cx + 25, cy + 5, 25, ...)
    draw_circle(cx - 15, cy + 5, 22, ...)
```

Each cloud is **5 overlapping white circles** of varying size, creating the fluffy cumulus shape. The positions of the circles within each cloud are manually tuned offsets (`+30`, `+55`, `-15` etc.) that make a natural puff shape.

Cloud X position is: `((c * 253 + 80) % 1200) - 100` — the same deterministic hash trick as stars, giving each cloud a fixed base position spread across a 1200px virtual canvas (wider than the 1024px screen, so clouds off the right edge don't suddenly jump). Adding `sin(time * 0.3 + c * 2) * 30` makes each cloud drift gently left and right, independently (different phase per cloud). Y position also bobs slowly with `sin(time * 0.5 + c) * 10`.

Alpha `c_alpha = 200 + to_int(sin(time * 0.4 + c) * 40)` makes clouds gently pulse between alpha 160 and 240, giving them a soft, living quality.

### Hills (Three Layers)

Three hill layers use the same column-by-column strip technique as the mountains, but for a daylit pastoral scene:

- **Far hills** (`hsl(150, 0.3, 0.65)`): pale, desaturated green. Base at y=480. Frequency 0.005 and 0.012 — very smooth, gentle.
- **Mid hills** (`hsl(120, 0.5, 0.45)`): rich medium green. Base at y=520. A subtle hue shift is added: `hsl(120 + wave * 20, ...)` where `wave = sin(time * 0.5 + hx * 0.01) * 0.05` — this causes the mid hills to very slowly shift their green hue, as if sunlight is moving across the grass.
- **Near meadow** (`hsl(100 + sin(hx * 0.01 + time * 0.3) * 15, 0.6, 0.4)`): warm, bright green. The hue shifts per column AND with time, making the near grass shimmer in warm and cool green tones like real sunlit grass in the wind.

### Moving Butterfly

```lua
while (bf < 4) {
    let bf_cx = 200 + bf * 200 + sin(time * 1.2 + bf * 3) * 80
    let bf_cy = 400 + sin(time * 0.8 + bf * 2.5) * 60 + cos(time * 1.5 + bf) * 30
    let wing_flap = abs(sin(time * 8 + bf * 4)) * 6
    let bf_col = hsl(bf * 70 + 30 + sin(time + bf) * 20, 0.9, 0.6)

    draw_circle(bf_cx - 4, bf_cy, wing_flap, [...])  # left wing
    draw_circle(bf_cx + 4, bf_cy, wing_flap, [...])  # right wing
    draw_rectangle(bf_cx - 1, bf_cy - 3, 2, 6, [40, 30, 20, 255])  # body
```

**Position:** Each butterfly moves along a **Lissajous-like path** — a compound curve made by adding together sine and cosine waves at different frequencies. The X position uses `sin(time * 1.2)` and Y uses `sin(time * 0.8)` + `cos(time * 1.5)`. Because 1.2, 0.8, and 1.5 are not simple integer ratios, the path never exactly repeats — each butterfly traces a continuously varying loop through the sky. Different butterflies have different phase offsets (`bf * 3`, `bf * 2.5`, etc.) so they all fly independently.

**Wing flapping:** `wing_flap = abs(sin(time * 8 + bf * 4)) * 6`. The `abs()` of a sine wave creates a shape that goes 0 → 1 → 0 → 1 (always positive, never negative), oscillating at double the apparent frequency of the sine. At `time * 8` this is 8 / (2π) ≈ 1.27 oscillations per second, which looks like a natural flapping rate. The radius of the two wing circles grows and shrinks between 0 and 6px, making the wings appear to open and close symmetrically on both sides of the body.

**Color:** `hsl(bf * 70 + 30 + sin(time + bf) * 20, ...)` — Each butterfly is a different hue (0, 70, 140, 210° → orange, yellow-green, cyan, blue-purple), with a slow color cycle of ±20° added over time.

**Body:** A tiny 2×6px dark rectangle centered between the two wing circles.

### Grass Blades

```lua
let sway = sin(time * 2.5 + gx * 0.02 + g * 0.5) * 4
draw_line(gx, g_base_y, gx + sway, g_base_y - g_height, 2, [...])
```

100 grass blades are drawn as lines from their base on the hill up to a top point that's shifted horizontally by `sway`. The `gx * 0.02` spatial phase offset means adjacent blades lean in slightly different directions, and the overall effect is a rippling wind across the meadow as `time` advances.

### Wildflowers

40 flowers each have a stem (a line) and a head (a circle). Stem height varies per flower using `sin(f * 2.3)`. Five hue types cycle: red (350°), yellow (45°), purple (280°), orange (25°), blue (200°). Each flower sways on the wind using the same `sin(time * 2.0 + fx * 0.02 + f * 0.8) * 3` formula. A small yellow center dot is drawn on top of every flower head regardless of petal color.

### Floating Pollen

25 small white-yellow circles (radius 2) drift around the meadow using:
```lua
let p_drift_x = sin(time * 0.7 + p * 1.3) * 30
let p_drift_y = sin(time * 0.5 + p * 0.9) * 20
```
Each pollen particle has a unique phase (`p * 1.3`, `p * 0.9`) so they all drift independently. Their alpha pulses gently. They don't travel across the screen — they bob in place — but the combination of 25 particles all bobbing at different phases in different directions creates a convincing impression of particles floating in the air.

---

## 7. `eye.lu` — The All-Seeing Eye

### Overview

An interactive psychedelic eyeball at the center of the screen that tracks your mouse. Clicking triggers a "chaos" effect that shakes the pupil, flashes the background, and sends geometry haywire.

### Background

```lua
let bg_hue = 260.0 + sin(time * 0.3) * 20.0
let bg_col = hsl(bg_hue, 0.6, 0.05)
```

The background is an extremely dark purple-violet (hue 260°, lightness 0.05 = 5% brightness). The hue slowly oscillates ±20° with `sin(time * 0.3)`, drifting between blue-purple and red-purple at a very subtle rate. When a click happens, `bg_flash` is set to 10 and counts down, drawing a brightening grey flash each frame — simulating a camera flash or shock effect.

### Concentric Pulsating Rings

```lua
while (ring < 12) {
    let r_radius = 80 + ring * 60 + sin(time * 1.5 + ring * 0.8) * 20
    let r_hue = 270.0 + ring * 15.0 + time * 20.0
```

12 rings expanding outward. Each ring's radius oscillates independently (`ring * 0.8` phase offset). The hue of each ring shifts: `ring * 15.0` gives each ring a different hue (0° to 165° spread), and `time * 20.0` makes all rings cycle through the color spectrum continuously. Because `time * 20.0` changes every frame, the entire ring system slowly color-cycles.

### The Sclera (White of the Eye)

```lua
let sy = -1.0
while (sy <= 1.0) {
    let ew = eye_w * sqrt(1.0 - sy * sy)
    let draw_y = eye_cy + sy * eye_h / 2
    draw_rectangle(to_int(eye_cx - ew/2), to_int(draw_y) - 2, to_int(ew), 4, [...])
    sy = sy + 0.05
}
```

An ellipse is approximated by drawing many thin horizontal rectangles. The width at each vertical position uses the **ellipse equation**: `x = a * sqrt(1 - (y/b)²)`, where `a = eye_w` and `b = eye_h/2`. This is the parametric form of an ellipse — for each normalized y coordinate `sy` from -1 to 1, the half-width at that point is `eye_w * sqrt(1 - sy²)`. Luna doesn't have a native draw_ellipse function, so this is how we build one manually.

`eye_w = 220 + sin(time * 1.2) * 10 + chaos * 30` — the eye *breathes*: its width gently oscillates. When chaos is high (post-click), it bulges dramatically.

### Mouse Tracking (Iris/Pupil Following)

```lua
let look_dx = mx - eye_cx
let look_dy = my - eye_cy
let look_dist = sqrt(look_dx * look_dx + look_dy * look_dy)
let max_offset = 50
if (look_dist > max_offset) {
    look_dx = look_dx * max_offset / look_dist
    look_dy = look_dy * max_offset / look_dist
}
```

The vector from eye center to mouse is computed. If the mouse is farther than 50px away, the vector is **normalized** and scaled to 50px: `look_dx = look_dx * max_offset / look_dist`. This is standard vector clamping — dividing by the distance gives a unit vector, then multiplying by `max_offset` gives a capped-length vector. The iris center position is then `eye_center + clamped_vector`, so the iris moves in the direction of the mouse but never shifts more than 50px from center.

### Chaos and Pupil Shake

```lua
if (chaos > 0.5) {
    pupil_shake_x = sin(time * 47) * chaos * 15
    pupil_shake_y = cos(time * 53) * chaos * 15
}
```

When chaos is active, high-frequency sine/cosine oscillations (47 and 53 radians/unit — chosen for being mutually prime and fast) create a violent jitter. The different frequencies for X and Y ensure the shake is not a simple back-and-forth but a chaotic Lissajous figure.

`chaos = 3.0` on click, then decays by `dt * 2.0` per frame — so it lasts about 1.5 seconds.

### Iris Rings

```lua
while (ir < 8) {
    let i_radius = iris_r - ir * 7
    let i_hue = 120.0 + ir * 20.0 + sin(time * 3) * 40 + chaos * 100
    let i_col = hsl(i_hue, 0.9, 0.3 + ir * 0.05)
    draw_circle(iris_x, iris_y, i_radius, [...])
```

8 concentric circles from `iris_r` (65px) down to `iris_r - 7*7 = 16px`, stepping 7px each. Each ring has a different hue offset (`ir * 20°`) creating a rainbow iris effect. The `sin(time * 3) * 40` makes all rings slowly cycle in hue together. When chaos > 0, `chaos * 100` jumps the hue by up to 300°, making the iris go psychedelic colors.

### Eyelids

```lua
let lid_close = sin(time * 0.4) * 0.1 + chaos * 0.3
let lid_top_h = to_int(40 + lid_close * 100)
```

Two large dark rectangles — one above, one below the eye. Their height varies with `lid_close`: normally they just slightly slide at 0.4 Hz (a slow blink rhythm). When chaos spikes, `chaos * 0.3` adds up to 0.9 to `lid_close`, nearly closing the eye. This creates the effect of the eye squinting or blinking in reaction to the click.

### Veins

6 red lines radiate outward from the sclera edge using trigonometry:
```lua
let v_angle = v * 1.047 + sin(time * 0.7 + v) * 0.3
let vx1 = eye_cx + cos(v_angle) * (eye_w / 2 - 10)
let vy1 = eye_cy + sin(v_angle) * (eye_h / 2 - 10)
let vx2 = eye_cx + cos(v_angle) * (eye_w / 2 + v_len)
```

`v * 1.047` = `v * π/3` evenly spaces 6 veins at 60° apart. The angles slowly wobble with `sin(time * 0.7 + v) * 0.3`. The start point is on the sclera edge, end point extends outward. They pulse in alpha with `sin(time + v) * 40`.

### Cycling Text

```lua
let msg_idx = to_int(time * 0.3) % 5
```

`time * 0.3` grows continuously. Converting to int and modding by 5 gives a value that steps 0→1→2→3→4→0 approximately every 3.3 seconds. This indexes into five creepy messages. Color cycles through hues with `hsl(time * 30, 0.8, 0.6)`.

---

## 8. `paint.lu` — The Drawing App

### Architecture

`paint.lu` is the most complex file in the folder — a fully functional drawing application. It uses a **render texture** as a persistent off-screen canvas that survives between frames.

```lua
canvas = load_render_texture(WIDTH, HEIGHT)
begin_texture_mode(canvas)
clear_background(245, 243, 240, 255)
end_texture_mode()
```

`load_render_texture` creates a GPU-side framebuffer. Anything drawn in `begin_texture_mode / end_texture_mode` goes onto this invisible texture rather than the screen. The texture persists between frames — unlike the screen which is cleared every frame. This is how drawing "sticks": each stroke is committed to the render texture once, and then displayed by `draw_render_texture(canvas, 0, TB_H)` every frame.

### The Brush Interpolation

```lua
let dx = mx - last_x
let dy = draw_y - last_y
let dist = sqrt(dx*dx + dy*dy)
let step = brush_size / 2.0
let steps = dist / step
let si = 0.0
while (si < steps) {
    let t = si / steps
    let cx = last_x + dx * t
    let cy = last_y + dy * t
    draw_circle(cx, cy, brush_size, draw_col)
    si = si + 1.0
}
```

If the mouse moves fast, there could be a large gap between the last drawn position and the current one. Drawing a single circle at the current position would leave dotted lines. Instead, we compute the vector `(dx, dy)` from last position to current, calculate the distance, then **interpolate** along that vector by placing circles every `brush_size/2` pixels. `t = si / steps` gives a linear interpolation parameter from 0 to 1. `cx = last_x + dx * t` computes a point along the line. This fills in the stroke completely regardless of mouse speed — the standard technique for smooth digital brush strokes.

### Rainbow Mode

```lua
rainbow_hue = rainbow_hue + 2.0
if (rainbow_hue > 360.0) { rainbow_hue = 0.0 }
...
draw_col = hsl(rainbow_hue, 1.0, 0.55)
```

`rainbow_hue` advances 2 degrees per frame, cycling through 0-360° = all hues. At 60 FPS this cycles a full rainbow every 3 seconds. The hue is converted to a fully saturated color via `hsl()`, and that color is used for each drawn circle. Because `rainbow_hue` is global and increments every frame (not just when drawing), the color is always advancing, so even pausing your stroke and starting again picks up a new color.

### Color Spectrum Bar

The top toolbar includes a clickable color picker rendered as six gradient segments:
```lua
draw_gradient_ex(spec_x,           spec_y, seg_w, spec_h, C_R, C_R, C_Y, C_Y)
draw_gradient_ex(spec_x + seg_w,   spec_y, seg_w, spec_h, C_Y, C_Y, C_G, C_G)
...
```

Six `draw_gradient_ex` calls cover R→Y, Y→G, G→C, C→B, B→M, M→R — the full hue wheel spread across a horizontal bar. When clicked, the position along the bar maps to a hue:
```lua
let pct = (mx - spec_x) / spec_w
let hue = pct * 360.0
brush_col = hsl(hue, 1.0, 0.5)
```

### Save and Clear

- `take_screenshot("luna_art.png")` — calls Raylib's `TakeScreenshot`, which writes the current screen (not the canvas texture) to a PNG file. We can also trigger it with `KEY_S`.
- Clear: `begin_texture_mode(canvas)` + `clear_background(245, 243, 240, 255)` + `end_texture_mode()` — fills the canvas texture with the off-white background color, effectively erasing everything.

### Palette Selection Detection

```lua
let dx2 = mx - sw_x
let dy2 = my - sw_y
if (dx2*dx2 + dy2*dy2 <= sw_r*sw_r) {
```

The palette circles are detected using a **point-in-circle test**: distance squared from mouse to circle center ≤ radius squared. Using `dx2*dx2 + dy2*dy2` avoids a `sqrt()` call, which is faster.

---

## 9. `sand.lu` — The Falling Sand Simulation

### Why We Made sand.lu

This was built to demonstrate that Luna can interface with native C plugins — not just Raylib. The falling sand simulation (where particles of sand, water, acid, stone, and fire interact physically) requires updating tens of thousands of cells per frame. Doing this in pure Luna (an interpreted language) would be too slow. So the physics engine was written as a native C plugin (`sand_lib.c`) compiled into Luna, and `sand.lu` is the Luna-side front-end that handles input, rendering, and UI.

### The Grid

```lua
let GRID_W = 200
let GRID_H = 150
let CELL_SIZE = 4
```
The simulation runs on a 200×150 grid. Each cell is 4×4 pixels on screen, giving a 800×600 display. The grid is managed entirely in C (inside `sand_init`, `sand_set`, `sand_get`, `sand_update`).

### sand_init(), sand_set(), sand_get(), sand_update()

These are **native C functions** registered into Luna's environment. From Luna's perspective they look like any other built-in function, but they actually call compiled C code directly:
- `sand_init()` — allocates the grid in C memory and zeros it out
- `sand_set(x, y, type)` — sets a cell to a particle type (0=empty, 1=sand, 2=water, 3=stone, 4=acid, 5=fire)
- `sand_get(x, y)` — returns the type of a cell
- `sand_update()` — runs one full physics step across the entire grid

### The Physics Logic (in C, called from Luna)

**Sand:** Falls straight down if the cell below is empty. If directly below is blocked, tries to fall diagonally (down-left or down-right). This gives sand its characteristic pile-up behavior.

**Water:** Falls down if possible. If blocked below, spreads sideways left or right — simulating liquid flowing outward. Water has a chance to spread multiple cells in one step, making it flow faster than sand falls.

**Stone:** Does not move at all. It's a permanent solid barrier.

**Acid:** Falls and spreads like water, but additionally has a chance each frame to *destroy* any adjacent non-empty, non-acid cell it touches — simulating chemical dissolution.

**Fire:** Rises upward (opposite of sand/water — fire has negative effective gravity). Randomly spreads to adjacent empty cells. Has a lifespan and disappears after some ticks. Can ignite other materials.

### Mouse Drawing

```lua
let r = brush_size
let by = -r
while (by <= r) {
    let bx = -r
    while (bx <= r) {
        if (rand(100) < 50) { sand_set(gx + bx, gy + by, current_type) }
```

A square brush of radius `brush_size` is applied around the mouse grid cell. The `rand(100) < 50` check means only ~50% of cells in the brush area get set per frame. This creates a **dithered spray** effect rather than a solid block, making it feel more like pouring material than stamping it.

Right-click erases by setting cells to `TYPE_EMPTY` without the 50% check — erasing is deterministic.

### Rendering

```lua
let type = sand_get(dx, dy)
if (type != 0) {
    draw_rectangle(dx * CELL_SIZE, dy * CELL_SIZE, CELL_SIZE, CELL_SIZE, col)
}
```

Only non-empty cells are drawn — empty cells are just the dark background. Each cell is a 4×4 rectangle colored by type. The grid is iterated in row-major order (outer loop = rows, inner = columns) to minimize cache misses.

---

## 10. `galaxy.lu` — The Milky Way

### Background Stars

500 stars are generated at startup using `rand()` for positions. Unlike the aurora's deterministic hash, here we use the actual PRNG because the galaxy doesn't need reproducible positions — it just needs scatter. Each star gets a random brightness `bg_stars_b[i]` between 50 and 200.

**Twinkle:**
```lua
let twinkle = bg_stars_b[i] + (sin(get_delta_time() * 10.0 + i) * 50)
```

Note: this uses `get_delta_time()` (the time for the last frame) not a global accumulating `time`. This is a minor bug/quirk: `get_delta_time()` at 60FPS ≈ 0.0167, so `0.0167 * 10 * i` for star 100 is only 16.7 radians. The twinkle is present but the effect is subtler than intended. Regardless, the sin value varies per star index `+ i`, giving each star a unique offset.

### Galaxy Particle Generation

**12,000 particles** are generated at startup in polar coordinates (angle, distance from center).

```lua
let arm = rand(0, num_arms)
let base_angle = (6.28318 / num_arms) * float(arm)
let distance_factor = pow(rand(), 2.5)
let r = distance_factor * max_radius
let spiral_offset = r * 0.012
let noise = (rand() - 0.5) * arm_spread * (1.0 + distance_factor * 2.0)
append(g_dist, r)
append(g_angle, base_angle + spiral_offset + noise)
```

**Why `pow(rand(), 2.5)` for distance?** `rand()` returns a uniform 0-1 value. Raising it to the power 2.5 *skews* the distribution heavily toward 0 — most particles end up near the center (small `r`), with fewer particles at large radii. This matches real galaxy structure: dense bright core, sparser outer arms.

**Spiral offset:** `spiral_offset = r * 0.012` — the further from the center, the more the angle is offset. Stars far from the core are rotated further along the arm, creating the characteristic trailing spiral shape. This is a linearized approximation of differential orbital rotation (faster at center, slower at edge).

**Noise:** `(rand() - 0.5) * arm_spread * (1.0 + distance_factor * 2.0)` — random angular scatter that increases with distance. The outer arms are "fuzzy" and loose, the core is tight and defined.

**Orbital speed:** `speed = 0.04 / (sqrt(r) + 0.5)` — approximates Keplerian orbital mechanics where inner orbits are faster. Stars close to the center orbit faster, outer stars slower, making the spiral arms wind and unwind over time.

### Per-Frame Orbital Update

```lua
g_angle[i] = g_angle[i] + g_speed[i]
let x = cx + cos(g_angle[i]) * g_dist[i]
let y = cy + sin(g_angle[i]) * g_dist[i]
y = cy + (y - cy) * 0.7
```

Each frame, each particle's angle advances by its speed. Converting polar to Cartesian gives screen position. The Y coordinate is scaled by 0.7 (`y = cy + (y - cy) * 0.7`) — this squashes the galaxy vertically to simulate viewing it at an angle from above (like seeing the Milky Way as an oval disc rather than a perfect circle).

### Nebula Colors

```lua
if (r_val > 0.90) { rr = 255; gg = 100; bb = 200 } # Pink dust
else if (r_val > 0.85) { rr = 100; gg = 200; bb = 255 } # Cyan dust
```

10% of stars are pink nebula dust, 5% are cyan nebula dust. The rest are in the blue-white gradient based on core distance. These clusters of pink/cyan particles visually create the nebula regions visible in real galaxy images.

### Galactic Core Bloom

```lua
draw_circle(cx, cy, 100, [255, 240, 200, 3])
draw_circle(cx, cy, 60, [255, 250, 220, 5])
draw_circle(cx, cy, 30, [255, 255, 255, 10])
draw_circle(cx, cy, 10, [255, 255, 255, 200])
```

Four large nearly-transparent circles create a soft bloom glow at the galactic center, simulating the concentrated starlight of a galactic bulge. The outermost is barely visible (alpha 3), building up to a bright white core (alpha 200). This is the same layered-circle bloom technique used for the sun in meadow.lu.

---

## 11. `ripples.lu` — Hypnotic Ripples

### The Grid

```lua
let spacing = 40
let cols = 1024 / spacing
let rows = 768 / spacing
```

A grid of dots is created, 40px apart — 26 columns × 20 rows = 520 dots total.

### The Ripple Formula

```lua
let dx = mx - cx
let dy = my - cy
let dist = sqrt(dx*dx + dy*dy)
let angle = (dist * 0.02) - time
let rain = (0.5 + sin(angle) * 0.5)
let radius = rain * (spacing * 0.8)
```

For each grid dot, the distance to the mouse is computed. Then `angle = dist * 0.02 - time` creates an outward-propagating wave: the `dist * 0.02` term means dots farther from the mouse are at a different phase of the wave, and subtracting `time` makes the whole pattern advance outward over time (the peaks travel away from the mouse).

`rain = (0.5 + sin(angle) * 0.5)` maps the sine output (-1 to 1) into (0 to 1) — so radius is always positive, ranging from 0 to `spacing * 0.8 = 32px`.

**Color:**
```lua
let r = to_int((sin(angle) * 100) + 127)
let g = to_int((cos(angle) * 100) + 127)
let b = 255
```

R uses sine of the angle, G uses cosine (90° phase offset). At any phase, R and G are out of sync, cycling through various combinations of red-green levels while blue stays constant. The result cycles between cyan (G=227, R=27), purple (R=227, G=27), teal, magenta — a hypnotic shifting palette.

**Mouse proximity size boost:**
```lua
if (dist < 200) {
    radius = radius + (200 - dist) * 0.1
}
```

Dots within 200px of the mouse are boosted in size by up to 20px, creating the effect of waves being larger at the source.

---

## 12. `car_retro.lu` — Neon Retro Racer

### Why This File Exists

This is the most fully-featured demo — a complete playable arcade game. It demonstrates that Luna can handle game loops, state machines, physics, collision detection, AI, scoring systems, and UI all in one file.

### Scrolling Grid (Retro Perspective Effect)

```lua
grid_offset = grid_offset + (game_speed * 0.5) * dt
if (grid_offset >= 40.0) { grid_offset = 0.0 }
let gy = grid_offset
while (gy < SCREEN_HEIGHT) {
    draw_line(0, to_int(gy), SCREEN_WIDTH, to_int(gy), [C_GRID, alpha])
    gy = gy + 40.0
}
draw_line(0, SCREEN_HEIGHT, to_int(SCREEN_WIDTH/2 - 50), 0, [C_GRID, 50])
draw_line(SCREEN_WIDTH, SCREEN_HEIGHT, to_int(SCREEN_WIDTH/2 + 50), 0, [C_GRID, 50])
```

Horizontal lines scroll downward by advancing `grid_offset` each frame. When it reaches 40px (the grid spacing), it wraps to 0, creating an infinite scroll illusion. The two diagonal lines converge toward a vanishing point near the top center — this is a classic **fake perspective** technique that gives the 2D scene a sense of 3D depth (as if looking down a road receding into the distance).

### Smooth Player Movement

```lua
target_x = ROAD_X + player_lane * LANE_WIDTH + (LANE_WIDTH - CAR_WIDTH) / 2
let dx = target_x - player_x
player_x = player_x + dx * 10.0 * dt
```

The player snaps to discrete lanes (0, 1, 2) but the visual position lerps smoothly toward the target. `player_x = player_x + dx * 10.0 * dt` is **exponential smoothing** — each frame, 10 * dt fraction of the remaining gap is closed. At 60FPS, `dt ≈ 0.0167`, so `10 * 0.0167 = 0.167` — 16.7% of the gap is closed each frame. This gives a fast, snappy but smooth slide between lanes.

### Traffic Spawning and Pooling

```lua
let POOL_SIZE = 20
let t_active = [0, 0, 0, 0, ...]
```

20 traffic car slots are pre-allocated. Instead of creating/destroying objects (which requires memory allocation), we reuse pool slots. When a car is needed, we find an inactive slot (`t_active[i] == 0`) and activate it. When a car goes off-screen (`t_y[i] > SCREEN_HEIGHT + 100`), it's deactivated. This **object pooling** pattern avoids memory fragmentation and is standard in game development.

Spawn rate: a timer counts down based on difficulty. On tick, 1-2 cars are spawned in random lanes.

### Traffic AI Lane Switching

```lua
if (rand(1000) < chance) {
    if (ln < player_lane) { new_lane = ln + 1 }
    else { if (ln > player_lane) { new_lane = ln - 1 } }
}
```

Each frame, active enemy cars roll a random number. If it falls below a difficulty-dependent threshold, the car switches lanes — biased toward the player's lane. This creates the feeling of enemies "chasing" the player. Easy mode: 0.1% chance. Hard/Nightmare: 1.0% chance. The cars then smoothly lerp to their new lane target using the same exponential smoothing as the player.

### Collision Detection

```lua
func check_collision(px, py, pw, ph, ex, ey, ew, eh) {
    if (px < ex + ew && px + pw > ex && py < ey + eh && py + ph > ey) {
        return 1
    }
    return 0
}
```

Standard **Axis-Aligned Bounding Box (AABB)** test. Two rectangles overlap if and only if neither is fully to the left, right, above, or below the other. The player's hitbox is slightly smaller than the visual car (`+5, +5, -10, -10`) to give a small margin of forgiveness.

### Difficulty Scaling

```lua
game_speed = speed_base + (score * inc)
if (game_speed > 2500.0) { game_speed = 2500.0 }
```

Speed increases linearly with score, capped at 2500. `inc` varies by difficulty (0.5 for Easy, 4.0 for Nightmare). The score itself grows over time (`score = score + (400.0 * 0.01) * dt`), so as the game goes on, both score and speed grow together. On Nightmare, speed can reach the cap in about 30 seconds. A 2× score bonus is awarded on Nightmare if you crash (rewarding the attempt).

### High Score Tracking

Three high scores are stored in a sorted list using a manual insertion-sort approach — the new score is compared against each slot in order and inserted where it belongs, pushing others down. This persists in memory only for the session (no file I/O).

### State Machine

The game has three states: `STATE_MENU`, `STATE_PLAY`, `STATE_GAMEOVER`. Each frame the code checks the current state and runs the appropriate update and draw logic. Transitions are triggered by keypresses (Enter/Space to play, Escape to menu, crash to game over). The background grid and road always draw regardless of state — only the foreground content changes.

---

## 13. `music.lu` — The Audio Visualizer

### Architecture: How it Talks to the Speaker
This is the most technically complex part of the Luna engine. The visualizer doesn't just animate to a timer; it reacts to real-time PCM audio data.

1. **Callback Attachment**: When `load_music_stream()` is called, the C backend (`gui_lib.c`) attaches an **Audio Stream Processor**. This registers a callback that runs on a dedicated high-priority audio thread.
2. **The Ring Buffer**: Every time a batch of audio frames is played, the callback averages the stereo samples to mono and writes them into a **2048-float circular ring buffer**.
3. **Snapshotting**: When the Luna script calls `get_music_fft()`, the C bridge snapshots the current state of the ring buffer into a local array. This prevents "data races" where the audio thread is writing while the interpreter is reading.

### The Math: Discrete Fourier Transform (DFT)
To turn a raw pressure wave (PCM) into frequency bars, we perform a **Direct DFT** on 32 narrow frequency bands.

**Logarithmic Spacing**: Human hearing is logarithmic (octaves). If we used a linear scale, half the bars would be high-pitched hiss. Instead, we space the 32 target frequencies exponentially from 60Hz to 16kHz so each bar represents one "musical" step.

**Correlation Loop**: For each of the 32 bands, we run this correlation in C:
```c
for (int n = 0; n < N; n++) {
    float angle = 2.0f * M_PI * band_freqs[b] * n / sample_rate;
    real += buffer[n] * cosf(angle);
    imag += buffer[n] * sinf(angle);
}
magnitude = sqrtf(real*real + imag*imag);
```
This is essentially "tuning" 32 virtual radios to specific frequencies. If the audio contains that frequency, the `magnitude` will be high. These 32 values are then returned to Luna to set the bar heights.

---

## 14. `boobs.lu` — Soft Body Physics

### Keyword Aliasing
This demo is famous for its use of Luna's **keyword aliasing** feature. It uses `balls`, `grab_balls`, and `spin_balls` which map directly to `let`, `func`, and `while` at the lexer level. This is purely a "skin" on the language to show off its flexibility.

### Spring-Mass System
The physics is a **discrete spring-mass system**. Each body has a center point and 18 skin points.
- **Structural Springs**: Connect the center to each skin point. They use Hooke's Law (`F = kx`) to pull points back to a rest radius, maintaining the body's circular shape.
- **Shear Springs**: Connect adjacent skin points. These resist deformation *along* the surface, preventing the points from bunching up.
- **Anchor Forces**: A soft spring pulls the center toward a resting height, allowing the body to bounce but eventually settle.

---

## 15. `vortex.lu` & `blackhole.lu` — Cosmic Physics

### The Vortex (`vortex.lu`)
Runs 18,000 particles using a combination of:
- **Central Gravity**: Pulls everything toward `(0,0)`.
- **Tangential Forces**: Rotates particles around the center.
- **Brownian Noise**: Adds random jitter to simulate turbulence.
- **Spiral Dynamics**: The tangential force is modulated by distance to keep the spiral arms defined rather than collapsing into a disk.

### The Black Hole (`blackhole.lu`)
Approximates the visual appearance of a **Schwarzschild black hole** via coordinate warping:
- **Gravitational Lensing**: Particles behind the singularity have their screen coordinates pushed outward and split into circular arcs, simulating an **Einstein Ring**.
- **Doppler Brightening**: Relative brightness (alpha) is modulated based on whether a particle's velocity is moving toward or away from the viewer, simulating relativistic optical shifts.

---

## Summary Table

| File | Core Technique | Key Math |
|------|---------------|----------|
| `aurora.lu` | Vertical ray curtains | Compound sine for wave top, quadratic fade, HSL color |
| `aurora_wave.lu` | Horizontal strip curtains | Sine X-offset per row, lake reflection ripple |
| `meadow.lu` | Layered hills + butterfly Lissajous | Compound sine for terrain, abs(sin) for wing flap |
| `eye.lu` | Ellipse via horizontal strips | Ellipse equation, vector normalization, chaos decay |
| `paint.lu` | Render texture canvas | Linear interpolation between mouse points |
| `sand.lu` | Native C physics plugin | Cellular automaton rules, 50% spray brush |
| `galaxy.lu` | Polar-to-Cartesian orbits | pow(rand, 2.5) distribution, Keplerian speed falloff |
| `ripples.lu` | Propagating wave field | dist-based sine phase, angle → RGB color |
| `car_retro.lu` | Game state machine | Exponential position smoothing, AABB collision, object pool |
| `music.lu` | Logarithmic DFT | Discrete Fourier Transform, mono ring buffer |
| `boobs.lu` | Soft body physics | Structural/Shear springs (Hooke's Law) |
| `blackhole.lu`| Gravitational lensing | Coordinate warping, Einstein Ring approximation |

Every single animation, effect, and interaction in this folder is built from the same small set of mathematical primitives — `sin()`, `cos()`, `sqrt()`, `lerp`, and careful use of time and position as phase offsets. The art is in how they're combined.

---

## Creative Credits & Legal Disclaimer

I do not own any of the songs or assets used in these demonstration projects. All rights belong to their respective artists. The music and assets are used here for educational and demonstrative purposes only. To the best of my knowledge, I have provided the names and sources for all assets used.

### Music Tracks Used:
- **`aurora.lu` / `aurora_wave.lu`**: [はじまりの曲](https://www.youtube.com/watch?v=tOq_kHo6jNc)
- **`eye.lu`**: [Goth](https://www.youtube.com/watch?v=KJoYBw5tJOc&list=PLxudKeZzeWR1SdMxcGlCbU-vC-lCGi8Ot&index=94) (Slowed + Reverb)
- **`meadow.lu`**: [グッド・ドクター](https://www.youtube.com/watch?v=P6kQNA2Zj40)
- **`car_retro.lu`**: [Nightcall](https://www.youtube.com/watch?v=MV_3Dpw-BRY) (by Kavinsky / *kavinsky.mp3*)
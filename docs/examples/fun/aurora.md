# Aurora (`aurora.lu`, `aurora_wave.lu`)

## `aurora.lu` — The Northern Lights (Vertical Ray Style)

This file renders a full Northern Lights scene with a night sky, stars, moon, three layers of aurora curtains rendered as vertical rays, a two-layer mountain silhouette, a treeline, and a dark ground strip.

### The Time Variable

```javascript
let time = 0.0
...
time = time + 0.008
```

`time` is the heartbeat of every animation in this file. It advances by 0.008 every frame. At 60 FPS that means time grows by `0.008 * 60 = 0.48` per second. Every `sin()` and `cos()` call in the file uses `time` as its argument (or a scaled version of it), which causes those functions to smoothly oscillate back and forth, driving all movement.

### The Night Sky Gradient

```javascript
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
```javascript
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

## `aurora_wave.lu` — The Northern Lights (Horizontal Wave Style)

This file is a different approach to the same aurora scene. The sky, stars, moon, and mountains are identical to `aurora.lu`. What changes is **how the aurora is drawn**: instead of vertical rays, it uses horizontal strips with a sinusoidal X offset — making the aurora look like flowing horizontal bands that wave side to side.

### How the Wave Pattern Works

```javascript
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

```javascript
let pct = (ay - 60.0) / 420.0
let a_alpha = to_int((1.0 - pct * pct) * 120)
```

`pct` goes from 0 (top of aurora) to 1 (bottom). `1.0 - pct * pct` is a downward parabola — bright at top, fading at bottom. Squaring makes the fade non-linear: the aurora stays bright through most of its height and only drops to transparent near the very bottom. Maximum alpha is 120 (semi-transparent), keeping the aurora ethereal rather than opaque.

### The Lake Reflection (Unique to aurora_wave.lu)

```javascript
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

## How Mountains Work (Both Aurora Files)

Both `aurora.lu` and `aurora_wave.lu` use the exact same mountain technique. Mountains are drawn as **column-by-column rectangle strips**, where the height of each strip is determined by sine waves.

### Layer 1 — Far Mountains (Lighter Color, Higher Up)

```javascript
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

```javascript
let peak2 = sin(mx * 0.006 + 0.5) * 80 + sin(mx * 0.02 + 3.0) * 40
let mtn_y = 570 - peak2
draw_rectangle(mx, to_int(mtn_y), 4, to_int(mtn_h), [8, 10, 18, 255])
```

Different spatial frequencies (0.006 and 0.02) and different phase offsets (0.5, 3.0) mean this layer has a completely different profile from layer 1 — they won't mirror each other. The base elevation is 570px instead of 520px, so this layer is **lower on screen and thus appears closer** (following perspective: things closer to you sit lower). The color `[8, 10, 18]` is darker — nearly pure black — reinforcing that these are nearer, more silhouetted mountains blocking the sky.

**Why two layers?** Atmospheric perspective: distant mountains appear lighter and more blue-grey because light scatters through the atmosphere. Near mountains are dark silhouettes. Two layers with different shades and different profiles creates convincing depth.

### The Treeline

```javascript
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

## How Stars Twinkle (Both Aurora Files)

### The Deterministic Position Trick

```javascript
let sx = (s * 137 + 53) % 1024
let sy = (s * 97 + 29) % 500
```

We need 80 stars at fixed positions — but we have no random seed available at this stage, and we want the positions to be deterministic (same every run). So instead of random numbers, we use **linear congruential hashing**: multiplying the star index `s` by a prime number (137, 97) and adding an offset, then modding by the screen dimensions. The primes ensure the distribution doesn't cluster — they're coprime to 1024 and 500, so the values spread evenly across the range. This is a classic trick for seeding fake randomness with no PRNG needed.

### The Twinkling Formula

```javascript
let twinkle = sin(time * (2.0 + s * 0.3) + s * 1.7)
let star_alpha = to_int(120 + twinkle * 130)
let star_r = 1.5 + twinkle * 0.8
```

Each star has a unique twinkling frequency: `time * (2.0 + s * 0.3)`. Star 0 oscillates at speed 2.0, star 1 at speed 2.3, star 10 at speed 5.0, and so on. The `+ s * 1.7` term adds a unique phase offset per star, so they don't all pulse in sync. The result is 80 stars all independently brightening and dimming at different rates — exactly like real stars.

Alpha ranges from 120 - 130 = -10 (clamped to 30) to 120 + 130 = 250, clamped to 255. Radius ranges from 1.5 - 0.8 = 0.7 (clamped to 0.8) to 1.5 + 0.8 = 2.3. The star literally grows and shrinks with its brightness, which is more visually convincing than just alpha alone.

### Bright Stars with Glow

```javascript
draw_circle(bx, by, 8, [200, 220, 255, glow_alpha])  # soft glow
draw_circle(bx, by, 3, [255, 255, 255, to_int(200 + glow * 50)])  # bright core
```

Each of the 6 bright stars is two concentric circles: a large dim blue circle (radius 8, low alpha) for the soft atmospheric glow, and a small white circle (radius 3, high alpha) for the sharp bright core. This two-circle technique is a simple substitute for a proper bloom/glow shader.

---

[Back to fun/ examples](README.md)

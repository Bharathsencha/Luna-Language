# Meadow (`meadow.lu`)

## Sky

```javascript
let sky_top = hsl(210.0, 0.7, 0.75)
let sky_mid = hsl(200.0, 0.6, 0.85)
draw_gradient_ex(0, 0, 1024, 768, sky_top, sky_mid, sky_mid, sky_top)
```

Hue 210° is sky blue, lightness 0.75/0.85 makes it bright and airy. This is a daytime sky — light at the top, slightly lighter near the horizon (inverted from the night sky where the horizon is lighter for a different reason). The `draw_gradient_ex` creates the corner-blended effect.

## The Sun

```javascript
draw_circle(sun_x, sun_y, 120, [255, 245, 200, 15])
draw_circle(sun_x, sun_y, 90, [255, 240, 180, 25])
draw_circle(sun_x, sun_y, 65, [255, 235, 160, 40])
draw_circle(sun_x, sun_y, to_int(45 * sun_pulse), [255, 250, 220, 255])
draw_circle(sun_x, sun_y, to_int(40 * sun_pulse), [255, 255, 240, 255])
```

The sun is built from **5 concentric circles** of decreasing radius and increasing alpha. The outer three are large and nearly transparent — they simulate the glow/haze around a bright sun in the sky. The inner two are the bright solid sun disc. `sun_pulse = 0.95 + sin(time * 0.8) * 0.05` makes the disc subtly breathe (±5% size) at a slow rate.

The 12 rays are lines drawn from radius 50 outward to `50 + r_len`, where `r_len = 70 + sin(time * 2 + ray * 1.3) * 20`. Each ray has a unique length that oscillates independently. The angle rotates slowly: `r_angle = ray * 0.524 + time * 0.3` (0.524 radians = 30°, so 12 rays evenly spaced, slowly spinning).

## Clouds

```javascript
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

## Hills (Three Layers)

Three hill layers use the same column-by-column strip technique as the mountains, but for a daylit pastoral scene:

- **Far hills** (`hsl(150, 0.3, 0.65)`): pale, desaturated green. Base at y=480. Frequency 0.005 and 0.012 — very smooth, gentle.
- **Mid hills** (`hsl(120, 0.5, 0.45)`): rich medium green. Base at y=520. A subtle hue shift is added: `hsl(120 + wave * 20, ...)` where `wave = sin(time * 0.5 + hx * 0.01) * 0.05` — this causes the mid hills to very slowly shift their green hue, as if sunlight is moving across the grass.
- **Near meadow** (`hsl(100 + sin(hx * 0.01 + time * 0.3) * 15, 0.6, 0.4)`): warm, bright green. The hue shifts per column AND with time, making the near grass shimmer in warm and cool green tones like real sunlit grass in the wind.

## Moving Butterfly

```javascript
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

## Grass Blades

```javascript
let sway = sin(time * 2.5 + gx * 0.02 + g * 0.5) * 4
draw_line(gx, g_base_y, gx + sway, g_base_y - g_height, 2, [...])
```

100 grass blades are drawn as lines from their base on the hill up to a top point that's shifted horizontally by `sway`. The `gx * 0.02` spatial phase offset means adjacent blades lean in slightly different directions, and the overall effect is a rippling wind across the meadow as `time` advances.

## Wildflowers

40 flowers each have a stem (a line) and a head (a circle). Stem height varies per flower using `sin(f * 2.3)`. Five hue types cycle: red (350°), yellow (45°), purple (280°), orange (25°), blue (200°). Each flower sways on the wind using the same `sin(time * 2.0 + fx * 0.02 + f * 0.8) * 3` formula. A small yellow center dot is drawn on top of every flower head regardless of petal color.

## Floating Pollen

25 small white-yellow circles (radius 2) drift around the meadow using:
```javascript
let p_drift_x = sin(time * 0.7 + p * 1.3) * 30
let p_drift_y = sin(time * 0.5 + p * 0.9) * 20
```
Each pollen particle has a unique phase (`p * 1.3`, `p * 0.9`) so they all drift independently. Their alpha pulses gently. They don't travel across the screen — they bob in place — but the combination of 25 particles all bobbing at different phases in different directions creates a convincing impression of particles floating in the air.

---

[Back to fun/ examples](README.md)

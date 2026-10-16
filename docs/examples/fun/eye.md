# Eye (`eye.lu`)

## Overview

An interactive psychedelic eyeball at the center of the screen that tracks your mouse. Clicking triggers a "chaos" effect that shakes the pupil, flashes the background, and sends geometry haywire.

## Background

```javascript
let bg_hue = 260.0 + sin(time * 0.3) * 20.0
let bg_col = hsl(bg_hue, 0.6, 0.05)
```

The background is an extremely dark purple-violet (hue 260°, lightness 0.05 = 5% brightness). The hue slowly oscillates ±20° with `sin(time * 0.3)`, drifting between blue-purple and red-purple at a very subtle rate. When a click happens, `bg_flash` is set to 10 and counts down, drawing a brightening grey flash each frame — simulating a camera flash or shock effect.

## Concentric Pulsating Rings

```javascript
while (ring < 12) {
    let r_radius = 80 + ring * 60 + sin(time * 1.5 + ring * 0.8) * 20
    let r_hue = 270.0 + ring * 15.0 + time * 20.0
```

12 rings expanding outward. Each ring's radius oscillates independently (`ring * 0.8` phase offset). The hue of each ring shifts: `ring * 15.0` gives each ring a different hue (0° to 165° spread), and `time * 20.0` makes all rings cycle through the color spectrum continuously. Because `time * 20.0` changes every frame, the entire ring system slowly color-cycles.

## The Sclera (White of the Eye)

```javascript
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

## Mouse Tracking (Iris/Pupil Following)

```javascript
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

## Chaos and Pupil Shake

```javascript
if (chaos > 0.5) {
    pupil_shake_x = sin(time * 47) * chaos * 15
    pupil_shake_y = cos(time * 53) * chaos * 15
}
```

When chaos is active, high-frequency sine/cosine oscillations (47 and 53 radians/unit — chosen for being mutually prime and fast) create a violent jitter. The different frequencies for X and Y ensure the shake is not a simple back-and-forth but a chaotic Lissajous figure.

`chaos = 3.0` on click, then decays by `dt * 2.0` per frame — so it lasts about 1.5 seconds.

## Iris Rings

```javascript
while (ir < 8) {
    let i_radius = iris_r - ir * 7
    let i_hue = 120.0 + ir * 20.0 + sin(time * 3) * 40 + chaos * 100
    let i_col = hsl(i_hue, 0.9, 0.3 + ir * 0.05)
    draw_circle(iris_x, iris_y, i_radius, [...])
```

8 concentric circles from `iris_r` (65px) down to `iris_r - 7*7 = 16px`, stepping 7px each. Each ring has a different hue offset (`ir * 20°`) creating a rainbow iris effect. The `sin(time * 3) * 40` makes all rings slowly cycle in hue together. When chaos > 0, `chaos * 100` jumps the hue by up to 300°, making the iris go psychedelic colors.

## Eyelids

```javascript
let lid_close = sin(time * 0.4) * 0.1 + chaos * 0.3
let lid_top_h = to_int(40 + lid_close * 100)
```

Two large dark rectangles — one above, one below the eye. Their height varies with `lid_close`: normally they just slightly slide at 0.4 Hz (a slow blink rhythm). When chaos spikes, `chaos * 0.3` adds up to 0.9 to `lid_close`, nearly closing the eye. This creates the effect of the eye squinting or blinking in reaction to the click.

## Veins

6 red lines radiate outward from the sclera edge using trigonometry:
```javascript
let v_angle = v * 1.047 + sin(time * 0.7 + v) * 0.3
let vx1 = eye_cx + cos(v_angle) * (eye_w / 2 - 10)
let vy1 = eye_cy + sin(v_angle) * (eye_h / 2 - 10)
let vx2 = eye_cx + cos(v_angle) * (eye_w / 2 + v_len)
```

`v * 1.047` = `v * π/3` evenly spaces 6 veins at 60° apart. The angles slowly wobble with `sin(time * 0.7 + v) * 0.3`. The start point is on the sclera edge, end point extends outward. They pulse in alpha with `sin(time + v) * 40`.

## Cycling Text

```javascript
let msg_idx = to_int(time * 0.3) % 5
```

`time * 0.3` grows continuously. Converting to int and modding by 5 gives a value that steps 0→1→2→3→4→0 approximately every 3.3 seconds. This indexes into five creepy messages. Color cycles through hues with `hsl(time * 30, 0.8, 0.6)`.

---

[Back to fun/ examples](README.md)

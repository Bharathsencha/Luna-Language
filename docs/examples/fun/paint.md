# Paint (`paint.lu`)

## Architecture

`paint.lu` is the most complex file in the folder — a fully functional drawing application. It uses a **render texture** as a persistent off-screen canvas that survives between frames.

```javascript
canvas = load_render_texture(WIDTH, HEIGHT)
begin_texture_mode(canvas)
clear_background(245, 243, 240, 255)
end_texture_mode()
```

`load_render_texture` creates a GPU-side framebuffer. Anything drawn in `begin_texture_mode / end_texture_mode` goes onto this invisible texture rather than the screen. The texture persists between frames — unlike the screen which is cleared every frame. This is how drawing "sticks": each stroke is committed to the render texture once, and then displayed by `draw_render_texture(canvas, 0, TB_H)` every frame.

## The Brush Interpolation

```javascript
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

## Rainbow Mode

```javascript
rainbow_hue = rainbow_hue + 2.0
if (rainbow_hue > 360.0) { rainbow_hue = 0.0 }
...
draw_col = hsl(rainbow_hue, 1.0, 0.55)
```

`rainbow_hue` advances 2 degrees per frame, cycling through 0-360° = all hues. At 60 FPS this cycles a full rainbow every 3 seconds. The hue is converted to a fully saturated color via `hsl()`, and that color is used for each drawn circle. Because `rainbow_hue` is global and increments every frame (not just when drawing), the color is always advancing, so even pausing your stroke and starting again picks up a new color.

## Color Spectrum Bar

The top toolbar includes a clickable color picker rendered as six gradient segments:
```javascript
draw_gradient_ex(spec_x,           spec_y, seg_w, spec_h, C_R, C_R, C_Y, C_Y)
draw_gradient_ex(spec_x + seg_w,   spec_y, seg_w, spec_h, C_Y, C_Y, C_G, C_G)
...
```

Six `draw_gradient_ex` calls cover R→Y, Y→G, G→C, C→B, B→M, M→R — the full hue wheel spread across a horizontal bar. When clicked, the position along the bar maps to a hue:
```javascript
let pct = (mx - spec_x) / spec_w
let hue = pct * 360.0
brush_col = hsl(hue, 1.0, 0.5)
```

## Save and Clear

- `take_screenshot("luna_art.png")` — reads the framebuffer with `glReadPixels` and writes the current screen (not the canvas texture) to a PNG file. We can also trigger it with `KEY_S`.
- Clear: `begin_texture_mode(canvas)` + `clear_background(245, 243, 240, 255)` + `end_texture_mode()` — fills the canvas texture with the off-white background color, effectively erasing everything.

## Palette Selection Detection

```javascript
let dx2 = mx - sw_x
let dy2 = my - sw_y
if (dx2*dx2 + dy2*dy2 <= sw_r*sw_r) {
```

The palette circles are detected using a **point-in-circle test**: distance squared from mouse to circle center ≤ radius squared. Using `dx2*dx2 + dy2*dy2` avoids a `sqrt()` call, which is faster.

---

[Back to fun/ examples](README.md)

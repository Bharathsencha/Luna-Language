# Ripples (`ripples.lu`)

## The Grid

```javascript
let spacing = 40
let cols = 1024 / spacing
let rows = 768 / spacing
```

A grid of dots is created, 40px apart — 26 columns × 20 rows = 520 dots total.

## The Ripple Formula

```javascript
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
```javascript
let r = to_int((sin(angle) * 100) + 127)
let g = to_int((cos(angle) * 100) + 127)
let b = 255
```

R uses sine of the angle, G uses cosine (90° phase offset). At any phase, R and G are out of sync, cycling through various combinations of red-green levels while blue stays constant. The result cycles between cyan (G=227, R=27), purple (R=227, G=27), teal, magenta — a hypnotic shifting palette.

**Mouse proximity size boost:**
```javascript
if (dist < 200) {
    radius = radius + (200 - dist) * 0.1
}
```

Dots within 200px of the mouse are boosted in size by up to 20px, creating the effect of waves being larger at the source.

---

[Back to fun/ examples](README.md)

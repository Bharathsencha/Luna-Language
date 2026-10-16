# Galaxy (`galaxy.lu`)

## Background Stars

500 stars are generated at startup using `rand()` for positions. Unlike the aurora's deterministic hash, here we use the actual PRNG because the galaxy doesn't need reproducible positions — it just needs scatter. Each star gets a random brightness `bg_stars_b[i]` between 50 and 200.

**Twinkle:**
```javascript
let twinkle = bg_stars_b[i] + (sin(get_delta_time() * 10.0 + i) * 50)
```

Note: this uses `get_delta_time()` (the time for the last frame) not a global accumulating `time`. This is a minor bug/quirk: `get_delta_time()` at 60FPS ≈ 0.0167, so `0.0167 * 10 * i` for star 100 is only 16.7 radians. The twinkle is present but the effect is subtler than intended. Regardless, the sin value varies per star index `+ i`, giving each star a unique offset.

## Galaxy Particle Generation

**12,000 particles** are generated at startup in polar coordinates (angle, distance from center).

```javascript
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

## Per-Frame Orbital Update

```javascript
g_angle[i] = g_angle[i] + g_speed[i]
let x = cx + cos(g_angle[i]) * g_dist[i]
let y = cy + sin(g_angle[i]) * g_dist[i]
y = cy + (y - cy) * 0.7
```

Each frame, each particle's angle advances by its speed. Converting polar to Cartesian gives screen position. The Y coordinate is scaled by 0.7 (`y = cy + (y - cy) * 0.7`) — this squashes the galaxy vertically to simulate viewing it at an angle from above (like seeing the Milky Way as an oval disc rather than a perfect circle).

## Nebula Colors

```javascript
if (r_val > 0.90) { rr = 255; gg = 100; bb = 200 } # Pink dust
else if (r_val > 0.85) { rr = 100; gg = 200; bb = 255 } # Cyan dust
```

10% of stars are pink nebula dust, 5% are cyan nebula dust. The rest are in the blue-white gradient based on core distance. These clusters of pink/cyan particles visually create the nebula regions visible in real galaxy images.

## Galactic Core Bloom

```javascript
draw_circle(cx, cy, 100, [255, 240, 200, 3])
draw_circle(cx, cy, 60, [255, 250, 220, 5])
draw_circle(cx, cy, 30, [255, 255, 255, 10])
draw_circle(cx, cy, 10, [255, 255, 255, 200])
```

Four large nearly-transparent circles create a soft bloom glow at the galactic center, simulating the concentrated starlight of a galactic bulge. The outermost is barely visible (alpha 3), building up to a bright white core (alpha 200). This is the same layered-circle bloom technique used for the sun in meadow.lu.

---

[Back to fun/ examples](README.md)

# Raceee Car Game (`cargame/`)

A top-down 2D driving demo with menus, a follow camera, and a tiled 15000x15000 world.

```bash
make run-cargame
```

| Action | Input |
|---|---|
| Accelerate / reverse | Up / Down arrow |
| Steer | Left / Right arrow |
| Back to menu | `Esc` |
| Menu buttons (Play, Settings, Exit) | Left click |

The Settings screen toggles the frame cap between 30 and 60 FPS.

## Layout

| Path | Contents |
|---|---|
| `cargame.lu` | The whole program |
| `assets/` | Car sprite, soil tile, menu background, arrows, font, menu music, click sound |

## How it works

**State machine.** `current_state` is one of three integer constants (`MENU = 0`, `GAME = 1`,
`SETTINGS = 2`). Each frame runs the update block for the current state, then the matching draw
block.

**Movement.** Speed rises by `car_speedup` while a key is held and falls linearly by
`car_slowdown` when released. Position advances along the heading:

```javascript
x_move = car_speed * cos(rotation) * dt
y_move = car_speed * sin(rotation) * dt
```

**Camera.** The 2D camera is a six-element list `[offset_x, offset_y, target_x, target_y,
rotation, zoom]` passed to `begin_mode_2d`. The target eases toward the car
(`target += delta * 2.0 * dt`), which gives the smooth follow.

**Tile culling.** The ground is `Soil_Tile.png` repeated across the world. Only the tiles around
the car are drawn:

```javascript
let col_idx = floor(p_x / soil_w)
let start_col = col_idx - 3
let end_col = col_idx + 4
```

The frame cost stays the same no matter how large `world_w` is.

## Tuning

Constants at the top of `cargame.lu`:

```javascript
let car_max_speed = 800.0   # pixels per second
let car_speedup = 200.0     # acceleration
let ROTATION_SPEED = 120.0  # degrees per second
```

- The world size is `world_w = 15000`.
- The car sprite is scaled to `desired_car_width` through `scale_factor`, so a replacement
  `Car_1_01.png` of any size works. A seamless replacement for `Soil_Tile.png` works the same way.

---

[Back to examples](../README.md)

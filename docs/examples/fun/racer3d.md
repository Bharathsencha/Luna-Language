# Seasonal Drift (`racer3d/`)

A closed-loop arcade racer split across eight modules. It is the largest end-to-end test of the
3D engine and the module system.

```bash
make run-racer
```

Controls: arrow keys drive and steer, `R` restarts after a crash.

## Modules

| File | Lines | Role |
|---|---:|---|
| `main.lu` | 209 | Window, game loop, chase camera, season blending, draw order |
| `config.lu` | 28 | Read-only tunables (`export let`): track size, physics, traffic count, season length |
| `road.lu` | 228 | Builds the spline, bakes it to `assets/cache/track.obj` + `.mtl`, loads it with `load_model` |
| `player.lu` | 79 | On-rails driving model: speed, lateral offset, steering smoothing, off-road drag, crash timer |
| `traffic.lu` | 70 | AI cars at their own speeds; returns `true` on collision with the player |
| `seasons.lu` | 69 | Four flat palette lists (sky, grass, ambient, sun, fog, particle mode) and blending |
| `weather.lu` | 98 | Petals / heat motes / leaves / snow in a box that wraps around the camera |
| `hud.lu` | 98 | 2D overlay: speed bar, season name, lap, minimap |

## How it fits together

- **Asset pipeline.** The track is not shipped as a model. `road.lu` samples 440 points on a
  closed spline, writes them as an OBJ with material colors in the MTL file using the normal file
  API (`open` / `write`), then loads the result with `load_model`. Material `Kd` colors are baked
  into vertex colors, so the whole circuit is one draw call.
- **State ownership.** Modules export pure functions and constants only. Mutable state lives in
  lists owned by `main.lu` (for example the player list `P = [s, lat, speed, steer, crash, lap]`)
  and is passed into helpers that mutate it in place. This keeps module state out of the
  cross-VM import path.
- **Seasons.** Every `SEASON_SECONDS` (26 s) the palette blends to the next season. The blended
  list drives `clear_background`, `set_ambient_light`, the sun light, `set_fog`, and the weather
  particle mode (snapped to an integer in the last slot).
- **Rendering.** The car is drawn with the transform stack (`push_matrix` / `translate_3d` /
  `rotate_3d`) so wheels and body compose without manual matrix math. Weather uses
  `draw_particle_3d` billboards; alpha below 255 goes into the sorted translucent queue.

---

[Back to fun/ examples](README.md)

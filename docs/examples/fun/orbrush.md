# Orb Rush (`orbrush.lu`)

A first-person arcade shooter: collect or shoot the orbs before the timer runs out.

```bash
make run-fun NAME=orbrush
```

Controls: `WASD` move, `Q` / `E` down and up, `Shift` sprint, left click shoot, `R` restart
after the round ends.

## Engine features used

| Feature | Calls |
|---|---|
| Captured-mouse free camera | `capture_cursor(1)`, `update_camera_free(cam, speed, sensitivity)` |
| Screen-center picking | `get_mouse_ray(cam)`, `ray_hits_box(ray, box)` |
| Follow spot light | `create_light(LIGHT_SPOT, ...)`, `set_light_position`, `set_light_target`, `set_light_cone` |
| Depth fog | `set_fog(color, density)` |
| Tracer lines | `draw_line_3d(a, b, color)` |
| Particle bursts | a fixed-size pool of parallel lists drawn with `draw_particle_3d` |
| Translucency | orb colors with alpha below 255 are sorted far-to-near automatically |

The burst particles and the orbs both use parallel lists (`px[]`, `py[]`, `pz[]`, ...) instead
of one object per particle. That keeps allocation out of the frame loop, so the GC has almost
nothing to do while the game runs.

---

[Back to fun/ examples](README.md)

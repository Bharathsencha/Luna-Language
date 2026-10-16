# fun/ - Small Programs

Short, self-contained programs that exercise the 2D/3D renderer, the audio backend, and the
native plugins. Each has its own page explaining how it works.

Run from the repository root (asset paths are root-relative):

```bash
make run-fun NAME=galaxy                 # any single-file program below
make run-racer                           # the multi-module 3D racer
./bin/luna docs/examples/fun/galaxy.lu   # same thing without make
```

## 2D

| Program | Page | What it shows | Music |
|---|---|---|---|
| `aurora.lu` | [aurora.md](aurora.md) | Vertical-ray aurora curtains, layered mountains, twinkling stars | `はじまりの曲` |
| `aurora_wave.lu` | [aurora.md](aurora.md) | Horizontal-strip aurora with a rippling lake reflection | `はじまりの曲` |
| `meadow.lu` | [meadow.md](meadow.md) | Layered hills, Lissajous butterfly, grass, pollen | `グッド・ドクター` |
| `eye.lu` | [eye.md](eye.md) | Ellipse-from-strips eye that tracks the mouse | `Pogo - Forget` |
| `paint.lu` | [paint.md](paint.md) | Drawing app on a render texture; `S` saves `luna_art.png` | none |
| `sand.lu` | [sand.md](sand.md) | Falling-sand cellular automaton in the native `sand_*` plugin | none |
| `galaxy.lu` | [galaxy.md](galaxy.md) | Polar-orbit particle galaxy with core bloom | none |
| `ripples.lu` | [ripples.md](ripples.md) | Distance-phase wave field | none |
| `car_retro.lu` | [car_retro.md](car_retro.md) | Neon lane racer: state machine, object pool, AABB collision | `kavinsky.mp3` |
| `boobs.lu` | [softbody.md](softbody.md) | Spring-mass soft bodies, written with the keyword aliases | none |
| `vortex.lu`, `blackhole.lu` | [cosmic.md](cosmic.md) | 18k-particle vortex; lensing approximation | none |

## 3D

| Program | Page | What it shows |
|---|---|---|
| `racer3d/main.lu` | [racer3d.md](racer3d.md) | Seasonal Drift: procedural track baked to OBJ, chase camera, weather, traffic AI, HUD |
| `orbrush.lu` | [orbrush.md](orbrush.md) | First-person shooter: captured-mouse camera, ray picking, spot light, particles |
| `scene3d.lu` | [scene3d.md](scene3d.md) | API smoke test (~360 frames): `_pro` primitives, transform stack, fog, picking |
| `scene_models.lu` | [scene3d.md](scene3d.md) | API smoke test (~300 frames): OBJ + MTL model loading |
| `neon3d.lu` | none yet | Neon Geometry showcase: lit primitives, orbiting lights |

## Other

- `modtest/` - module-resolution check (script-relative imports, single-load cache).
  Run `./bin/luna docs/examples/fun/modtest/main.lu`.
- `music/` - audio files used by the programs above. `Call of the Night`, `Drunken Trio` and
  `Sunflower` are present but not referenced by any program.
- `assets/` - arrow sprites used by `car_retro.lu`.

## How audio is wired

Every program with music follows the same pattern:

```javascript
init_audio_device()
let music = load_music_stream("docs/examples/fun/music/kavinsky.mp3")
play_music_stream(music)

while (window_open()) {
    update_music_stream(music)   # every frame, or the stream buffer runs dry
    # ... draw ...
}

unload_music_stream(music)
close_audio_device()
```

`load_music_stream` streams from disk; `load_sound` decodes the whole file into memory and is
meant for short effects. See [stdlib/gui.md](../../stdlib/gui.md#audio) for the full audio API.

## Credits

None of the music in `music/` is owned by this project. All rights belong to the artists; the
tracks are included for demonstration only.

- `aurora.lu`, `aurora_wave.lu`: [はじまりの曲](https://www.youtube.com/watch?v=tOq_kHo6jNc)
- `eye.lu`: [Goth (slowed + reverb)](https://www.youtube.com/watch?v=KJoYBw5tJOc)
- `meadow.lu`: [グッド・ドクター](https://www.youtube.com/watch?v=P6kQNA2Zj40)
- `car_retro.lu`: [Nightcall - Kavinsky](https://www.youtube.com/watch?v=MV_3Dpw-BRY)

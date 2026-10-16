# Examples

Complete programs written in Luna. Each folder holds the code, its assets, and a page explaining
how it works. Run everything from the repository root: asset paths inside the scripts are
relative to the root.

| Example | Run | Page |
|---|---|---|
| Car game (2D, menus, follow camera) | `make run-cargame` | [cargame/README.md](cargame/README.md) |
| Music player (album art, live spectrum) | `make run-music` | [musicplayer/README.md](musicplayer/README.md) |
| Seasonal Drift (3D racer, 8 modules) | `make run-racer` | [fun/racer3d.md](fun/racer3d.md) |
| Small 2D and 3D programs | `make run-fun NAME=<file>` | [fun/README.md](fun/README.md) |

Other entry points outside this folder:

- `main.lu` at the repository root: scratch script for `make run`.
- `3D/main.lu`: standalone 3D scene, `./bin/luna 3D/main.lu`.

## Media

The music in `musicplayer/songs/` and `fun/music/` belongs to its artists and is included for
demonstration only. See [musicplayer/CREDITS.md](musicplayer/CREDITS.md) and the credits section
of [fun/README.md](fun/README.md#credits).

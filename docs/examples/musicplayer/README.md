# Music Player (`musicplayer/`)

A playlist player with album art, a seek bar, and a 32-band live spectrum visualizer.

```bash
make run-music
```

| Action | Input |
|---|---|
| Play / pause | Click the center button |
| Next / previous track | Click the right / left arrow |
| Seek | Click anywhere on the progress bar |
| Exit | Close the window |

Tracks advance automatically when one finishes.

## Layout

| Path | Contents |
|---|---|
| `music.lu` | The whole program (900x700 window) |
| `songs/` | 14 MP3 files; the playlist is the `songs` list at the top of `music.lu` |
| `assets/` | Play / pause / forward icons and the `ShadeBlue` font (`vinyl.png` and `neon.jpg` are present but unused) |
| `CREDITS.md` | Sources for every track, icon and font |

To add a track, copy the MP3 into `songs/` and append its path (root-relative, starting with
`docs/examples/musicplayer/songs/`) to the `songs` list.

## How it works

**Background.** Every frame draws one `draw_gradient_ex` rectangle covering the window. The four
corner colors come from `hsl()`, with hues driven by `sin(time)` and kept within 180-290 so the
palette stays in the blue/purple range. Note that `hsl(h, s, v)` actually performs an HSV
conversion (`gl_color_from_hsv`) despite its name.

**Album art.** `load_music_cover(path)` reads the MP3's ID3v2 header, extracts the embedded
picture frame, and uploads it as a texture. It returns `-1` when the file has no cover; the
player then leaves the art area empty. The cover is scaled to fit a square with
`draw_texture_pro`, preserving aspect ratio.

**Spectrum.** The visualizer pipeline:

```
miniaudio data callback -> PCM ring buffer -> get_music_fft() -> 32 floats -> draw_rectangle bars
  gui/audio_backend.c        audio_get_pcm_buffer()   gui/gui_lib.c                music.lu
```

1. While the current stream plays, the miniaudio data callback in `gui/audio_backend.c` averages
   each stereo frame to mono and writes it into a ring buffer (only the most recently loaded
   stream is captured).
2. `get_music_fft(music)` in `gui/gui_lib.c` copies the buffer, then runs a direct DFT over the
   last 1024 samples (48 kHz) at 32 logarithmically spaced frequencies from 60 Hz to 16 kHz:

   ```c
   float freq_ratio = powf(16000.0f / 60.0f, 1.0f / (NUM_BANDS - 1));
   for (int n = 0; n < N; n++) {
       float angle = 2.0f * M_PI * band_freqs[b] * n / sample_rate;
       real += buffer[n] * cosf(angle);
       imag += buffer[n] * sinf(angle);
   }
   ```

3. Magnitudes are divided by the largest band (so the loudest band is 1.0) and returned as a
   Luna list.
4. `music.lu` maps each value to a bar height and a hue from 180 (cyan) to 340 (magenta).

Logarithmic spacing matters because hearing is logarithmic: with linear bands, most bars would
show high-frequency content.

---

[Back to examples](../README.md)

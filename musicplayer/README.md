# Music Player Documentation

This document explains the internal logic, usage, and customization of the **Luna Music Player** (`musicplayer/music.lu`).

## Overview
The Luna Music Player is a fully-featured audio application built with the Luna Language, featuring animated gradient backgrounds, album art support, a real-time audio visualizer, and an organized project structure.

## Features
-   **Animated Gradient Background**: Dynamic HSL color cycling with purple-to-blue tones.
-   **Asset Organization**: Dedicated folders for songs and UI resources.
-   **Album Art**: Automated extraction from MP3 ID3v2 tags.
-   **Playlist System**: Supports multiple tracks with automated advancement.
-   **UI Controls**: Seek bar, Play/Pause (with resume), Next/Previous controls.
-   **Real-Time Audio Visualizer**: 32-band frequency equalizer bars driven by live PCM data.

---

## Project Structure

| Folder | Purpose |
| :--- | :--- |
| `musicplayer/` | Contains the main script (`music.lu`) and documentation. |
| `musicplayer/assets/` | UI textures, icons, and custom fonts. |
| `musicplayer/songs/` | MP3 files for the playlist. |

---

## Controls

| Action | Input |
| :--- | :--- |
| **Play / Pause** | Click the Play/Pause button (Center) |
| **Next Song** | Click the Right Arrow |
| **Previous Song** | Click the Left Arrow |
| **Seek** | Click anywhere on the progress bar |
| **Exit** | Close the window |

---

## Aesthetics & Background

The player features an animated gradient background.

### Background Engine
The background is rendered using:
-   **`draw_gradient_ex`**: 4-corner gradient with dynamic HSL colors.
-   **Dynamic HSL**: Colors are calculated using `hsl()` based on `sin(time)` to create a breathing effect.

### Color Limits
The background is restricted to the cool spectrum (cyan, purple, blue) by bounding hue values between `200` and `290`.

---

## Audio Visualizer

The visualizer displays 32 frequency bars that react to the actual audio output in real-time. Here is how it works end-to-end.

### Architecture

```
Audio Stream → PCM Callback → Ring Buffer → DFT → Luna List → Drawing
 (raylib)      (gui_lib.c)    (gui_lib.c)  (gui_lib.c) (library.c) (music.lu)
```

### 1. PCM Sample Capture (`gui/gui_lib.c`)

When a music stream is loaded via `load_music_stream()`, the C function `lib_gui_load_music` attaches an audio processor callback using raylib's `AttachAudioStreamProcessor`.

```c
// Ring buffer for raw audio samples
#define FFT_BUFFER_SIZE 2048
static float fft_buffer[FFT_BUFFER_SIZE];
static int fft_write_cursor = 0;

// Callback: receives raw PCM frames from raylib's audio pipeline
static void audio_processor_callback(void *bufferData, unsigned int frames) {
    float *samples = (float *)bufferData;
    for (unsigned int i = 0; i < frames; i++) {
        // Average stereo channels to mono
        float mono = (samples[i * 2] + samples[i * 2 + 1]) * 0.5f;
        fft_buffer[fft_write_cursor] = mono;
        fft_write_cursor = (fft_write_cursor + 1) % FFT_BUFFER_SIZE;
    }
}
```

This callback runs automatically whenever raylib processes audio frames. The stereo signal is averaged to mono and written to a circular buffer of 2048 samples.

### 2. DFT Frequency Analysis (`gui/gui_lib.c`)

The function `lib_gui_get_music_fft` computes frequency band magnitudes:

1. **Snapshot**: Copies the ring buffer to avoid race conditions with the audio thread.
2. **Frequency bands**: Defines 32 logarithmically spaced frequencies from 60 Hz to 16 kHz.
3. **DFT**: For each band, computes the magnitude using a direct Fourier transform over the last 1024 samples at 48 kHz sample rate.
4. **Normalization**: Divides each magnitude by the maximum to produce values between 0.0 and 1.0.
5. **Return**: Packages the 32 floats into a Luna list using `value_list` / `value_float` / `value_list_append`.

```c
// Logarithmic frequency spacing
float base_freq = 60.0f;
float freq_ratio = powf(16000.0f / 60.0f, 1.0f / (NUM_BANDS - 1));
for (int b = 0; b < NUM_BANDS; b++) {
    band_freqs[b] = base_freq * powf(freq_ratio, (float)b);
}
```

### 3. Registration (`src/library.c`)

The C function is registered as a Luna builtin:
```c
env_def(env, "get_music_fft", value_native(lib_gui_get_music_fft));
```

This makes `get_music_fft(music_id)` callable from any Luna script.

### 4. Rendering (`musicplayer/music.lu`)

The Luna script calls `get_music_fft()` every frame and draws 32 colored bars:

```python
let fft = get_music_fft(current_music)
if (fft != null) {
    let num_bars = len(fft)
    let b = 0
    while (b < num_bars) {
        let mag = fft[b]
        let h = to_int(mag * viz_height * 0.9)
        # Color gradient: cyan(180) -> magenta(340) across bars
        let bar_hue = 180.0 + (b * 160.0 / num_bars)
        let bar_col = hsl(bar_hue, 1.0, 0.55)
        draw_rectangle(bx, by, vbar_w, h, [bar_col[0], bar_col[1], bar_col[2], 220])
        b = b + 1
    }
}
```

### Files Involved

| File | Role |
| :--- | :--- |
| `gui/gui_lib.c` | PCM ring buffer, audio callback, DFT computation, `lib_gui_get_music_fft` |
| `gui/gui_lib.h` | Function declaration for `lib_gui_get_music_fft` |
| `src/library.c` | Registers `get_music_fft` as a Luna builtin |
| `musicplayer/music.lu` | Calls `get_music_fft()` and draws the equalizer bars |

---

## Code Snippets

### Initialization
```python
# Load font from organized asset folder
let FONT_L = load_font("musicplayer/assets/ShadeBlue-2OozX.ttf", 60)
```

### Playlist Configuration

---

## Credits
For full details on the music, icons, and fonts used in this project, see **[CREDITS.md](CREDITS.md)**.
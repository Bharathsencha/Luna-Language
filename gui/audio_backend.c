// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// audio_backend.c — miniaudio-based audio backend for Luna GUI
// Audio backend using miniaudio. Supports music streaming, sound effects, and PCM capture.

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audio_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// INTERNAL STATE

static ma_engine g_engine;
static int g_engine_initialized = 0;

#define PCM_BUFFER_SIZE 2048
static float g_pcm_buffer[PCM_BUFFER_SIZE];
static int g_pcm_cursor = 0;

#define MAX_MUSIC_SLOTS 16
typedef struct {
    ma_sound sound;       // miniaudio sound (used for streaming too)
    ma_decoder decoder;   // decoder for time queries
    int active;
    int decoder_active;
    float total_length;   // cached length in seconds
} MusicSlot;

static MusicSlot g_music[MAX_MUSIC_SLOTS];
static int g_music_count = 0;

#define MAX_SOUND_SLOTS 16
typedef struct {
    ma_sound sound;
    int active;
} SoundSlot;

static SoundSlot g_sounds[MAX_SOUND_SLOTS];
static int g_sound_count = 0;

// PCM CAPTURE (data source node callback approach)

// We use a custom node to capture PCM samples passing through the audio graph.
// Simpler approach: use ma_engine's notification or just read from sounds directly.
// For now, we'll capture in the music update function by reading the decoder.

// Actually the simplest approach: use a data source callback.
// But miniaudio's ma_sound doesn't expose a per-frame PCM callback easily.
// Instead, we'll use a custom node inserted into the pipeline.

// Simplest approach that works: use a node_graph callback.
// Actually let's just do it the simple way — capture PCM by reading
// the output of the engine node. We'll use a custom effect node.

// For maximum simplicity that:
// We'll just capture data in the data callback.

// (reserved for future use)

// Custom data callback that captures PCM while playing
static ma_uint32 g_capture_source = -1; // Which music slot to capture from

// LIFECYCLE

int audio_init(void) {
    if (g_engine_initialized) return 0;

    ma_engine_config config = ma_engine_config_init();
    config.channels = 2;
    config.sampleRate = 48000;

    if (ma_engine_init(&config, &g_engine) != MA_SUCCESS) {
        fprintf(stderr, "[Audio Backend] Failed to initialize audio engine\n");
        return -1;
    }

    g_engine_initialized = 1;
    memset(g_pcm_buffer, 0, sizeof(g_pcm_buffer));
    memset(g_music, 0, sizeof(g_music));
    memset(g_sounds, 0, sizeof(g_sounds));
    g_music_count = 0;
    g_sound_count = 0;
    return 0;
}

void audio_close(void) {
    // Unload all active music
    for (int i = 0; i < g_music_count; i++) {
        if (g_music[i].active) {
            ma_sound_uninit(&g_music[i].sound);
            if (g_music[i].decoder_active) {
                ma_decoder_uninit(&g_music[i].decoder);
            }
            g_music[i].active = 0;
        }
    }
    // Unload all active sounds
    for (int i = 0; i < g_sound_count; i++) {
        if (g_sounds[i].active) {
            ma_sound_uninit(&g_sounds[i].sound);
            g_sounds[i].active = 0;
        }
    }

    if (g_engine_initialized) {
        ma_engine_uninit(&g_engine);
        g_engine_initialized = 0;
    }

    g_music_count = 0;
    g_sound_count = 0;
}

// MUSIC

AMusic audio_load_music(const char *path) {
    if (!g_engine_initialized || g_music_count >= MAX_MUSIC_SLOTS) return -1;

    int idx = g_music_count;
    MusicSlot *slot = &g_music[idx];

    // Initialize sound with streaming flag
    ma_uint32 flags = MA_SOUND_FLAG_STREAM;
    if (ma_sound_init_from_file(&g_engine, path, flags, NULL, NULL, &slot->sound) != MA_SUCCESS) {
        fprintf(stderr, "[Audio Backend] Failed to load music: %s\n", path);
        return -1;
    }

    // Initialize decoder for time queries and PCM capture
    ma_decoder_config dec_config = ma_decoder_config_init(ma_format_f32, 2, 48000);
    if (ma_decoder_init_file(path, &dec_config, &slot->decoder) == MA_SUCCESS) {
        slot->decoder_active = 1;
        // Get total length
        ma_uint64 total_frames;
        if (ma_decoder_get_length_in_pcm_frames(&slot->decoder, &total_frames) == MA_SUCCESS) {
            slot->total_length = (float)total_frames / 48000.0f;
        } else {
            slot->total_length = 0;
        }
    } else {
        slot->decoder_active = 0;
        slot->total_length = 0;
    }

    slot->active = 1;
    g_music_count++;

    // Set this as the capture source for PCM/FFT
    g_capture_source = idx;

    return idx;
}

void audio_unload_music(AMusic m) {
    if (m < 0 || m >= g_music_count || !g_music[m].active) return;
    ma_sound_uninit(&g_music[m].sound);
    if (g_music[m].decoder_active) {
        ma_decoder_uninit(&g_music[m].decoder);
        g_music[m].decoder_active = 0;
    }
    g_music[m].active = 0;
    if (g_capture_source == (ma_uint32)m) g_capture_source = -1;
}

void audio_play_music(AMusic m) {
    if (m < 0 || m >= g_music_count || !g_music[m].active) return;
    ma_sound_start(&g_music[m].sound);
}

void audio_stop_music(AMusic m) {
    if (m < 0 || m >= g_music_count || !g_music[m].active) return;
    ma_sound_stop(&g_music[m].sound);
    ma_sound_seek_to_pcm_frame(&g_music[m].sound, 0);
}

void audio_pause_music(AMusic m) {
    if (m < 0 || m >= g_music_count || !g_music[m].active) return;
    ma_sound_stop(&g_music[m].sound);
}

void audio_resume_music(AMusic m) {
    if (m < 0 || m >= g_music_count || !g_music[m].active) return;
    ma_sound_start(&g_music[m].sound);
}

void audio_update_music(AMusic m) {
    if (m < 0 || m >= g_music_count || !g_music[m].active) return;

    // Capture PCM samples for FFT visualization
    // We read from the engine's output by reading the sound's cursor position
    // and using the decoder to read PCM data at that position.
    if (g_music[m].decoder_active && g_capture_source == (ma_uint32)m) {
        // Get current cursor in PCM frames
        ma_uint64 cursor_frames;
        if (ma_sound_get_cursor_in_pcm_frames(&g_music[m].sound, &cursor_frames) == MA_SUCCESS) {
            // Seek decoder to current position and read a chunk
            ma_decoder_seek_to_pcm_frame(&g_music[m].decoder, cursor_frames);
            float temp[512]; // 256 frames * 2 channels
            ma_uint64 frames_read;
            if (ma_decoder_read_pcm_frames(&g_music[m].decoder, temp, 256, &frames_read) == MA_SUCCESS) {
                for (ma_uint64 i = 0; i < frames_read; i++) {
                    // Average stereo to mono
                    float mono = (temp[i * 2] + temp[i * 2 + 1]) * 0.5f;
                    g_pcm_buffer[g_pcm_cursor] = mono;
                    g_pcm_cursor = (g_pcm_cursor + 1) % PCM_BUFFER_SIZE;
                }
            }
        }
    }
}

void audio_seek_music(AMusic m, float position) {
    if (m < 0 || m >= g_music_count || !g_music[m].active) return;
    ma_uint64 frame = (ma_uint64)(position * 48000.0f);
    ma_sound_seek_to_pcm_frame(&g_music[m].sound, frame);
}

float audio_get_music_length(AMusic m) {
    if (m < 0 || m >= g_music_count || !g_music[m].active) return 0;

    // Try to get length from the sound's data source
    float length = 0;
    ma_uint64 total_frames = 0;
    ma_sound_get_length_in_pcm_frames(&g_music[m].sound, &total_frames);
    if (total_frames > 0) {
        ma_uint32 sample_rate;
        ma_sound_get_data_format(&g_music[m].sound, NULL, NULL, &sample_rate, NULL, 0);
        if (sample_rate > 0) {
            length = (float)total_frames / (float)sample_rate;
        }
    }
    // Fallback to cached value
    if (length <= 0) length = g_music[m].total_length;
    return length;
}

float audio_get_music_played(AMusic m) {
    if (m < 0 || m >= g_music_count || !g_music[m].active) return 0;

    ma_uint64 cursor;
    if (ma_sound_get_cursor_in_pcm_frames(&g_music[m].sound, &cursor) == MA_SUCCESS) {
        ma_uint32 sample_rate;
        ma_sound_get_data_format(&g_music[m].sound, NULL, NULL, &sample_rate, NULL, 0);
        if (sample_rate > 0) {
            return (float)cursor / (float)sample_rate;
        }
    }
    return 0;
}

void audio_get_pcm_buffer(float *out_buffer, int *out_size) {
    if (out_buffer && out_size) {
        *out_size = PCM_BUFFER_SIZE;
        int cursor = g_pcm_cursor;
        for (int i = 0; i < PCM_BUFFER_SIZE; i++) {
            out_buffer[i] = g_pcm_buffer[(cursor + i) % PCM_BUFFER_SIZE];
        }
    }
}

// SOUNDS

ASound audio_load_sound(const char *path) {
    if (!g_engine_initialized || g_sound_count >= MAX_SOUND_SLOTS) return -1;

    int idx = g_sound_count;
    // Load fully decoded into memory (not streaming) for low-latency playback
    ma_uint32 flags = MA_SOUND_FLAG_DECODE;
    if (ma_sound_init_from_file(&g_engine, path, flags, NULL, NULL, &g_sounds[idx].sound) != MA_SUCCESS) {
        fprintf(stderr, "[Audio Backend] Failed to load sound: %s\n", path);
        return -1;
    }

    g_sounds[idx].active = 1;
    g_sound_count++;
    return idx;
}

void audio_unload_sound(ASound s) {
    if (s < 0 || s >= g_sound_count || !g_sounds[s].active) return;
    ma_sound_uninit(&g_sounds[s].sound);
    g_sounds[s].active = 0;
}

void audio_play_sound(ASound s) {
    if (s < 0 || s >= g_sound_count || !g_sounds[s].active) return;
    // Rewind to start and play
    ma_sound_seek_to_pcm_frame(&g_sounds[s].sound, 0);
    ma_sound_start(&g_sounds[s].sound);
}

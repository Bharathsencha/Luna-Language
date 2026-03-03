// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// audio_backend.h — miniaudio-based audio backend for Luna GUI
// Audio backend using miniaudio.

#ifndef AUDIO_BACKEND_H
#define AUDIO_BACKEND_H

// Opaque handles
typedef int AMusic;
typedef int ASound;

// Audio system lifecycle
int  audio_init(void);
void audio_close(void);

// Music (streaming)
AMusic audio_load_music(const char *path);
void   audio_unload_music(AMusic m);
void   audio_play_music(AMusic m);
void   audio_stop_music(AMusic m);
void   audio_pause_music(AMusic m);
void   audio_resume_music(AMusic m);
void   audio_update_music(AMusic m);
void   audio_seek_music(AMusic m, float position);
float  audio_get_music_length(AMusic m);
float  audio_get_music_played(AMusic m);

// PCM capture for FFT visualizer
void  audio_get_pcm_buffer(float *out_buffer, int *out_size);

// Sound effects (one-shot)
ASound audio_load_sound(const char *path);
void   audio_unload_sound(ASound s);
void   audio_play_sound(ASound s);

#endif // AUDIO_BACKEND_H

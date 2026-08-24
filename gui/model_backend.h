// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// model_backend.h — OBJ mesh loading for Luna 3D (colors-first pipeline)

#ifndef MODEL_BACKEND_H
#define MODEL_BACKEND_H

#include "gl_backend_3d.h"

#ifdef __cplusplus
extern "C" {
#endif

// Parse an .obj (+ optional .mtl), bake material diffuse colors into vertex
// colors, upload one VAO. Returns handle >= 0 or -1 on failure.
int  model_load(const char *path);

int  model_vert_count(int id);

// Draw composed with the current transform stack; tint multiplies baked colors.
void model_draw(int id, GMat4 local_model, GColor tint, int lit);

void model_unload(int id);
void model_unload_all(void);

#ifdef __cplusplus
}
#endif

#endif // MODEL_BACKEND_H

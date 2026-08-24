// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// model_backend.c — OBJ mesh loading for Luna 3D
//
// Colors-first pipeline: material diffuse colors (MTL Kd) are baked into
// per-vertex colors at load time, so models render with the same single
// shader as primitives — one VAO, one draw call, no texture units.
//
// Uses vendored tinyobj_loader_c.h (MIT, Syoyo Fujita) in vendor/.

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "../vendor/tinyobj_loader_c.h"

#include "model_backend.h"
#include "gl_backend_3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_MODELS 64
#define MODEL_VERT_STRIDE 10 // x,y,z,nx,ny,nz,r,g,b,a

typedef struct {
    int in_use;
    int vao, vbo;
    int vert_count;
} LoadedModel;

static LoadedModel g_models[MAX_MODELS];


static int is_absolute_os_path(const char *p) {
    return p && p[0] == '/';
}

static void model_read_file(void *ctx, const char *filename, int is_mtl,
                            const char *obj_filename, char **buf, size_t *len) {
    (void)ctx; (void)is_mtl;
    FILE *f = fopen(filename, "rb");
    if (!f && obj_filename && !is_absolute_os_path(filename)) {
        // mtllib paths are relative to the .obj file's directory
        char joined[1024];
        const char *slash = strrchr(obj_filename, '/');
        if (slash) {
            size_t dirlen = (size_t)(slash - obj_filename);
            size_t namelen = strlen(filename);
            if (dirlen + namelen + 2 < sizeof(joined)) {
                memcpy(joined, obj_filename, dirlen);
                joined[dirlen] = '/';
                memcpy(joined + dirlen + 1, filename, namelen + 1);
                f = fopen(joined, "rb");
            }
        }
    }
    if (!f) {
        *buf = NULL;
        *len = 0;
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); *buf = NULL; *len = 0; return; }

    char *data = (char *)malloc((size_t)sz + 1);
    if (!data) { fclose(f); *buf = NULL; *len = 0; return; }
    size_t got = fread(data, 1, (size_t)sz, f);
    fclose(f);
    data[got] = '\0';
    *buf = data;
    *len = got;
}


static GColor color_from_material(const tinyobj_material_t *mat) {
    if (!mat) return (GColor){200, 200, 200, 255};
    GColor c;
    c.r = (unsigned char)(mat->diffuse[0] > 1.0f ? 255 : mat->diffuse[0] * 255.0f);
    c.g = (unsigned char)(mat->diffuse[1] > 1.0f ? 255 : mat->diffuse[1] * 255.0f);
    c.b = (unsigned char)(mat->diffuse[2] > 1.0f ? 255 : mat->diffuse[2] * 255.0f);
    c.a = (mat->dissolve >= 0.999f) ? 255 : (unsigned char)(mat->dissolve * 255.0f);
    return c;
}

int model_load(const char *path) {
    int slot = -1;
    for (int i = 0; i < MAX_MODELS; i++) {
        if (!g_models[i].in_use) { slot = i; break; }
    }
    if (slot < 0) return -1;

    tinyobj_attrib_t attrib;
    tinyobj_shape_t *shapes = NULL;
    size_t num_shapes = 0;
    tinyobj_material_t *materials = NULL;
    size_t num_materials = 0;

    tinyobj_attrib_init(&attrib);
    int ret = tinyobj_parse_obj(&attrib, &shapes, &num_shapes, &materials,
                                &num_materials, path, model_read_file, NULL,
                                TINYOBJ_FLAG_TRIANGULATE);
    if (ret != TINYOBJ_SUCCESS) {
        fprintf(stderr, "[GL3D] Failed to parse model '%s'\n", path);
        tinyobj_attrib_free(&attrib);
        return -1;
    }

    // NB: tinyobj_loader_c's num_faces is the total index count;
    // num_face_num_verts is the actual number of faces.
    int face_count = (int)attrib.num_face_num_verts;
    int vert_count = 0;
    for (int i = 0; i < face_count; i++) vert_count += attrib.face_num_verts[i];

    float *verts = (float *)malloc(sizeof(float) * (size_t)vert_count * MODEL_VERT_STRIDE);
    if (!verts) {
        tinyobj_shapes_free(shapes, num_shapes);
        tinyobj_materials_free(materials, num_materials);
        tinyobj_attrib_free(&attrib);
        return -1;
    }

    int n = 0;
    int face_offset = 0;
    for (int i = 0; i < face_count; i++) {
        int fnum = attrib.face_num_verts[i];
        int mat_id = attrib.material_ids[i];
        GColor col = color_from_material(
            (mat_id >= 0 && (size_t)mat_id < num_materials) ? &materials[mat_id] : NULL);

        tinyobj_vertex_index_t idx[4]; // triangulate fans of up to 4 corners
        for (int k = 0; k < fnum && k < 4; k++) {
            idx[k] = attrib.faces[face_offset + k];
        }

        float px[3], py[3], pz[3];
        for (int t = 0; t < 3; t++) {
            px[t] = attrib.vertices[3 * idx[t].v_idx + 0];
            py[t] = attrib.vertices[3 * idx[t].v_idx + 1];
            pz[t] = attrib.vertices[3 * idx[t].v_idx + 2];
        }

        float fnx = 0, fny = 0, fnz = 0;
        int has_normals = (idx[0].vn_idx >= 0);
        if (!has_normals) {
            float e1x = px[1]-px[0], e1y = py[1]-py[0], e1z = pz[1]-pz[0];
            float e2x = px[2]-px[0], e2y = py[2]-py[0], e2z = pz[2]-pz[0];
            fnx = e1y*e2z - e1z*e2y;
            fny = e1z*e2x - e1x*e2z;
            fnz = e1x*e2y - e1y*e2x;
            float len = sqrtf(fnx*fnx + fny*fny + fnz*fnz);
            if (len > 1e-9f) { fnx /= len; fny /= len; fnz /= len; }
        }

        for (int k = 0; k < fnum - 2; k++) {
            tinyobj_vertex_index_t tri[3] = { idx[0], idx[k+1], idx[k+2] };
            for (int t = 0; t < 3; t++) {
                float nx = fnx, ny = fny, nz = fnz;
                if (has_normals) {
                    nx = attrib.normals[3 * tri[t].vn_idx + 0];
                    ny = attrib.normals[3 * tri[t].vn_idx + 1];
                    nz = attrib.normals[3 * tri[t].vn_idx + 2];
                }

                int o = n * MODEL_VERT_STRIDE;
                verts[o+0] = px[t];
                verts[o+1] = py[t];
                verts[o+2] = pz[t];
                verts[o+3] = nx;
                verts[o+4] = ny;
                verts[o+5] = nz;
                verts[o+6] = col.r / 255.0f;
                verts[o+7] = col.g / 255.0f;
                verts[o+8] = col.b / 255.0f;
                verts[o+9] = col.a / 255.0f;
                n++;
            }
        }
        face_offset += fnum;
    }

    int vao = 0, vbo = 0;
    int rc = gl3d_upload_mesh(verts, n, &vao, &vbo);
    free(verts);
    tinyobj_shapes_free(shapes, num_shapes);
    tinyobj_materials_free(materials, num_materials);
    tinyobj_attrib_free(&attrib);

    if (rc != 0) return -1;

    g_models[slot].in_use = 1;
    g_models[slot].vao = vao;
    g_models[slot].vbo = vbo;
    g_models[slot].vert_count = n;
    return slot;
}

int model_vert_count(int id) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].in_use) return 0;
    return g_models[id].vert_count;
}

void model_draw(int id, GMat4 local_model, GColor tint, int lit) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].in_use) return;
    gl3d_draw_mesh(g_models[id].vao, g_models[id].vert_count, local_model, tint, lit);
}

void model_unload(int id) {
    if (id < 0 || id >= MAX_MODELS || !g_models[id].in_use) return;
    gl3d_free_mesh(g_models[id].vao, g_models[id].vbo);
    g_models[id].in_use = 0;
    g_models[id].vao = 0;
    g_models[id].vbo = 0;
    g_models[id].vert_count = 0;
}

void model_unload_all(void) {
    for (int i = 0; i < MAX_MODELS; i++) {
        if (g_models[i].in_use) model_unload(i);
    }
}

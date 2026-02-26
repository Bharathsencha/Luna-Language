// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "value.h"
#include "env.h"

// Define Grid Dimensions
#define GRID_W 200
#define GRID_H 150
#define GRID_SIZE (GRID_W * GRID_H)

// Static Grid Data
// 0 = Empty, 1 = Sand, 2 = Water, 3 = Stone, 4 = Acid, 5 = Fire
static int sand_grid[GRID_SIZE];

// Init Grid
Value lib_sand_init(int argc, Value *argv, Env *env) {
    memset(sand_grid, 0, sizeof(sand_grid));
    srand(time(NULL)); // Seed C random
    return value_int(1);
}

// Set Cell
Value lib_sand_set(int argc, Value *argv, Env *env) {
    if (argc != 3) return value_int(0);
    int x = (int)(argv[0].i);
    int y = (int)(argv[1].i);
    int type = (int)(argv[2].i);

    if (x >= 0 && x < GRID_W && y >= 0 && y < GRID_H) {
        sand_grid[y * GRID_W + x] = type;
    }
    return value_int(1);
}

// Get Cell
Value lib_sand_get(int argc, Value *argv, Env *env) {
    if (argc != 2) return value_int(0);
    int x = (int)(argv[0].i);
    int y = (int)(argv[1].i);

    if (x >= 0 && x < GRID_W && y >= 0 && y < GRID_H) {
        return value_int(sand_grid[y * GRID_W + x]);
    }
    return value_int(3); // STONE (Border)
}

// Update Loop (FAST C Implementation)
Value lib_sand_update(int argc, Value *argv, Env *env) {
    // Iterate Bottom-Up
    for (int y = GRID_H - 2; y >= 0; y--) {
        // Simple Left-to-Right for now
        for (int x = 0; x < GRID_W; x++) {
            int idx = y * GRID_W + x;
            int type = sand_grid[idx];

            if (type == 0 || type == 3) continue; // Skip Empty/Stone

            if (type == 1) { // SAND
                int below_idx = (y + 1) * GRID_W + x;
                int below = sand_grid[below_idx];

                if (below == 0 || below == 2) { // Empty or Water
                    sand_grid[idx] = below;
                    sand_grid[below_idx] = 1;
                } else {
                    int dl_idx = (y + 1) * GRID_W + (x - 1);
                    int dr_idx = (y + 1) * GRID_W + (x + 1);
                    
                    int can_l = (x > 0 && (sand_grid[dl_idx] == 0 || sand_grid[dl_idx] == 2));
                    int can_r = (x < GRID_W - 1 && (sand_grid[dr_idx] == 0 || sand_grid[dr_idx] == 2));

                    if (can_l && can_r) {
                        if (rand() % 2 == 0) {
                            sand_grid[idx] = sand_grid[dl_idx];
                            sand_grid[dl_idx] = 1;
                        } else {
                            sand_grid[idx] = sand_grid[dr_idx];
                            sand_grid[dr_idx] = 1;
                        }
                    } else if (can_l) {
                        sand_grid[idx] = sand_grid[dl_idx];
                        sand_grid[dl_idx] = 1;
                    } else if (can_r) {
                        sand_grid[idx] = sand_grid[dr_idx];
                        sand_grid[dr_idx] = 1;
                    }
                }
            } else if (type == 2) { // WATER
                int below_idx = (y + 1) * GRID_W + x;
                if (sand_grid[below_idx] == 0) {
                    sand_grid[idx] = 0;
                    sand_grid[below_idx] = 2;
                } else {
                    int side = (rand() % 2 == 0) ? -1 : 1;
                    int s_idx = y * GRID_W + (x + side);
                    if (x + side >= 0 && x + side < GRID_W && sand_grid[s_idx] == 0) {
                        sand_grid[idx] = 0;
                        sand_grid[s_idx] = 2;
                    }
                }
            } else if (type == 4) { // ACID
                 int below_idx = (y + 1) * GRID_W + x;
                 if (sand_grid[below_idx] != 0 && sand_grid[below_idx] != 4 && sand_grid[below_idx] != 3) {
                     // Dissolve non-empty, non-acid, non-stone
                     sand_grid[below_idx] = 0;
                     sand_grid[idx] = 0; // Acid used up
                 } else if (sand_grid[below_idx] == 0) {
                     sand_grid[idx] = 0;
                     sand_grid[below_idx] = 4;
                 }
            } else if (type == 5) { // FIRE
                if (rand() % 10 < 3) { // 30% chance to die
                    sand_grid[idx] = 0; 
                } else {
                    // Rise up
                    int up_idx = (y - 1) * GRID_W + x;
                    if (y > 0 && sand_grid[up_idx] == 0) {
                        sand_grid[idx] = 0;
                        sand_grid[up_idx] = 5;
                    }
                }
            }
        }
    }
    return value_int(1);
}
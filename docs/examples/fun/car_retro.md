# Neon Retro Racer (`car_retro.lu`)

## Why This File Exists

This is the most fully-featured demo — a complete playable arcade game. It demonstrates that Luna can handle game loops, state machines, physics, collision detection, AI, scoring systems, and UI all in one file.

## Scrolling Grid (Retro Perspective Effect)

```javascript
grid_offset = grid_offset + (game_speed * 0.5) * dt
if (grid_offset >= 40.0) { grid_offset = 0.0 }
let gy = grid_offset
while (gy < SCREEN_HEIGHT) {
    draw_line(0, to_int(gy), SCREEN_WIDTH, to_int(gy), [C_GRID, alpha])
    gy = gy + 40.0
}
draw_line(0, SCREEN_HEIGHT, to_int(SCREEN_WIDTH/2 - 50), 0, [C_GRID, 50])
draw_line(SCREEN_WIDTH, SCREEN_HEIGHT, to_int(SCREEN_WIDTH/2 + 50), 0, [C_GRID, 50])
```

Horizontal lines scroll downward by advancing `grid_offset` each frame. When it reaches 40px (the grid spacing), it wraps to 0, creating an infinite scroll illusion. The two diagonal lines converge toward a vanishing point near the top center — this is a classic **fake perspective** technique that gives the 2D scene a sense of 3D depth (as if looking down a road receding into the distance).

## Smooth Player Movement

```javascript
target_x = ROAD_X + player_lane * LANE_WIDTH + (LANE_WIDTH - CAR_WIDTH) / 2
let dx = target_x - player_x
player_x = player_x + dx * 10.0 * dt
```

The player snaps to discrete lanes (0, 1, 2) but the visual position lerps smoothly toward the target. `player_x = player_x + dx * 10.0 * dt` is **exponential smoothing** — each frame, 10 * dt fraction of the remaining gap is closed. At 60FPS, `dt ≈ 0.0167`, so `10 * 0.0167 = 0.167` — 16.7% of the gap is closed each frame. This gives a fast, snappy but smooth slide between lanes.

## Traffic Spawning and Pooling

```javascript
let POOL_SIZE = 20
let t_active = [0, 0, 0, 0, ...]
```

20 traffic car slots are pre-allocated. Instead of creating/destroying objects (which requires memory allocation), we reuse pool slots. When a car is needed, we find an inactive slot (`t_active[i] == 0`) and activate it. When a car goes off-screen (`t_y[i] > SCREEN_HEIGHT + 100`), it's deactivated. This **object pooling** pattern avoids memory fragmentation and is standard in game development.

Spawn rate: a timer counts down based on difficulty. On tick, 1-2 cars are spawned in random lanes.

## Traffic AI Lane Switching

```javascript
if (rand(1000) < chance) {
    if (ln < player_lane) { new_lane = ln + 1 }
    else { if (ln > player_lane) { new_lane = ln - 1 } }
}
```

Each frame, active enemy cars roll a random number. If it falls below a difficulty-dependent threshold, the car switches lanes — biased toward the player's lane. This creates the feeling of enemies "chasing" the player. Easy mode: 0.1% chance. Hard/Nightmare: 1.0% chance. The cars then smoothly lerp to their new lane target using the same exponential smoothing as the player.

## Collision Detection

```javascript
func check_collision(px, py, pw, ph, ex, ey, ew, eh) {
    if (px < ex + ew && px + pw > ex && py < ey + eh && py + ph > ey) {
        return 1
    }
    return 0
}
```

Standard **Axis-Aligned Bounding Box (AABB)** test. Two rectangles overlap if and only if neither is fully to the left, right, above, or below the other. The player's hitbox is slightly smaller than the visual car (`+5, +5, -10, -10`) to give a small margin of forgiveness.

## Difficulty Scaling

```javascript
game_speed = speed_base + (score * inc)
if (game_speed > 2500.0) { game_speed = 2500.0 }
```

Speed increases linearly with score, capped at 2500. `inc` varies by difficulty (0.5 for Easy, 4.0 for Nightmare). The score itself grows over time (`score = score + (400.0 * 0.01) * dt`), so as the game goes on, both score and speed grow together. On Nightmare, speed can reach the cap in about 30 seconds. A 2× score bonus is awarded on Nightmare if you crash (rewarding the attempt).

## High Score Tracking

Three high scores are stored in a sorted list using a manual insertion-sort approach — the new score is compared against each slot in order and inserted where it belongs, pushing others down. This persists in memory only for the session (no file I/O).

## State Machine

The game has three states: `STATE_MENU`, `STATE_PLAY`, `STATE_GAMEOVER`. Each frame the code checks the current state and runs the appropriate update and draw logic. Transitions are triggered by keypresses (Enter/Space to play, Escape to menu, crash to game over). The background grid and road always draw regardless of state — only the foreground content changes.

---

[Back to fun/ examples](README.md)

# Car Game Documentation

This document explains the logic, physics, and customization of the **Raceee Car Game** (`cargame/cargame.lu`).

## Overview
Raceee is a top-down 2D racing technical demo. It features a simple car game with camera moment and simple graphics.

## Features
-   **Physics Engine**: Acceleration, deceleration (friction), and angular movement.
-   **Camera**: Smooth follow cam that keeps the player centered.
-   **Infinite-style World**: Uses a tiling system to simulate a large world.
-   **Menus**: Fully interactive Play/Settings/Exit menus.

---

## Controls

| Action | Key |
| :--- | :--- |
| **Accelerate** | `UP Arrow` |
| **Reverse / Brake** | `DOWN Arrow` |
| **Turn Left** | `LEFT Arrow` |
| **Turn Right** | `RIGHT Arrow` |
| **Pause / Menu** | `ESC` |
| **Select (Menu)** | `Left Mouse Click` |

---

## Code Breakdown

### 1. State Machine
The game uses a simple integer state machine to manage screens.
```python
let MENU = 0
let GAME = 1
let SETTINGS = 2
```
The `main()` loop checks `current_state` to determine which logic block to run (Menu Logic vs Game Logic).

### 2. Car Physics
The car movement is vector-based.
-   **Acceleration**: Modifies `car_speed`.
-   **Drag**: If no key is pressed, `car_speed` is multiplied by a friction factor or subtracted linearly.
-   **Rotation**: stored in `car_rotation` (Degrees).
-   **Velocity Calculation**:
    ```python
    x_move = car_speed * cos(rotation) * dt
    y_move = car_speed * sin(rotation) * dt
    ```

### 3. Rendering Optimization (Culling)
The map is created by tiling `Soil_Tile.png`. 
Instead of drawing the entire 15000x15000 world, we only draw the visible tiles around the car:
```python
let col_idx = floor(car_x / tile_width)
let start_col = col_idx - 3
let end_col = col_idx + 4
# Only loop from start_col to end_col
```
This keeps the frame rate high regardless of map size.

---

## Customization

### Adjusting Car Speed
Change the constants at the top of `cargame.lu`:
```python
let car_max_speed = 800.0  # Max pixel/sec
let car_speedup = 200.0    # Acceleration
let ROTATION_SPEED = 120.0 # Turning speed
```

### Changing the Map
1.  Replace `cargame/assets/Soil_Tile.png` with a new texture (seamless textures work best).
2.  Adjust `let world_w = 15000` to change the boundaries.

### Changing the Car
1.  Replace `cargame/assets/Car_1_01.png`.
2.  The code automatically scales the car to be ~120px wide via `scale_factor`.


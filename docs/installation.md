# Luna Installation & Setup Guide 

This document provides step-by-step instructions for setting up the Luna development environment, including its core dependencies and the graphics backend.

---

## 1. Core Build Tools

Luna requires a standard C/C++ build stack:

```bash
sudo apt update
sudo apt install gcc build-essential git cmake pkg-config
```



## 3. Raylib Setup Guide

The `fun/` demos and GUI features require **Raylib**. Follow these steps to build and install it from source.

### A. Install Graphics Dependencies

```bash
sudo apt install libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev
sudo apt install libgl1-mesa-dev libglu1-mesa-dev
```

### B. Download Raylib

```bash
git clone https://github.com/raysan5/raylib.git
cd raylib
```

### C. Build and Install

```bash
mkdir build && cd build
cmake .. -DBUILD_SHARED_LIBS=ON
make -j$(nproc)
sudo make install
```

This installs raylib into `/usr/local/lib`.

### D. Configure Library Path

```bash
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/raylib.conf
sudo ldconfig
```

> [!NOTE]
> It is recommended to restart your system or log out/in to ensure the library path changes take effect.

---

## 4. Building Luna

Once all dependencies are installed, clone the Luna repository and run:

```bash
make
```

---

## 5. Verification: Test Raylib

To verify that Raylib is correctly installed and linkable, you can compile this minimal C program:

**main.c**
```c
#include "raylib.h"

int main(void) {
    InitWindow(800, 450, "Hello Raylib!");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Raylib is working!", 190, 200, 20, LIGHTGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

**Compile & Run:**
```bash
gcc main.c -o main -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
./main
```

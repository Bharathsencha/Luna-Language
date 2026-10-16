# 3D API Smoke Tests (`scene3d.lu`, `scene_models.lu`)

Two short scripts that open a window, exercise part of the 3D API, and exit on their own. Run
them after changing anything in `gui/gl_backend_3d.c` or `gui/gui_lib_3d.c`.

```bash
make run-fun NAME=scene3d         # ~360 frames
make run-fun NAME=scene_models    # ~300 frames
```

## scene3d.lu

Covers rotated `_pro` primitives (`draw_cube_pro`, `draw_sphere_pro`, `draw_plane_pro`,
`draw_cylinder_pro`), the transform stack, `update_camera_orbit`, `draw_particle_3d`, fog, spot
light cones, `set_material`, and mouse-ray picking against a box and the ground plane.

## scene_models.lu

Loads `assets/models/test/cube.obj` with `load_model`, which reads the MTL library next to it and
bakes each material's `Kd` color into the vertex colors. The model is drawn with `draw_model` at
several positions, scales and rotations, then released with `unload_model`.

---

[Back to fun/ examples](README.md)

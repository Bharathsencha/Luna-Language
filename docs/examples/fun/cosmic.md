# Vortex and Black Hole (`vortex.lu`, `blackhole.lu`)

## The Vortex (`vortex.lu`)
Runs 18,000 particles using a combination of:
- **Central Gravity**: Pulls everything toward `(0,0)`.
- **Tangential Forces**: Rotates particles around the center.
- **Brownian Noise**: Adds random jitter to simulate turbulence.
- **Spiral Dynamics**: The tangential force is modulated by distance to keep the spiral arms defined rather than collapsing into a disk.

## The Black Hole (`blackhole.lu`)
Approximates the visual appearance of a **Schwarzschild black hole** via coordinate warping:
- **Gravitational Lensing**: Particles behind the singularity have their screen coordinates pushed outward and split into circular arcs, simulating an **Einstein Ring**.
- **Doppler Brightening**: Relative brightness (alpha) is modulated based on whether a particle's velocity is moving toward or away from the viewer, simulating relativistic optical shifts.

---

[Back to fun/ examples](README.md)

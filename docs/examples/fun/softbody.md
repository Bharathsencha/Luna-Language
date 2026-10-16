# Soft Body Physics (`boobs.lu`)

## Keyword Aliasing
This demo is famous for its use of Luna's **keyword aliasing** feature. It uses `balls`, `grab_balls`, and `spin_balls` which map directly to `let`, `func`, and `while` at the lexer level. This is purely a "skin" on the language to show off its flexibility.

## Spring-Mass System
The physics is a **discrete spring-mass system**. Each body has a center point and 18 skin points.
- **Structural Springs**: Connect the center to each skin point. They use Hooke's Law (`F = kx`) to pull points back to a rest radius, maintaining the body's circular shape.
- **Shear Springs**: Connect adjacent skin points. These resist deformation *along* the surface, preventing the points from bunching up.
- **Anchor Forces**: A soft spring pulls the center toward a resting height, allowing the body to bounce but eventually settle.

---

[Back to fun/ examples](README.md)

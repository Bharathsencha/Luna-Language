# Math Library

## These functions provide standard mathematical operations.

## Basic Utility

| Function             | Description                                       | Example Code            |
| -------------------- | ------------------------------------------------- | ----------------------- |
| `abs(x)`             | Returns the absolute value of x.                  | `abs(-10) → 10`         |
| `min(a, b)`          | Returns the smaller of two values.                | `min(10, 5) → 5`        |
| `max(a, b)`          | Returns the larger of two values.                 | `max(10, 5) → 10`       |
| `clamp(x, min, max)` | Constrains x between min and max.                 | `clamp(15, 0, 10) → 10` |
| `sign(x)`            | Returns -1 if negative, 1 if positive, 0 if zero. | `sign(-50) → -1`        |

## Powers & Roots

| Function         | Description                              | Example Code        |
| ---------------- | ---------------------------------------- | ------------------- |
| `pow(base, exp)` | Returns base raised to the power of exp. | `pow(2, 3) → 8.0`   |
| `sqrt(x)`        | Returns the square root of x.            | `sqrt(16) → 4.0`    |
| `cbrt(x)`        | Returns the cube root of x.              | `cbrt(27) → 3.0`    |
| `exp(x)`         | Returns e raised to the power of x.      | `exp(1) → 2.718...` |
| `ln(x)`          | Returns the natural logarithm of x.      | `ln(10) → 2.302...` |
| `log10(x)`       | Returns the base-10 logarithm of x.      | `log10(100) → 2.0`  |

## Rounding

| Function    | Description                                    | Example Code        |
| ----------- | ---------------------------------------------- | ------------------- |
| `floor(x)`  | Rounds down to the nearest integer.            | `floor(5.9) → 5`    |
| `ceil(x)`   | Rounds up to the nearest integer.              | `ceil(5.1) → 6`     |
| `round(x)`  | Rounds to the nearest integer.                 | `round(5.5) → 6`    |
| `trunc(x)`  | Removes the fractional part.                   | `trunc(5.9) → 5`    |
| `fract(x)`  | Returns only the fractional part.              | `fract(5.9) → 0.9`  |
| `mod(x, y)` | Returns the floating-point remainder of x / y. | `mod(5.5, 2) → 1.5` |

## Trigonometry

| Function      | Description                                    | Example Code               |
| ------------- | ---------------------------------------------- | -------------------------- |
| `sin(rad)`    | Returns sine of an angle (in radians).         | `sin(0) → 0.0`             |
| `cos(rad)`    | Returns cosine of an angle (in radians).       | `cos(0) → 1.0`             |
| `tan(rad)`    | Returns tangent of an angle (in radians).      | `tan(0) → 0.0`             |
| `asin(x)`     | Returns arc sine.                              | `asin(0) → 0.0`            |
| `acos(x)`     | Returns arc cosine.                            | `acos(1) → 0.0`            |
| `atan(x)`     | Returns arc tangent.                           | `atan(0) → 0.0`            |
| `atan2(y, x)` | Returns arc tangent of y/x (correct quadrant). | `atan2(10, 10) → 0.785...` |

## Hyperbolic

| Function  | Description                 | Example Code    |
| --------- | --------------------------- | --------------- |
| `sinh(x)` | Returns hyperbolic sine.    | `sinh(0) → 0.0` |
| `cosh(x)` | Returns hyperbolic cosine.  | `cosh(0) → 1.0` |
| `tanh(x)` | Returns hyperbolic tangent. | `tanh(0) → 0.0` |

## Random

| Function            | Description                                   | Example Code         |
| ------------------- | --------------------------------------------- | -------------------- |
| `rand()`            | Returns a random float between 0.0 and 1.0.   | `rand() → 0.42...`   |
| `randint(min, max)` | Returns a random integer between min and max. | `randint(1, 10) → 7` |
| `srand(seed)`       | Seeds the random number generator.            | `srand(123)`         |

## Conversions

| Function          | Description                                | Example Code                  |
| ----------------- | ------------------------------------------ | ----------------------------- |
| `deg_to_rad(deg)` | Converts degrees to radians.               | `deg_to_rad(180) → 3.14...`   |
| `rad_to_deg(rad)` | Converts radians to degrees.               | `rad_to_deg(3.14) → 179.9...` |
| `lerp(a, b, t)`   | Linear interpolation between a and b by t. | `lerp(0, 100, 0.5) → 50.0`    |

----

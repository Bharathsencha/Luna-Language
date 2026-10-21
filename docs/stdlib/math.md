# Math

Numeric functions, random numbers, and vector math. All of these are builtins. Arguments must
be numbers (an int or a float); anything else is a `Type Error` such as
`sqrt() argument 1 must be a number, got string`. Results that are not whole numbers print
with up to six significant digits, so `sqrt(2)` prints `1.41421`, and a float without a
fractional part prints without `.0` (`sqrt(16)` prints `4`). The `4.0` shown below means "a float".

## Basic utilities

| Function | Description | Example |
|---|---|---|
| `abs(x)` | Absolute value, same type as `x` | `abs(-10)` is `10` |
| `min(a, b)` | The smaller of two values | `min(10, 5)` is `5` |
| `max(a, b)` | The larger of two values | `max(10, 5)` is `10` |
| `clamp(x, lo, hi)` | Constrains `x` to the range `[lo, hi]` | `clamp(15, 0, 10)` is `10` |
| `sign(x)` | `-1`, `0` or `1` | `sign(-50)` is `-1` |

## Powers and roots

These always return a float.

| Function | Description | Example |
|---|---|---|
| `pow(base, exp)` | `base` raised to `exp` | `pow(2, 3)` is `8.0` |
| `sqrt(x)` | Square root; a negative argument gives `nan` | `sqrt(16)` is `4.0` |
| `cbrt(x)` | Cube root | `cbrt(27)` is `3.0` |
| `exp(x)` | e raised to `x` | `exp(1)` is `2.718...` |
| `ln(x)` | Natural logarithm | `ln(10)` is `2.302...` |
| `log10(x)` | Base-10 logarithm | `log10(100)` is `2.0` |

## Rounding

`floor`, `ceil`, `round` and `trunc` return an int. `fract` and `mod` return a float.

| Function | Description | Example |
|---|---|---|
| `floor(x)` | Rounds down | `floor(5.9)` is `5` |
| `ceil(x)` | Rounds up | `ceil(5.1)` is `6` |
| `round(x)` | Nearest integer, halves away from zero | `round(5.5)` is `6`, `round(-5.5)` is `-6` |
| `trunc(x)` | Removes the fractional part (toward 0) | `trunc(5.9)` is `5` |
| `fract(x)` | The fractional part | `fract(5.9)` is `0.9` |
| `mod(x, y)` | Floating-point remainder of `x / y` | `mod(5.5, 2)` is `1.5` |

## Trigonometry

Angles are in radians. All of these return a float.

| Function | Description |
|---|---|
| `sin(x)`, `cos(x)`, `tan(x)` | Sine, cosine, tangent |
| `asin(x)`, `acos(x)`, `atan(x)` | Inverse functions |
| `atan2(y, x)` | Arc tangent of `y / x` with the correct quadrant: `atan2(10, 10)` is `0.785...` |
| `sinh(x)`, `cosh(x)`, `tanh(x)` | Hyperbolic functions |
| `deg_to_rad(d)`, `rad_to_deg(r)` | Angle conversion: `deg_to_rad(180)` is `3.14159...` |
| `lerp(a, b, t)` | Linear interpolation: `lerp(0, 100, 0.5)` is `50.0` |

## Random numbers

| Function | Description |
|---|---|
| `rand()` | A float in `[0, 1)` |
| `rand(max)` | An integer from `0` to `max`, both ends included |
| `rand(min, max)` | An integer from `min` to `max`, both ends included (swapped if `min > max`) |
| `srand(seed)` | Seeds the generator, for reproducible sequences. `srand()` reseeds from the OS |
| `trand()` | A 64-bit integer read from the OS entropy source (`/dev/urandom`) |

The generator is xoroshiro128++, and `srand` expands the seed with SplitMix64. Without `srand`,
every program run starts from an OS-entropy seed. `shuffle(list)` and `sort(list)` are in
[core.md](core.md).

```javascript
srand(7)
print(rand(1, 100))   # 17
print(rand(1, 100))   # 100
```

Numbers from a seed are fixed by the generator, so the same seed gives the same numbers on
every run.

## Dense lists and vector math

A dense list is a contiguous array of floats, created with `dense_list(n, fill)` (the fill
value must be a float). It prints as `d[...]`, its `type()` is `list`, indexing works as on a
list, and its size is fixed: `append` does not accept it.

| Function | Description |
|---|---|
| `dense_list(size, fill)` | `dense_list(3, 1.5)` is `d[1.5, 1.5, 1.5]` |
| `vec_add(a, b)`, `vec_sub(a, b)`, `vec_mul(a, b)`, `vec_div(a, b)` | Element-wise operation on two lists of numbers; returns a new dense list as long as the shorter input |
| `vec_mul_inline(a, b)` | Multiplies `a` by `b` in place, without allocating |
| `mat_mul(a, b)` | Matrix product of two lists of rows (lists or dense lists); returns a list of dense rows |

`+ - * /` between two lists of numbers do the same as the `vec_*` functions:
`[1, 2] + [3, 4]` is `d[4, 6]`.

```javascript
print(vec_mul([1, 2], [3, 4]))                      # d[3, 8]
print(mat_mul([[1, 2], [3, 4]], [[5, 6], [7, 8]]))  # [d[19, 22], d[43, 50]]
```

`mat_mul` is a plain triple loop that runs on one thread. If the column count of `a` does not
equal the row count of `b`, it prints `Runtime Error: Matrix dimension mismatch` and returns
`null` without stopping the program.

## Example: distance between two points

```javascript
func distance(x1, y1, x2, y2) {
    let dx = x2 - x1
    let dy = y2 - y1
    return sqrt(pow(dx, 2) + pow(dy, 2))
}

print(distance(0, 0, 3, 4))   # 5
```

## Example: dice statistics

```javascript
func roll_dice(num_rolls) {
    srand(42)
    let total = 0
    let lowest = 6
    let highest = 1
    for (let i = 0; i < num_rolls; i++) {
        let roll = rand(1, 6)
        total += roll
        lowest = min(lowest, roll)
        highest = max(highest, roll)
    }
    print("total {total}")
    print("average {float(total) / float(num_rolls)}")
    print("min {lowest}, max {highest}")
}

roll_dice(10)
```

## Example: shuffle and sort

```javascript
let deck = range(1, 11)
shuffle(deck)
print(len(deck))      # 10
sort(deck)
print(deck)           # [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
```

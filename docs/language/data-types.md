# Data Types

Luna has four ways to define your own structured values:

| Type | Mutable | Memory | Best for |
|---|---|---|---|
| `data` | yes | GC heap (a map) | quick records |
| `template` | yes | GC heap, schema-checked | entities, game state, anything that changes |
| `bloc` | no | small GC object, at most 32 bytes | vectors, colors, small immutable values |
| `box` | yes | manual byte buffer, freed at scope end | scratch bytes, up to 4096 |

All of their rules are enforced the same way in the bytecode VM and the legacy interpreter.
`make test` checks every rule on both engines.

## data

```javascript
data Vec2 { x, y }
let p = Vec2(3, 4)
p.x = 9
print(p.x)          # 9
print(shape(p))     # Vec2
```

A `data` value is a map with a constructor. `type(p)` is `"map"`.

## template

```javascript
template Player { name, hp, pos }
let hero = Player("Astra", 100, [0, 0])
hero.hp = 80
print(hero.hp)      # 80
```

Templates are checked against their schema:

| Rule | Error |
|---|---|
| The constructor needs exactly one value per field | `Data constructor 'Player' expects 3 field value(s), but got 1` |
| Reading or writing an undeclared field | `Template field 'z' does not exist` |
| Field names must be unique | `template has duplicate field names` |
| A name keeps one field list (an identical redefinition is fine) | `template 'A' is already defined with different fields` |

A template field can hold anything, including another template, a bloc, or a box.

## bloc

A bloc is a small immutable value. Each field holds an int, float, bool or char, or another
bloc.

```javascript
bloc Vec2 { x, y }
let a = Vec2{1, 2}
let b = Vec2{1.5, 2.5}      # field kinds can differ between instances
print(a == Vec2{1, 2})      # true: blocs compare by value

bloc Rect { min, max }
let r = Rect{Vec2{0, 0}, Vec2{10, 10}}
print(r.max.x)              # 10
```

| Rule | Error |
|---|---|
| At most 32 bytes: every primitive field takes 8, a nested bloc takes its own size | `bloc exceeds the cache-line size limit (32 bytes)` |
| Fields hold only int, float, bool, char, or a bloc | `Bloc field 'name' only accepts int, float, bool, char, or another bloc` |
| A nested-bloc field keeps the same bloc type | `Bloc 'R' field 'min' must use the same nested bloc type` |
| The constructor needs one value per field | `Bloc 'Vec2' expects 2 field value(s), but got 1` |
| Blocs are immutable | `bloc values are immutable` |
| Unknown fields | `Field 'z' does not exist on this bloc value` |
| Unique field names, one field list per name | `bloc has duplicate field names`, `bloc 'V' is already defined with different fields` |

So a bloc holds at most four numbers, for example `Color{r, g, b, a}`. For anything bigger or
mutable, use a template.

## box

A box is a fixed-size byte buffer that you own. It is freed automatically when the scope that
created it ends, or earlier with `free()`.

```javascript
let buf = box[16]
buf[0] = 200          # bytes are integers 0-255
buf[-1] = 7           # negative indexes count from the end
print(buf[0], buf.len)
free(buf)
```

| Rule | Error |
|---|---|
| Size from 1 to 4096 bytes | `box size must be between 1 and 4096 bytes` |
| At most 4096 boxes live at once (freed boxes are reused) | `box allocation failed: more than 4096 boxes are live at once` |
| Index from `0` to `len - 1` (or negative from the end) | `box index 2 is out of bounds for a box of 2 bytes` |
| Bytes are integers 0-255 | `box bytes must be integers from 0 to 255` |
| No use after free, including `.len` and `len()` | `box was already freed (use-after-free)` |
| Free exactly once | `box was already freed (double free)` |
| A box must not outlive its scope | `a box cannot be returned from a function`, `... stored in a list or map`, `... assigned to a global from inside a function` |

To keep a box beyond its scope, put it in a template. The template then owns it, and the box
lives as long as the template:

```javascript
template Packet { bytes }

func make_packet() {
    let b = box[4]
    b[0] = 9
    return Packet(b)        # allowed: the template owns the box
}

let p = make_packet()
print(p.bytes[0])           # 9
```

## Inside unsafe blocks

`box` and `bloc` values may not be created inside `unsafe { }`
(`box and bloc values are not allowed inside unsafe blocks`). Use `alloc()` for raw memory there.
See [internals/unsafe.md](../internals/unsafe.md).

## How it works

- **bloc**: a small GC object (`BlocObj` in `src/value.c`) holding up to 32 bytes plus one kind
  byte per 8-byte slot. Bloc objects have no outgoing references, so the GC never traces into
  them. Reading a nested bloc field copies its bytes into a new bloc.
- **box**: a slot in a fixed table of 4096 entries. The handle encodes a generation number,
  which is bumped when the box is freed, so a reused slot never resurrects an old handle. The
  Rust runtime (`rust/data_rt`) tracks which handles are live.
- **template**: a GC object with one `Value` per field, plus write barriers. A box stored in a
  template is marked as template-owned and freed when the template is collected.
- **Validation**: names, duplicate fields, the 32-byte cap, box sizes and template schemas are
  checked by `libluna_data_rt.a` (`rust/data_rt`). The C side reports the error with the right
  line.

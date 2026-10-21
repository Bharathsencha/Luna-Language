# Strings

Text manipulation, searching and formatting. All of these are builtins. Strings are
byte strings: `len("héllo")` is 6, and indexing is zero-based by byte. `to_upper` and
`to_lower` change ASCII letters only.

## Inspection

| Function | Description | Example |
|---|---|---|
| `len(x)` | Length of a string, list, map or box | `len("abc")` is `3` |
| `str_len(s)` | Length of a string (same as `len` for strings) | `str_len("abc")` is `3` |
| `is_empty(s)` | `true` if the string has length 0; a non-string is a `Type Error` | `is_empty("")` is `true` |
| `is_digit(s)`, `is_alpha(s)`, `is_alnum(s)`, `is_space(s)` | `true` if the string (or char) is non-empty and every character is in the class | `is_digit("123")` is `true`, `is_digit("1a")` is `false` |

## Slicing and access

| Function | Description | Example |
|---|---|---|
| `char_at(s, i)` | The character at index `i` as a one-character string; `""` when out of range | `char_at("ABC", 1)` is `"B"` |
| `substring(s, start, n)` | `n` characters starting at `start`; the result is cut at the end of the string | `substring("Hello", 0, 2)` is `"He"` |
| `slice(s, start, end)` | From `start` up to, not including, `end` (bounds are clamped) | `slice("Hello", 1, 4)` is `"ell"` |

`s[i]` returns a `char` (not a string): `"Luna"[0]` is `'L'` and `"Luna"[-1]` is `'a'`. A
`char` and a one-character string are different types: `'a' == "a"` is an error, but both
work with `contains`, `index_of`, `starts_with` and `ends_with`.

## Searching

| Function | Description | Example |
|---|---|---|
| `contains(s, sub)` | `true` if `sub` is found in `s`; also works on a list | `contains("Team", "ea")` is `true` |
| `index_of(s, sub)` | Index of the first occurrence, `-1` if none | `index_of("banana", "nan")` is `2` |
| `last_index_of(s, sub)` | Index of the last occurrence, `-1` if none | `last_index_of("banana", "a")` is `5` |
| `starts_with(s, pre)` | `true` if `s` starts with `pre` | `starts_with("file.txt", "file")` is `true` |
| `ends_with(s, suf)` | `true` if `s` ends with `suf` | `ends_with("file.txt", ".txt")` is `true` |

## Manipulation

| Function | Description | Example |
|---|---|---|
| `concat(s1, s2)` | Joins two strings; both must be strings | `concat("He", "llo")` is `"Hello"` |
| `to_upper(s)`, `to_lower(s)` | Case conversion (ASCII) | `to_upper("abc")` is `"ABC"` |
| `trim(s)`, `trim_left(s)`, `trim_right(s)` | Remove whitespace from both ends, the start or the end | `trim("  hi  ")` is `"hi"` |
| `replace(s, old, new)` | Replaces all occurrences of `old` with `new`. An empty `old` changes nothing | `replace("foobar", "o", "a")` is `"faabar"` |
| `reverse(s)` | Reverses a string (also a list) | `reverse("Luna")` is `"anuL"` |
| `repeat(s, n)` | `n` copies of `s`; `""` when `n` is 0 or less | `repeat("Na", 3)` is `"NaNaNa"` |
| `pad_left(s, w, ch)` | Pads the start of `s` with the character `ch` up to width `w` | `pad_left("7", 3, "0")` is `"007"` |
| `pad_right(s, w, ch)` | Pads the end of `s` | `pad_right("Ok", 5, ".")` is `"Ok..."` |
| `format(s, ...)` | Replaces each `{}` with the next argument; extra placeholders stay as `{}` | `format("Hi {}", "Luna")` is `"Hi Luna"` |
| `to_string(v)` | Text of any value | `to_string(12)` is `"12"` |
| `to_int(s)`, `to_float(s)` | Parse a number; `0` when the text has no leading number | `to_int("12")` is `12` |

`replace` always replaces every match; there is no single-occurrence version. `+` joins two
strings only: `"a" + 1` is a `Type Error`, so write `"a{1}"` or `"a" + to_string(1)`.

## Lists and strings

| Function | Description | Example |
|---|---|---|
| `split(s, delim)` | Splits into a list of strings. Empty pieces are dropped. An empty delimiter splits into characters | `split("a,b,c", ",")` is `["a", "b", "c"]` |
| `join(list, delim)` | Joins list items (converted to text) with the delimiter | `join(["a", "b", "c"], "-")` is `"a-b-c"` |

## Interpolation

A string literal evaluates any expression written inside `{ }` and inserts its text.

| Syntax | Result |
|---|---|
| `"Hello {name}"` | `"Hello Luna"` when `name` is `"Luna"` |
| `"sum = {1 + 2}"` | `"sum = 3"` |
| `"slot = {items[0]}"` | `"slot = 7"` when `items` is `[7]` |

The text inside `{ }` is parsed as a normal Luna expression. Write `\{` for a literal opening
brace. `format()` is still useful when you want positional `{}` placeholders.

Values print as follows: integers in decimal, floats with up to six significant digits
(`1.0` prints as `1`), `true` / `false`, `null`, lists as `[1, 2]` (strings inside a list are
not quoted), and maps as `{"key": value}`.

```javascript
let name = "Nova"
let hp = 87
let items = [3, 4]

print("Pilot {name} has {hp} hp")
print("next={hp + 1}, first={items[0]}")
print(replace("one one one", "one", "two"))
```

Output:

```
Pilot Nova has 87 hp
next=88, first=3
two two two
```

## Example: username validator

```javascript
func validate_username(username) {
    let clean = trim(username)
    if (is_empty(clean)) {
        print("Error: username cannot be empty")
        return false
    }
    let n = len(clean)
    if (n < 3) {
        print("Error: username too short (min 3 characters)")
        return false
    }
    if (n > 20) {
        print("Error: username too long (max 20 characters)")
        return false
    }
    if (is_digit(char_at(clean, 0))) {
        print("Error: username cannot start with a number")
        return false
    }
    print("username valid: {clean}")
    return true
}

validate_username("  john123  ")
validate_username("a")
validate_username("9gamer")
```

## Example: text formatter

```javascript
func format_title(text) {
    let upper = to_upper(text)
    let border = repeat("=", len(upper) + 4)
    print(border)
    print("| " + upper + " |")
    print(border)
}

func censor_text(text, bad_word) {
    return replace(text, bad_word, repeat("*", len(bad_word)))
}

format_title("welcome to luna")
print(join(["apples", "bread", "milk"], ", "))
print(censor_text("This is a darn good example", "darn"))
```



# String Library

### These functions facilitate text manipulation and inspection.
---
## Inspection

| Function      | Description                     | Example Code          |
| ------------- | ------------------------------- | --------------------- |
| `str_len(s)`  | Returns the length of string s. | `str_len("abc") → 3`  |
| `is_empty(s)` | Returns true if s length is 0.  | `is_empty("") → true` |

## Slicing & Access

| Function                   | Description                                | Example Code                      |
| -------------------------- | ------------------------------------------ | --------------------------------- |
| `char_at(s, i)`            | Returns character at index i as a string.  | `char_at("ABC", 1) → "B"`         |
| `substring(s, start, len)` | Extracts len chars starting at start.      | `substring("Hello", 0, 2) → "He"` |
| `slice(s, start, end)`     | Extracts from start index up to end index. | `slice("Hello", 1, 4) → "ell"`    |

## Searching

| Function                | Description                               | Example Code                             |
| ----------------------- | ----------------------------------------- | ---------------------------------------- |
| `contains(s, sub)`      | Returns true if sub is inside s.          | `contains("Team", "ea") → true`          |
| `index_of(s, sub)`      | Returns index of first occurrence of sub. | `index_of("banana", "nan") → 2`          |
| `last_index_of(s, sub)` | Returns index of last occurrence of sub.  | `last_index_of("banana", "a") → 5`       |
| `starts_with(s, pre)`   | Returns true if s starts with pre.        | `starts_with("file.txt", "file") → true` |
| `ends_with(s, suf)`     | Returns true if s ends with suf.          | `ends_with("file.txt", ".txt") → true`   |

## Manipulation

| Function                | Description                           | Example Code                             |
| ----------------------- | ------------------------------------- | ---------------------------------------- |
| `concat(s1, s2)`        | Concatenates two strings.             | `concat("He", "llo") → "Hello"`          |
| `to_upper(s)`           | Converts string to uppercase.         | `to_upper("abc") → "ABC"`                |
| `to_lower(s)`           | Converts string to lowercase.         | `to_lower("ABC") → "abc"`                |
| `trim(s)`               | Removes whitespace from both ends.    | `trim("  hi  ") → "hi"`                  |
| `trim_left(s)`          | Removes whitespace from the start.    | `trim_left("  hi") → "hi"`               |
| `trim_right(s)`         | Removes whitespace from the end.      | `trim_right("hi  ") → "hi"`              |
| `replace(s, old, new)`  | Replaces occurrences of old with new. | `replace("foobar", "o", "a") → "faabar"` |
| `reverse(s)`            | Reverses the string.                  | `reverse("Luna") → "anuL"`               |
| `repeat(s, n)`          | Repeats string s, n times.            | `repeat("Na", 3) → "NaNaNa"`             |
| `pad_left(s, w, char)`  | Pads start of s to width w with char. | `pad_left("7", 3, "0") → "007"`          |
| `pad_right(s, w, char)` | Pads end of s to width w with char.   | `pad_right("Ok", 5, ".") → "Ok..."`      |

## Lists & Formatting

| Function            | Description                         | Example Code                            |
| ------------------- | ----------------------------------- | --------------------------------------- |
| `split(s, delim)`   | Splits string into a list by delim. | `split("a,b,c", ",") → ["a", "b", "c"]` |
| `join(list, delim)` | Joins a list of strings into        |                                         |

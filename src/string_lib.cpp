// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <locale>
#include <ctype.h>
#include <luna/string_lib.h>
#include <luna/mystr.h> 
#include <luna/luna_error.h>

// Helpers
static int check_args(int argc, int expected, std::string_view name)
{
    if (argc != expected)
    {
        fprintf(stderr, "Runtime Error: %s() takes %d arguments.\n", name.data(), expected);
        return 0;
    }
    return 1;
}

// Helper to ensure an argument is a string and return it
static std::string_view get_str_arg(Value *argv, int index)
{
    if (argv[index].type != VAL_STRING)
    {
        return {};
    }
    return argv[index].s;
}

// Basic Operations
// consolidated len() implementation in string_lib.c or a general lib file
Value lib_len(int argc, Value *argv)
{
    if (argc != 1)
    {
        error_report
        (
            ERR_ARGUMENT, 
            0, 
            0, 
            "len() expects exactly 1 argument", 
            "Usage: len(variable)"
        );
        return value_null();
    }

    Value v = argv[0];
    if (v.type == VAL_STRING)
    {
        std::string_view s = get_str_arg(&v, 0);
        return value_int(static_cast<long long>(s.size()));
    }
    else if (v.type == VAL_LIST)
    {
        return value_int(static_cast<long long>(v.list.count));
    }
    else
    {
        error_report
        (
            ERR_TYPE, 
            0, 
            0, 
            "len() cannot be used on this type",
            "len() works on strings and lists."
        );
        return value_null();
    }
}

Value lib_str_len(int argc, Value *argv)
{
    if (argc != 1)
    {
        error_report
        (
            ERR_ARGUMENT,
            0, 0,
            "len() expects exactly 1 argument",
            "Usage: len(variable)"
        );
        return value_null();
    }

    Value v = argv[0];
    switch (v.type)
    {
        case VAL_STRING:
        {
            // Use the stored length (O(1)) instead of strlen
            std::string_view s = get_str_arg(&v, 0); 
            return value_int(static_cast<long long>(s.size()));
        }
        case VAL_LIST:
        {
            return value_int
            (
                static_cast<long long>(v.list.count)
            );
        }
            
        default:
            error_report
            (
                ERR_TYPE,
                0, 0,
                "len() cannot be used on this type",
                "len() works on strings and lists."
            );
            return value_null();
    }
}

Value lib_str_is_empty(int argc, Value *argv)
{
    if (!check_args(argc, 1, "is_empty"))
    {
        return value_null();
    }
        
    std::string_view s = get_str_arg(argv, 0);
    return value_bool(s.empty());
}

Value lib_str_concat(int argc, Value *argv)
{
    if (!check_args(argc, 2, "concat"))
    {
        return value_null();
    }

    // Get string_views directly from the arguments
    std::string_view s1 = get_str_arg(argv, 0);
    std::string_view s2 = get_str_arg(argv, 1);

    // Preallocate the concatenated string
    std::string result;
    if ((s1.size() + s2.size()) > 15) 
    {
        result.reserve(s1.size() + s2.size());
    }

    result.append(s1);
    result.append(s2);

    return make_string_value(result);
}

// Slicing
Value lib_str_substring(int argc, Value *argv)
{
    if (!check_args(argc, 3, "substring"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    if (s.empty()) return make_string_value({});

    long long start = argv[1].i;
    long long len = argv[2].i;
    long long str_len = static_cast<long long>(s.size());

    if (start < 0) start = 0;
    if (start >= str_len) return make_string_value({});
    if (len < 0) len = 0;
    if (start + len > str_len) len = str_len - start;

    // Directly construct a std::string from the substring view
    std::string sub = std::string(s.substr(start, len));

    return make_string_value(sub);
}

Value lib_str_slice(int argc, Value *argv)
{
    if (!check_args(argc, 3, "slice"))
        return value_null();

    // --- List support ---
    if (argv[0].type == VAL_LIST)
    {
        Value src = argv[0];
        long long start = argv[1].i;
        long long end = argv[2].i;
        long long count = src.list.count;

        // Handle negative indices
        if (start < 0) start += count;
        if (end < 0) end += count;

        // Clamp
        if (start < 0) start = 0;
        if (end > count) end = count;
        if (start >= end) return value_list();

        Value result = value_list();
        for (long long i = start; i < end; i++)
        {
            value_list_append(&result, src.list.items[i]);
        }   
        
        return result;
    }

    std::string_view s = get_str_arg(argv, 0);
    if (s.empty()) return make_string_value({});

    long long start = argv[1].i;
    long long end = argv[2].i;
    long long str_len = static_cast<long long>(s.size());

    if (start < 0) start += str_len;
    if (end < 0) end += str_len;

    start = std::max(start, 0LL);
    end = std::min(end, str_len);
    if (start >= end) return make_string_value({});

    // Create a string that Value will own
    std::string sub(s.data() + start, end - start);
    return make_string_value(sub); 
}

Value lib_str_char_at(int argc, Value *argv)
{
    if (!check_args(argc, 2, "char_at"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    if (s.empty())
    {
        return make_string_value({});  
    }

    long long idx = argv[1].i;

    if (idx < 0 || static_cast<size_t>(idx) >= s.size())
    {
        return make_string_value({});  
    }

    // Create string_view of single character and use make_string_value
    std::string_view single_char = s.substr(static_cast<size_t>(idx), 1);
    return make_string_value(single_char); 
}

//  Searching
Value lib_str_index_of(int argc, Value *argv)
{
    if (!check_args(argc, 2, "index_of"))
    {
        return value_null();
    }

    std::string_view haystack = get_str_arg(argv, 0);
    std::string_view needle   = get_str_arg(argv, 1);

    if (haystack.empty() || needle.empty())
    {
        return value_int(-1);
    }

    size_t pos = haystack.find(needle);
    if (pos == std::string_view::npos)
    {
        return value_int(-1);
    }

    return value_int(static_cast<long long>(pos));
}

Value lib_str_last_index_of(int argc, Value *argv)
{
    if (!check_args(argc, 2, "last_index_of"))
    {
        return value_null();
    }

    std::string_view haystack = get_str_arg(argv, 0);
    std::string_view needle   = get_str_arg(argv, 1);

    if (haystack.empty() || needle.empty())
    {
        return value_int(-1);
    }

    size_t pos = haystack.rfind(needle);
    if (pos == std::string_view::npos)
    {
        return value_int(-1);
    }

    return value_int(static_cast<long long>(pos));
}

Value lib_str_contains(int argc, Value *argv)
{
    Value idx = lib_str_index_of(argc, argv);
    int found = (idx.i != -1);
    value_free(idx); // Just checking existence
    return value_bool(found);
}

Value lib_str_starts_with(int argc, Value *argv)
{
    if (!check_args(argc, 2, "starts_with"))
    {
        return value_null();
    }

    std::string_view s   = get_str_arg(argv, 0);
    std::string_view pre = get_str_arg(argv, 1);

    if (s.size() < pre.size())
    {
        return value_bool(0);
    }

    return value_bool(s.starts_with(pre));
}

Value lib_str_ends_with(int argc, Value *argv)
{
    if (!check_args(argc, 2, "ends_with"))
    {
        return value_null();
    }

    std::string_view s   = get_str_arg(argv, 0);
    std::string_view suf = get_str_arg(argv, 1);
    
    if (s.size() < suf.size())
    {
        return value_bool(0);
    }

    return value_bool(s.ends_with(suf));
}

// Transformations
Value lib_str_to_upper(int argc, Value *argv)
{
    if (!check_args(argc, 1, "to_upper"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    if (s.empty())
    {
        return make_string_value({});
    }

    // Construct std::string directly from string_view
    std::string result(s);
    std::transform
    (
        result.begin(), result.end(), result.begin(),
        [](unsigned char c)
        {
            return std::toupper(c); 
        }
    );

    return make_string_value(result);
}

Value lib_str_to_lower(int argc, Value *argv)
{
    if (!check_args(argc, 1, "to_lower"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    if (s.empty())
    {
        return make_string_value({});
    }

    std::string result(s);
    std::transform
    (
        result.begin(), result.end(), result.begin(),
        [](unsigned char c)
        {
            return std::tolower(c);
        }
    );

    return make_string_value(result);
}

Value lib_str_trim(int argc, Value *argv)
{
    if (!check_args(argc, 1, "trim"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    if (s.empty())
    {
        return make_string_value({});
    }

    size_t start = 0;
    size_t end = s.size();

    // Trim left
    while 
    (
        start < end &&
        std::isspace
        (
            static_cast<unsigned char>(s[start])
        )
    ) ++start;

    // Trim right
    while
    (
        end > start &&
        std::isspace
        (
            static_cast<unsigned char>(s[end - 1])
        )
    ) --end;

    if (start >= end)
    {
        return make_string_value({});
    }   

    return make_string_value(s.substr(start, end - start));
}

Value lib_str_trim_left(int argc, Value *argv)
{
    if (!check_args(argc, 1, "trim_left"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    if (s.empty())
    {
        return make_string_value({});
    }

    size_t start = 0;
    const size_t n = s.size();

    while 
    (
        start < n &&
        std::isspace
        (
            static_cast<unsigned char>(s[start])
        )
    )
    {
        ++start;
    }

    if (start >= n)
    {
        return make_string_value({});
    }

    return make_string_value(s.substr(start));
}


Value lib_str_trim_right(int argc, Value *argv)
{
    if (!check_args(argc, 1, "trim_right"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    if (s.empty())
    {
        return make_string_value({}); 
    }

    // Trim trailing whitespace
    while 
    (
        !s.empty() && 
        std::isspace
        (
            static_cast<unsigned char>(s.back())
        )
    )
    {
        s.remove_suffix(1);
    }

    // Return a new Value containing the trimmed string
    return make_string_value(s);
}

Value lib_str_replace(int argc, Value *argv)
{
    if (!check_args(argc, 3, "replace"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    std::string_view old = get_str_arg(argv, 1);
    std::string_view new_str = get_str_arg(argv, 2);

    if (s.empty() || old.empty())
    {
        return make_string_value(s);
    }  
        
    std::string result;
    result.reserve(s.size()); 

    size_t pos = 0;
    while (pos < s.size())
    {
        size_t found = s.find(old, pos);
        if (found == std::string_view::npos)
        {
            result.append(s.substr(pos));
            break;
        }
        result.append(s.substr(pos, found - pos)); // append segment before old
        result.append(new_str);                    // append replacement
        pos = found + old.size();                  // move past the matched old string
    }

    return make_string_value(result);
}

Value lib_str_reverse(int argc, Value *argv)
{
    if (!check_args(argc, 1, "reverse"))
    {
        return value_null();
    }   

    std::string_view s = get_str_arg(argv, 0);
    if (s.empty())
    {
        return make_string_value({});
    }

    std::string rev(s.rbegin(), s.rend());
    return make_string_value(rev);
}

Value lib_str_repeat(int argc, Value *argv)
{
    if (!check_args(argc, 2, "repeat"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    long long count = argv[1].i;

    if (s.empty() || count <= 0)
    {
        return make_string_value({});
    }
        
    size_t len = s.size();
    std::string result;
    result.reserve(len * count);  

    for (long long i = 0; i < count; i++)
    {
        result.append(s);
    }

    return make_string_value(result);
}

Value lib_str_pad_left(int argc, Value *argv)
{
    if (!check_args(argc, 3, "pad_left"))
    {
        return value_null();
    }
        
    std::string_view s = get_str_arg(argv, 0);
    long long width = argv[1].i;
    
    std::string_view pad_char_str = get_str_arg(argv, 2);
    char pad_c = (pad_char_str.empty()) ? ' ' : pad_char_str[0];

    if (s.empty())
    {
        return make_string_value({});
    }

    size_t len = s.size();
    if (static_cast<long long>(len) >= width)
    {
        return make_string_value(s);
    }

    size_t pad_len = width - len;
    std::string result;
    result.reserve(width);

    // Fill padding
    result.append(pad_len, pad_c);

    // Append original string
    result.append(s);

    return make_string_value(result);
}

Value lib_str_pad_right(int argc, Value *argv)
{
    if (!check_args(argc, 3, "pad_right"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    long long width = argv[1].i;
    
    std::string_view pad_char_view = get_str_arg(argv, 2);
    char pad_c = pad_char_view.empty() ? ' ' : pad_char_view[0];

    if (s.empty())
    {
        return make_string_value({});
    }

    if (static_cast<long long>(s.size()) >= width)
    {
        return make_string_value(s);
    }

    std::string result;
    result.reserve(static_cast<size_t>(width));
    result.append(s);
    result.append(static_cast<size_t>(width - s.size()), pad_c);

    return make_string_value(result);
}

// Lists (Split/Join) 
// Thread safe and fastest
Value lib_str_split(int argc, Value *argv)
{
    if (!check_args(argc, 2, "split"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    std::string_view delim = get_str_arg(argv, 1);

    if (s.empty())
    {
        return value_list();
    }

    Value list = value_list();

    if (delim.empty())
    {
        // If empty delimiter, split into characters
        for (char c : s)
        {
            std::string_view char_sv(&c, 1);
            Value str_val = make_string_value(char_sv);
            value_list_append(&list, str_val);  // Use append function
        }
        return list;
    }

    // Pure C++20 implementation
    size_t start = 0;
    size_t end = 0;
    
    // Now perform the actual split
    while ((end = s.find(delim, start)) != std::string_view::npos)
    {
        if (end != start) // Avoid empty tokens unless consecutive delimiters
        {
            std::string_view token = s.substr(start, end - start);
            Value token_val = make_string_value(token);
            value_list_append(&list, token_val);  // Use append function
        }
        start = end + delim.size();
    }
    
    // Add the last token
    if (start < s.size())
    {
        std::string_view token = s.substr(start);
        Value token_val = make_string_value(token);
        value_list_append(&list, token_val);  // Use append function
    }

    return list;
}

Value lib_str_join(int argc, Value *argv)
{
    if (!check_args(argc, 2, "join"))
    {
        return make_string_value({});
    }

    if (argv[0].type != VAL_LIST)
    {
        return make_string_value({});
    }

    std::string_view delim = get_str_arg(argv, 1);

    const auto& list = argv[0].list;
    if (list.count == 0)
    {
        return make_string_value({});
    }

    // Calculate total size
    size_t total_size = 0;
    for (int i = 0; i < list.count; ++i)
    {
        if (list.items[i].type == VAL_STRING && list.items[i].s)
        {
            total_size += strlen(list.items[i].s);
        }
    }
    
    if (list.count > 1)
    {
        total_size += delim.size() * (list.count - 1);
    }

    // Build the result
    std::string result;
    result.reserve(total_size);
    
    for (int i = 0; i < list.count; ++i)
    {
        if (i > 0 && !delim.empty())
        {
            result.append(delim);
        }
        
        if (list.items[i].type == VAL_STRING && list.items[i].s)
        {
            result.append(list.items[i].s);
        }
    }

    return make_string_value(result);
}

// Character Checks
Value lib_str_is_digit(int argc, Value *argv)
{
    if (!check_args(argc, 1, "is_digit"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    if (s.empty())
    {
        return value_bool(false);
    }

    return value_bool
    (
        std::all_of
        (
            s.begin(), s.end(),
            [](unsigned char c) 
            { 
                return std::isdigit(c); 
            }
        )
    );
}

Value lib_str_is_alpha(int argc, Value *argv)
{
    if (!check_args(argc, 1, "is_alpha"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    if (s.empty())
    {
        return value_bool(false);
    }

    return value_bool
    (
        std::all_of
        (
            s.begin(), s.end(),
            [](unsigned char c) 
            { 
                return std::isalpha(c); 
            }
        )
    );
}

Value lib_str_is_alnum(int argc, Value *argv)
{
    if (!check_args(argc, 1, "is_alnum"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    if (s.empty())
    {
        return value_bool(false);
    }

    return value_bool
    (
        std::all_of
        (
            s.begin(), s.end(),
            [](unsigned char c) 
            { 
                return std::isalnum(c); 
            }
        )
    );
}

Value lib_str_is_space(int argc, Value *argv)
{
    if (!check_args(argc, 1, "is_space"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    if (s.empty())
    {
        return value_bool(false);
    }

    return value_bool
    (
        std::all_of
        (
            s.begin(), s.end(),
            [](unsigned char c) 
            { 
                return std::isspace(c); 
            }
        )
    );
}

// Type Conversions
Value lib_str_to_int(int argc, Value *argv)
{
    if (!check_args(argc, 1, "to_int"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    if (s.empty())
    {
        return value_int(0);
    }

    // Use strtoll directly without exceptions
    char *endptr = nullptr;
    long long result = std::strtoll(s.data(), &endptr, 10);
    
    // Check if entire string was successfully converted
    if (endptr != s.data() + s.size())
    {
        return value_int(0);
    }

    return value_int(result);
}

Value lib_str_to_float(int argc, Value *argv)
{
    if (!check_args(argc, 1, "to_float"))
    {
        return value_null();
    }

    std::string_view s = get_str_arg(argv, 0);
    if (s.empty())
    {
        return value_float(0.0);
    }

    // Use strtod directly without exceptions
    char *endptr = nullptr;
    double result = std::strtod(s.data(), &endptr);
    
    // Check if entire string was successfully converted
    if (endptr != s.data() + s.size())
    {
        return value_float(0.0);
    }

    return value_float(result);
}
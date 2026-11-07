// Path helpers for song and external file urls.

#include "nuade_path.h"

#include "nuade_base.h"
#include <ctype.h>
#include <string.h>

static bool is_separator(char c) {
#if defined(PLATFORM_WINDOWS)
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

static bool is_absolute(const char* path) {
    if (is_separator(path[0]) || strstr(path, "://") != nullptr) {
        return true;
    }
#if defined(PLATFORM_WINDOWS)
    if (path[0] != '\0' && path[1] == ':') {
        return true;
    }
#endif
    return false;
}

// Length of the directory part of `path`, including its last separator; 0 when there is none.
static size_t dir_length(const char* path) {
    size_t len = 0;
    for (size_t i = 0; path[i] != '\0'; i++) {
        if (is_separator(path[i])) {
            len = i + 1;
        }
    }
    return len;
}

// out = a[0..a_len) + b, or false with `out` empty when it does not fit.
static bool concat(const char* a, size_t a_len, const char* b, char* out, size_t out_cap) {
    const size_t b_len = strlen(b);
    if (a_len + b_len + 1 > out_cap) {
        out[0] = '\0';
        return false;
    }
    if (a_len > 0) {
        memcpy(out, a, a_len);
    }
    memcpy(out + a_len, b, b_len + 1);
    return true;
}

static bool starts_with_nocase(const char* s, const char* lower_prefix, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\0' || tolower((unsigned char)s[i]) != lower_prefix[i]) {
            return false;
        }
    }
    return true;
}

const char* nuade_path_basename(const char* path) {
    return path + dir_length(path);
}

bool nuade_path_parent(const char* path, char* out, size_t out_cap) {
    if (out == nullptr || out_cap == 0) {
        return false;
    }
    const size_t len = dir_length(path);
    if (len == 0) {
        out[0] = '\0';
        return false;
    }
    // Keep the separator only when it is the root ("/song" -> "/").
    return concat(path, len > 1 ? len - 1 : len, "", out, out_cap);
}

bool nuade_path_join(const char* dir, const char* name, char* out, size_t out_cap) {
    if (name == nullptr || out == nullptr || out_cap == 0) {
        return false;
    }
    const size_t dir_len = dir != nullptr ? strlen(dir) : 0;
    if (dir_len == 0 || is_separator(dir[dir_len - 1])) {
        return concat(dir, dir_len, name, out, out_cap);
    }
    if (!concat(dir, dir_len, "/", out, out_cap)) {
        return false;
    }
    return concat(out, dir_len + 1, name, out, out_cap);
}

bool nuade_path_resolve(const char* base_url, const char* relative, char* out, size_t out_cap) {
    if (relative == nullptr || out == nullptr || out_cap == 0) {
        return false;
    }
    const size_t dir_len = (base_url != nullptr && !is_absolute(relative)) ? dir_length(base_url) : 0;
    return concat(base_url, dir_len, relative, out, out_cap);
}

bool nuade_path_has_ext(const char* path, const char* ext) {
    if (path == nullptr || ext == nullptr) {
        return false;
    }
    const char* base = nuade_path_basename(path);
    const size_t len = strlen(ext);
    if (strlen(base) > len && base[len] == '.' && starts_with_nocase(base, ext, len)) {
        return true;
    }
    const char* dot = strrchr(base, '.');
    return dot != nullptr && strlen(dot + 1) == len && starts_with_nocase(dot + 1, ext, len);
}

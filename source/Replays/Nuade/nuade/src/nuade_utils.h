
// Small helpers shared by the framework and players: array size, string copy, big-endian reads (Amiga formats are
// big-endian).

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Number of elements of a fixed-size array (not a pointer).
#define nuade_sizeof_array(x) (sizeof(x) / sizeof((x)[0]))

static inline uint16_t nuade_read_be16(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline uint32_t nuade_read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// Case-insensitive (ASCII) compare of at most `n` characters, like POSIX strncasecmp, which not every platform has.
static inline int nuade_strncasecmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        const int ca = (a[i] >= 'A' && a[i] <= 'Z') ? a[i] - 'A' + 'a' : (unsigned char)a[i];
        const int cb = (b[i] >= 'A' && b[i] <= 'Z') ? b[i] - 'A' + 'a' : (unsigned char)b[i];
        if (ca != cb || ca == 0) {
            return ca - cb;
        }
    }
    return 0;
}

// Case-insensitive (ASCII) compare, like POSIX strcasecmp.
static inline int nuade_strcasecmp(const char* a, const char* b) {
    return nuade_strncasecmp(a, b, SIZE_MAX);
}

// malloc'd copy of `s` (caller frees); 0 when `s` is 0 or out of memory.
static inline char* nuade_strdup(const char* s) {
    if (s == NULL) {
        return NULL;
    }
    const size_t len = strlen(s) + 1;
    char* out = (char*)malloc(len);
    if (out != NULL) {
        memcpy(out, s, len);
    }
    return out;
}

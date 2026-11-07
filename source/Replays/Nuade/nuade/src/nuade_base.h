// Internal base header for the nuade framework and players: platform detection, nullptr, short type names.
// Not part of the public API (include/nuade).

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
#define PLATFORM_MACOS 1
#elif defined(__linux__)
#define PLATFORM_LINUX 1
#endif

#if !defined(PLATFORM_WINDOWS)
#define PLATFORM_POSIX 1
#endif

#if !defined(__cplusplus) && !defined(nullptr)
#define nullptr ((void*)0)
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef float f32;
typedef double f64;

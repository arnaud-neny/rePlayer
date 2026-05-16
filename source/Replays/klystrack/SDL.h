#pragma once

#include <inttypes.h>
typedef uint8_t Uint8;
typedef int8_t Sint8;
typedef uint16_t Uint16;
typedef int16_t Sint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;
typedef uint64_t Uint64;
typedef int64_t Sint64;

typedef void SDL_mutex;

#define AUDIO_S16SYS 0

typedef struct SDL_AudioSpec
{
    int freq;
    int format;
    uint8_t channels;
    uint8_t silence;
    uint16_t samples;
    uint32_t size;
    void* callback;
    void* userdata;
} SDL_AudioSpec;

#define SDL_CloseAudio() (0)
#define SDL_Delay(a) (0)
#define SDL_OpenAudio(a, b) (0)
#define SDL_PauseAudio(a) (0)

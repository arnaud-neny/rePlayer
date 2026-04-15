#pragma once

#include <stdint.h>

#define SHARED_MEMORY_FORMAT    "Local\\rePlayerMVX%sMemory"
#define REQUEST_EVENT_FORMAT    "Local\\rePlayerMVX%sRequestEvent"
#define RESPONSE_EVENT_FORMAT   "Local\\rePlayerMVX%sResponseEvent"

struct SharedMemory
{
    bool isQuitRequested;
    bool isRestartRequested;
    bool hasFailed;
    uint8_t bankIndex;
    uint32_t songSize;
    uint32_t songDuration;
    uint32_t numMachines;
    union
    {
        struct
        {
            struct
            {
                bool hasLooped;
                float samples[1024 * 2];
            } bank[2];
        };
        uint8_t songData[65536];
    };
};
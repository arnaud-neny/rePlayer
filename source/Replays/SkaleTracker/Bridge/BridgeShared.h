#pragma once

#include <stdint.h>

#define SHARED_MEMORY_FORMAT    "Local\\rePlayerSkaleTracker%sMemory"
#define REQUEST_EVENT_FORMAT    "Local\\rePlayerSkaleTracker%sRequestEvent"
#define RESPONSE_EVENT_FORMAT   "Local\\rePlayerSkaleTracker%sResponseEvent"

struct SharedMemory
{
    bool isQuitRequested;
    bool isRestartRequested;
    bool hasFailed;
    uint8_t bankIndex;
    uint32_t songSize;
    union
    {
        struct
        {
            struct
            {
                bool hasLooped;
                float samples[1024 * 2];
            } bank[2];
            char title[256];
        };
        uint8_t songData[65536];
    };
};
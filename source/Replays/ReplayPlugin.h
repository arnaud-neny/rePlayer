#pragma once

#include "ReplayTypes.h"
#include <Core/SharedContext.h>
#include <Helpers/CommandBuffer.h>

#include <string>

namespace core
{
    class Window;
}
// namespace core

namespace core::io
{
    class Stream;
}
// namespace core::io

namespace rePlayer
{
    using namespace core;

    class Replay;
    struct ReplayMetadataContext;

    struct ReplayPlugin
    {
        // data filled by the dll for rePlayer

        static constexpr uint32_t kMajorVersion = 0;
        static constexpr uint32_t kMinorVersion = 0;
        static constexpr uint32_t kRevision = 0;

        uint32_t revision : 16 = kRevision;
        uint32_t minorVersion : 8 = kMinorVersion;
        uint32_t majorVersion : 8 = kMajorVersion;

        eReplay replayId;
        bool isThreadSafe = true;

        const char* name = "Unknown";
        const char* extensions = "";
        const char* about = nullptr;
        const char* settings = nullptr;

        bool (*init)(SharedContexts*, Window&) = [](SharedContexts* ctx, Window&) { ctx->Init(); return false; };
        void (*release)() = [](){};

        Replay* (*load)(io::Stream*, CommandBuffer) = [](io::Stream*, CommandBuffer) { Replay* replay = nullptr;  return replay; };

        bool (*displaySettings)() = [](){ return false; };

        std::string(*getFileFilter)() = nullptr;

        void (*editMetadata)(ReplayMetadataContext&) = [](ReplayMetadataContext&) {};

        // data filled by rePlayer for the dll

        char* dllName;

        Array<uint8_t> (*download)(const char*) = nullptr;
        void (*onDelete)(Replay*) = [](Replay*) {};

        // shared data from one dll to another dll

        void* globals = nullptr;
    };
}
// namespace rePlayer
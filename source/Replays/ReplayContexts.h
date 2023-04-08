#pragma once

#include <Helpers/CommandBuffer.inl.h>

namespace rePlayer
{
    using namespace core;

    struct ReplayMetadataContext
    {
        ReplayMetadataContext(CommandBuffer commandBuffer);

        CommandBuffer metadata;

        uint32_t duration = 0;
        int16_t songIndex = 0;
        bool isSongEndEditorEnabled = false;
    };

    inline ReplayMetadataContext::ReplayMetadataContext(CommandBuffer commandBuffer)
        : metadata(commandBuffer)
    {}
}
// namespace rePlayer
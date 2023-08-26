#pragma once

#include <Helpers/CommandBuffer.inl.h>

namespace rePlayer
{
    using namespace core;

    struct ReplayMetadataContext
    {
        ReplayMetadataContext(CommandBuffer commandBuffer, uint16_t _lastSubsongIndex);

        CommandBuffer metadata;

        uint32_t duration = 0;
        uint16_t lastSubsongIndex;
        uint16_t subsongIndex : 15 = 0;
        uint16_t isSongEndEditorEnabled : 1 = false;
    };

    inline ReplayMetadataContext::ReplayMetadataContext(CommandBuffer commandBuffer, uint16_t _lastSubsongIndex)
        : metadata(commandBuffer)
        , lastSubsongIndex(_lastSubsongIndex)
    {}
}
// namespace rePlayer
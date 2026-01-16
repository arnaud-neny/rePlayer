#pragma once

#include <Helpers/CommandBuffer.inl.h>

namespace rePlayer
{
    using namespace core;

    struct ReplayMetadataContext
    {
        ReplayMetadataContext(CommandBuffer commandBuffer, uint16_t _lastSubsongIndex);

        CommandBuffer metadata;

        LoopInfo loop = {};
        uint16_t lastSubsongIndex;
        uint16_t subsongIndex = 0;
        bool isSongEndEditorEnabled = false;
    };

    inline ReplayMetadataContext::ReplayMetadataContext(CommandBuffer commandBuffer, uint16_t _lastSubsongIndex)
        : metadata(commandBuffer)
        , lastSubsongIndex(_lastSubsongIndex)
    {}
}
// namespace rePlayer
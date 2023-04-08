#pragma once

#include "ReplayTypes.h"

#include <Audio/AudioTypes.h>
#include <IO/Stream.h>
#include <Replays/ReplayContexts.h>

#include <string>

struct ImGuiContext;

namespace core
{
    class SharedContexts;
    class Window;
}
// namespace core

namespace rePlayer
{
    using namespace core;

    class Replay
    {
    public:
        template <typename CommandType, auto type>
        struct Command : public CommandBuffer::CommandExclusive<CommandType, type, 0, uint16_t(eReplay::Count) - 1>
        {
            template <typename T1, typename T2>
            struct GetSet
            {
                T1 get;
                T2 set;

                GetSet(T1&& g, T2&& s) : get(g), set(s) {}
                operator auto() { return get(); }
                auto& operator=(auto&& other) { set(other); return *this; }
            };
            #define GETSET(This, Member) GetSet([This]() { return This->Member; }, [This](auto v) { This->Member = v; })

            static void SliderOverride(const char* id, auto&& isEnabled, auto&& currentValue, int32_t defaultValue, int32_t min, int32_t max, const char* format, int32_t valueOffset = 0);
            template <typename... Items> // Items as const char* const
            static void ComboOverride(const char* id, auto&& isEnabled, auto&& currentValue, int32_t defaultValue, Items&&... items);
            static void Durations(ReplayMetadataContext& context, uint32_t* durations, uint32_t numDurations, const char* format);
        };

    public:
        virtual ~Replay() {}

        virtual uint32_t GetSampleRate() const = 0;
        virtual bool IsSeekable() const { return false; }

        virtual uint32_t Render(StereoSample* output, uint32_t numSamples) = 0;
        virtual uint32_t Seek(uint32_t timeInMs) { ResetPlayback(); return 0 * timeInMs; }

        virtual void ResetPlayback() = 0;

        virtual void ApplySettings(const CommandBuffer metadata) = 0;
        virtual void SetSubsong(uint16_t subsongIndex) = 0;

        MediaType GetMediaType() const { return m_mediaType; }
        virtual uint32_t GetDurationMs() const = 0;
        virtual uint32_t GetNumSubsongs() const = 0;
        virtual std::string GetSubsongTitle() const { return {}; }
        virtual std::string GetExtraInfo() const = 0;
        virtual std::string GetInfo() const = 0;

        virtual bool CanLoop() const { return true; }

    protected:
        Replay(MediaType mediaType) : m_mediaType(mediaType) {}
        Replay(const char* const ext, eReplay replay) : m_mediaType(ext, replay) {}
        Replay(eExtension ext, eReplay replay) : m_mediaType(ext, replay) {}

    protected:
        uint32_t m_subsongIndex = 0;
        mutable MediaType m_mediaType;
    };
}
// namespace rePlayer
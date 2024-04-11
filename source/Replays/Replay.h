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
            static void SliderOverride(const char* id, auto&& isEnabled, auto&& currentValue, float defaultValue, float min, float max, const char* format);
            template <typename... Items> // Items as const char* const
            static void ComboOverride(const char* id, auto&& isEnabled, auto&& currentValue, int32_t defaultValue, Items&&... items);
            static void Durations(ReplayMetadataContext& context, uint32_t* durations, uint32_t numDurations, const char* format);
        };

        struct Patterns
        {
            enum Flags : uint32_t
            {
                kDisableAll = 0,
                kEnableRowNumbers = 1 << 0,
                kEnableInstruments = 1 << 1,
                kEnableVolume = 1 << 2,
                kEnableEffects = 1 << 3,
                kEnableColors = 1 << 4,
                kEnableHighlight = 1 << 5,
                kEnablePatterns = 1 << 6
            };
            static constexpr Flags kDisplayAll = Flags(kEnableRowNumbers | kEnableInstruments | kEnableVolume | kEnableEffects | kEnableColors | kEnableHighlight);

            enum ColorIndex : char
            {
                kColorDefault = 0,
                kColorText,
                kColorEmpty,
                kColorInstrument,
                kColorVolume,
                kColorPitch,
                kColorGlobal,
                kNumColors
            };

            // only characters in the range of [32;127] are allowed
            // if < 0, then it's an index in the colors
            Array<char> lines; // 0 terminated lines
            Array<uint16_t> sizes; // number of chars on a line
            int32_t currentLine = 0;
            int32_t width = 0;

            static const uint32_t colors[kNumColors];
        };

    public:
        virtual ~Replay();

        virtual uint32_t GetSampleRate() const = 0;
        virtual bool IsSeekable() const { return false; }
        virtual bool IsStreaming() const { return false; }

        virtual uint32_t Render(StereoSample* output, uint32_t numSamples) = 0;
        virtual uint32_t Seek(uint32_t timeInMs) { ResetPlayback(); return 0 * timeInMs; }

        virtual void ResetPlayback() = 0;

        virtual void ApplySettings(const CommandBuffer metadata) = 0;
        virtual void SetSubsong(uint32_t subsongIndex) = 0;

        MediaType GetMediaType() const { return m_mediaType; }
        virtual uint32_t GetDurationMs() const = 0;
        virtual uint32_t GetNumSubsongs() const = 0;
        virtual std::string GetSubsongTitle() const { return {}; }
        virtual std::string GetStreamingTitle() const { return {}; }
        virtual std::string GetStreamingArtist() const { return {}; }
        virtual std::string GetExtraInfo() const = 0;
        virtual std::string GetInfo() const = 0;

        virtual bool CanLoop() const { return true; }

        virtual Patterns UpdatePatterns(uint32_t numSamples, uint32_t numLines, uint32_t charWidth, uint32_t spaceWidth, Patterns::Flags flags = Patterns::kDisplayAll) { (void)numSamples; (void)numLines; (void)charWidth; (void)spaceWidth; (void)flags; return {}; }

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
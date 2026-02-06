#include "replayHively.h"

#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "hvl/hvl_replay.h"
#ifdef __cplusplus
}
#endif

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Hively,
        .name = "HivelyTracker",
        .extensions = "ahx;hvl;thx",
        .about = "HivelyTracker 1.9\nCopyright (c) 2006-2018 Pete Gordon",
        .settings = "HivelyTracker 1.9",
        .init = ReplayHively::Init,
        .load = ReplayHively::Load,
        .displaySettings = ReplayHively::DisplaySettings,
        .editMetadata = ReplayHively::Settings::Edit
    };

    bool ReplayHively::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        hvl_InitReplayer();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayHivelyStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayHivelySurround");
        window.RegisterSerializedData(ms_surround, "ReplayHivelyPatterns");

        return false;
    }

    Replay* ReplayHively::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        if (stream->GetSize() < 8 || stream->GetSize() > 1024 * 1024 * 128)
            return nullptr;

        auto data = stream->Read();
        auto module = hvl_ParseTune(data.Items(), static_cast<uint32_t>(data.Size()), kSampleRate, 2, [](size_t size) { auto* ptr = Alloc(size); memset(ptr, 0, size); return ptr; }, [](void* ptr) { Free(ptr); });
        if (!module)
            return nullptr;

        const char* extension;
        if ((data[0] == 'T') &&
            (data[1] == 'H') &&
            (data[2] == 'X') &&
            (data[3] < 3))
        {
            extension = "ahx";
        }
        else/* if ((data[0] == 'H') &&
            (data[1] == 'V') &&
            (data[2] == 'L') &&
            (data[3] < 2))*/
        {
            extension = "hvl";
        }

        return new ReplayHively(module, hvl_ParseTune(data.Items(), static_cast<uint32_t>(data.Size()), kSampleRate, 2, [](size_t size) { auto* ptr = Alloc(size); memset(ptr, 0, size); return ptr; }, [](void* ptr) { Free(ptr); }), extension);
    }

    bool ReplayHively::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        const char* const patterns[] = { "Disable", "Enable" };
        changed |= ImGui::Combo("Show Patterns", &ms_patterns, patterns, NumItemsOf(patterns));
        return changed;
    }

    void ReplayHively::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplayHively::ms_stereoSeparation = 100;
    int32_t ReplayHively::ms_surround = 1;
    int32_t ReplayHively::ms_patterns = 1;

    ReplayHively::~ReplayHively()
    {
        hvl_FreeTune(m_modulePlayback);
        hvl_FreeTune(m_moduleVisuals);
        delete[] m_tickData;
    }

    ReplayHively::ReplayHively(hvl_tune* modulePlayback, hvl_tune* moduleVisuals, const char* extension)
        : Replay(extension, eReplay::Hively)
        , m_modulePlayback(modulePlayback)
        , m_moduleVisuals(moduleVisuals)
        , m_tickData(new int16_t[2 * kSampleRate / 50 / modulePlayback->ht_SpeedMultiplier])
        , m_surround(kSampleRate)
    {}

    uint32_t ReplayHively::Render(StereoSample* output, uint32_t numSamples)
    {
        uint32_t maxSamples = kSampleRate / 50 / m_modulePlayback->ht_SpeedMultiplier;
        uint32_t numRenderedSamples = 0;

        auto numAvailableSamples = m_numPlaybackAvailableSamples;
        auto tickData = m_tickData + (maxSamples - numAvailableSamples) * 2;

        while (numSamples > 0)
        {
            auto numSamplesToConvert = numAvailableSamples <= numSamples ? numAvailableSamples : numSamples;

            auto surround = m_surround.Begin();
            for (uint32_t i = 0; i < numSamplesToConvert; i++)
            {
                *output++ = surround({ tickData[0] / 32767.0f , tickData[1] / 32767.0f });
                tickData += 2;
            }
            m_surround.End(surround);
            numRenderedSamples += numSamplesToConvert;
            numAvailableSamples -= numSamplesToConvert;
            numSamples -= numSamplesToConvert;

            if (numAvailableSamples == 0)
            {
                if (m_modulePlayback->ht_SongEndReached)
                {
                    if (numRenderedSamples > 0)
                    {
                        m_numPlaybackAvailableSamples = 0;
                        return numRenderedSamples;
                    }
                    else
                    {
                        m_modulePlayback->ht_SongEndReached = 0;
                        return 0;
                    }
                }

                tickData -= maxSamples * 2;
                hvl_Decode(m_modulePlayback, tickData, maxSamples);
                numAvailableSamples = maxSamples;
            }
        }

        m_numPlaybackAvailableSamples = numAvailableSamples;

        return numRenderedSamples;
    }

    uint32_t ReplayHively::Seek(uint32_t timeInMs)
    {
        timeInMs = hvl_Seek(m_modulePlayback, timeInMs);
        hvl_Seek(m_moduleVisuals, timeInMs);
        m_surround.Reset();
        return timeInMs;
    }

    void ReplayHively::ResetPlayback()
    {
        hvl_InitSubsong(m_modulePlayback, m_subsongIndex);
        hvl_InitSubsong(m_moduleVisuals, m_subsongIndex);
        m_surround.Reset();
        m_numPlaybackAvailableSamples = 0;
        m_numVisualsAvailableSamples = 0;
    }

    void ReplayHively::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        hvl_UpdateStereo(m_modulePlayback, (settings && settings->overrideStereoSeparation) ? 100 - settings->stereoSeparation : ms_stereoSeparation);
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
    }

    void ReplayHively::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        hvl_InitSubsong(m_modulePlayback, subsongIndex);
        hvl_InitSubsong(m_moduleVisuals, subsongIndex);
        m_numPlaybackAvailableSamples = 0;
        m_numVisualsAvailableSamples = 0;
    }

    uint32_t ReplayHively::GetDurationMs() const
    {
        return hvl_GetDuration(m_modulePlayback);
    }

    uint32_t ReplayHively::GetNumSubsongs() const
    {
        return m_modulePlayback->ht_SubsongNr > 0 ? m_modulePlayback->ht_SubsongNr : 1;
    }

    std::string ReplayHively::GetExtraInfo() const
    {
        std::string metadata;
        metadata = m_modulePlayback->ht_Name;
        for (uint8_t i = 1; i <= m_modulePlayback->ht_InstrumentNr; i++)
        {
            metadata += "\n";
            metadata += m_modulePlayback->ht_Instruments[i].ins_Name;
        }
        return metadata;
    }

    std::string ReplayHively::GetInfo() const
    {
        std::string info;

        char buf[32];
        sprintf(buf, "%d", m_modulePlayback->ht_Channels);
        info += buf;
        info += " channels\n";

        if (m_mediaType.ext == eExtension::_ahx)
        {
            info += "AHX v";
            info += '1' + m_modulePlayback->ht_Version;
        }
        else
        {
            info += "HVL 1.0-1.4";
        }
        info += "\nHivelyTracker 1.9";

        return info;
    }

    Replay::Patterns ReplayHively::UpdatePatterns(uint32_t numSamples, uint32_t numLines, uint32_t charWidth, uint32_t spaceWidth, Patterns::Flags flags)
    {
        uint32_t maxSamples = kSampleRate / 50 / m_moduleVisuals->ht_SpeedMultiplier;
        uint32_t numRenderedSamples = 0;

        auto numAvailableSamples = m_numVisualsAvailableSamples;
        auto tickData = m_tickData + (maxSamples - numAvailableSamples) * 2;

        while (numSamples > 0)
        {
            auto numSamplesToConvert = numAvailableSamples <= numSamples ? numAvailableSamples : numSamples;

            numRenderedSamples += numSamplesToConvert;
            numAvailableSamples -= numSamplesToConvert;
            numSamples -= numSamplesToConvert;

            if (numAvailableSamples == 0)
            {
                if (m_moduleVisuals->ht_SongEndReached)
                    m_moduleVisuals->ht_SongEndReached = 0;

                tickData -= maxSamples * 2;
                hvl_play_irq(m_moduleVisuals);
                numAvailableSamples = maxSamples;
            }
        }
        m_numVisualsAvailableSamples = numAvailableSamples;

        if (!m_arePatternsDisplayed && !(flags & Patterns::kEnablePatterns)) // kEnablePatterns has priority over internal flag
            return {};

        int32_t numChannels = m_moduleVisuals->ht_Channels;

        Patterns patterns;
        patterns.lines.Reserve(65536);
        patterns.sizes.Resize(numLines);
        patterns.currentLine = m_moduleVisuals->ht_NoteNr;
        if (flags & (Patterns::kEnableInstruments | Patterns::kEnableEffects))
            patterns.width = (numChannels * 4 - 1) * charWidth;
        else
            patterns.width = (numChannels * 3) * charWidth + (numChannels - 1) * spaceWidth;
        uint16_t size = uint16_t(numChannels * 4 - 1);
        if (flags & Patterns::kEnableRowNumbers)
        {
            patterns.width += 3 * charWidth;
            size += uint16_t(3);
        }
        if (flags & Patterns::kEnableInstruments)
        {
            patterns.width += numChannels * 2 * charWidth + spaceWidth * numChannels;
            size += uint16_t(numChannels * 3);
        }
        if (flags & Patterns::kEnableEffects)
        {
            patterns.width += (numChannels * 3 * charWidth + spaceWidth * numChannels) * 2;
            size += uint16_t(numChannels * 4 * 2);
        }

        int32_t numRows = m_moduleVisuals->ht_TrackLength;
        auto currentRow = patterns.currentLine - int32_t(numLines) / 2;

        for (uint32_t line = 0; line < numLines; currentRow++, line++)
        {
            if (currentRow >= 0 && currentRow < numRows)
            {
                char colorBit = -128; // 0x80
                if (flags & Patterns::kEnableRowNumbers)
                {
                    if (flags & Patterns::kEnableColors)
                        patterns.lines.Add(colorBit | Patterns::kColorDefault);
                    patterns.lines.Add('0' + ((currentRow / 10) % 10));
                    patterns.lines.Add('0' + (currentRow % 10));
                    patterns.lines.Add('\x7f');
                }

                for (int32_t channel = 0;;)
                {
                    auto& track = m_moduleVisuals->ht_Tracks[m_moduleVisuals->ht_Positions[m_moduleVisuals->ht_PosNr].pos_Track[channel]];
                    auto& step = track[currentRow];

                    static const char* notenames[] = { "...",
                        "C-1", "C#1", "D-1", "D#1", "E-1", "F-1", "F#1", "G-1", "G#1", "A-1", "A#1", "B-1",
                        "C-2", "C#2", "D-2", "D#2", "E-2", "F-2", "F#2", "G-2", "G#2", "A-2", "A#2", "B-2",
                        "C-3", "C#3", "D-3", "D#3", "E-3", "F-3", "F#3", "G-3", "G#3", "A-3", "A#3", "B-3",
                        "C-4", "C#4", "D-4", "D#4", "E-4", "F-4", "F#4", "G-4", "G#4", "A-4", "A#4", "B-4",
                        "C-5", "C#5", "D-5", "D#5", "E-5", "F-5", "F#5", "G-5", "G#5", "A-5", "A#5", "B-5" };

                    if (flags & Patterns::kEnableColors)
                    {
                        patterns.lines.Add(colorBit | (step.stp_Note ? Patterns::kColorText : Patterns::kColorEmpty));
                    }
                    patterns.lines.Add(notenames[step.stp_Note], 3);
                    if (flags & Patterns::kEnableInstruments)
                    {
                        patterns.lines.Add(' ');
                        if (step.stp_Instrument)
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorInstrument);
                            patterns.lines.Add('0' + ((step.stp_Instrument / 10) % 10));
                            patterns.lines.Add('0' + (step.stp_Instrument % 10));
                        }
                        else
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                            patterns.lines.Add("..", 2);
                        }
                    }
                    if (flags & Patterns::kEnableEffects)
                    {
                        if (step.stp_FX || step.stp_FXParam)
                        {
                            if (flags & Patterns::kEnableColors)
                            {
                                if (step.stp_FX == 1 || step.stp_FX == 2 || step.stp_FX == 3 || step.stp_FX == 5 || (step.stp_FX == 0xE && ((step.stp_FXParam & 0xf0) == 0x10 || (step.stp_FXParam & 0xf0) == 0x20 || (step.stp_FXParam & 0xf0) == 0x40)))
                                    patterns.lines.Add(colorBit | Patterns::kColorPitch);
                                else if (step.stp_FX == 0xb || step.stp_FX == 0xd || step.stp_FX == 0xf)
                                    patterns.lines.Add(colorBit | Patterns::kColorGlobal);
                                else if (step.stp_FX == 0xa || step.stp_FX == 0xc || (step.stp_FX == 0xE && ((step.stp_FXParam & 0xf0) == 0xa0 || (step.stp_FXParam & 0xf0) == 0xb0)))
                                    patterns.lines.Add(colorBit | Patterns::kColorVolume);
                                else
                                    patterns.lines.Add(colorBit | Patterns::kColorText);
                            }
                            char buf[5];
                            sprintf(buf, " %01X%02X", step.stp_FX, step.stp_FXParam & 0xff);
                            patterns.lines.Add(buf, 4);
                        }
                        else
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                            patterns.lines.Add(" ...", 4);
                        }
                        if (step.stp_FXb || step.stp_FXbParam)
                        {
                            if (flags & Patterns::kEnableColors)
                            {
                                if (step.stp_FXb == 1 || step.stp_FXb == 2 || step.stp_FXb == 3 || step.stp_FXb == 5 || (step.stp_FXb == 0xE && ((step.stp_FXbParam & 0xf0) == 0x10 || (step.stp_FXbParam & 0xf0) == 0x20 || (step.stp_FXbParam & 0xf0) == 0x40)))
                                    patterns.lines.Add(colorBit | Patterns::kColorPitch);
                                else if (step.stp_FXb == 0xb || step.stp_FXb == 0xd || step.stp_FXb == 0xf)
                                    patterns.lines.Add(colorBit | Patterns::kColorGlobal);
                                else if (step.stp_FXb == 0xa || step.stp_FXb == 0xc || (step.stp_FXb == 0xE && ((step.stp_FXbParam & 0xf0) == 0xa0 || (step.stp_FXbParam & 0xf0) == 0xb0)))
                                    patterns.lines.Add(colorBit | Patterns::kColorVolume);
                                else
                                    patterns.lines.Add(colorBit | Patterns::kColorText);
                            }
                            char buf[5];
                            sprintf(buf, " %01X%02X", step.stp_FXb, step.stp_FXbParam & 0xff);
                            patterns.lines.Add(buf, 4);
                        }
                        else
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                            patterns.lines.Add(" ...", 4);
                        }
                    }
                    if (++channel == numChannels)
                        break;
                    if (flags & (Patterns::kEnableInstruments| Patterns::kEnableEffects))
                        patterns.lines.Add('\x7f');
                    else
                        patterns.lines.Add(' ');
                }

                patterns.sizes[line] = size;
            }
            else
                patterns.sizes[line] = 0;
            patterns.lines.Add('\0');
        }

        return patterns;
    }
}
// namespace rePlayer
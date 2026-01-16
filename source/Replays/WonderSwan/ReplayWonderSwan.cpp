#include "ReplayWonderSwan.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ReplayDll.h>

#include "wsr_player_common/wsr_player_common.h"
#include "wsr_player_common/wsr_player_intf.h"
#include "foo_input_wsr/extended_m3u_playlist.h"
#include "foo_input_wsr/pfc.h"

namespace rePlayer
{
    namespace in_wsr
    {
        extern "C" {
#           include "in_wsr/nec/nec.c"
#           include "in_wsr/ws_audio.c"
#           include "in_wsr/ws_io.c"
#           include "in_wsr/ws_memory.c"
#           include "in_wsr/wsr_player.c"
        }
    }
    // namespace in_wsr

    namespace mednafen
    {
        extern WSRPlayerApi g_wsr_player_api;
    }
    // namespace mednafen

    namespace oswan
    {
        extern WSRPlayerApi g_wsr_player_api;
    }
    // namespace oswan

    static WSRPlayerApi* s_coreSwan[] = {
        &mednafen::g_wsr_player_api,
        &oswan::g_wsr_player_api,
        &in_wsr::g_wsr_player_api
    };

    #define WONDERSWAN "WonderSwan 0.30"

    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::WonderSwan, .isThreadSafe = false,
        .name = "WonderSwan",
        .extensions = "wsr",
        .about = WONDERSWAN "\nin_wsr by unknown japanese author\nOswan Copyright (c) toyo\nmednafen Copyright (c) 2005-2022 Mednafen Team",
        .settings = WONDERSWAN,
        .init = ReplayWonderSwan::Init,
        .load = ReplayWonderSwan::Load,
        .displaySettings = ReplayWonderSwan::DisplaySettings,
        .editMetadata = ReplayWonderSwan::Settings::Edit,
        .globals = &ReplayWonderSwan::ms_globals
    };

    bool ReplayWonderSwan::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        if (&window != nullptr)
        {
            window.RegisterSerializedData(ms_globals.surround, "ReplayWonderSwanSurround");
            window.RegisterSerializedData(ms_globals.core, "ReplayWonderSwanCore");
        }

        return false;
    }

    Replay* ReplayWonderSwan::Load(io::Stream* stream, CommandBuffer metadata)
    {
        if (stream->GetSize() <= 0x20)
            return nullptr;
        uint32_t hdr;
        stream->Seek(-0x20, io::Stream::kSeekEnd);
        stream->Read(&hdr, sizeof(hdr));
        if (hdr != WSR_HEADER_MAGIC)
            return nullptr;

        auto* globals = static_cast<Globals*>(g_replayPlugin.globals);
        auto settings = metadata.Find<Settings>();
        auto core = (settings && settings->overrideCore) ? CoreSwan(settings->core) : globals->core;

        stream->Seek(0, io::Stream::kSeekBegin);
        auto buffer = stream->Read();
        if (s_coreSwan[int(core)]->pLoadWSR(buffer.Items(), unsigned(buffer.Size())))
        {
            Array<std::string> titles;
            uint32_t oldValue = 0;
            if (settings)
            {
                if (settings->NumSubsongs() == 1 && settings->subsongs[0] == 0 && settings->subsongs[1] == 0 && settings->subsongs[2] == 0 && settings->subsongs[3] == 0
                    && settings->subsongs[4] == 0 && settings->subsongs[5] == 0 && settings->subsongs[6] == 0 && settings->subsongs[7] == 0)
                {
                    oldValue = settings->value;
                    metadata.Remove(settings->commandId);
                    settings = nullptr;
                }
            }
            if (settings == nullptr)
            {
                // detect subsongs
                uint32_t subsongs[8] = { 0 };
                uint8_t tracks[256] = { 0 };
                uint32_t numSubsongs = 0;
                uint64_t buf[512];
                static constexpr uint32_t kBufSamples = sizeof(buf) / sizeof(int16_t) / 2;
                for (uint32_t i = 0; i < 256; i++)
                {
                    s_coreSwan[int(core)]->pResetWSR(i);
                    s_coreSwan[int(core)]->pSetFrequency(kSampleRate);

                    uint64_t smp = 0;
                    auto numSamples = (kSampleRate * 4) & ~(kBufSamples - 1); // around 4 seconds check
                    while (numSamples)
                    {
                        s_coreSwan[int(core)]->pUpdateWSR(buf, sizeof(buf), kBufSamples);
                        if (numSamples == ((kSampleRate * 4) & ~(kBufSamples - 1)))
                            smp = buf[0];
                        numSamples -= kBufSamples;

                        for (uint32_t j = 0; j < 512; j++)
                        {
                            if (buf[j] != smp)
                            {
                                tracks[numSubsongs] = uint8_t(i);
                                subsongs[i / 32] |= 1 << (i & 31);
                                numSubsongs++;
                                numSamples = 0;
                                break;
                            }
                        }
                    }
                }
                if (numSubsongs == 0)
                {
                    s_coreSwan[int(core)]->pCloseWSR();
                    return nullptr;
                }
                titles.Resize(numSubsongs);
                settings = metadata.Create<Settings>(sizeof(Settings) + numSubsongs * sizeof(LoopInfo), numSubsongs);
                settings->value = oldValue;
                memcpy(settings->subsongs, subsongs, sizeof(subsongs));

                // read the playlist if any
                auto filename = stream->GetName();
                auto pos = filename.find_last_of('.');
                if (pos != filename.npos)
                {
                    extended_m3u_playlist exm3u;

                    filename.resize(pos + 1);
                    filename += "m3u8";
                    auto m3uStream = stream->Open(filename);
                    if (m3uStream)
                    {
                        auto m3uData = m3uStream->Read();
                        if (exm3u.load(m3uData.Items(), m3uData.Size()) != extended_m3u_playlist::exm3u_success)
                        {
                            m3uStream.Reset();
                            exm3u = {};
                        }

                    }
                    if (!m3uStream)
                    {
                        filename.resize(pos + 1);
                        filename += "m3u";
                        m3uStream = stream->Open(filename);
                        if (m3uStream)
                        {
                            auto m3uData = m3uStream->Read();
                            const uint8_t* p_exm3u_buf = m3uData.Items();
                            //BOM of utf-8
                            if (0xef != p_exm3u_buf[0] || 0xbb != p_exm3u_buf[1] || 0xbf != p_exm3u_buf[2])
                            {
                                //Check encoding
                                if (!pfc::is_valid_utf8((const char*)p_exm3u_buf, m3uData.Size()))
                                {
                                    auto conv = pfc::string_ansi_to_utf8((const char*)p_exm3u_buf, m3uData.Size());
                                    if (exm3u.load(conv.Items(), conv.Size<size_t>()) != extended_m3u_playlist::exm3u_success)
                                        exm3u = {};
                                    else
                                        m3uStream.Reset();
                                }
                                if (m3uStream.IsValid() && exm3u.load(m3uData.Items(), m3uData.Size()) != extended_m3u_playlist::exm3u_success)
                                    exm3u = {};
                            }
                        }
                    }
                    for (int i = 0; i < exm3u.size(); i++)
                    {
                        for (uint32_t j = 0; j < numSubsongs; j++)
                        {
                            if (tracks[j] == uint32_t(exm3u[i].track))
                            {
                                settings->loops[j] = { 0, uint32_t(exm3u[i].length) };
                                titles[j] = exm3u[i].name;
                                break;
                            }
                        }
                    }
                }
            }

            return new ReplayWonderSwan(core, metadata, std::move(titles));
        }
        return nullptr;
    }

    bool ReplayWonderSwan::DisplaySettings()
    {
        bool changed = false;
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_globals.surround, surround, NumItemsOf(surround));

        const char* const coreSwan[] = { "mednafen", "Oswan", "in_wsr" };
        changed |= ImGui::Combo("Core", reinterpret_cast<int*>(&ms_globals.core), coreSwan, NumItemsOf(coreSwan));

        return changed;
    }

    void ReplayWonderSwan::Settings::Edit(ReplayMetadataContext& context)
    {
        const auto settingsSize = sizeof(Settings) + (context.lastSubsongIndex + 1) * sizeof(LoopInfo);
        auto* dummy = new (_alloca(settingsSize)) Settings(context.lastSubsongIndex);
        auto* entry = context.metadata.Find<Settings>(dummy);
        if (!entry || entry->NumSubsongs() != context.lastSubsongIndex + 1u)
        {
            if (entry)
            {
                dummy->value = entry->value;
                context.metadata.Remove(entry->commandId);
            }
            entry = dummy;
        }

        ComboOverride("Output", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_globals.surround, "Output: Stereo", "Output: Surround");
        ComboOverride("Core", GETSET(entry, overrideCore), GETSET(entry, core),
            int32_t(ms_globals.core), "Core: mednafen", "Core: Oswan", "Core: in_wsr");
        Loops(context, entry->loops, entry->NumSubsongs());

        context.metadata.Update(entry, false);
    }

    ReplayWonderSwan::Globals ReplayWonderSwan::ms_globals = {
        .surround = 0,
        .core = CoreSwan::kMednafen
    };

    ReplayWonderSwan::~ReplayWonderSwan()
    {
        s_coreSwan[int(m_core)]->pCloseWSR();
    }

    ReplayWonderSwan::ReplayWonderSwan(CoreSwan core, CommandBuffer metadata, Array<std::string>&& titles)
        : Replay(eExtension::_wsr, eReplay::WonderSwan)
        , m_core(core)
        , m_surround(kSampleRate)
        , m_titles(std::move(titles))
    {
        s_coreSwan[int(m_core)]->pResetWSR(0);
        s_coreSwan[int(m_core)]->pSetFrequency(kSampleRate);
        BuildDurations(metadata);
    }

    uint32_t ReplayWonderSwan::Render(StereoSample* output, uint32_t numSamples)
    {
        auto currentPosition = m_currentPosition;
        auto currentDuration = m_currentDuration;
        if (currentDuration != 0 && (currentPosition + numSamples) >= currentDuration)
        {
            numSamples = currentPosition < currentDuration ? uint32_t(currentDuration - currentPosition) : 0;
            if (numSamples == 0)
            {
                m_currentPosition = 0;
                m_currentDuration = (uint64_t(m_loops[m_subsongIndex].length) * kSampleRate) / 1000;
                return 0;
            }
        }
        m_currentPosition = currentPosition + numSamples;
        m_globalPosition += numSamples;

        auto remainingSamples = numSamples;
        while (remainingSamples)
        {
            if (m_remainingSamples == 0)
            {
                s_coreSwan[int(m_core)]->pUpdateWSR(m_samples, sizeof(m_samples), kNumSamples);
                m_remainingSamples = kNumSamples;
            }
            auto numSamplesToCopy = Min(remainingSamples, m_remainingSamples);
            output = output->Convert(m_surround, m_samples + (kNumSamples - m_remainingSamples) * 2, numSamplesToCopy, 100);
            remainingSamples -= numSamplesToCopy;
            m_remainingSamples -= numSamplesToCopy;
        }
        return numSamples;
    }

    uint32_t ReplayWonderSwan::Seek(uint32_t timeInMs)
    {
        auto seekPosition = (uint64_t(timeInMs) * kSampleRate) / 1000;
        auto seekSamples = seekPosition;
        if (seekPosition < m_globalPosition)
            ResetPlayback();
        else
            seekSamples -= m_globalPosition;
        m_surround.Reset();

        auto currentDuration = m_currentDuration;
        while (seekSamples)
        {
            auto numSamples = uint32_t(Min(32768, seekSamples));

            auto currentPosition = m_currentPosition;
            if (currentDuration != 0 && (currentPosition + numSamples) >= currentDuration)
            {
                numSamples = currentPosition < currentDuration ? uint32_t(currentDuration - currentPosition) : 0;
                if (numSamples == 0)
                {
                    m_currentPosition = 0;
                    currentDuration = (uint64_t(m_loops[m_subsongIndex].length) * kSampleRate) / 1000;
                }
            }
            m_currentPosition = currentPosition + numSamples;
            m_globalPosition += numSamples;
            seekSamples -= numSamples;

            auto remainingSamples = numSamples;
            while (remainingSamples)
            {
                if (m_remainingSamples == 0)
                {
                    s_coreSwan[int(m_core)]->pUpdateWSR(m_samples, sizeof(m_samples), kNumSamples);
                    m_remainingSamples = kNumSamples;
                }
                auto numSamplesToCopy = Min(remainingSamples, m_remainingSamples);
                remainingSamples -= numSamplesToCopy;
                m_remainingSamples -= numSamplesToCopy;
            }
        }

        return timeInMs;
    }

    void ReplayWonderSwan::ResetPlayback()
    {
        s_coreSwan[int(m_core)]->pResetWSR(m_subsongs[m_subsongIndex]);
        s_coreSwan[int(m_core)]->pSetFrequency(kSampleRate);
        m_currentPosition = m_globalPosition = 0;
        m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
        m_surround.Reset();
    }

    void ReplayWonderSwan::ApplySettings(const CommandBuffer metadata)
    {
        auto* globals = static_cast<Globals*>(g_replayPlugin.globals);
        auto* settings = metadata.Find<Settings>();
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : globals->surround);

        if (settings)
        {
            for (uint32_t i = 0; i < m_numSubsongs; ++i)
                m_loops[i] = settings->loops[i].GetFixed();
            m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
        }
    }

    void ReplayWonderSwan::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayWonderSwan::GetDurationMs() const
    {
        return m_loops[m_subsongIndex].GetDuration();
    }

    uint32_t ReplayWonderSwan::GetNumSubsongs() const
    {
        return m_numSubsongs;
    }

    std::string ReplayWonderSwan::GetSubsongTitle() const
    {
        if (m_subsongIndex < m_titles.NumItems())
            return m_titles[m_subsongIndex];
        return {};
    }

    std::string ReplayWonderSwan::GetExtraInfo() const
    {
        std::string info;
        return info;
    }

    std::string ReplayWonderSwan::GetInfo() const
    {
        std::string info;
        info = "4 channels\nCore: ";
        switch (m_core)
        {
        case CoreSwan::kMednafen:
            info += "mednafen"; break;
        case CoreSwan::kOswan:
            info += "Oswan"; break;
        case CoreSwan::kInWSR:
            info += "in_wsr"; break;
        }
        info += "\n" WONDERSWAN;
        return info;
    }

    void ReplayWonderSwan::BuildDurations(CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_numSubsongs = settings->NumSubsongs();
        auto* subsongs = m_subsongs;
        for (uint32_t i = 0; i < 256; i++)
        {
            if (settings->subsongs[i / 32] & (1 << (i & 31)))
                *subsongs++ = uint8_t(i);
        }
        for (uint32_t i = 0; i < m_numSubsongs; ++i)
            m_loops[i] = settings->loops[i].GetFixed();
        m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
    }
}
// namespace rePlayer
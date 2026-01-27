#include "ReplayGME.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

#include <gme/gme.h>
#include <gme/Gme_File.h>

namespace rePlayer
{
    static_assert(GME_VERSION == 0x000604);
    #define GME_VERSION_STRING "0.6.4"

    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::GME,
        .name = "Game-Music-Emu",
        .extensions = "ay;gbs;gym;hes;kss;nsf;nsfe;sap;spc;vgm;vgz",
        .about = "Game-Music-Emu " GME_VERSION_STRING "\nShay Green, Vitaly Novichkov & Michael Pyne",
        .settings = "Game-Music-Emu " GME_VERSION_STRING,
        .init = ReplayGME::Init,
        .load = ReplayGME::Load,
        .displaySettings = ReplayGME::DisplaySettings,
        .editMetadata = ReplayGME::Settings::Edit
    };

    bool ReplayGME::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayGMEStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayGMESurround");

        return false;
    }

    Replay* ReplayGME::Load(io::Stream* stream, CommandBuffer metadata)
    {
        if (stream->GetSize() > 1024 * 1024 * 128)
            return nullptr;
        auto data = stream->Read();
        Music_Emu* emu;
        if (gme_open_data(data.Items(), long(data.Size()), &emu, kSampleRate))
            return nullptr;
        gme_set_stereo_depth(emu, 1.0f);
        gme_ignore_silence(emu, true);

        uint16_t numTracks = uint16_t(gme_track_count(emu));
        auto tracks = new uint16_t[numTracks];
        auto type = gme_type(emu);
        if (type != gme_hes_type && type != gme_kss_type)
        {
            for (uint16_t i = 0; i < numTracks; i++)
                tracks[i] = i;
        }
        else
        {
            memset(tracks, 0, sizeof(uint16_t) * numTracks);
            auto buf = new int64_t[kSampleRate * 2]; // 4sec buffer
            uint16_t n = 0;
            for (uint16_t i = 0; i < numTracks; i++)
            {
                gme_start_track(emu, i);
                gme_play(emu, 4 * kSampleRate * 2, reinterpret_cast<int16_t*>(buf));
                for (auto b = buf, e = buf + kSampleRate * 2; b < e;)
                {
                    if (*b++ != 0)
                    {
                        tracks[n++] = i;
                        break;
                    }
                }
            }
            gme_start_track(emu, 0);
            delete buf;
            numTracks = n;
        }
        if (numTracks == 0)
        {
            delete[] tracks;
            gme_delete(emu);
            return nullptr;
        }

        return new ReplayGME(emu, tracks, numTracks, metadata, gme_identify_header(data.Items()));
    }

    bool ReplayGME::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        return changed;
    }

    void ReplayGME::Settings::Edit(ReplayMetadataContext& context)
    {
        auto* entry = context.metadata.Find<Settings>();
        if (entry == nullptr)
        {
            // ok, we are here because we never played this song in this player
            entry = context.metadata.Create<Settings>();
        }

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");
        Loops(context, entry->loops, entry->numSongs);
    }

    int32_t ReplayGME::ms_stereoSeparation = 100;
    int32_t ReplayGME::ms_surround = 1;

    ReplayGME::~ReplayGME()
    {
        gme_delete(m_emu);
        delete[] m_tracks;
        delete[] m_loops;
    }

    ReplayGME::ReplayGME(Music_Emu* emu, uint16_t* tracks, uint16_t numTracks, CommandBuffer metadata, const char* ext)
        : Replay(ext, eReplay::GME)
        , m_emu(emu)
        , m_surround(kSampleRate)
        , m_tracks(tracks)
        , m_numTracks(numTracks)
        , m_loops(new LoopInfo[numTracks])
    {
        SetupMetadata(metadata);
    }

    uint32_t ReplayGME::Render(StereoSample* output, uint32_t numSamples)
    {
        if (gme_track_ended(m_emu))
            return 0;

        auto currentPosition = m_currentPosition;
        auto currentDuration = m_currentDuration;
        if ((currentPosition + numSamples) >= currentDuration)
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

        auto buf = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;
        gme_play(m_emu, numSamples * 2, buf);

        output->Convert(m_surround, buf, numSamples, m_stereoSeparation);

        return numSamples;
    }

    uint32_t ReplayGME::Seek(uint32_t timeInMs)
    {
        m_surround.Reset();
        if (gme_seek(m_emu, timeInMs))
            return 0;
        return timeInMs;
    }

    void ReplayGME::ResetPlayback()
    {
        gme_start_track(m_emu, m_tracks[m_subsongIndex]);
        m_surround.Reset();
        m_currentPosition = 0;
        m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;

        gme_info_t* gmeInfo;
        gme_track_info(m_emu, &gmeInfo, m_tracks[m_subsongIndex]);
        gme_free_info(gmeInfo);
        // disable fade, we handle it ourself
        gme_set_fade_msecs(m_emu, -1, 0);
    }

    void ReplayGME::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);

        if (settings)
        {
            for (uint16_t i = 0; i < settings->numSongs; i++)
                m_loops[i] = settings->loops[i].GetFixed();
            m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
        }
    }

    void ReplayGME::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayGME::GetDurationMs() const
    {
        return m_loops[m_subsongIndex].GetDuration();
    }

    uint32_t ReplayGME::GetNumSubsongs() const
    {
        return m_numTracks;
    }

    std::string ReplayGME::GetExtraInfo() const
    {
        std::string metadata;

        gme_info_t* gmeInfo;
        gme_track_info(m_emu, &gmeInfo, m_tracks[m_subsongIndex]);
        metadata  =   "Title     : ";
        metadata += gmeInfo->song;
        metadata += "\nArtist    : ";
        metadata += gmeInfo->author;
        metadata += "\nGame      : ";
        metadata += gmeInfo->game;
        metadata += "\nCopyright : ";
        metadata += gmeInfo->copyright;
        metadata += "\nDumper    : ";
        metadata += gmeInfo->dumper;
        metadata += "\nComment   : ";
        metadata += gmeInfo->comment;
        gme_free_info(gmeInfo);
        return metadata;
    }

    std::string ReplayGME::GetInfo() const
    {
        std::string info;
        gme_info_t* gmeInfo;
        gme_track_info(m_emu, &gmeInfo, m_tracks[m_subsongIndex]);
        auto numChannels = gme_voice_count(m_emu);
        if (numChannels > 1)
        {
            char buf[32];
            sprintf(buf, "%d channels\n", numChannels);
            info = buf;
        }
        else
            info = "1 channel\n";
        info += gmeInfo->system;
        info += "\nGame-Music-Emu " GME_VERSION_STRING;
        gme_free_info(gmeInfo);
        return info;
    }

    void ReplayGME::SetupMetadata(CommandBuffer metadata)
    {
        auto numSongs = m_numTracks;
        auto settings = metadata.Find<Settings>();
        if (settings && settings->numSongs == numSongs)
        {
            for (uint32_t i = 0; i < numSongs; i++)
                m_loops[i] = settings->loops[i].GetFixed();
        }
        else
        {
            auto value = settings ? settings->value : 0;
            settings = metadata.Create<Settings>(sizeof(Settings) + numSongs * sizeof(LoopInfo));
            settings->value = value;
            settings->numSongs = numSongs;
            for (uint16_t i = 0; i < numSongs; i++)
            {
                gme_info_t* gmeInfo;
                gme_track_info(m_emu, &gmeInfo, i);
                settings->loops[i] = { uint32_t(gmeInfo->play_length >= 0 ? gmeInfo->play_length : 0), uint32_t(gmeInfo->loop_length >= 0 ? gmeInfo->loop_length : 0) };
                m_loops[i] = settings->loops[i].GetFixed();
                gme_free_info(gmeInfo);
            }
        }
        m_currentDuration = (uint64_t(m_loops[0].GetDuration()) * kSampleRate) / 1000;
    }
}
// namespace rePlayer
#include "ReplayGBSPlay.h"

#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

extern "C"
{
#include "gbsplay/impulsegen.h"
#include "gbsplay/impulse.h"
    extern const int32_t* base_impulse;

#define IMPULSE_CUTOFF 1.0 /* Cutoff at nyquist limit (no cutoff) */
}

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::GBSPlay,
        .name = "GBSPlay",
        .extensions = "gbs;gbr;gb;vgm;vgz",
        .about = "GBSPlay " GBS_VERSION "\nCopyright (c) 2003-2022 by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>\nChristian Garbs <mitch@cgarbs.de>\nMaximilian Rehkopf <otakon@gmx.net>\nVegard Nossum <vegardno@ifi.uio.no>",
        .settings = "GBSPlay " GBS_VERSION,
        .init = ReplayGBSPlay::Init,
        .release = ReplayGBSPlay::Release,
        .load = ReplayGBSPlay::Load,
        .displaySettings = ReplayGBSPlay::DisplaySettings,
        .editMetadata = ReplayGBSPlay::Settings::Edit
    };

    bool ReplayGBSPlay::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayGBSPlayStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayGBSPlaySurround");
        window.RegisterSerializedData(ms_filter, "ReplayGBSPlayFilter");

        base_impulse = gen_impulsetab(IMPULSE_W_SHIFT, IMPULSE_N_SHIFT, IMPULSE_CUTOFF);

        return false;
    }

    void ReplayGBSPlay::Release()
    {
        free((void*)base_impulse);
    }

    Replay* ReplayGBSPlay::Load(io::Stream* stream, CommandBuffer metadata)
    {
        auto data = stream->Read();
        auto gbs = gbs_open_memory(stream->GetName().c_str(), (char*)data.Items(), data.Size());
        if (gbs == nullptr)
            return nullptr;

        return new ReplayGBSPlay(gbs, metadata);
    }

    bool ReplayGBSPlay::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        const char* const filters[] = { "Off", "Game Boy Classic", "Game Boy Color" };
        changed |= ImGui::Combo("Filter", &ms_filter, filters, NumItemsOf(filters));
        return changed;
    }

    void ReplayGBSPlay::Settings::Edit(ReplayMetadataContext& context)
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
        ComboOverride("Filter", GETSET(entry, overrideFilter), GETSET(entry, filter),
            ms_filter, "Filter: Off", "Filter: Game Boy Classic", "Filter: Game Boy Color");
        Durations(context, entry->GetDurations(), entry->numSongs, "Subsong #%d Duration");
    }

    int32_t ReplayGBSPlay::ms_filter = FILTER_DMG;
    int32_t ReplayGBSPlay::ms_stereoSeparation = 100;
    int32_t ReplayGBSPlay::ms_surround = 1;

    ReplayGBSPlay::~ReplayGBSPlay()
    {
        gbs_close(m_gbs);
        delete[] m_durations;
    }

    ReplayGBSPlay::ReplayGBSPlay(gbs* gbs, CommandBuffer metadata)
        : Replay(GetExtension(gbs), eReplay::GBSPlay)
        , m_gbs(gbs)
        , m_surround(kSampleRate)
        , m_durations(new uint32_t[gbs_get_status(gbs)->songs])
    {
        m_buf.data = m_bufData;
        m_buf.bytes = sizeof(m_bufData);
        m_buf.pos = 0;
        gbs_configure(gbs, 0, 0, 0, 0, 0);
        gbs_configure_output(gbs, &m_buf, kSampleRate);
        gbs_set_sound_callback(gbs, OnSound, this);
        gbs_set_loop_mode(gbs, LOOP_SINGLE);

        SetupMetadata(metadata);
    }

    void ReplayGBSPlay::OnSound(struct gbs* const /*gbs*/, struct gbs_output_buffer* buf, void* priv)
    {
        auto This = reinterpret_cast<ReplayGBSPlay*>(priv);
        This->m_bufPos = 0;
        memcpy(This->m_bufDataCopy, buf->data, sizeof(This->m_bufDataCopy));
    }

    uint32_t ReplayGBSPlay::Render(StereoSample* output, uint32_t numSamples)
    {
        auto currentPosition = m_currentPosition;
        auto currentDuration = m_currentDuration;
        if ((currentPosition + numSamples) >= currentDuration)
        {
            numSamples = currentPosition < currentDuration ? uint32_t(currentDuration - currentPosition) : 0;
            if (numSamples == 0)
            {
                m_currentPosition = 0;
                return 0;
            }
        }
        m_currentPosition = currentPosition + numSamples;

        auto remainingSamples = numSamples;
        auto surround = m_surround.Begin();
        float stereo = 0.5f - 0.5f * m_stereoSeparation / 100.0f;
        while (remainingSamples > 0)
        {
            auto currrentPos = m_bufPos;
            auto endPos = m_buf.pos;
            for (auto buf = m_bufDataCopy + currrentPos * 2; currrentPos < endPos && remainingSamples; ++currrentPos, --remainingSamples)
            {
                float left = *buf++ / 32767.0f;
                float right = *buf++ / 32767.0f;
                StereoSample s;
                s.left = left + (right - left) * stereo;
                s.right = right + (left - right) * stereo;

                *output++ = surround(s);
            }
            m_bufPos = currrentPos;

            if (remainingSamples > 0)
            {
                while (m_bufPos >= m_buf.pos)
                    gbs_step(m_gbs, (kMaxSamples * 1000 / 2) / kSampleRate);
            }
        };
        m_surround.End(surround);

        return numSamples;
    }

    void ReplayGBSPlay::ResetPlayback()
    {
        gbs_init(m_gbs, m_subsongIndex);
        m_surround.Reset();
        m_currentPosition = 0;
        m_currentDuration = (uint64_t(m_durations[m_subsongIndex]) * kSampleRate) / 1000;
        m_bufPos = kMaxSamples;
    }

    void ReplayGBSPlay::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
        gbs_set_filter(m_gbs, gbs_filter_type((settings && settings->overrideFilter) ? settings->filter : ms_filter));

        if (settings)
        {
            auto durations = settings->GetDurations();
            for (uint16_t i = 0; i < settings->numSongs; i++)
                m_durations[i] = durations[i];
            m_currentDuration = (uint64_t(durations[m_subsongIndex]) * kSampleRate) / 1000;
        }
    }

    void ReplayGBSPlay::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    eExtension ReplayGBSPlay::GetExtension(gbs* gbs)
    {
        switch (gbs_get_filetype(gbs))
        {
        case FILETYPE_GBS:
        case FILETYPE_GBS_Z:
            return eExtension::_gbs;
        case FILETYPE_GBR:
        case FILETYPE_GBR_Z:
            return eExtension::_gbr;
        case FILETYPE_GB:
        case FILETYPE_GB_Z:
            return eExtension::_gb;
        case FILETYPE_VGM:
            return eExtension::_vgm;
        case FILETYPE_VGZ:
            return eExtension::_vgz;
        }
        return eExtension::Unknown;
    }

    uint32_t ReplayGBSPlay::GetDurationMs() const
    {
        return m_durations[m_subsongIndex];
    }

    uint32_t ReplayGBSPlay::GetNumSubsongs() const
    {
        auto status = gbs_get_status(m_gbs);
        return status->songs;
    }

    std::string ReplayGBSPlay::GetExtraInfo() const
    {
        std::string metadata;
        auto m = gbs_get_metadata(m_gbs);
        metadata  =   "Title     : ";
        if (m->title)
            metadata += m->title;
        metadata += "\nArtist    : ";
        if (m->author)
            metadata += m->author;
        metadata += "\nCopyright : ";
        if (m->copyright)
            metadata += m->copyright;
        return metadata;
    }

    std::string ReplayGBSPlay::GetInfo() const
    {
        std::string info;
        info = "4 channels\n";
        static const char* const formats[] = {
            "Game Boy Sound System",  "Game Boy Sound System (zip)",
            "Game Boy Ripped Format", "Game Boy Ripped Format (zip)",
            "Game Boy ROM",           "Game Boy ROM (zip)",
            "Video Game Music",       "Video Game Music (zip)"
        };
        info += formats[gbs_get_filetype(m_gbs)];
        info += "\ngbsplay " GBS_VERSION;
        return info;
    }

    void ReplayGBSPlay::SetupMetadata(CommandBuffer metadata)
    {
        auto numSongs = gbs_get_status(m_gbs)->songs;
        auto settings = metadata.Find<Settings>();
        if (settings && settings->numSongs == numSongs)
        {
            auto durations = settings->GetDurations();
            for (uint32_t i = 0; i < numSongs; i++)
                m_durations[i] = durations[i];
            m_currentDuration = (uint64_t(durations[m_subsongIndex]) * kSampleRate) / 1000;
        }
        else
        {
            auto value = settings ? settings->value : 0;
            settings = metadata.Create<Settings>(sizeof(Settings) + numSongs * sizeof(uint32_t));
            settings->value = value;
            settings->numSongs = numSongs;
            auto durations = settings->GetDurations();
            for (uint16_t i = 0; i < numSongs; i++)
            {
                SetSubsong(i);

                uint64_t len = gbs_get_status(m_gbs)->subsong_len;
                if (len == 0)
                    len = kDefaultSongDuration;
                else
                    len = (len * 1000) / 1024;
                durations[i] = uint32_t(len);
                m_durations[i] = uint32_t(len);
            }
            m_currentDuration = (uint64_t(durations[m_subsongIndex]) * kSampleRate) / 1000;
            SetSubsong(0);
        }
    }
}
// namespace rePlayer
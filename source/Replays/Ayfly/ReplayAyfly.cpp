#include "ReplayAyfly.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Ayfly,
        .name = "Ayfly",
        .extensions = "ay;vtx;ym;psg;asc;pt1;pt2;pt3;stc;stp;psc;sqt",
        .about = "ayfly " AYFLY_VERSION_TEXT "b",
        .init = ReplayAyfly::Init,
        .load = ReplayAyfly::Load,
        .displaySettings = ReplayAyfly::DisplaySettings,
        .editMetadata = ReplayAyfly::Settings::Edit
    };

    bool ReplayAyfly::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayAyFlyStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayAyFlySurround");
        window.RegisterSerializedData(ms_oversample, "ReplayAyFlyOversample");

        return false;
    }

    Replay* ReplayAyfly::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {

        auto data = stream->Read();
        auto song = reinterpret_cast<AYSongInfo*>(ay_initsongindirect(const_cast<uint8_t*>(data.Items()), kSampleRate, unsigned long(data.Size())));
        if (!song)
            return nullptr;

        return new ReplayAyfly(song);
    }

    bool ReplayAyfly::DisplaySettings()
    {
        bool changed = false;
        if (ImGui::CollapsingHeader("Ayfly 0.0.25b", ImGuiTreeNodeFlags_None))
        {
            if (!ImGui::GetIO().KeyCtrl)
                ImGui::PushID("Ayfly");
            const char* const oversamples[] = { "x1", "x2", "x3", "x4" };
            changed |= ImGui::Combo("Oversample", &ms_oversample, oversamples, _countof(oversamples));
            changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
            const char* const surround[] = { "Stereo", "Surround" };
            changed |= ImGui::Combo("Output", &ms_surround, surround, _countof(surround));
            if (!ImGui::GetIO().KeyCtrl)
                ImGui::PopID();
        }
        return changed;
    }

    void ReplayAyfly::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find<Settings>(&dummy);

        ComboOverride("Oversample", GETSET(entry, overrideOversample), GETSET(entry, oversample),
            ms_oversample, "Oversample: x1", "Oversample: x2", "Oversample: x3", "Oversample: x4");
        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplayAyfly::ms_stereoSeparation = 100;
    int32_t ReplayAyfly::ms_surround = 1;
    int32_t ReplayAyfly::ms_oversample = 0;

    ReplayAyfly::~ReplayAyfly()
    {
        ay_closesong(reinterpret_cast<void**>(&m_song));
    }

    ReplayAyfly::ReplayAyfly(AYSongInfo* song)
        : Replay(ay_sys_getPlayerExt(*song).c_str() + 1, eReplay::Ayfly)
        , m_song(song)
        , m_surround(kSampleRate)
    {
        ay_setelapsedcallback(song, OnEnd, this);
    }

    bool ReplayAyfly::OnEnd(void* replay)
    {
        reinterpret_cast<ReplayAyfly*>(replay)->m_hasEnded = true;
        return true;
    }

    uint32_t ReplayAyfly::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_hasEnded)
        {
            m_hasEnded = false;
            m_song->stopping = false;
            return 0;
        }

        auto buf = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;

        auto count = ay_rendersongbuffer(m_song, reinterpret_cast<uint8_t*>(buf), numSamples * sizeof(int16_t) * 2);

        numSamples = count / sizeof(int16_t) / 2;
        if (count > 0)
        {
            output->Convert(m_surround, buf, numSamples, m_stereoSeparation);
        }
        else
        {
            m_hasEnded = false;
            m_song->stopping = false;
        }

        return numSamples;
    }

    uint32_t ReplayAyfly::Seek(uint32_t timeInMs)
    {
        ay_seeksong(m_song, timeInMs * 50 / 1000);
        m_surround.Reset();
        return timeInMs;
    }

    void ReplayAyfly::ResetPlayback()
    {
        m_surround.Reset();
        ay_sys_getsonginfoindirect(*m_song);
        ay_resetsong(m_song);
    }

    void ReplayAyfly::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        ay_setoversample(m_song, 1 + ((settings && settings->overrideOversample) ? settings->oversample : ms_oversample));
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
    }

    void ReplayAyfly::SetSubsong(uint16_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        m_song->CurrentSong = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayAyfly::GetDurationMs() const
    {
        return uint32_t((m_song->Length * 1000ull) / 50);
    }

    uint32_t ReplayAyfly::GetNumSubsongs() const
    {
        return m_song->NumSongs;
    }

    std::string ReplayAyfly::GetSubsongTitle() const
    {
        return m_song->TrackName.c_str();
    }

    std::string ReplayAyfly::GetExtraInfo() const
    {
        std::string metadata;
        metadata  = "Title    : ";
        metadata += m_song->Name.c_str();
        metadata += "\nArtist   : ";
        metadata += m_song->Author.c_str();
        metadata += "\nProgram  : ";
        metadata += m_song->PrgName.c_str();
        metadata += "\nTrack    : ";
        metadata += m_song->TrackName.c_str();
        metadata += "\nCompiler : ";
        metadata += m_song->CompName.c_str();
        return metadata;
    }

    std::string ReplayAyfly::GetInfo() const
    {
        std::string info;
        info = "2 channels\n";
        static const char* const players[] = {
            "aylet player", "Vortex", "YM", "PSG", "ASC Sound Master", "Pro Tracker 2", "Pro Tracker 3", "ST Song Compiler", "Sound Tracker Pro", "Pro Sound Creator", "SQ Tracker", "Pro Tracker 1"
        };
        info += players[m_song->player_num];
        info += "\nAyfly 0.0.25b";
        return info;
    }
}
// namespace rePlayer
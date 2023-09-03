#include "ReplaySNDHPlayer.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    #define SndhPlayerVersion "0.71"

    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::SNDHPlayer, .isThreadSafe = false,
        .name = "SNDH-Player",
        .extensions = "sndh",
        .about = "SNDH-Player " SndhPlayerVersion "\nCopyright (c) 2023 Arnaud Carré",
        .settings = "SNDH-Player " SndhPlayerVersion,
        .init = ReplaySNDHPlayer::Init,
        .load = ReplaySNDHPlayer::Load,
        .displaySettings = ReplaySNDHPlayer::DisplaySettings,
        .editMetadata = ReplaySNDHPlayer::Settings::Edit,
        .globals = &ReplaySNDHPlayer::ms_surround
    };

    bool ReplaySNDHPlayer::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        if (&window != nullptr)
            window.RegisterSerializedData(ms_surround, "ReplaySNDHPlayerSurround");

        return false;
    }

    Replay* ReplaySNDHPlayer::Load(io::Stream* stream, CommandBuffer metadata)
    {
        auto data = stream->Read();

        auto* sndh = new SndhFile();
        if (sndh->Load(data.Items(), int(data.Size()), kSampleRate))
            return new ReplaySNDHPlayer(sndh, metadata);
        delete sndh;

        return nullptr;
    }

    bool ReplaySNDHPlayer::DisplaySettings()
    {
        bool changed = false;
        const char* const surround[] = { "Default", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, _countof(surround));
        return changed;
    }

    void ReplaySNDHPlayer::Settings::Edit(ReplayMetadataContext& context)
    {
        const auto settingsSize = sizeof(Settings) + (context.lastSubsongIndex + 1) * sizeof(uint32_t);
        auto* dummy = new (_alloca(settingsSize)) Settings(context.lastSubsongIndex);
        auto* entry = context.metadata.Find<Settings>(dummy);
        if (entry->NumSubsongs() != context.lastSubsongIndex + 1u)
        {
            context.metadata.Remove(entry->commandId);
            entry = dummy;
        }

        ComboOverride("Output", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Default", "Output: Surround");

        const float buttonSize = ImGui::GetFrameHeight();
        auto* durations = entry->durations;
        bool isZero = entry->value == 0;
        for (uint16_t i = 0; i <= context.lastSubsongIndex; i++)
        {
            ImGui::PushID(i);
            bool isEnabled = durations[i] != 0;
            uint32_t duration = isEnabled ? durations[i] : kDefaultSongDuration;
            auto pos = ImGui::GetCursorPosX();
            if (ImGui::Checkbox("##Checkbox", &isEnabled))
                duration = kDefaultSongDuration;
            ImGui::SameLine();
            ImGui::BeginDisabled(!isEnabled);
            char txt[64];
            sprintf(txt, "Subsong #%d Duration", i + 1);
            auto width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 4 - buttonSize;
            ImGui::SetNextItemWidth(2.0f * width / 3.0f - ImGui::GetCursorPosX() + pos);
            ImGui::DragUint("##Duration", &duration, 1000.0f, 1, 0xffFFffFF, txt, ImGuiSliderFlags_NoInput, ImVec2(0.0f, 0.5f));
            int32_t milliseconds = duration % 1000;
            int32_t seconds = (duration / 1000) % 60;
            int32_t minutes = duration / 60000;
            ImGui::SameLine();
            width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3 - buttonSize;
            ImGui::SetNextItemWidth(width / 3.0f);
            ImGui::DragInt("##Minutes", &minutes, 0.1f, 0, 65535, "%d m", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2 - buttonSize;
            ImGui::SetNextItemWidth(width / 2.0f);
            ImGui::DragInt("##Seconds", &seconds, 0.1f, 0, 59, "%d s", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x - buttonSize;
            ImGui::SetNextItemWidth(width);
            ImGui::DragInt("##Milliseconds", &milliseconds, 1.0f, 0, 999, "%d ms", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            if (ImGui::Button("E", ImVec2(buttonSize, 0.0f)))
            {
                context.duration = duration;
                context.subsongIndex = i;
                context.isSongEndEditorEnabled = true;
            }
            else if (context.isSongEndEditorEnabled == false && context.duration != 0 && context.subsongIndex == i)
            {
                milliseconds = context.duration % 1000;
                seconds = (context.duration / 1000) % 60;
                minutes = context.duration / 60000;
                context.duration = 0;
            }
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Open Waveform Viewer");
            ImGui::EndDisabled();
            durations[i] = isEnabled ? uint32_t(minutes) * 60000 + uint32_t(seconds) * 1000 + uint32_t(milliseconds) : 0;
            ImGui::PopID();

            isZero &= durations[i] == 0;
        }

        context.metadata.Update(entry, isZero);
    }

    int32_t ReplaySNDHPlayer::ms_surround = 1;

    ReplaySNDHPlayer::~ReplaySNDHPlayer()
    {
        delete m_sndh;
        delete[] m_durations;
    }

    ReplaySNDHPlayer::ReplaySNDHPlayer(SndhFile* sndh, CommandBuffer metadata)
        : Replay(eExtension::_sndh, eReplay::SNDHPlayer)
        , m_sndh(sndh)
        , m_durations(new uint32_t[sndh->GetSubsongCount()])
        , m_surround(kSampleRate)
    {
        BuildHash(sndh);
        BuildDurations(metadata);
    }

    uint32_t ReplaySNDHPlayer::Render(StereoSample* output, uint32_t numSamples)
    {
        auto currentPosition = m_currentPosition;
        auto currentDuration = m_currentDuration;
        if (currentDuration != 0 && (currentPosition + numSamples) >= currentDuration)
        {
            numSamples = currentPosition < currentDuration ? uint32_t(currentDuration - currentPosition) : 0;
            if (numSamples == 0)
            {
                m_currentPosition = 0;
                return 0;
            }
        }
        m_currentPosition = currentPosition + numSamples;

        if (m_surround.IsEnabled())
        {
            auto* samples = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;
            m_sndh->AudioRenderStereo(samples, numSamples, reinterpret_cast<uint32_t*>(output));
            auto activeChannels = m_activeChannels;
            for (uint32_t i = 0; i < numSamples; i++)
                activeChannels |= reinterpret_cast<uint32_t*>(output)[i];
            m_activeChannels = activeChannels;
            output->Convert(m_surround, samples, numSamples, 100, 1.333f);
        }
        else
        {
            auto* samples = reinterpret_cast<int16_t*>(output + numSamples) - numSamples;
            m_sndh->AudioRender(samples, numSamples, reinterpret_cast<uint32_t*>(output));
            auto activeChannels = m_activeChannels;
            for (uint32_t i = 0; i < numSamples; i++)
                activeChannels |= reinterpret_cast<uint32_t*>(output)[i];
            m_activeChannels = activeChannels;
            output->Convert(m_surround, samples, samples, numSamples, 100);
        }

        return numSamples;
    }

    uint32_t ReplaySNDHPlayer::Seek(uint32_t timeInMs)
    {
        auto currentDuration = m_currentDuration;
        auto seekPosition = (uint64_t(timeInMs) * kSampleRate) / 1000;
        auto currentPosition = m_currentPosition;
        if (seekPosition > currentDuration)
            seekPosition = currentDuration;
        if (seekPosition < currentPosition)
        {
            m_sndh->InitSubSong(m_subsongIndex + 1);
            currentPosition = 0;
        }
        m_sndh->AudioNull(int(seekPosition - currentPosition));
        m_currentPosition = seekPosition;
        if (seekPosition != currentPosition)
            m_surround.Reset();
        return uint32_t((seekPosition * 1000) / kSampleRate);
    }

    void ReplaySNDHPlayer::ResetPlayback()
    {
        m_sndh->InitSubSong(m_subsongIndex + 1);
        m_activeChannels = 0;
        m_currentPosition = 0;
        m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
        m_surround.Reset();
    }

    void ReplaySNDHPlayer::ApplySettings(const CommandBuffer metadata)
    {
        auto* settings = metadata.Find<Settings>();
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : *static_cast<int32_t*>(g_replayPlugin.globals));

        if (settings && settings->NumSubsongs() == GetNumSubsongs())
        {
            auto* durations = settings->durations;
            for (uint32_t i = 0, e = GetNumSubsongs(); i < e; i++)
                m_durations[i] = durations[i];
            m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
        }
    }

    void ReplaySNDHPlayer::SetSubsong(uint16_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplaySNDHPlayer::GetDurationMs() const
    {
        SndhFile::SubSongInfo subsongInfo;
        m_sndh->GetSubsongInfo(m_subsongIndex + 1, subsongInfo);
        uint32_t currentDuration = m_durations[m_subsongIndex];
        if (currentDuration == 0)
            currentDuration = uint32_t(((subsongInfo.playerTickCount == 0 ? GetTickCountFromSc68() : subsongInfo.playerTickCount) * 1000) / subsongInfo.playerTickRate);
        return currentDuration;
    }

    uint32_t ReplaySNDHPlayer::GetNumSubsongs() const
    {
        return uint32_t(m_sndh->GetSubsongCount());
    }

    std::string ReplaySNDHPlayer::GetExtraInfo() const
    {
        SndhFile::SubSongInfo subsongInfo;
        m_sndh->GetSubsongInfo(m_subsongIndex + 1, subsongInfo);

        std::string metadata;
        metadata  = "Title    : ";
        if (subsongInfo.musicName)
            metadata += subsongInfo.musicName;
        metadata += "\nArtist   : ";
        if (subsongInfo.musicAuthor)
            metadata += subsongInfo.musicAuthor;
        metadata += "\nYear     : ";
        if (subsongInfo.year)
            metadata += subsongInfo.year;
        metadata += "\nRipper   : ";
        if (subsongInfo.ripper)
            metadata += subsongInfo.ripper;
        metadata += "\nConverter: ";
        if (subsongInfo.converter)
            metadata += subsongInfo.converter;
        return metadata;
    }

    std::string ReplaySNDHPlayer::GetInfo() const
    {
        SndhFile::SubSongInfo subsongInfo;
        m_sndh->GetSubsongInfo(m_subsongIndex + 1, subsongInfo);

        auto activeChannels = m_activeChannels;
        char numChannels = activeChannels & 0xff ? '1' : '0';
        numChannels += activeChannels & 0xff00 ? 1 : 0;
        numChannels += activeChannels & 0xff0000 ? 1 : 0;
        numChannels += activeChannels & 0xff000000 ? 1 : 0;

        std::string info;
        info = numChannels;
        info += numChannels < '2' ? " channel\n" : " channels\n";

        static const char* types[] = { "YM / ", "YM / ", "STE / ", "YM-STE / " };
        info += types[((activeChannels & 0xffFFff) ? 1 : 0) | ((activeChannels & 0xff000000) ? 2 : 0)];

        char txt[16];
        sprintf(txt, "%d", subsongInfo.playerTickRate);
        info += txt;
        info += " Hz\nSNDH-Player " SndhPlayerVersion;
        return info;
    }

    int32_t ReplaySNDHPlayer::GetTickCountFromSc68() const
    {
        #define HBIT 32                         /* # of bit for hash     */
        #define TBIT 6                          /* # of bit for track    */
        #define WBIT 6                          /* # of bit for hardware */
        #define FBIT (64-HBIT-TBIT-WBIT)        /* # of bit for frames   */
        #define HFIX (32-HBIT)

        #define TIMEDB_ENTRY(HASH,TRACK,FRAMES,FLAGS) \
            { 0x##HASH>>HFIX, TRACK-1, FLAGS, FRAMES }
        #define E_EMPTY { 0,0,0,0 }

        typedef struct
        {
            unsigned int hash : HBIT;           /* hash code              */
            unsigned int track : TBIT;           /* track number (0-based) */
            unsigned int flags : WBIT;           /* see enum               */
            unsigned int frames : FBIT;           /* length in frames       */
        } dbentry_t;

        #define STE 0
        #define YM  0
        #define TA  0
        #define TB  0
        #define TC  0
        #define TD  0
        #define NA  0

        static dbentry_t s_db[] = {
#           include "..\SC68\sc68\file68\src\timedb.inc.h"
        };

        dbentry_t e;
        e.hash = m_hash >> HFIX;
        e.track = m_subsongIndex;
        if (auto* s = reinterpret_cast<dbentry_t*>(bsearch(&e, s_db, sizeof(s_db) / sizeof(dbentry_t), sizeof(dbentry_t), [](const void* ea, const void* eb)
        {
            auto* a = reinterpret_cast<const dbentry_t*>(ea);
            auto* b = reinterpret_cast<const dbentry_t*>(eb);

            int v = a->hash - b->hash;
            if (!v)
                v = a->track - b->track;
            return v;
        })))
            return s->frames;
        return 0;
    }

    void ReplaySNDHPlayer::BuildHash(SndhFile* sndh)
    {
        // Hash taken from sc68
        uint32_t h = 0;
        int n = 32;
        const uint8_t* k = reinterpret_cast<const uint8_t*>(sndh->GetRawData());
        do
        {
            h += *k++;
            h += h << 10;
            h ^= h >> 6;
        } while (--n);

        n = sndh->GetRawDataSize();
        k = reinterpret_cast<const uint8_t*>(sndh->GetRawData());
        do
        {
            h += *k++;
            h += h << 10;
            h ^= h >> 6;
        } while (--n);
        m_hash = h;
    }

    void ReplaySNDHPlayer::BuildDurations(CommandBuffer metadata)
    {
        uint32_t numSubsongs = GetNumSubsongs();
        auto settings = metadata.Find<Settings>();
        if (settings && settings->NumSubsongs() == numSubsongs)
        {
            for (uint32_t i = 0; i < numSubsongs; i++)
                m_durations[i] = settings->durations[i];
        }
        else
        {
            metadata.Remove(Settings::kCommandId);
            for (uint16_t i = 0; i < numSubsongs; i++)
                m_durations[i] = 0;
        }
        m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
    }
}
// namespace rePlayer
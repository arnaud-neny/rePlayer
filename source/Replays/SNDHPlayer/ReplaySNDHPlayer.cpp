#include "ReplaySNDHPlayer.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::SNDHPlayer, .isThreadSafe = false,
        .name = "SNDH-Player",
        .extensions = "sndh",
        .about = "AtariAudio " ATARI_AUDIO_VERSION "\nCopyright (c) 2023-2026 Arnaud Carré",
        .settings = "SNDH-Player/AtariAudio " ATARI_AUDIO_VERSION,
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
        if (stream->GetSize() < 8 || stream->GetSize() > 1024 * 1024)
            return nullptr;
        auto data = stream->Read();

        if (auto* sndh = SndhRenderer::Create(data.Items(), int(data.Size()), kSampleRate))
            return new ReplaySNDHPlayer(sndh, metadata);

        return nullptr;
    }

    bool ReplaySNDHPlayer::DisplaySettings()
    {
        bool changed = false;
        const char* const surround[] = { "Default", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        return changed;
    }

    void ReplaySNDHPlayer::Settings::Edit(ReplayMetadataContext& context)
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
            ms_surround, "Output: Default", "Output: Surround");
        bool isZero = Loops(context, entry->loops, context.lastSubsongIndex + 1, kDefaultSongDuration);

        context.metadata.Update(entry, entry->value == 0 && isZero);
    }

    int32_t ReplaySNDHPlayer::ms_surround = 1;

    ReplaySNDHPlayer::~ReplaySNDHPlayer()
    {
        SndhRenderer::Destroy(m_sndh);
        delete[] m_loops;
    }

    ReplaySNDHPlayer::ReplaySNDHPlayer(SndhRenderer* sndh, CommandBuffer metadata)
        : Replay(eExtension::_sndh, eReplay::SNDHPlayer)
        , m_sndh(sndh)
        , m_loops(new LoopInfo[sndh->GetSongInfo().subsongCount])
        , m_surround(kSampleRate)
    {
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
                if (m_loops[m_subsongIndex].IsValid())
                    m_currentDuration = (uint64_t(m_loops[m_subsongIndex].length) * kSampleRate) / 1000;
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
            output->Convert(m_surround, samples, numSamples, 100);
        }
        else
        {
            auto* samples = reinterpret_cast<int16_t*>(output + numSamples) - numSamples;
            m_sndh->AudioRenderWithVisualInfos(samples, numSamples, reinterpret_cast<uint32_t*>(output));
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
        m_sndh->AudioRender(nullptr, uint32_t(seekPosition - currentPosition));
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
            auto* loops = settings->loops;
            for (uint32_t i = 0, e = GetNumSubsongs(); i < e; i++)
                m_loops[i] = loops[i].GetFixed();
            m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
        }
    }

    void ReplaySNDHPlayer::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplaySNDHPlayer::GetDurationMs() const
    {
        uint32_t currentDuration = m_loops[m_subsongIndex].GetDuration();
        if (currentDuration == 0)
            currentDuration = m_sndh->GetSubsongDurationMs(m_subsongIndex + 1);
        return currentDuration;
    }

    uint32_t ReplaySNDHPlayer::GetNumSubsongs() const
    {
        return uint32_t(m_sndh->GetSongInfo().subsongCount);
    }

    std::string ReplaySNDHPlayer::GetExtraInfo() const
    {
        auto& songInfo = m_sndh->GetSongInfo();

        std::string metadata;
        metadata  = "Title    : ";
        if (songInfo.musicName)
            metadata += songInfo.musicName;
        metadata += "\nArtist   : ";
        if (songInfo.musicAuthor)
            metadata += songInfo.musicAuthor;
        metadata += "\nYear     : ";
        if (songInfo.year)
            metadata += songInfo.year;
        metadata += "\nRipper   : ";
        if (songInfo.ripper)
            metadata += songInfo.ripper;
        metadata += "\nConverter: ";
        if (songInfo.converter)
            metadata += songInfo.converter;
        return metadata;
    }

    std::string ReplaySNDHPlayer::GetInfo() const
    {
        auto& songInfo = m_sndh->GetSongInfo();

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
        sprintf(txt, "%d", songInfo.playerTickRate);
        info += txt;
        info += " Hz\nAtariAudio " ATARI_AUDIO_VERSION;
        return info;
    }

    void ReplaySNDHPlayer::BuildDurations(CommandBuffer metadata)
    {
        uint32_t numSubsongs = GetNumSubsongs();
        auto settings = metadata.Find<Settings>();
        if (settings && settings->NumSubsongs() == numSubsongs)
        {
            for (uint32_t i = 0; i < numSubsongs; i++)
                m_loops[i] = settings->loops[i].GetFixed();
        }
        else
        {
            metadata.Remove(Settings::kCommandId);
            for (uint16_t i = 0; i < numSubsongs; i++)
                m_loops[i] = {};
        }
        m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
    }
}
// namespace rePlayer
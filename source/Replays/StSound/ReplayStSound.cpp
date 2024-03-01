#include "ReplayStSound.h"
#include "StSound/YmMusic.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::StSound,
        .name = "StSound",
        .extensions = "ym",
        .about = "StSound\nCopyright (c) 2021 Arnaud Carré",
        .settings = "StSound",
        .init = ReplayStSound::Init,
        .load = ReplayStSound::Load,
        .displaySettings = ReplayStSound::DisplaySettings,
        .editMetadata = ReplayStSound::Settings::Edit
    };

    bool ReplayStSound::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_isLowpassFilterEnabled, "ReplayStSoundLowpassFilter");
        window.RegisterSerializedData(ms_surround, "ReplayStSoundSurround");

        return false;
    }

    Replay* ReplayStSound::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        if (stream->GetSize() > 1024 * 1024)
            return nullptr;
        auto module = new CYmMusic(kSampleRate);
        auto data = stream->Read();
        if (!module->loadMemory(const_cast<uint8_t*>(data.Items()), static_cast<ymu32>(data.Size())))
        {
            delete module;
            return nullptr;
        }

        return new ReplayStSound(module);
    }

    bool ReplayStSound::DisplaySettings()
    {
        bool changed = false;
        const char* const filter[] = { "Off", "On" };
        int32_t index = ms_isLowpassFilterEnabled ? 1 : 0;
        changed |= ImGui::Combo("Lowpass filter", &index, filter, _countof(filter));
        ms_isLowpassFilterEnabled = index != 0;
        const char* const surround[] = { "Default", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, _countof(surround));
        return changed;
    }

    void ReplayStSound::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find<Settings>(&dummy);

        ComboOverride("LowPassFilter", GETSET(entry, overrideLowPassFilter), GETSET(entry, lowPassFilter),
            ms_isLowpassFilterEnabled, "No filter", "Low-pass filter");
        ComboOverride("Output", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Default", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    bool ReplayStSound::ms_isLowpassFilterEnabled = true;
    int32_t ReplayStSound::ms_surround = 0;

    ReplayStSound::~ReplayStSound()
    {
        delete m_module;
    }

    ReplayStSound::ReplayStSound(CYmMusic* module)
        : Replay(eExtension::_ym, eReplay::StSound)
        , m_module(module)
        , m_surround(kSampleRate)
    {
        module->setLoopMode(YMTRUE);
    }

    bool ReplayStSound::IsSeekable() const
    {
        return m_module->isSeekable();
    }

    uint32_t ReplayStSound::Render(StereoSample* output, uint32_t numSamples)
    {
        auto loop = m_module->getLoop();
        std::swap(m_loop, loop);
        if (m_loop != loop)
            return 0;

        const uint32_t bufferSize = kSampleRate / 50;

        auto numSamplesLeft = numSamples;
        auto isStereo = m_surround.IsEnabled();
        while (numSamplesLeft)
        {
            auto numSamplesToUpdate = Min(bufferSize, numSamplesLeft);
            auto samples = reinterpret_cast<ymsample*>(output + numSamplesToUpdate) - numSamplesToUpdate * 2;
            m_module->update(samples, numSamplesToUpdate, isStereo);
            output = output->Convert(m_surround, samples, numSamplesToUpdate, 100, isStereo ? 1.333f : 1.0f);
            numSamplesLeft -= numSamplesToUpdate;
            loop = m_module->getLoop();
            if (m_loop != loop)
                break;
            m_loop = loop;
        }

        return numSamples - numSamplesLeft;
    }

    uint32_t ReplayStSound::Seek(uint32_t timeInMs)
    {
        if (m_module->isSeekable())
        {
            m_surround.Reset();
            return m_module->setMusicTime(timeInMs);
        }
        return Replay::Seek(timeInMs);
    }


    void ReplayStSound::ResetPlayback()
    {
        m_module->stop();
        m_module->play();
        m_surround.Reset();
    }

    void ReplayStSound::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_module->setLowpassFilter((settings && settings->overrideLowPassFilter) ? settings->lowPassFilter != 0 : ms_isLowpassFilterEnabled);
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
    }

    void ReplayStSound::SetSubsong(uint16_t /*subsongIndex*/)
    {
    }

    uint32_t ReplayStSound::GetDurationMs() const
    {
        ymMusicInfo_t info;
        m_module->getMusicInfo(&info);
        return static_cast<uint32_t>(info.musicTimeInMs);
    }

    uint32_t ReplayStSound::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayStSound::GetExtraInfo() const
    {
        std::string metadata;

        ymMusicInfo_t ymMusicInfo;
        m_module->getMusicInfo(&ymMusicInfo);

        if (ymMusicInfo.pSongName[0])
        {
            metadata = ymMusicInfo.pSongName;
            if (metadata.back() != '\n')
                metadata += "\n";
        }
        if (ymMusicInfo.pSongAuthor[0])
        {
            metadata += ymMusicInfo.pSongAuthor;
            if (metadata.back() != '\n')
                metadata += "\n";
        }
        if (ymMusicInfo.pSongComment[0])
            metadata += ymMusicInfo.pSongComment;

        return metadata;
    }

    std::string ReplayStSound::GetInfo() const
    {
        std::string info;

        auto songType = m_module->getSongType();
        if ((songType >= YM_MIX1) && (songType < YM_MIXMAX))
            info = "1 channel\n";
        else if ((songType >= YM_TRACKER1) && (songType < YM_TRACKERMAX))
        {
            auto nbVoice = m_module->getNbVoice();
            if (nbVoice == 1)
                info = "1 channel\n";
            else
            {
                char buf[16];
                sprintf(buf, "%d channels\n", nbVoice);
                info = buf;
            }
        }
        else
            info = "3 channels\n";

        ymMusicInfo_t ymMusicInfo;
        m_module->getMusicInfo(&ymMusicInfo);

        info += ymMusicInfo.pSongType;
        info += "\nStSound ";
        info += ymMusicInfo.pSongPlayer;

        return info;
    }
}
// namespace rePlayer
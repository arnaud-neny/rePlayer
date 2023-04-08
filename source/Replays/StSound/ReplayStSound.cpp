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
        .init = ReplayStSound::Init,
        .load = ReplayStSound::Load,
        .displaySettings = ReplayStSound::DisplaySettings,
        .editMetadata = ReplayStSound::Settings::Edit
    };

    bool ReplayStSound::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_isLowpassFilterEnabled, "ReplayStSoundLowpassFilter");

        return false;
    }

    Replay* ReplayStSound::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
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
        if (ImGui::CollapsingHeader("StSound", ImGuiTreeNodeFlags_None))
        {
            if (!ImGui::GetIO().KeyCtrl)
                ImGui::PushID("StSound");

            const char* const filter[] = { "Off", "On" };
            int32_t index = ms_isLowpassFilterEnabled ? 1 : 0;
            changed |= ImGui::Combo("Lowpass filter", &index, filter, _countof(filter));
            ms_isLowpassFilterEnabled = index != 0;

            if (!ImGui::GetIO().KeyCtrl)
                ImGui::PopID();
        }
        return changed;
    }

    void ReplayStSound::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find<Settings>(&dummy);

        ComboOverride("LowPassFilter", GETSET(entry, overrideLowPassFilter), GETSET(entry, lowPassFilter),
            ms_isLowpassFilterEnabled, "No filter", "Low-pass filter");

        context.metadata.Update(entry, entry->value == 0);
    }

    bool ReplayStSound::ms_isLowpassFilterEnabled = true;

    ReplayStSound::~ReplayStSound()
    {
        delete m_module;
    }

    ReplayStSound::ReplayStSound(CYmMusic* module)
        : Replay(eExtension::_ym, eReplay::StSound)
        , m_module(module)
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
        while (numSamplesLeft)
        {
            auto numSamplesToUpdate = Min(bufferSize, numSamplesLeft);
            auto samples = reinterpret_cast<ymsample*>(output + numSamples) - numSamplesToUpdate;
            m_module->update(samples, numSamplesToUpdate);
            numSamplesLeft -= numSamplesToUpdate;
            output = output->ConvertMono(samples, numSamplesToUpdate);
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
            return m_module->setMusicTime(timeInMs);
        return Replay::Seek(timeInMs);
    }


    void ReplayStSound::ResetPlayback()
    {
        m_module->stop();
        m_module->play();
    }

    void ReplayStSound::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_module->setLowpassFilter((settings && settings->overrideLowPassFilter) ? settings->lowPassFilter != 0 : ms_isLowpassFilterEnabled);
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
        std::string info = "1 channel\n";

        ymMusicInfo_t ymMusicInfo;
        m_module->getMusicInfo(&ymMusicInfo);

        info += ymMusicInfo.pSongType;
        info += "\nStSound ";
        info += ymMusicInfo.pSongPlayer;

        return info;
    }
}
// namespace rePlayer
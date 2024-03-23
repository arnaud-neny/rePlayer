#include "ReplayOrganya.h"

#include <Audio/Audiotypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Organya,
        .name = "Organya",
        .extensions = "org",
        .about = "Organya 3.0.7\nCopyright (c) 1999 Studio Pixel\nOrg Maker 2 by Rxo Inverse\nOrg Maker 3 by Strultz\nfoo_input_org by Christopher Snowhill",
        .init = ReplayOrganya::Init,
        .release = ReplayOrganya::Release,
        .load = ReplayOrganya::Load
    };

    bool ReplayOrganya::Init(SharedContexts* ctx, Window& window)
    {
        (void)window;
        ctx->Init();
        Organya::initialize();

        return false;
    }

    void ReplayOrganya::Release()
    {
        Organya::release();
    }

    Replay* ReplayOrganya::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        if (auto* song = Organya::Load(stream, kSampleRate))
            return new ReplayOrganya(song);
        return nullptr;
    }

    ReplayOrganya::~ReplayOrganya()
    {
        Organya::Unload(m_song);
    }

    ReplayOrganya::ReplayOrganya(Organya::Song* song)
        : Replay(eExtension::_org, eReplay::Organya)
        , m_song(song)
    {}


    uint32_t ReplayOrganya::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_hasLooped)
        {
            m_hasLooped = false;
            return 0;
        }

        auto numSamplesToRender = numSamples;
        auto remainingSamples = m_remainingSamples;
        auto currentPosition = m_currentPosition;
        while (numSamplesToRender)
        {
            if (remainingSamples == 0)
            {
                m_samples = Organya::Render(m_song);
                if (m_samples.empty())
                {
                    m_remainingSamples = remainingSamples;
                    m_hasLooped = numSamplesToRender != numSamples;
                    return numSamples - numSamplesToRender;
                }
                remainingSamples = uint32_t(m_samples.size() / 2);
            }
            auto numSamplesToCopy = Min(numSamplesToRender, remainingSamples);
            memcpy(output, m_samples.data() + (m_samples.size() - remainingSamples * 2), numSamplesToCopy * sizeof(StereoSample));
            output += numSamplesToCopy;
            numSamplesToRender -= numSamplesToCopy;
            remainingSamples -= numSamplesToCopy;
            currentPosition += numSamplesToCopy;
        }
        m_remainingSamples = remainingSamples;
        m_currentPosition = currentPosition;
        return numSamples;
    }

    uint32_t ReplayOrganya::Seek(uint32_t timeInMs)
    {
        auto currentPosition = m_currentPosition;
        auto pos = uint64_t(timeInMs) * kSampleRate / 1000ull;
        if (pos < currentPosition)
        {
            currentPosition = 0;
            ResetPlayback();
        }
        auto numSamples = pos - currentPosition;
        auto remainingSamples = m_remainingSamples;
        while (numSamples)
        {
            if (remainingSamples == 0)
            {
                m_samples = Organya::Render(m_song);
                remainingSamples = uint32_t(m_samples.size() / 2);
            }
            auto numSamplesToCopy = uint32_t(Min(numSamples, remainingSamples));
            numSamples -= numSamplesToCopy;
            remainingSamples -= numSamplesToCopy;
            currentPosition += numSamplesToCopy;
        }
        m_remainingSamples = remainingSamples;
        m_currentPosition = currentPosition;
        return timeInMs;
    }

    void ReplayOrganya::ResetPlayback()
    {
        Organya::Reset(m_song);
        m_hasLooped = 0;
        m_remainingSamples = 0;
        m_currentPosition = 0;
    }

    void ReplayOrganya::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayOrganya::SetSubsong(uint32_t subsongIndex)
    {
        (void)subsongIndex;
    }

    uint32_t ReplayOrganya::GetDurationMs() const
    {
        return Organya::GetDuration(m_song);
    }

    uint32_t ReplayOrganya::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayOrganya::GetExtraInfo() const
    {
        std::string metadata;
        return metadata;
    }

    std::string ReplayOrganya::GetInfo() const
    {
        std::string info;
        info = "2 channels\n";
        auto version = Organya::GetVersion(m_song);
        if (version == 1)
            info += "Org-01";
        else if(version == 2)
            info += "Org-02";
        else
            info += "Org-03";
        info += "\nOrganya 3.0.7";
        return info;
    }
}
// namespace rePlayer
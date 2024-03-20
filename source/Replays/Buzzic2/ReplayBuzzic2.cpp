#include "ReplayBuzzic2.h"

#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Buzzic2,
        .name = "Buzzic 2",
        .extensions = "buz2",
        .about = "Buzzic 2\nCopyright (c) 2010 Stan/Rebels",
        .settings = "Buzzic 2",
        .load = ReplayBuzzic2::Load,
    };

    Replay* ReplayBuzzic2::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto data = stream->Read();
        if (auto buzzic2 = Buzzic2Load(data.Items(), data.Size()))
            return new ReplayBuzzic2(buzzic2);
        return nullptr;
    }

    ReplayBuzzic2::~ReplayBuzzic2()
    {
        Buzzic2Release(m_buzzic2);
    }

    ReplayBuzzic2::ReplayBuzzic2(Buzzic2* buzzic2)
        : Replay(eExtension::_buz2, eReplay::Buzzic2)
        , m_buzzic2(buzzic2)
    {
    }

    uint32_t ReplayBuzzic2::Render(StereoSample* output, uint32_t numSamples)
    {
        return Buzzic2Render(m_buzzic2, output, numSamples);
    }

    void ReplayBuzzic2::ResetPlayback()
    {
        Buzzic2Reset(m_buzzic2);
    }

    void ReplayBuzzic2::ApplySettings(const CommandBuffer /*metadata*/)
    {
    }

    void ReplayBuzzic2::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
    }

    uint32_t ReplayBuzzic2::GetDurationMs() const
    {
        return Buzzic2DurationMs(m_buzzic2);
    }

    uint32_t ReplayBuzzic2::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayBuzzic2::GetExtraInfo() const
    {
        std::string metadata;
        for (uint32_t i = 0; i < Buzzic2NumIntruments(m_buzzic2); i++)
        {
            auto* label = Buzzic2IntrumentName(m_buzzic2, i);
            if (label[0])
            {
                if (!metadata.empty())
                    metadata += "\n";
                metadata += label;
            }
        }
        return metadata;
    }

    std::string ReplayBuzzic2::GetInfo() const
    {
        std::string info;

        info += "2 channels\n";
        char txt[8];
        sprintf(txt, "%u", Buzzic2NumIntruments(m_buzzic2));
        info += txt;
        info += Buzzic2NumIntruments(m_buzzic2) > 1 ? " instruments" : " instrument";
        info += "\nBuzzic 2";

        return info;
    }
}
// namespace rePlayer
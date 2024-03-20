#include "ReplayIXalance.h"

#include "webixs/Module.h"
#include "webixs/PlayerCore.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <ReplayDll.h>

#include <bit>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::iXalance, .isThreadSafe = false,
        .name = "iXalance",
        .extensions = "ixs",
        .about = "iXalance\nCopyright (c) 2022 Juergen Wothke\nCopyright (c) original x86 code : Shortcut Software Development BV",
        .load = ReplayIXalance::Load
    };

    Replay* ReplayIXalance::Load(io::Stream* stream, CommandBuffer metadata)
    {
        (void)metadata;
        uint32_t magic = 0;
        stream->Read(&magic, sizeof(magic));
        if (magic != 0x21535849)
            return nullptr;
        stream->Seek(0, io::Stream::SeekWhence::kSeekBegin);

        IXS::PlayerIXS* player = IXS::IXS__PlayerIXS__createPlayer_00405d90(kSampleRate);
        float progress = 0.0f;
        auto data = stream->Read();
        int r = (player->vftable->loadIxsFileData)(player, (byte*)data.Items(), uint32_t(data.Size()), 0, 0, &progress);
        if (r == 0)
        {
            (player->vftable->initAudioOut)(player);  // depends on loaded song
            return new ReplayIXalance(player);
        }

        player->vftable->delete0(player);
        return nullptr;
    }

    ReplayIXalance::~ReplayIXalance()
    {
        m_player->vftable->delete0(m_player);
    }

    ReplayIXalance::ReplayIXalance(IXS::PlayerIXS* player)
        : Replay(eExtension::_ixs, eReplay::iXalance)
        , m_player(player)
    {}

    uint32_t ReplayIXalance::Render(StereoSample* output, uint32_t numSamples)
    {
        auto remainingSamples = numSamples;
        while (remainingSamples)
        {
            if (m_size != 0)
            {
                auto numToConvert = Min(m_size, remainingSamples);
                output = output->Convert(m_buffer, numToConvert);
                m_buffer += numToConvert * 2;
                m_size -= numToConvert;
                remainingSamples -= numToConvert;
            }
            else
            {
                auto loopCount = m_player->vftable->isSongEnd(m_player);
                if (loopCount != m_lastLoop)
                {
                    m_lastLoop = loopCount;
                    return 0;
                }
                (m_player->vftable->genAudio)(m_player);

                m_buffer = reinterpret_cast<int16_t*>((m_player->vftable->getAudioBuffer)(m_player));

                auto is16bit = (m_player->vftable->isAudioOutput16Bit)(m_player);
                auto isStereo = (m_player->vftable->isAudioOutputStereo)(m_player);
                assert(is16bit && isStereo);

                m_size = (m_player->vftable->getAudioBufferLen)(m_player);
            }
        }

        return numSamples;
    }

    void ReplayIXalance::ResetPlayback()
    {
        m_lastLoop = 0;
        (m_player->vftable->initAudioOut)(m_player);
    }

    void ReplayIXalance::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayIXalance::SetSubsong(uint32_t)
    {}

    uint32_t ReplayIXalance::GetDurationMs() const
    {
        return 0;
    }

    uint32_t ReplayIXalance::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayIXalance::GetExtraInfo() const
    {
        std::string info;
        info = (m_player->vftable->getSongTitle)(m_player);
        bool doReturn = false;
        for (uint32_t i = 0; i < m_player->ptrCore_0x4->ptrModule_0x8->impulseHeader_0x0.SmpNum_0x24; i++)
        {
            if (m_player->ptrCore_0x4->ptrModule_0x8->smplHeadPtrArr0_0xd0[i]->filename_0x4[0] != 0)
            {
                if (doReturn = !doReturn)
                    info += "\n";
                else
                    info += " / ";
                info += m_player->ptrCore_0x4->ptrModule_0x8->smplHeadPtrArr0_0xd0[i]->filename_0x4;
            }
        }
        return info;
    }

    std::string ReplayIXalance::GetInfo() const
    {
        char buf[64];
        sprintf(buf, "%d channels\niXalance\nWebIXS/IXSPlayer 1.20", std::popcount(m_player->ptrCore_0x4->ptrModule_0x8->channels));
        return buf;
    }
}
// namespace rePlayer
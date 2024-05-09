#include "ReplayEuphony.h"

#include <Audio/Audiotypes.inl.h>
#include <Core/String.h>
#include <Imgui.h>
#include <ReplayDll.h>

#include <filesystem>

namespace rePlayer
{
    #define EuphonyVersion "eupmini @a45c3f2"

    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Euphony,
        .name = "eupmini",
        .extensions = "eup",
        .about = "eupmini\nCopyright (c) 1995-1997, 2000 Tomoaki Hayasaka\nWin32 porting 2002, 2003 IIJIMA Hiromitsu aka Delmonta, and anonymous K\n2023 Giangiacomo Zaffini",
        .load = ReplayEuphony::Load
    };

    Replay* ReplayEuphony::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        Array<uint32_t> subsongs;
        uint32_t fileIndex = 0;
        for (SmartPtr<io::Stream> eupStream = stream; eupStream; eupStream = eupStream->Next())
        {
            if (stream->GetSize() > sizeof(EUPHEAD))
            {
                auto eupbuf = stream->Read();
                auto* eupHeader = eupbuf.Items<const EUPHEAD>();
                if (eupHeader->size >= 0 && (eupbuf.Size() - eupHeader->size) == 3598)
                    subsongs.Add(fileIndex);
            }
            fileIndex++;
        }
        if (subsongs.IsNotEmpty())
            return new ReplayEuphony(stream, std::move(subsongs));
        return nullptr;
    }

    EUPPlayer* ReplayEuphony::Load()
    {
        auto stream = m_stream;
        stream->Seek(0, io::Stream::kSeekBegin);
        for (uint32_t fileIndex = 0; stream; fileIndex++)
        {
            if (fileIndex == m_subsongs[m_subsongIndex])
            {
                std::filesystem::path entryPath(stream->GetName());
                m_title = reinterpret_cast<const char*>(entryPath.stem().u8string().c_str());

                m_data.Resize(uint32_t(stream->GetSize()));
                stream->Read(m_data.Items(), m_data.NumItems());
                break;
            }
        }

        // とりあえず, TOWNS emu のみに対応.

        if (m_data.Size() < 2048 + 6 + 6)
            return nullptr;

        // ヘッダ読み込み用バッファ
        auto* eupHeader = m_data.Items<const EUPHEAD>();

        if (eupHeader->size < 0 || (m_data.Size() - eupHeader->size) != 3598)
            return nullptr;

        EUP_TownsEmulator* device = new EUP_TownsEmulator;
        EUPPlayer* player = new EUPPlayer;
        /* signed 16 bit sample, little endian */
        device->outputSampleUnsigned(false);
        device->outputSampleLSBFirst(true);
        device->outputSampleSize(streamAudioSampleOctectSize); /* octects a.k.a. bytes */
        device->outputSampleChannels(streamAudioChannelsNum); /* 1 = monaural, 2 = stereophonic */
        device->rate(streamAudioRate); /* Hz */

        player->outputDevice(device);

        // ヘッダ情報のコピー
        for (int trk = 0; trk < 32; trk++)
            player->mapTrack_toChannel(trk, eupHeader->trk_midi_ch[trk]);
        for (int trk = 0; trk < 6; trk++)
            device->assignFmDeviceToChannel(eupHeader->fm_midi_ch[trk]);
        for (int trk = 0; trk < 8; trk++)
            device->assignPcmDeviceToChannel(eupHeader->pcm_midi_ch[trk]);

        //player->tempo(eupbuf[2048 + 5] + 30);
        player->tempo(eupHeader->first_tempo + 30);
        // 初期テンポの設定のつもり.  これで正しい?

        {
            uint8_t instrument[] = {
                ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // name
                17, 33, 10, 17,                         // detune / multiple
                25, 10, 57, 0,                          // output level
                154, 152, 218, 216,                     // key scale / attack rate
                15, 12, 7, 12,                          // amon / decay rate
                0, 5, 3, 5,                             // sustain rate
                38, 40, 70, 40,                         // sustain level / release rate
                20,                                     // feedback / algorithm
                0xc0,                                   // pan, LFO
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            };
            for (int n = 0; n < 128; n++)
                device->setFmInstrumentParameter(n, instrument);
        }
        /* FMB */
        {
            char fn0[16];
            memcpy(fn0, eupHeader->fm_file_name/*eupbuf + 0x6e2*/, 8); /* verified 0x06e2 offset */
            fn0[8] = '\0';
            for (int32_t i = 7; i >= 0; i--)
            {
                if (fn0[i] == ' ')
                    fn0[i] = 0;
                else
                    break;
            }
            if (fn0[0])
            {
                std::filesystem::path fn1(fn0);
                fn1.replace_extension("fmb");
                if (auto fmbStream = m_stream->Open(fn1.string()))
                {
                    auto fmbData = fmbStream->Read();
                    for (size_t n = 0; n < (fmbData.Size() - 8) / 48; n++)
                        device->setFmInstrumentParameter(int(n), fmbData.Items(8 + 48 * n));
                }
            }
        }
        /* PMB */
        {
            char fn0[16];
            memcpy(fn0, eupHeader->pcm_file_name/*eupbuf + 0x6ea*/, 8); /* verified 0x06ea offset */
            fn0[8] = '\0';
            for (int32_t i = 7; i >= 0; i--)
            {
                if (fn0[i] == ' ')
                    fn0[i] = 0;
                else
                    break;
            }
            if (fn0[0])
            {
                std::filesystem::path fn1(fn0);
                fn1.replace_extension("pmb");
                if (auto pmbStream = m_stream->Open(fn1.string()))
                {
                    auto pmbData = pmbStream->Read();
                    device->setPcmInstrumentParameters(pmbData.Items(), pmbData.Size());
                }
            }
        }

        player->startPlaying(m_data.Items(2048 + 6));
        memset(&device->pcm, 0, sizeof(device->pcm));

        return player;
    }

    ReplayEuphony::~ReplayEuphony()
    {
        if (m_player)
        {
            auto* device = reinterpret_cast<EUP_TownsEmulator*>(m_player->outputDevice());
            delete m_player;
            delete device;
        }
    }

    ReplayEuphony::ReplayEuphony(io::Stream* stream, Array<uint32_t>&& subsongs)
        : Replay(eExtension::_eup, eReplay::Euphony)
        , m_stream(stream)
        , m_subsongs(std::move(subsongs))
    {
        m_player = Load();
    }

    uint32_t ReplayEuphony::GetSampleRate() const
    {
        return streamAudioRate;
    }

    uint32_t ReplayEuphony::Render(StereoSample* output, uint32_t numSamples)
    {
        if (!m_player)
            return 0;

        auto& pcm = reinterpret_cast<EUP_TownsEmulator*>(m_player->outputDevice())->pcm;
        uint32_t numSamplesRead = 0;
        while (m_player->isPlaying())
        {
            if (pcm.read_pos < pcm.write_pos)
            {
                auto numSamplesToCopy = Min(numSamples - numSamplesRead, uint32_t(pcm.write_pos - pcm.read_pos));
                output = output->Convert(pcm.buffer + pcm.read_pos * 2, numSamplesToCopy);
                numSamplesRead += numSamplesToCopy;
                pcm.read_pos += numSamplesToCopy;
                if (numSamplesRead == numSamples)
                    break;
            }
            else
            {
                pcm.write_pos = 0;
                pcm.read_pos = streamAudioSamplesBuffer;
                m_player->nextTick();
                pcm.read_pos = 0;
            }
        }
        m_currentPosition += numSamplesRead;
        return numSamplesRead;
    }

    uint32_t ReplayEuphony::Seek(uint32_t timeInMs)
    {
        if (!m_player)
            return 0;

        auto numSamples = (uint64_t(timeInMs) * GetSampleRate()) / 1000;
        if (m_currentPosition <= numSamples)
        {
            auto hold = numSamples;
            numSamples = numSamples - m_currentPosition;
            m_currentPosition = hold;
        }
        else
        {
            ResetPlayback();
            m_currentPosition = numSamples;
        }
        auto& pcm = reinterpret_cast<EUP_TownsEmulator*>(m_player->outputDevice())->pcm;
        uint32_t numSamplesRead = 0;
        while (m_player->isPlaying())
        {
            if (pcm.read_pos < pcm.write_pos)
            {
                auto numSamplesToCopy = Min(uint32_t(numSamples - numSamplesRead), uint32_t(pcm.write_pos - pcm.read_pos));
                numSamplesRead += numSamplesToCopy;
                pcm.read_pos += numSamplesToCopy;
                if (numSamplesRead == numSamples)
                    break;
            }
            else
            {
                pcm.write_pos = 0;
                pcm.read_pos = streamAudioSamplesBuffer;
                m_player->nextTick();
                pcm.read_pos = 0;
            }
        }
        return timeInMs;
    }

    void ReplayEuphony::ResetPlayback()
    {
        if (auto* player = Load())
        {
            if (m_player)
            {
                delete reinterpret_cast<EUP_TownsEmulator*>(m_player->outputDevice());
                delete m_player;
            }
            m_player = player;
        }
        else if (m_player)
        {
            m_player->stopPlaying();
            m_player->startPlaying(m_subsongs.IsEmpty() ? m_stream->Read().Items(2048 + 6) : m_data.Items(2048 + 6));
            player->tempo(m_stream->Read().Items<const EUPHEAD>()->first_tempo + 30);
            auto* device = reinterpret_cast<EUP_TownsEmulator*>(m_player->outputDevice());
            memset(&device->pcm, 0, sizeof(device->pcm));
        }
        m_currentPosition = 0;
    }

    void ReplayEuphony::ApplySettings(const CommandBuffer metadata)
    {
        (void)metadata;
    }

    void ReplayEuphony::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayEuphony::GetDurationMs() const
    {
        return 0;
    }

    uint32_t ReplayEuphony::GetNumSubsongs() const
    {
        return m_subsongs.NumItems();
    }

    std::string ReplayEuphony::GetSubsongTitle() const
    {
        return m_title;
    }

    std::string ReplayEuphony::GetExtraInfo() const
    {
        std::string metadata;
        auto* eupHeader = m_data.Items<const EUPHEAD>();
        if (eupHeader->title[0])
        {
            metadata = "Title  : ";
            for (auto c : eupHeader->title)
            {
                if (!c)
                    break;
                metadata += c;
            }
        }
        if (eupHeader->artist[0])
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Artist : ";
            for (auto c : eupHeader->artist)
            {
                if (!c)
                    break;
                metadata += c;
            }
        }
        for (auto& trk_name : eupHeader->trk_name)
        {
            if (trk_name[0])
            {
                if (!metadata.empty())
                    metadata += "\n";
                for (auto c : trk_name)
                {
                    if (!c)
                        break;
                    metadata += c;
                }
            }
        }
        for (auto& short_trk_name : eupHeader->short_trk_name)
        {
            if (short_trk_name[0])
            {
                if (!metadata.empty())
                    metadata += "\n";
                for (auto c : short_trk_name)
                {
                    if (!c)
                        break;
                    metadata += c;
                }
            }
        }
        return metadata;
    }

    std::string ReplayEuphony::GetInfo() const
    {
        return "14 channels\nFM Towns / Euphony\n" EuphonyVersion;
    }
}
// namespace rePlayer
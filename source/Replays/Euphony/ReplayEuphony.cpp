#include "ReplayEuphony.h"

#include <Audio/Audiotypes.inl.h>
#include <Core/String.h>
#include <Imgui.h>
#include <IO/StreamMemory.h>
#include <ReplayDll.h>

#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>
#include <filesystem>

namespace rePlayer
{
    #define EuphonyVersion "eupmini @a45c3f2"

    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Euphony,
        .name = "eupmini",
        .extensions = "eup;eupPk",
        .about = "eupmini\nCopyright (c) 1995-1997, 2000 Tomoaki Hayasaka\nWin32 porting 2002, 2003 IIJIMA Hiromitsu aka Delmonta, and anonymous K\n2023 Giangiacomo Zaffini",
        .load = ReplayEuphony::Load
    };

    Replay* ReplayEuphony::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        if (stream->GetSize() < 8 || stream->GetSize() > 1024 * 1024 * 64)
            return nullptr;
        auto eupbuf = stream->Read();
        if (memcmp(eupbuf.Items(), "EUPH-PKG", 8) == 0)
        {
            auto* archive = archive_read_new();
            archive_read_support_format_all(archive);
            uint32_t fileIndex = 0;
            Array<uint32_t> subsongs;
            if (archive_read_open_memory(archive, eupbuf.Items(8), eupbuf.Size() - 8) == ARCHIVE_OK)
            {
                archive_entry* entry;
                while (archive_read_next_header(archive, &entry) == ARCHIVE_OK)
                {
                    std::filesystem::path entryPath(archive_entry_pathname(entry));
                    if (_stricmp(entryPath.extension().string().c_str(), ".eup") == 0)
                        subsongs.Add(fileIndex);
                    fileIndex++;
                }
            }
            archive_read_free(archive);

            if (subsongs.IsNotEmpty())
                return new ReplayEuphony(stream, std::move(subsongs));
            return nullptr;
        }
        else if (eupbuf.Size() < 2048 + 6 + 6)
            return nullptr;

        auto* eupHeader = eupbuf.Items<const EUPHEAD>();
        if (eupHeader->size < 0 || (eupbuf.Size() - eupHeader->size) != 3598)
            return nullptr;

        return new ReplayEuphony(stream, {});
    }

    EUPPlayer* ReplayEuphony::Load(int32_t subsongIndex)
    {
        auto eupbuf = m_stream->Read();
        if (subsongIndex >= 0)
        {
            auto data = m_stream->Read();
            auto* archive = archive_read_new();
            archive_read_support_format_all(archive);
            archive_read_open_memory(archive, data.Items(8), data.Size() - 8);
            archive_entry* entry;
            for (int32_t fileIndex = 0; fileIndex < subsongIndex; ++fileIndex)
            {
                archive_read_next_header(archive, &entry);
                archive_read_data_skip(archive);
            }
            archive_read_next_header(archive, &entry);
            std::filesystem::path entryPath(archive_entry_pathname(entry));
            m_title = reinterpret_cast<const char*>(entryPath.stem().u8string().c_str());

            auto fileSize = archive_entry_size(entry);
            m_data.Resize(fileSize);
            auto readSize = archive_read_data(archive, m_data.Items(), fileSize);
            assert(readSize == fileSize);
            eupbuf = { m_data.Items(), m_data.NumItems() };
        }

        // とりあえず, TOWNS emu のみに対応.

        if (eupbuf.Size() < 2048 + 6 + 6)
            return nullptr;

        // ヘッダ読み込み用バッファ
        auto* eupHeader = eupbuf.Items<const EUPHEAD>();

        if (eupHeader->size < 0 || (eupbuf.Size() - eupHeader->size) != 3598)
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
                if (subsongIndex < 0)
                {
                    std::filesystem::path fn1(fn0);
                    fn1.replace_extension(".fmb");
                    if (auto fmbStream = m_stream->Open(fn1.string()))
                    {
                        auto fmbData = fmbStream->Read();
                        for (size_t n = 0; n < (fmbData.Size() - 8) / 48; n++)
                            device->setFmInstrumentParameter(int(n), fmbData.Items(8 + 48 * n));
                    }
                }
                else
                {
                    strcat_s(fn0, ".fmb");
                    auto data = m_stream->Read();
                    auto* archive = archive_read_new();
                    archive_read_support_format_all(archive);
                    archive_read_open_memory(archive, data.Items(8), data.Size() - 8);
                    archive_entry* entry;
                    while (archive_read_next_header(archive, &entry) == ARCHIVE_OK)
                    {
                        auto entryName = archive_entry_pathname(entry);
                        if (_stricmp(entryName, fn0) == 0)
                        {
                            auto fileSize = archive_entry_size(entry);
                            Array<uint8_t> fmbData(fileSize);
                            auto readSize = archive_read_data(archive, fmbData.Items(), fileSize);
                            assert(readSize == fileSize);

                            for (la_ssize_t n = 0; n < (readSize - 8) / 48; n++)
                                device->setFmInstrumentParameter(int(n), fmbData.Items(8 + 48 * n));
                            break;
                        }
                    }
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
                if (subsongIndex < 0)
                {
                    std::filesystem::path fn1(fn0);
                    fn1.replace_extension(".pmb");
                    if (auto pmbStream = m_stream->Open(fn1.string()))
                    {
                        auto pmbData = pmbStream->Read();
                        device->setPcmInstrumentParameters(pmbData.Items(), pmbData.Size());
                    }
                }
                else
                {
                    strcat_s(fn0, ".pmb");
                    auto data = m_stream->Read();
                    auto* archive = archive_read_new();
                    archive_read_support_format_all(archive);
                    archive_read_open_memory(archive, data.Items(8), data.Size() - 8);
                    archive_entry* entry;
                    while (archive_read_next_header(archive, &entry) == ARCHIVE_OK)
                    {
                        auto entryName = archive_entry_pathname(entry);
                        if (_stricmp(entryName, fn0) == 0)
                        {
                            auto fileSize = archive_entry_size(entry);
                            Array<uint8_t> pmbData(fileSize);
                            auto readSize = archive_read_data(archive, pmbData.Items(), fileSize);
                            assert(readSize == fileSize);

                            device->setPcmInstrumentParameters(pmbData.Items(), readSize);
                            break;
                        }
                    }
                }
            }
        }

        player->startPlaying(eupbuf.Items(2048 + 6));
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
        : Replay(subsongs.IsEmpty() ? eExtension::_eup : eExtension::_eupPk, eReplay::Euphony)
        , m_stream(stream)
        , m_subsongs(std::move(subsongs))
    {
        m_player = Load(m_subsongs.IsEmpty() ? -1 : m_subsongs[0]);
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
        if (auto* player = Load(m_subsongs.IsNotEmpty() ? m_subsongs[m_subsongIndex] : -1))
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
        return m_subsongs.IsEmpty() ? 1 : m_subsongs.NumItems();
    }

    std::string ReplayEuphony::GetSubsongTitle() const
    {
        return m_title;
    }

    std::string ReplayEuphony::GetExtraInfo() const
    {
        std::string metadata;
        auto* eupHeader = m_subsongs.IsEmpty() ? m_stream->Read().Items<const EUPHEAD>() : m_data.Items<const EUPHEAD>();
        if (!eupHeader)
            return metadata;
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
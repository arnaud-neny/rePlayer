#include "ReplayHighlyAdvanced.h"
#include <mgba/core/blip_buf.h>
#include <mgba-util/vfs.h>

#include <Audio/AudioTypes.inl.h>
#include <Containers/Array.inl.h>
#include <Core/String.h>
#include <Imgui.h>
#include <IO/StreamFile.h>
#include <IO/StreamMemory.h>
#include <ReplayDll.h>

#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::HighlyAdvanced,
        .name = "Highly Advanced",
        .extensions = "minigsf;gsfPk",
        .about = "Highly Advanced 3.0.23\nChristopher Snowhill",
        .init = ReplayHighlyAdvanced::Init,
        .load = ReplayHighlyAdvanced::Load
    };

    bool ReplayHighlyAdvanced::Init(SharedContexts* ctx, Window& /*window*/)
    {
        ctx->Init();

        return false;
    }

    Replay* ReplayHighlyAdvanced::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto replay = new ReplayHighlyAdvanced(stream);
        return replay->Load();
    }

    void* ReplayHighlyAdvanced::OpenPSF(void* context, const char* uri)
    {
        auto This = reinterpret_cast<ReplayHighlyAdvanced*>(context);
        if (This->m_stream->GetName() == uri)
            return This->m_stream->Clone().Detach();
        if (This->m_streamArchive.IsValid())
        {
            auto data = This->m_streamArchive->Read();

            auto* archive = archive_read_new();
            archive_read_support_format_all(archive);
            archive_read_open_memory(archive, data.Items(), data.Size());
            archive_entry* entry;
            io::StreamMemory* stream = nullptr;
            while (archive_read_next_header(archive, &entry) == ARCHIVE_OK)
            {
                auto entryName = archive_entry_pathname(entry);
                if (_stricmp(entryName, uri) == 0)
                {
                    auto fileSize = archive_entry_size(entry);
                    Array<uint8_t> unpackedData;
                    unpackedData.Resize(fileSize);
                    auto readSize = archive_read_data(archive, unpackedData.Items(), fileSize);
                    assert(readSize == fileSize);

                    stream = io::StreamMemory::Create(entryName, unpackedData.Items(), fileSize, false).Detach();
                    unpackedData.Detach();
                    break;
                }
            }
            archive_read_free(archive);
            return stream;
        }
        return io::StreamFile::Create(uri).Detach();
    }

    size_t ReplayHighlyAdvanced::ReadPSF(void* buffer, size_t size, size_t count, void* handle)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return stream->Read(buffer, size * count) / size;
    }

    int ReplayHighlyAdvanced::SeekPSF(void* handle, int64_t offset, int whence)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return stream->Seek(offset, io::Stream::SeekWhence(whence)) == Status::kOk ? 0 : -1;
    }

    int ReplayHighlyAdvanced::ClosePSF(void* handle)
    {
        if (handle)
            reinterpret_cast<io::Stream*>(handle)->Release();
        return 0;
    }

    long ReplayHighlyAdvanced::TellPSF(void* handle)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return long(stream->GetPosition());
    }

    int ReplayHighlyAdvanced::InfoMetaPSF(void* context, const char* name, const char* value)
    {
        auto This = reinterpret_cast<ReplayHighlyAdvanced*>(context);
        auto tag = name;
        if (!_stricmp(tag, "replaygain_"))
        {}
        else if (!_stricmp(tag, "length"))
        {
            auto getDigit = [](const char*& value)
            {
                bool isDigit = false;
                int32_t digit = 0;
                while (*value && (*value < '0' || *value > '9'))
                    ++value;
                while (*value && *value >= '0' && *value <= '9')
                {
                    isDigit = true;
                    digit = digit * 10 + *value - '0';
                    ++value;
                }
                if (isDigit)
                    return digit;
                else
                    return -1;
            };
            auto length = getDigit(value);
            if (length >= 0)
            {
                while (*value && *value != ':' && *value != '.')
                    ++value;
                if (*value)
                {
                    if (*value == ':')
                    {
                        auto d = getDigit(++value);
                        if (d >= 0)
                            length = length * 60 + Clamp(d, 0, 59);
                    }
                    length *= 1000;
                    while (*value && *value != '.')
                        ++value;
                    if (*value == '.')
                    {
                        auto d = getDigit(++value);
                        if (d >= 0)
                            length += Clamp(d, 0, 999);
                    }
                }
                else
                    length *= 1000;

                if (length > 0)
                    This->m_length = length;
            }
        }
        else if (!_stricmp(tag, "fade"))
        {}
        else if (!_stricmp(tag, "utf8"))
        {}
        else if (!_stricmp(tag, "_lib"))
        {}
        else if (tag[0] == '_')
        {
            return -1;
        }
        else
        {
            This->m_tags.Add(tag);
            This->m_tags.Add(value);
        }
        return 0;
    }

    int ReplayHighlyAdvanced::GsfLoad(void* context, const uint8_t* exe, size_t exe_size, const uint8_t* /*reserved*/, size_t /*reserved_size*/)
    {
        if (exe_size < 12) return -1;

        auto* state = reinterpret_cast<gsf_loader_state*>(context);

        auto get_le32 = [](void const* p)
        {
            return  (unsigned)((unsigned char const*)p)[3] << 24 |
                (unsigned)((unsigned char const*)p)[2] << 16 |
                (unsigned)((unsigned char const*)p)[1] << 8 |
                (unsigned)((unsigned char const*)p)[0];
        };

        unsigned char* iptr;
        unsigned isize;
        unsigned char* xptr;
        unsigned xentry = get_le32(exe + 0);
        unsigned xsize = get_le32(exe + 8);
        unsigned xofs = get_le32(exe + 4) & 0x1ffffff;
        if (xsize < exe_size - 12) return -1;
        if (!state->entry_set)
        {
            state->entry = xentry;
            state->entry_set = 1;
        }
        {
            iptr = state->data;
            isize = uint32_t(state->data_size);
            state->data = 0;
            state->data_size = 0;
        }
        if (!iptr)
        {
            unsigned rsize = xofs + xsize;
            {
                rsize -= 1;
                rsize |= rsize >> 1;
                rsize |= rsize >> 2;
                rsize |= rsize >> 4;
                rsize |= rsize >> 8;
                rsize |= rsize >> 16;
                rsize += 1;
            }
            iptr = (unsigned char*)malloc(rsize + 10);
            if (!iptr)
                return -1;
            memset(iptr, 0, rsize + 10);
            isize = rsize;
        }
        else if (isize < xofs + xsize)
        {
            unsigned rsize = xofs + xsize;
            {
                rsize -= 1;
                rsize |= rsize >> 1;
                rsize |= rsize >> 2;
                rsize |= rsize >> 4;
                rsize |= rsize >> 8;
                rsize |= rsize >> 16;
                rsize += 1;
            }
            xptr = (unsigned char*)realloc(iptr, xofs + rsize + 10);
            if (!xptr)
            {
                free(iptr);
                return -1;
            }
            iptr = xptr;
            isize = rsize;
        }
        memcpy(iptr + xofs, exe + 12, xsize);
        {
            state->data = iptr;
            state->data_size = isize;
        }

        return 0;
    }

    ReplayHighlyAdvanced::~ReplayHighlyAdvanced()
    {
        if (m_gbaCore)
        {
            mCoreConfigDeinit(&m_gbaCore->config);
            m_gbaCore->deinit(m_gbaCore);
        }
    }

    ReplayHighlyAdvanced::ReplayHighlyAdvanced(io::Stream* stream)
        : Replay(eExtension::Unknown, eReplay::HighlyAdvanced)
        , m_stream(stream)
    {
        m_gbaStream = {};
        m_gbaStream.postAudioBuffer = [](mAVStream* stream, blip_t* left, blip_t* right)
        {
            auto state = reinterpret_cast<GbaStream*>(stream);
            blip_read_samples(left, state->samples, 2048, true);
            blip_read_samples(right, state->samples + 1, 2048, true);
            state->numSamples = 2048;
        };
    }

    ReplayHighlyAdvanced* ReplayHighlyAdvanced::Load()
    {
        auto data = m_stream->Read();

        auto* archive = archive_read_new();
        archive_read_support_format_all(archive);
        if (archive_read_open_memory(archive, data.Items(), data.Size()) != ARCHIVE_OK)
        {
            if (psf_load(m_stream->GetName().c_str(), &m_psfFileSystem, 0x22, nullptr, nullptr, InfoMetaPSF, this, 0, nullptr, nullptr) >= 0)
            {
                auto extPos = m_stream->GetName().find_last_of('.');
                if (extPos == std::string::npos || _stricmp(m_stream->GetName().c_str() + extPos + 1, "gsflib") != 0)
                {
                    if (psf_load(m_stream->GetName().c_str(), &m_psfFileSystem, 0x22, GsfLoad, &m_gbaRom, nullptr, nullptr, 0, nullptr, nullptr) >= 0)
                        m_mediaType.ext = eExtension::_minigsf;
                }
            }
        }
        else
        {
            std::swap(m_stream, m_streamArchive);
            uint32_t fileIndex = 0;
            Array<uint32_t> subsongs;
            Array<uint8_t> unpackedData;
            archive_entry* entry;
            while (archive_read_next_header(archive, &entry) == ARCHIVE_OK)
            {
                std::string entryName = archive_entry_pathname(entry);
                auto fileSize = archive_entry_size(entry);
                unpackedData.Resize(fileSize);
                auto readSize = archive_read_data(archive, unpackedData.Items(), fileSize);
                assert(readSize == fileSize);

                m_stream = io::StreamMemory::Create(entryName.c_str(), unpackedData.Items(), fileSize, true);

                if (psf_load(entryName.c_str(), &m_psfFileSystem, 0x22, nullptr, nullptr, InfoMetaPSF, this, 0, nullptr, nullptr) >= 0)
                {
                    auto extPos = entryName.find_last_of('.');
                    if (extPos == std::string::npos || _stricmp(entryName.c_str() + extPos + 1, "gsflib") != 0)
                    {
                        m_mediaType.ext = eExtension::_gsfPk;
                        m_subsongs.Add(fileIndex);
                    }
                }
                m_tags.Clear();

                fileIndex++;
            }
        }
        archive_read_free(archive);

        if (m_mediaType.ext == eExtension::Unknown)
        {
            delete this;
            return nullptr;
        }

        return this;
    }

    uint32_t ReplayHighlyAdvanced::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_gbaCore == nullptr)
            return 0;

        uint64_t length = m_length;
        uint64_t currentPosition = m_currentPosition;
        length *= kSampleRate;
        length /= 1000;
        if (currentPosition == length)
        {
            m_currentPosition = 0;
            return 0;
        }

        uint64_t nextPosition = currentPosition + numSamples;
        if (nextPosition > length)
            numSamples = uint32_t(length - currentPosition);

        auto remainingSamples = numSamples;
        while (remainingSamples)
        {
            if (m_gbaStream.numSamples != 0)
            {
                auto numSamplesToConvert = Min(remainingSamples, m_gbaStream.numSamples);
                output = output->Convert(m_gbaStream.samples + (2048 - m_gbaStream.numSamples) * 2, numSamplesToConvert);

                remainingSamples -= numSamplesToConvert;
                m_gbaStream.numSamples -= numSamplesToConvert;
            }
            else while (m_gbaStream.numSamples == 0)
                m_gbaCore->runFrame(m_gbaCore);
        }

        m_currentPosition = currentPosition + numSamples;
        m_globalPosition += numSamples;

        return numSamples;
    }

    uint32_t ReplayHighlyAdvanced::Seek(uint32_t timeInMs)
    {
        uint64_t seekPosition = timeInMs;
        seekPosition *= kSampleRate;
        seekPosition /= 1000;
        uint64_t globalPosition = m_globalPosition;

        if (seekPosition < globalPosition)
        {
            ResetPlayback();
            globalPosition = 0;
        }
        auto numSamples = seekPosition - globalPosition;
        while (numSamples)
        {
            if (m_gbaStream.numSamples != 0)
            {
                auto numSamplesToConvert = Min(numSamples, m_gbaStream.numSamples);
                numSamples -= numSamplesToConvert;
                m_gbaStream.numSamples -= uint32_t(numSamplesToConvert);
                globalPosition += numSamplesToConvert;
            }
            else while (m_gbaStream.numSamples == 0)
                m_gbaCore->runFrame(m_gbaCore);
        }

        m_globalPosition = globalPosition;
        seekPosition = globalPosition;
        uint64_t length = m_length;
        length *= kSampleRate;
        length /= 1000;
        m_currentPosition = globalPosition % length;
        return uint32_t((seekPosition * 1000) / kSampleRate);
    }

    void ReplayHighlyAdvanced::ResetPlayback()
    {
        auto subsongIndex = m_subsongIndex;
        if (m_streamArchive.IsValid() && m_currentSubsongIndex != subsongIndex)
        {
            m_gbaRom = {};
            m_tags.Clear();
            m_title.clear();
            m_length = kDefaultLength;

            auto data = m_streamArchive->Read();

            auto* archive = archive_read_new();
            archive_read_support_format_all(archive);
            archive_read_open_memory(archive, data.Items(), data.Size());
            archive_entry* entry;
            for (uint32_t fileIndex = 0; fileIndex < m_subsongs[subsongIndex]; ++fileIndex)
            {
                archive_read_next_header(archive, &entry);
                archive_read_data_skip(archive);
            }
            archive_read_next_header(archive, &entry);
            m_title = archive_entry_pathname(entry);

            auto fileSize = archive_entry_size(entry);
            Array<uint8_t> unpackedData;
            unpackedData.Resize(fileSize);
            auto readSize = archive_read_data(archive, unpackedData.Items(), fileSize);
            assert(readSize == fileSize);

            m_stream = io::StreamMemory::Create(m_title.c_str(), unpackedData.Items(), fileSize, true);

            psf_load(m_stream->GetName().c_str(), &m_psfFileSystem, 0x22, GsfLoad, &m_gbaRom, InfoMetaPSF, this, 0, nullptr, nullptr);

            archive_read_free(archive);
        }

        m_currentPosition = 0;
        m_globalPosition = 0;
        m_currentSubsongIndex = subsongIndex;
        m_gbaStream.numSamples = 0;

        if (m_gbaCore)
        {
            mCoreConfigDeinit(&m_gbaCore->config);
            m_gbaCore->deinit(m_gbaCore);
            m_gbaCore = nullptr;
        }

        auto* rom = VFileFromConstMemory(m_gbaRom.data, m_gbaRom.data_size);
        auto* core = mCoreFindVF(rom);
        if (!core)
        {
            rom->close(rom);
            return;
        }
        core->init(core);
        core->setAVStream(core, &m_gbaStream);
        mCoreInitConfig(core, nullptr);

        core->setAudioBufferSize(core, 2048);

        blip_set_rates(core->getAudioChannel(core, 0), core->frequency(core), kSampleRate);
        blip_set_rates(core->getAudioChannel(core, 1), core->frequency(core), kSampleRate);

        struct mCoreOptions opts = {};
        opts.useBios = false;
        opts.skipBios = true;
        opts.volume = 0x100;
        opts.sampleRate = kSampleRate;
        mCoreConfigLoadDefaults(&core->config, &opts);

        core->loadROM(core, rom);
        core->reset(core);
        m_gbaCore = core;
    }

    void ReplayHighlyAdvanced::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayHighlyAdvanced::SetSubsong(uint16_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayHighlyAdvanced::GetDurationMs() const
    {
        return uint32_t(m_length);
    }

    uint32_t ReplayHighlyAdvanced::GetNumSubsongs() const
    {
        return Max(1ul, m_subsongs.NumItems());
    }

    std::string ReplayHighlyAdvanced::GetSubsongTitle() const
    {
        for (size_t i = 0, e = m_tags.NumItems(); i < e; i += 2)
        {
            if (_stricmp(m_tags[i].c_str(), "title") == 0)
                return m_tags[i + 1];
        }
        return m_title;
    }

    std::string ReplayHighlyAdvanced::GetExtraInfo() const
    {
        std::string metadata;
        for (size_t i = 0, e = m_tags.NumItems(); i < e; i += 2)
        {
            if (i != 0)
                metadata += "\n";
            metadata += m_tags[i];
            metadata += ": ";
            metadata += m_tags[i + 1];
        }
        return metadata;
    }

    std::string ReplayHighlyAdvanced::GetInfo() const
    {
        return "2 channels\nGame Boy Advance Sound Format\nHighly Advanced 3.0.23";
    }
}
// namespace rePlayer
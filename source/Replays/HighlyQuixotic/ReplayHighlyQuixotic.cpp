#include "ReplayHighlyQuixotic.h"
#include "highly_quixotic/qsound.h"

#include <Audio/AudioTypes.inl.h>
#include <Containers/Array.inl.h>
#include <Core/String.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::HighlyQuixotic,
        .name = "Highly Quixotic",
        .extensions = "miniqsf",
        .about = "Highly Quixotic 3.1.1\nChristopher Snowhill",
        .init = ReplayHighlyQuixotic::Init,
        .load = ReplayHighlyQuixotic::Load
    };

    bool ReplayHighlyQuixotic::Init(SharedContexts* ctx, Window& /*window*/)
    {
        ctx->Init();

        qsound_init();

        return false;
    }

    Replay* ReplayHighlyQuixotic::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto replay = new ReplayHighlyQuixotic(stream);
        return replay->Load();
    }

    void* ReplayHighlyQuixotic::OpenPSF(void* context, const char* uri)
    {
        auto This = reinterpret_cast<ReplayHighlyQuixotic*>(context);
        if (This->m_stream->GetName() == uri)
            return This->m_stream->Clone().Detach();
        return This->m_stream->Open(uri).Detach();
    }

    size_t ReplayHighlyQuixotic::ReadPSF(void* buffer, size_t size, size_t count, void* handle)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return stream->Read(buffer, size * count) / size;
    }

    int ReplayHighlyQuixotic::SeekPSF(void* handle, int64_t offset, int whence)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return stream->Seek(offset, io::Stream::SeekWhence(whence)) == Status::kOk ? 0 : -1;
    }

    int ReplayHighlyQuixotic::ClosePSF(void* handle)
    {
        if (handle)
            reinterpret_cast<io::Stream*>(handle)->Release();
        return 0;
    }

    long ReplayHighlyQuixotic::TellPSF(void* handle)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return long(stream->GetPosition());
    }

    int ReplayHighlyQuixotic::InfoMetaPSF(void* context, const char* name, const char* value)
    {
        auto This = reinterpret_cast<ReplayHighlyQuixotic*>(context);
        auto tag = name;
        if (!_stricmp(tag, "replaygain_"))
        {
        }
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

    int ReplayHighlyQuixotic::QsoundLoad(void* context, const uint8_t* exe, size_t exe_size, const uint8_t* /*reserved*/, size_t /*reserved_size*/)
    {
        auto This = reinterpret_cast<ReplayHighlyQuixotic*>(context);
        for (;;)
        {
            char s[4];
            if (exe_size < 11) break;
            memcpy(s, exe, 3); exe += 3; exe_size -= 3;
            s[3] = 0;
            uint32_t dataofs = *(uint32_t*)exe; exe += 4; exe_size -= 4;
            uint32_t datasize = *(uint32_t*)exe; exe += 4; exe_size -= 4;
            if (datasize > exe_size)
                return -1;

            {
                char* section = s;
                uint32_t start = dataofs;
                const uint8_t* data = exe;
                uint32_t size = datasize;

                Array<uint8_t>* pArray = NULL;
                Array<valid_range>* pArrayValid = NULL;
                uint32_t maxsize = 0x7FFFFFFF;

                if (!strcmp(section, "KEY")) { pArray = &This->m_aKey; pArrayValid = &This->m_aKeyValid; maxsize = 11; }
                else if (!strcmp(section, "Z80")) { pArray = &This->m_aZ80ROM; pArrayValid = &This->m_aZ80ROMValid; }
                else if (!strcmp(section, "SMP")) { pArray = &This->m_aSampleROM; pArrayValid = &This->m_aSampleROMValid; }
                else
                {
                    return -1;
                }

                if ((start + size) < start)
                {
                    return -1;
                }

                uint32_t newsize = start + size;
                uint32_t oldsize = uint32_t(pArray->NumItems());
                if (newsize > maxsize)
                {
                    return -1;
                }

                if (newsize > oldsize)
                {
                    pArray->Resize(newsize);
                    memset(pArray->Items() + oldsize, 0, newsize - oldsize);
                }

                memcpy(pArray->Items() + start, data, size);

                oldsize = uint32_t(pArrayValid->NumItems());
                pArrayValid->Resize(oldsize + 1);
                valid_range& range = (pArrayValid->Items())[oldsize];
                range.start = start;
                range.size = size;
            }

            exe += datasize;
            exe_size -= datasize;
        }

        return 0;
    }

    ReplayHighlyQuixotic::~ReplayHighlyQuixotic()
    {
        delete[] m_qsoundState;
    }

    ReplayHighlyQuixotic::ReplayHighlyQuixotic(io::Stream* stream)
        : Replay(eExtension::_miniqsf, eReplay::HighlyQuixotic)
        , m_stream(stream)
    {}

    ReplayHighlyQuixotic* ReplayHighlyQuixotic::Load()
    {
        auto stream = m_stream;
        uint32_t fileIndex = 0;
        do
        {
            if (psf_load(stream->GetName().c_str(), &m_psfFileSystem, 0x41, nullptr, nullptr, InfoMetaPSF, this, 0, nullptr, nullptr) >= 0)
            {
                auto extPos = stream->GetName().find_last_of('.');
                if (extPos == std::string::npos || _stricmp(stream->GetName().c_str() + extPos + 1, "qsflib") != 0)
                {
                    if (psf_load(stream->GetName().c_str(), &m_psfFileSystem, 0x41, QsoundLoad, this, nullptr, nullptr, 0, nullptr, nullptr) >= 0)
                        m_subsongs.Add(fileIndex);
                }
            }
            m_tags.Clear();

            fileIndex++;
        } while (stream = stream->Next());

        if (m_subsongs.IsEmpty())
        {
            delete this;
            return nullptr;
        }

        m_qsoundState = new uint8_t[qsound_get_state_size()];
        return this;
    }

    uint32_t ReplayHighlyQuixotic::Render(StereoSample* output, uint32_t numSamples)
    {
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

        auto buf = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;
        if (qsound_execute(m_qsoundState, 0x7fFFffFF, buf, &numSamples) <= 0)
            return 0;

        output->Convert(buf, numSamples);

        m_currentPosition = currentPosition + numSamples;
        m_globalPosition += numSamples;

        return numSamples;
    }

    uint32_t ReplayHighlyQuixotic::Seek(uint32_t timeInMs)
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
        while (numSamples > 0)
        {
            uint32_t blockSamples = uint32_t(Min(2048, numSamples));
            if (qsound_execute(m_qsoundState, 0x7fFFffFF, nullptr, &blockSamples) < 0 || blockSamples == 0)
                break;
            globalPosition += blockSamples;
            numSamples -= blockSamples;
        }
        m_globalPosition = globalPosition;
        seekPosition = globalPosition;
        uint64_t length = m_length;
        length *= kSampleRate;
        length /= 1000;
        m_currentPosition = globalPosition % length;
        return uint32_t((seekPosition * 1000) / kSampleRate);
    }

    void ReplayHighlyQuixotic::ResetPlayback()
    {
        auto subsongIndex = m_subsongIndex;
        if (m_currentSubsongIndex != subsongIndex)
        {
            m_aKey.Clear();
            m_aKeyValid.Clear();
            m_aZ80ROM.Clear();
            m_aZ80ROMValid.Clear();
            m_aSampleROM.Clear();
            m_aSampleROMValid.Clear();
            m_tags.Clear();
            m_title.clear();
            m_length = kDefaultLength;

            auto stream = m_stream;
            for (uint32_t fileIndex = 0; stream; fileIndex++)
            {
                if (fileIndex == m_subsongs[m_subsongIndex])
                {
                    m_title = stream->GetName();
                    psf_load(stream->GetName().c_str(), &m_psfFileSystem, 0x41, QsoundLoad, this, InfoMetaPSF, this, 0, nullptr, nullptr);
                    break;
                }
                stream = stream->Next();
            }
        }

        m_currentPosition = 0;
        m_globalPosition = 0;
        m_currentSubsongIndex = subsongIndex;

        qsound_clear_state(m_qsoundState);

        if (m_aKey.NumItems() == 11)
        {
            uint8_t* ptr = m_aKey.Items();
            uint32_t swap_key1 = *(uint32_t*)(ptr + 0);
            uint32_t swap_key2 = *(uint32_t*)(ptr + 4);
            uint32_t addr_key = *(uint32_t*)(ptr + 8);
            uint8_t  xor_key = *(ptr + 10);
            qsound_set_kabuki_key(m_qsoundState, swap_key1, swap_key2, uint16_t(addr_key), xor_key);
        }
        else
        {
            qsound_set_kabuki_key(m_qsoundState, 0, 0, 0, 0);
        }
        qsound_set_z80_rom(m_qsoundState, m_aZ80ROM.Items(), uint32_t(m_aZ80ROM.NumItems()));
        qsound_set_sample_rom(m_qsoundState, m_aSampleROM.Items(), uint32_t(m_aSampleROM.NumItems()));
    }

    void ReplayHighlyQuixotic::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayHighlyQuixotic::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayHighlyQuixotic::GetDurationMs() const
    {
        return m_length;
    }

    uint32_t ReplayHighlyQuixotic::GetNumSubsongs() const
    {
        return Max(1ul, m_subsongs.NumItems());
    }

    std::string ReplayHighlyQuixotic::GetSubsongTitle() const
    {
        for (size_t i = 0, e = m_tags.NumItems(); i < e; i += 2)
        {
            if (_stricmp(m_tags[i].c_str(), "title") == 0)
                return m_tags[i + 1];
        }
        auto offset = m_title.find_last_of('.');
        if (offset != m_title.npos)
            return std::string(m_title.c_str(), offset);
        return m_title;
    }

    std::string ReplayHighlyQuixotic::GetExtraInfo() const
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

    std::string ReplayHighlyQuixotic::GetInfo() const
    {
        return "16 channels\nCapcom Q-Sound Format\nHighly Quixotic 3.1.1";
    }
}
// namespace rePlayer
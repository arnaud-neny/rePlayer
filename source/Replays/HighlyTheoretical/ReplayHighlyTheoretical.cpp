#include "ReplayHighlyTheoretical.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::HighlyTheoretical,
        .name = "Highly Theoretical",
        .extensions = "ssf;minissf;dsf;minidsf",
        .about = "Highly Theoretical\nChristopher Snowhill",
        .init = ReplayHighlyTheoretical::Init,
        .load = ReplayHighlyTheoretical::Load,
        .editMetadata = ReplayHighlyTheoretical::Settings::Edit
    };

    bool ReplayHighlyTheoretical::Init(SharedContexts* ctx, Window& /*window*/)
    {
        ctx->Init();

        sega_init();

        return false;
    }

    Replay* ReplayHighlyTheoretical::Load(io::Stream* stream, CommandBuffer metadata)
    {
        auto replay = new ReplayHighlyTheoretical(stream);
        return replay->Load(metadata);
    }

    void ReplayHighlyTheoretical::Settings::Edit(ReplayMetadataContext& context)
    {
        auto* entry = context.metadata.Find<Settings>();
        if (entry == nullptr)
        {
            // ok, we are here because we never played this song in this player
            entry = context.metadata.Create<Settings>(sizeof(Settings) + sizeof(LoopInfo));
            entry->loops[0] = {};
        }

        Loops(context, entry->loops, entry->numSongsMinusOne + 1, kDefaultSongDuration);
    }

    void* ReplayHighlyTheoretical::OpenPSF(void* context, const char* uri)
    {
        auto This = reinterpret_cast<ReplayHighlyTheoretical*>(context);
        if (This->m_stream->GetName() == uri)
            return This->m_stream->Clone().Detach();
        return This->m_stream->Open(uri).Detach();
    }

    size_t ReplayHighlyTheoretical::ReadPSF(void* buffer, size_t size, size_t count, void* handle)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return size_t(stream->Read(buffer, size * count) / size);
    }

    int ReplayHighlyTheoretical::SeekPSF(void* handle, int64_t offset, int whence)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return stream->Seek(offset, io::Stream::SeekWhence(whence)) == Status::kOk ? 0 : -1;
    }

    int ReplayHighlyTheoretical::ClosePSF(void* handle)
    {
        if (handle)
            reinterpret_cast<io::Stream*>(handle)->Release();
        return 0;
    }

    long ReplayHighlyTheoretical::TellPSF(void* handle)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return long(stream->GetPosition());
    }

    int ReplayHighlyTheoretical::InfoMetaPSF(void* context, const char* name, const char* value)
    {
        auto This = reinterpret_cast<ReplayHighlyTheoretical*>(context);
        if (!_strnicmp(name, "replaygain_", sizeof("replaygain_") - 1))
        {}
        else if (!_stricmp(name, "length"))
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
        else if (!_stricmp(name, "fade"))
        {}
        else if (!_stricmp(name, "utf8"))
        {}
        else if (!_stricmp(name, "_lib"))
        {
            This->m_hasLib = true;
        }
        else if (name[0] == '_')
        {}
        else
        {
            This->m_tags.Add(name);
            This->m_tags.Add(value);
        }
        return 0;
    }

    static inline unsigned get_le32(void const* p)
    {
        return (unsigned)((unsigned char const*)p)[3] << 24 |
            (unsigned)((unsigned char const*)p)[2] << 16 |
            (unsigned)((unsigned char const*)p)[1] << 8 |
            (unsigned)((unsigned char const*)p)[0];
    }

    static inline void set_le32(void* p, unsigned n)
    {
        ((unsigned char*)p)[0] = (unsigned char)n;
        ((unsigned char*)p)[1] = (unsigned char)(n >> 8);
        ((unsigned char*)p)[2] = (unsigned char)(n >> 16);
        ((unsigned char*)p)[3] = (unsigned char)(n >> 24);
    }

    int ReplayHighlyTheoretical::SdsfLoad(void* context, const uint8_t* exe, size_t exe_size, const uint8_t* /*reserved*/, size_t /*reserved_size*/)
    {
        auto* This = reinterpret_cast<ReplayHighlyTheoretical*>(context);
        if (exe_size < 4) return -1;

        uint8_t* dst = This->m_loaderState.data;

        if (This->m_loaderState.data_size < 4) {
            This->m_loaderState.data = dst = (uint8_t*)malloc(exe_size);
            This->m_loaderState.data_size = exe_size;
            memcpy(dst, exe, exe_size);
            return 0;
        }

        uint32_t dst_start = get_le32(dst);
        uint32_t src_start = get_le32(exe);
        dst_start &= 0x7fffff;
        src_start &= 0x7fffff;
        size_t dst_len = This->m_loaderState.data_size - 4;
        size_t src_len = exe_size - 4;
        if (dst_len > 0x800000) dst_len = 0x800000;
        if (src_len > 0x800000) src_len = 0x800000;

        if (src_start < dst_start) {
            uint32_t diff = dst_start - src_start;
            This->m_loaderState.data_size = dst_len + 4 + diff;
            This->m_loaderState.data = dst = (uint8_t*)realloc(dst, This->m_loaderState.data_size);
            memmove(dst + 4 + diff, dst + 4, dst_len);
            memset(dst + 4, 0, diff);
            dst_len += diff;
            dst_start = src_start;
            set_le32(dst, dst_start);
        }
        if ((src_start + src_len) > (dst_start + dst_len)) {
            size_t diff = (src_start + src_len) - (dst_start + dst_len);
            This->m_loaderState.data_size = dst_len + 4 + diff;
            This->m_loaderState.data = dst = (uint8_t*)realloc(dst, This->m_loaderState.data_size);
            memset(dst + 4 + dst_len, 0, diff);
        }

        memcpy(dst + 4 + (src_start - dst_start), exe + 4, src_len);

        return 0;
    }

    ReplayHighlyTheoretical::~ReplayHighlyTheoretical()
    {
        delete[] m_segaState;
    }

    ReplayHighlyTheoretical::ReplayHighlyTheoretical(io::Stream* stream)
        : Replay(eExtension::Unknown, eReplay::HighlyTheoretical)
        , m_stream(stream)
    {}

    ReplayHighlyTheoretical* ReplayHighlyTheoretical::Load(CommandBuffer metadata)
    {
        auto stream = m_stream;
        uint32_t fileIndex = 0;
        do
        {
            m_length = kDefaultSongDuration;
            auto psfType = psf_load(stream->GetName().c_str(), &m_psfFileSystem, 0, nullptr, nullptr, InfoMetaPSF, this, 0, nullptr, nullptr);
            if (psfType == 0x11 || psfType == 0x12)
            {
                auto extPos = stream->GetName().find_last_of('.');
                if (extPos == std::string::npos || _stricmp(stream->GetName().c_str() + extPos + 1, psfType == 0x11 ? "ssflib" : "dsflib") != 0)
                {
                    if (psf_load(stream->GetName().c_str(), &m_psfFileSystem, uint8_t(psfType), SdsfLoad, this, nullptr, nullptr, 0, nullptr, nullptr) >= 0)
                    {
                        m_psfType = uint8_t(psfType);
                        m_mediaType.ext = psfType == 0x11 ? m_hasLib ? eExtension::_minissf : eExtension::_ssf : m_hasLib ? eExtension::_minidsf : eExtension::_dsf;
                        m_subsongs.Add({ fileIndex, uint32_t(m_length) });
                    }
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

        m_segaState = new uint8_t[sega_get_state_size(m_psfType - 0x10)];

        SetupMetadata(metadata);

        return this;
    }

    uint32_t ReplayHighlyTheoretical::Render(StereoSample* output, uint32_t numSamples)
    {
        auto currentPosition = m_currentPosition;
        auto currentDuration = m_currentDuration;
        if (currentDuration != 0 && (currentPosition + numSamples) >= currentDuration)
        {
            numSamples = currentPosition < currentDuration ? uint32_t(currentDuration - currentPosition) : 0;
            if (numSamples == 0)
            {
                m_currentPosition = 0;
                if (m_subsongs[m_subsongIndex].loop.IsValid())
                    m_currentDuration = (uint64_t(m_subsongs[m_subsongIndex].loop.length) * kSampleRate) / 1000;
                return 0;
            }
        }
        m_currentPosition = currentPosition + numSamples;

        auto buf = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;
        sega_execute(m_segaState, 0x7fffffff, buf, &numSamples);
        output->Convert(buf, numSamples);

        return numSamples;
    }

    void ReplayHighlyTheoretical::ResetPlayback()
    {
        auto subsongIndex = m_subsongIndex;
        if (m_currentSubsongIndex != subsongIndex)
        {
            m_loaderState.Clear();
            m_tags.Clear();
            m_title.clear();

            auto stream = m_stream;
            for (uint32_t fileIndex = 0; stream; fileIndex++)
            {
                if (fileIndex == m_subsongs[m_subsongIndex].index)
                {
                    m_title = stream->GetName();
                    psf_load(stream->GetName().c_str(), &m_psfFileSystem, m_psfType, SdsfLoad, this, InfoMetaPSF, this, 0, nullptr, nullptr);
                    break;
                }
                stream = stream->Next();
            }
        }

        m_currentPosition = 0;
        m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
        m_currentSubsongIndex = subsongIndex;

        sega_clear_state(m_segaState, m_psfType - 0x10);

        sega_enable_dry(m_segaState, 1);
        sega_enable_dsp(m_segaState, 1);
        sega_enable_dsp_dynarec(m_segaState, 0);

        uint32_t start = *reinterpret_cast<uint32_t*>(m_loaderState.data);
        size_t length = m_loaderState.data_size;
        const size_t maxLength = (m_psfType == 0x12) ? 0x800000 : 0x80000;
        if ((start + (length - 4)) > maxLength)
        {
            length = maxLength - start + 4;
        }
        sega_upload_program(m_segaState, m_loaderState.data, (uint32_t)length);
    }

    void ReplayHighlyTheoretical::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        if (settings)
        {
            for (uint16_t i = 0; i <= settings->numSongsMinusOne; i++)
                m_subsongs[i].loop = settings->loops[i].GetFixed();
            m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
        }
    }

    void ReplayHighlyTheoretical::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayHighlyTheoretical::GetDurationMs() const
    {
        return m_subsongs[m_subsongIndex].loop.IsValid() ? m_subsongs[m_subsongIndex].loop.GetDuration() : m_subsongs[m_subsongIndex].duration;
    }

    uint32_t ReplayHighlyTheoretical::GetNumSubsongs() const
    {
        return Max(1ul, m_subsongs.NumItems());
    }

    std::string ReplayHighlyTheoretical::GetSubsongTitle() const
    {
        for (uint32_t i = 0, e = m_tags.NumItems(); i < e; i += 2)
        {
            if (_stricmp(m_tags[i].c_str(), "title") == 0)
                return m_tags[i + 1];
        }
        auto offset = m_title.find_last_of('.');
        if (offset != m_title.npos)
            return std::string(m_title.c_str(), offset);
        return m_title;
    }

    std::string ReplayHighlyTheoretical::GetExtraInfo() const
    {
        std::string metadata;
        for (uint32_t i = 0, e = m_tags.NumItems(); i < e; i += 2)
        {
            if (i != 0)
                metadata += "\n";
            metadata += m_tags[i];
            metadata += ": ";
            metadata += m_tags[i + 1];
        }
        return metadata;
    }

    std::string ReplayHighlyTheoretical::GetInfo() const
    {
        return m_psfType == 0x11 ? "32 channels\nSaturn Sound Format\nHighly Theoretical" : "64 channels\nDreamcast Sound Format\nHighly Theoretical";
    }

    void ReplayHighlyTheoretical::SetupMetadata(CommandBuffer metadata)
    {
        uint32_t numSongsMinusOne = m_subsongs.NumItems() - 1;
        auto settings = metadata.Find<Settings>();
        if (settings && settings->numSongsMinusOne == numSongsMinusOne)
        {
            for (uint32_t i = 0; i <= numSongsMinusOne; i++)
                m_subsongs[i].loop = settings->loops[i].GetFixed();
        }
        else
        {
            auto value = settings ? settings->value : 0;
            settings = metadata.Create<Settings>(sizeof(Settings) + (numSongsMinusOne + 1) * sizeof(LoopInfo));
            settings->value = value;
            settings->numSongsMinusOne = numSongsMinusOne;
            for (uint16_t i = 0; i <= numSongsMinusOne; i++)
                settings->loops[i] = {};
        }
        m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
    }
}
// namespace rePlayer
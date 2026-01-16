#include "ReplayHighlyExperimental.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

#include "hebios.h"

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::HighlyExperimental,
        .name = "Highly Experimental",
        .extensions = "psf;minipsf;psfPk;psf2;minipsf2;psf2Pk",
        .about = "Highly Experimental\nChristopher Snowhill",
        .init = ReplayHighlyExperimental::Init,
        .load = ReplayHighlyExperimental::Load,
        .editMetadata = ReplayHighlyExperimental::Settings::Edit
    };

    bool ReplayHighlyExperimental::Init(SharedContexts* ctx, Window& /*window*/)
    {
        ctx->Init();

        static bool isInit = false;
        if (!isInit)
        {
            bios_set_image(hebios, HEBIOS_SIZE);
            psx_init();
            isInit = true;
        }

        return false;
    }

    Replay* ReplayHighlyExperimental::Load(io::Stream* stream, CommandBuffer metadata)
    {
        auto replay = new ReplayHighlyExperimental(stream);
        return replay->Load(metadata);
    }

    void ReplayHighlyExperimental::Settings::Edit(ReplayMetadataContext& context)
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

    void* ReplayHighlyExperimental::OpenPSF(void* context, const char* uri)
    {
        auto This = reinterpret_cast<ReplayHighlyExperimental*>(context);
        if (This->m_stream->GetName() == uri)
            return This->m_stream->Clone().Detach();
        return This->m_stream->Open(uri).Detach();
    }

    size_t ReplayHighlyExperimental::ReadPSF(void* buffer, size_t size, size_t count, void* handle)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return size_t(stream->Read(buffer, size * count) / size);
    }

    int ReplayHighlyExperimental::SeekPSF(void* handle, int64_t offset, int whence)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return stream->Seek(offset, io::Stream::SeekWhence(whence)) == Status::kOk ? 0 : -1;
    }

    int ReplayHighlyExperimental::ClosePSF(void* handle)
    {
        if (handle)
            reinterpret_cast<io::Stream*>(handle)->Release();
        return 0;
    }

    long ReplayHighlyExperimental::TellPSF(void* handle)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return long(stream->GetPosition());
    }

    int ReplayHighlyExperimental::InfoMetaPSF(void* context, const char* name, const char* value)
    {
        auto This = reinterpret_cast<ReplayHighlyExperimental*>(context);
        if (!_stricmp(name, "_refresh"))
        {
            sscanf_s(value, "%u", &This->m_loaderState.refresh);
        }
        else if (!_strnicmp(name, "replaygain_", sizeof("replaygain_") - 1))
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

    struct exec_header_t
    {
        uint32_t pc0;
        uint32_t gp0;
        uint32_t t_addr;
        uint32_t t_size;
        uint32_t d_addr;
        uint32_t d_size;
        uint32_t b_addr;
        uint32_t b_size;
        uint32_t s_ptr;
        uint32_t s_size;
        uint32_t sp, fp, gp, ret, base;
    };

    struct psxexe_hdr_t
    {
        char key[8];
        uint32_t text;
        uint32_t data;
        exec_header_t exec;
        char title[60];
    };

    int ReplayHighlyExperimental::PsfLoad(void* context, const uint8_t* exe, size_t exe_size, const uint8_t* /*reserved*/, size_t /*reserved_size*/)
    {
        auto* This = reinterpret_cast<ReplayHighlyExperimental*>(context);
        psxexe_hdr_t* psx = (psxexe_hdr_t*)exe;

        if (exe_size < 0x800)
            return -1;
        if (exe_size > UINT_MAX)
            return -1;

        uint32_t addr = get_le32(&psx->exec.t_addr);
        uint32_t size = (uint32_t)exe_size - 0x800;

        addr &= 0x1fffff;
        if ((addr < 0x10000) || (size > 0x1f0000) || (addr + size > 0x200000))
            return -1;

        void* pIOP = psx_get_iop_state(This->m_psxState);
        iop_upload_to_ram(pIOP, addr, exe + 0x800, size);

        if (!This->m_loaderState.refresh)
        {
            if (!_strnicmp((const char*)exe + 113, "Japan", 5))
                This->m_loaderState.refresh = 60;
            else if (!_strnicmp((const char*)exe + 113, "Europe", 6))
                This->m_loaderState.refresh = 50;
            else if (!_strnicmp((const char*)exe + 113, "North America", 13))
                This->m_loaderState.refresh = 60;
        }

        if (This->m_loaderState.first)
        {
            void* pR3000 = iop_get_r3000_state(pIOP);
            r3000_setreg(pR3000, R3000_REG_PC, get_le32(&psx->exec.pc0));
            r3000_setreg(pR3000, R3000_REG_GEN + 29, get_le32(&psx->exec.s_ptr));
            This->m_loaderState.first = false;
        }

        return 0;
    }

    ReplayHighlyExperimental::~ReplayHighlyExperimental()
    {
        if (m_psf2fs)
            psf2fs_delete(m_psf2fs);
        delete[] m_psxState;
    }

    ReplayHighlyExperimental::ReplayHighlyExperimental(io::Stream* stream)
        : Replay(eExtension::Unknown, eReplay::HighlyExperimental)
        , m_stream(stream)
    {}

    ReplayHighlyExperimental* ReplayHighlyExperimental::Load(CommandBuffer metadata)
    {
        auto stream = m_stream;
        uint32_t fileIndex = 0;
        do
        {
            m_length = kDefaultSongDuration;
            auto psfType = psf_load(stream->GetName().c_str(), &m_psfFileSystem, 0, nullptr, nullptr, InfoMetaPSF, this, 1, nullptr, nullptr);
            if (psfType == 0x1 || psfType == 0x2)
            {
                auto extPos = stream->GetName().find_last_of('.');
                if (extPos == std::string::npos || _stricmp(stream->GetName().c_str() + extPos + 1, psfType == 0x1 ? "psflib" : "psf2lib") != 0)
                {
                    if (m_psxState == nullptr)
                        m_psxState = new uint8_t[psx_get_state_size(uint8_t(psfType))];
                    psx_clear_state(m_psxState, uint8_t(psfType));
                    if (psfType == 2 && m_psf2fs == nullptr)
                        m_psf2fs = psf2fs_create();
                    if (psf_load(stream->GetName().c_str(), &m_psfFileSystem, uint8_t(psfType), psfType == 1 ? PsfLoad : psf2fs_load_callback, psfType == 1 ? this : m_psf2fs, nullptr, nullptr, 0, nullptr, nullptr) >= 0)
                    {
                        m_psfType = uint8_t(psfType);
                        m_mediaType.ext = psfType == 0x1 ? m_hasLib ? eExtension::_minipsf : eExtension::_psf : m_hasLib ? eExtension::_minipsf2 : eExtension::_psf2;
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

        SetupMetadata(metadata);

        return this;
    }

    uint32_t ReplayHighlyExperimental::Render(StereoSample* output, uint32_t numSamples)
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
                    m_currentDuration = (uint64_t(m_subsongs[m_subsongIndex].loop.length) * GetSampleRate()) / 1000;
                return 0;
            }
        }
        m_currentPosition = currentPosition + numSamples;

        auto buf = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;
        psx_execute(m_psxState, 0x7fffffff, buf, &numSamples, 0);
        output->Convert(buf, numSamples);

        return numSamples;
    }

    void ReplayHighlyExperimental::ResetPlayback()
    {
        psx_clear_state(m_psxState, uint8_t(m_psfType));
        if (m_psf2fs)
        {
            psf2fs_delete(m_psf2fs);
            m_psf2fs = psf2fs_create();
        }
        m_loaderState.Clear();

        auto subsongIndex = m_subsongIndex;
        if (m_currentSubsongIndex != subsongIndex)
        {
            m_tags.Clear();
            m_title.clear();

            auto stream = m_stream;
            for (uint32_t fileIndex = 0; stream; fileIndex++)
            {
                if (fileIndex == m_subsongs[m_subsongIndex].index)
                {
                    m_title = stream->GetName();
                    psf_load(stream->GetName().c_str(), &m_psfFileSystem, m_psfType, m_psfType == 1 ? PsfLoad : psf2fs_load_callback, m_psfType == 1 ? this : m_psf2fs, InfoMetaPSF, this, 1, nullptr, nullptr);
                    break;
                }
                stream = stream->Next();
            }
        }
        else
        {
            auto stream = m_stream;
            for (uint32_t fileIndex = 0; stream; fileIndex++)
            {
                if (fileIndex == m_subsongs[m_subsongIndex].index)
                {
                    m_title = stream->GetName();
                    psf_load(stream->GetName().c_str(), &m_psfFileSystem, m_psfType, m_psfType == 1 ? PsfLoad : psf2fs_load_callback, m_psfType == 1 ? this : m_psf2fs, nullptr, this, 0, nullptr, nullptr);
                    break;
                }
                stream = stream->Next();
            }
        }

        m_currentPosition = 0;
        m_currentDuration = (uint64_t(GetDurationMs()) * GetSampleRate()) / 1000;
        m_currentSubsongIndex = subsongIndex;

        if (m_loaderState.refresh)
            psx_set_refresh(m_psxState, m_loaderState.refresh);
        if (m_psfType == 2)
        {
            struct
            {
                static int EMU_CALL virtual_readfile(void* context, const char* path, int offset, char* buffer, int length) {
                    return psf2fs_virtual_readfile(context, path, offset, buffer, length);
                }
            } cb;
            psx_set_readfile(m_psxState, cb.virtual_readfile, m_psf2fs);
        }

        void* pIOP = psx_get_iop_state(m_psxState);
        iop_set_compat(pIOP, IOP_COMPAT_HARSH);
    }

    void ReplayHighlyExperimental::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        if (settings)
        {
            for (uint16_t i = 0; i <= settings->numSongsMinusOne; i++)
                m_subsongs[i].loop = settings->loops[i].GetFixed();
            m_currentDuration = (uint64_t(GetDurationMs()) * GetSampleRate()) / 1000;
        }
    }

    void ReplayHighlyExperimental::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayHighlyExperimental::GetDurationMs() const
    {
        return m_subsongs[m_subsongIndex].loop.IsValid() ? m_subsongs[m_subsongIndex].loop.GetDuration() : m_subsongs[m_subsongIndex].duration;
    }

    uint32_t ReplayHighlyExperimental::GetNumSubsongs() const
    {
        return Max(1ul, m_subsongs.NumItems());
    }

    std::string ReplayHighlyExperimental::GetSubsongTitle() const
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

    std::string ReplayHighlyExperimental::GetExtraInfo() const
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

    std::string ReplayHighlyExperimental::GetInfo() const
    {
        return m_psfType == 0x1 ? "24 channels\nPlaystation Sound Format\nHighly Experimental" : "48 channels\nPlaystation 2 Sound Format\nHighly Experimental";
    }

    void ReplayHighlyExperimental::SetupMetadata(CommandBuffer metadata)
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
        m_currentDuration = (uint64_t(GetDurationMs()) * GetSampleRate()) / 1000;
    }
}
// namespace rePlayer
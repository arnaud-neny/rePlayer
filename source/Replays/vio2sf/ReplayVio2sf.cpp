#include "ReplayVio2sf.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

#include <zlib.h>

#include "desmume/barray.h"

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::vio2sf,
        .name = "vio2sf",
        .extensions = "2sf;mini2sf",
        .about = "vio2sf\nChristopher Snowhill",
        .settings = "vio2sf",
        .init = ReplayVio2sf::Init,
        .load = ReplayVio2sf::Load,
        .displaySettings = ReplayVio2sf::DisplaySettings,
        .editMetadata = ReplayVio2sf::Settings::Edit
    };

    bool ReplayVio2sf::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_interpolation, "ReplayVio2sfInterpolation");

        return false;
    }

    Replay* ReplayVio2sf::Load(io::Stream* stream, CommandBuffer metadata)
    {
        auto replay = new ReplayVio2sf(stream);
        return replay->Load(metadata);
    }

    bool ReplayVio2sf::DisplaySettings()
    {
        bool changed = false;
        const char* const interpolation[] = { "none", "blep", "linear", "cubic", "sinc" };
        changed |= ImGui::Combo("Interpolation", &ms_interpolation, interpolation, _countof(interpolation));
        return changed;
    }

    void ReplayVio2sf::Settings::Edit(ReplayMetadataContext& context)
    {
        auto* entry = context.metadata.Find<Settings>();
        if (entry == nullptr)
        {
            // ok, we are here because we never played this song in this player
            entry = context.metadata.Create<Settings>(sizeof(Settings) + sizeof(uint32_t));
            entry->GetDurations()[0] = 0;
        }

        ComboOverride("Interpolation", GETSET(entry, overrideInterpolation), GETSET(entry, interpolation),
            ms_interpolation, "Interpolation: none", "Interpolation: blep", "Interpolation: linear", "Interpolation: cubic", "Interpolation: sinc");
        const float buttonSize = ImGui::GetFrameHeight();
        auto durations = entry->GetDurations();
        for (uint16_t i = 0; i <= entry->numSongsMinusOne; i++)
        {
            ImGui::PushID(i);
            bool isEnabled = durations[i] != 0;
            uint32_t duration = isEnabled ? durations[i] : kDefaultSongDuration;
            auto pos = ImGui::GetCursorPosX();
            if (ImGui::Checkbox("##Checkbox", &isEnabled))
                duration = kDefaultSongDuration;
            ImGui::SameLine();
            ImGui::BeginDisabled(!isEnabled);
            char txt[64];
            sprintf(txt, "Subsong #%d Duration", i + 1);
            auto width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 4 - buttonSize;
            ImGui::SetNextItemWidth(2.0f * width / 3.0f - ImGui::GetCursorPosX() + pos);
            ImGui::DragUint("##Duration", &duration, 1000.0f, 1, 0xffFFffFF, txt, ImGuiSliderFlags_NoInput, ImVec2(0.0f, 0.5f));
            int32_t milliseconds = duration % 1000;
            int32_t seconds = (duration / 1000) % 60;
            int32_t minutes = duration / 60000;
            ImGui::SameLine();
            width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3 - buttonSize;
            ImGui::SetNextItemWidth(width / 3.0f);
            ImGui::DragInt("##Minutes", &minutes, 0.1f, 0, 65535, "%d m", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2 - buttonSize;
            ImGui::SetNextItemWidth(width / 2.0f);
            ImGui::DragInt("##Seconds", &seconds, 0.1f, 0, 59, "%d s", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x - buttonSize;
            ImGui::SetNextItemWidth(width);
            ImGui::DragInt("##Milliseconds", &milliseconds, 1.0f, 0, 999, "%d ms", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            if (ImGui::Button("E", ImVec2(buttonSize, 0.0f)))
            {
                context.duration = duration;
                context.subsongIndex = i;
                context.isSongEndEditorEnabled = true;
            }
            else if (context.isSongEndEditorEnabled == false && context.duration != 0 && context.subsongIndex == i)
            {
                milliseconds = context.duration % 1000;
                seconds = (context.duration / 1000) % 60;
                minutes = context.duration / 60000;
                context.duration = 0;
            }
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Open Waveform Viewer");
            ImGui::EndDisabled();
            durations[i] = isEnabled ? uint32_t(minutes) * 60000 + uint32_t(seconds) * 1000 + uint32_t(milliseconds) : 0;
            ImGui::PopID();
        }
    }

    void* ReplayVio2sf::OpenPSF(void* context, const char* uri)
    {
        auto This = reinterpret_cast<ReplayVio2sf*>(context);
        if (This->m_stream->GetName() == uri)
            return This->m_stream->Clone().Detach();
        return This->m_stream->Open(uri).Detach();
    }

    size_t ReplayVio2sf::ReadPSF(void* buffer, size_t size, size_t count, void* handle)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return size_t(stream->Read(buffer, size * count) / size);
    }

    int ReplayVio2sf::SeekPSF(void* handle, int64_t offset, int whence)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return stream->Seek(offset, io::Stream::SeekWhence(whence)) == Status::kOk ? 0 : -1;
    }

    int ReplayVio2sf::ClosePSF(void* handle)
    {
        if (handle)
            reinterpret_cast<io::Stream*>(handle)->Release();
        return 0;
    }

    long ReplayVio2sf::TellPSF(void* handle)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return long(stream->GetPosition());
    }

    int ReplayVio2sf::InfoMetaPSF(void* context, const char* name, const char* value)
    {
        auto This = reinterpret_cast<ReplayVio2sf*>(context);
        char* end;
        if (!_stricmp(name, "_frames"))
        {
            This->m_loaderState.initial_frames = strtoul(value, &end, 10);
        }
        else if (!_stricmp(name, "_clockdown"))
        {
            This->m_loaderState.clockdown = strtoul(value, &end, 10);
        }
        else if (!_stricmp(name, "_vio2sf_sync_type"))
        {
            This->m_loaderState.sync_type = strtoul(value, &end, 10);
        }
        else if (!_stricmp(name, "_vio2sf_arm9_clockdown_level"))
        {
            This->m_loaderState.arm9_clockdown_level = strtoul(value, &end, 10);
        }
        else if (!_stricmp(name, "_vio2sf_arm7_clockdown_level"))
        {
            This->m_loaderState.arm7_clockdown_level = strtoul(value, &end, 10);
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

    int ReplayVio2sf::TwosfLoad(void* context, const uint8_t* exe, size_t exe_size, const uint8_t* reserved, size_t reserved_size)
    {
        auto* This = reinterpret_cast<ReplayVio2sf*>(context);
        if (exe_size >= 8)
        {
            if (This->TwosfLoadMap(0, exe, (unsigned)exe_size))
                return -1;
        }

        if (reserved_size)
        {
            size_t resv_pos = 0;
            if (reserved_size < 16)
                return -1;
            while (resv_pos + 12 < reserved_size)
            {
                unsigned save_size = get_le32(reserved + resv_pos + 4);
                unsigned save_crc = get_le32(reserved + resv_pos + 8);
                if (get_le32(reserved + resv_pos + 0) == 0x45564153)
                {
                    if (resv_pos + 12 + save_size > reserved_size)
                        return -1;
                    if (This->TwosfLoadMapz(1, reserved + resv_pos + 12, save_size, save_crc))
                        return -1;
                }
                resv_pos += 12 + save_size;
            }
        }

        return 0;
    }

    int ReplayVio2sf::TwosfLoadMap(int issave, const unsigned char* udata, unsigned usize)
    {
        if (usize < 8) return -1;

        unsigned char* iptr;
        size_t isize;
        unsigned char* xptr;
        unsigned xsize = get_le32(udata + 4);
        unsigned xofs = get_le32(udata + 0);
        if (issave)
        {
            iptr = m_loaderState.state;
            isize = m_loaderState.state_size;
            m_loaderState.state = 0;
            m_loaderState.state_size = 0;
        }
        else
        {
            iptr = m_loaderState.rom;
            isize = m_loaderState.rom_size;
            m_loaderState.rom = 0;
            m_loaderState.rom_size = 0;
        }
        if (!iptr)
        {
            size_t rsize = xofs + xsize;
            if (!issave)
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
            size_t rsize = xofs + xsize;
            if (!issave)
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
        memcpy(iptr + xofs, udata + 8, xsize);
        if (issave)
        {
            m_loaderState.state = iptr;
            m_loaderState.state_size = isize;
        }
        else
        {
            m_loaderState.rom = iptr;
            m_loaderState.rom_size = isize;
        }
        return 0;
    }

    int ReplayVio2sf::TwosfLoadMapz(int issave, const unsigned char* zdata, unsigned zsize, unsigned zcrc)
    {
        int ret;
        int zerr;
        uLongf usize = 8;
        uLongf rsize = usize;
        unsigned char* udata;
        unsigned char* rdata;

        udata = (unsigned char*)malloc(usize);
        if (!udata)
            return -1;

        while (Z_OK != (zerr = uncompress(udata, &usize, zdata, zsize)))
        {
            if (Z_MEM_ERROR != zerr && Z_BUF_ERROR != zerr)
            {
                free(udata);
                return -1;
            }
            if (usize >= 8)
            {
                usize = get_le32(udata + 4) + 8;
                if (usize < rsize)
                {
                    rsize += rsize;
                    usize = rsize;
                }
                else
                    rsize = usize;
            }
            else
            {
                rsize += rsize;
                usize = rsize;
            }
            rdata = (unsigned char*)realloc(udata, usize);
            if (!rdata)
            {
                free(udata);
                return -1;
            }
            udata = rdata;
        }

        rdata = (unsigned char*)realloc(udata, usize);
        if (!rdata)
        {
            free(udata);
            return -1;
        }

        if (0)
        {
            uLong ccrc = crc32(crc32(0L, Z_NULL, 0), rdata, (uInt)usize);
            if (ccrc != zcrc)
                return -1;
        }

        ret = TwosfLoadMap(issave, rdata, (unsigned)usize);
        free(rdata);
        return ret;
    }

    int32_t ReplayVio2sf::ms_interpolation = 0;

    ReplayVio2sf::~ReplayVio2sf()
    {
        state_deinit(&m_ndsState);
    }

    ReplayVio2sf::ReplayVio2sf(io::Stream* stream)
        : Replay(eExtension::Unknown, eReplay::vio2sf)
        , m_stream(stream)
    {}

    ReplayVio2sf* ReplayVio2sf::Load(CommandBuffer metadata)
    {
        auto stream = m_stream;
        uint32_t fileIndex = 0;
        do
        {
            m_length = kDefaultSongDuration;
            if (psf_load(stream->GetName().c_str(), &m_psfFileSystem, 0x24, nullptr, nullptr, InfoMetaPSF, this, 0, nullptr, nullptr) >= 0)
            {
                auto extPos = stream->GetName().find_last_of('.');
                if (extPos == std::string::npos || _stricmp(stream->GetName().c_str() + extPos + 1, "2sflib") != 0)
                {
                    if (psf_load(stream->GetName().c_str(), &m_psfFileSystem, 0x24, TwosfLoad, this, nullptr, nullptr, 0, nullptr, nullptr) >= 0)
                    {
                        m_mediaType.ext = m_hasLib ? eExtension::_mini2sf : eExtension::_2sf;
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

    uint32_t ReplayVio2sf::Render(StereoSample* output, uint32_t numSamples)
    {
        auto currentPosition = m_currentPosition;
        auto currentDuration = m_currentDuration;
        if (m_currentDuration != 0 && (currentPosition + numSamples) >= currentDuration)
        {
            numSamples = currentPosition < currentDuration ? uint32_t(currentDuration - currentPosition) : 0;
            if (numSamples == 0)
            {
                m_currentPosition = 0;
                return 0;
            }
        }
        m_currentPosition = currentPosition + numSamples;

        auto buf = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;
        state_render(&m_ndsState, buf, numSamples);
        output->Convert(buf, numSamples);

        return numSamples;
    }

    void ReplayVio2sf::ResetPlayback()
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
                    psf_load(stream->GetName().c_str(), &m_psfFileSystem, 0x24, TwosfLoad, this, InfoMetaPSF, this, 0, nullptr, nullptr);
                    break;
                }
                stream = stream->Next();
            }
        }

        m_currentPosition = 0;
        m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
        m_currentSubsongIndex = subsongIndex;

        state_deinit(&m_ndsState);
        state_init(&m_ndsState);

        m_ndsState.dwInterpolation = 0;
        m_ndsState.dwChannelMute = 0;

        if (!m_loaderState.arm7_clockdown_level)
            m_loaderState.arm7_clockdown_level = m_loaderState.clockdown;
        if (!m_loaderState.arm9_clockdown_level)
            m_loaderState.arm9_clockdown_level = m_loaderState.clockdown;

        m_ndsState.initial_frames = m_loaderState.initial_frames;
        m_ndsState.sync_type = m_loaderState.sync_type;
        m_ndsState.arm7_clockdown_level = m_loaderState.arm7_clockdown_level;
        m_ndsState.arm9_clockdown_level = m_loaderState.arm9_clockdown_level;

        if (m_loaderState.rom)
            state_setrom(&m_ndsState, m_loaderState.rom, (u32)m_loaderState.rom_size, 1);

        state_loadstate(&m_ndsState, m_loaderState.state, (u32)m_loaderState.state_size);
    }

    void ReplayVio2sf::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        if (settings)
        {
            auto durations = settings->GetDurations();
            for (uint16_t i = 0; i <= settings->numSongsMinusOne; i++)
                m_subsongs[i].overriddenDuration = durations[i];
            m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
        }
        m_ndsState.dwInterpolation = (settings && settings->overrideInterpolation) ? settings->interpolation : ms_interpolation;
    }

    void ReplayVio2sf::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayVio2sf::GetDurationMs() const
    {
        return m_subsongs[m_subsongIndex].overriddenDuration > 0 ? m_subsongs[m_subsongIndex].overriddenDuration : m_subsongs[m_subsongIndex].duration;
    }

    uint32_t ReplayVio2sf::GetNumSubsongs() const
    {
        return Max(1ul, m_subsongs.NumItems());
    }

    std::string ReplayVio2sf::GetSubsongTitle() const
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

    std::string ReplayVio2sf::GetExtraInfo() const
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

    std::string ReplayVio2sf::GetInfo() const
    {
        return "16 channels\nNintendo DS Sound Format\nvio2sf";
    }

    void ReplayVio2sf::SetupMetadata(CommandBuffer metadata)
    {
        uint32_t numSongsMinusOne = m_subsongs.NumItems() - 1;
        auto settings = metadata.Find<Settings>();
        if (settings && settings->numSongsMinusOne == numSongsMinusOne)
        {
            auto durations = settings->GetDurations();
            for (uint32_t i = 0; i <= numSongsMinusOne; i++)
                m_subsongs[i].overriddenDuration = durations[i];
        }
        else
        {
            auto value = settings ? settings->value : 0;
            settings = metadata.Create<Settings>(sizeof(Settings) + (numSongsMinusOne + 1) * sizeof(int32_t));
            settings->value = value;
            settings->numSongsMinusOne = numSongsMinusOne;
            auto durations = settings->GetDurations();
            for (uint16_t i = 0; i <= numSongsMinusOne; i++)
                durations[i] = 0;
        }
        m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
    }
}
// namespace rePlayer
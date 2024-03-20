#include "ReplayHighlyCompetitive.h"
#include "ReplayHighlyCompetitiveWrapper.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <IO/StreamMemory.h>
#include <ReplayDll.h>

#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>

#include <zlib.h>

#include <filesystem>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::HighlyCompetitive, .isThreadSafe = false,
        .name = "Highly Competitive/snsf9x",
        .extensions = "snsf;minisnsf;snsfPk",
        .about = "Highly Competitive/snsf9x 0.05\nChristopher Snowhill & loveemu",
        .settings = "Highly Competitive/snsf9x 0.05",
        .init = ReplayHighlyCompetitive::Init,
        .load = ReplayHighlyCompetitive::Load,
        .displaySettings = ReplayHighlyCompetitive::DisplaySettings,
        .editMetadata = ReplayHighlyCompetitive::Settings::Edit,
        .globals = &ReplayHighlyCompetitive::ms_interpolation
    };

    bool ReplayHighlyCompetitive::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        if (&window != nullptr)
            window.RegisterSerializedData(ms_interpolation, "ReplayHighlyCompetitiveInterpolation");

        return false;
    }

    Replay* ReplayHighlyCompetitive::Load(io::Stream* stream, CommandBuffer metadata)
    {
        auto replay = new ReplayHighlyCompetitive(stream);
        return replay->Load(metadata);
    }

    bool ReplayHighlyCompetitive::DisplaySettings()
    {
        bool changed = false;
        const char* const interpolation[] = { "none", "linear", "gaussian", "cubic", "sinc" };
        changed |= ImGui::Combo("Interpolation", &ms_interpolation, interpolation, _countof(interpolation));
        return changed;
    }

    void ReplayHighlyCompetitive::Settings::Edit(ReplayMetadataContext& context)
    {
        auto* entry = context.metadata.Find<Settings>();
        if (entry == nullptr)
        {
            // ok, we are here because we never played this song in this player
            entry = context.metadata.Create<Settings>(sizeof(Settings) + sizeof(uint32_t));
            entry->GetDurations()[0] = 0;
        }

        ComboOverride("Interpolation", GETSET(entry, overrideInterpolation), GETSET(entry, interpolation),
            ms_interpolation, "Interpolation: none", "Interpolation: linear", "Interpolation: gaussian", "Interpolation: cubic", "Interpolation: sinc");

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

    void* ReplayHighlyCompetitive::OpenPSF(void* context, const char* uri)
    {
        auto This = reinterpret_cast<ReplayHighlyCompetitive*>(context);
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
        return This->m_stream->Open(uri).Detach();
    }

    size_t ReplayHighlyCompetitive::ReadPSF(void* buffer, size_t size, size_t count, void* handle)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return stream->Read(buffer, size * count) / size;
    }

    int ReplayHighlyCompetitive::SeekPSF(void* handle, int64_t offset, int whence)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return stream->Seek(offset, io::Stream::SeekWhence(whence)) == Status::kOk ? 0 : -1;
    }

    int ReplayHighlyCompetitive::ClosePSF(void* handle)
    {
        if (handle)
            reinterpret_cast<io::Stream*>(handle)->Release();
        return 0;
    }

    long ReplayHighlyCompetitive::TellPSF(void* handle)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return long(stream->GetPosition());
    }

    int ReplayHighlyCompetitive::InfoMetaPSF(void* context, const char* name, const char* value)
    {
        auto This = reinterpret_cast<ReplayHighlyCompetitive*>(context);
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

    int ReplayHighlyCompetitive::SnsfLoad(void* context, const uint8_t* exe, size_t exe_size, const uint8_t* reserved, size_t reserved_size)
    {
        auto* This = reinterpret_cast<ReplayHighlyCompetitive*>(context);
        if (exe_size >= 8)
        {
            if (This->SnsfLoadMap(0, exe, (unsigned)exe_size))
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
                    if (This->SnsfLoadMapz(1, reserved + resv_pos + 12, save_size, save_crc))
                        return -1;
                }
                resv_pos += 12 + save_size;
            }
        }

        return 0;
    }

    int ReplayHighlyCompetitive::SnsfLoadMap(int issave, const unsigned char* udata, unsigned usize)
    {
        if (usize < 8)
            return -1;

        unsigned char* iptr;
        size_t isize;
        unsigned char* xptr;
        unsigned xsize = get_le32(udata + 4);
        unsigned xofs = get_le32(udata + 0);
        if (issave)
        {
            iptr = m_loaderState.sram;
            isize = m_loaderState.sramSize;
            m_loaderState.sram = 0;
            m_loaderState.sramSize = 0;
        }
        else
        {
            iptr = m_loaderState.rom;
            isize = m_loaderState.romSize;
            m_loaderState.rom = 0;
            m_loaderState.romSize = 0;
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
            m_loaderState.sram = iptr;
            m_loaderState.sramSize = isize;
        }
        else
        {
            m_loaderState.rom = iptr;
            m_loaderState.romSize = isize;
        }
        return 0;
    }

    int ReplayHighlyCompetitive::SnsfLoadMapz(int issave, const unsigned char* zdata, unsigned zsize, unsigned zcrc)
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

        ret = SnsfLoadMap(issave, rdata, (unsigned)usize);
        free(rdata);
        return ret;
    }

    int32_t ReplayHighlyCompetitive::ms_interpolation = 2;

    ReplayHighlyCompetitive::~ReplayHighlyCompetitive()
    {
        Snes9xRelease();
    }

    ReplayHighlyCompetitive::ReplayHighlyCompetitive(io::Stream* stream)
        : Replay(eExtension::Unknown, eReplay::HighlyCompetitive)
        , m_stream(stream)
    {}

    ReplayHighlyCompetitive* ReplayHighlyCompetitive::Load(CommandBuffer metadata)
    {
        auto data = m_stream->Read();

        auto* archive = archive_read_new();
        archive_read_support_format_all(archive);
        if (archive_read_open_memory(archive, data.Items(), data.Size()) != ARCHIVE_OK)
        {
            m_length = kDefaultSongDuration;
            if (psf_load(m_stream->GetName().c_str(), &m_psfFileSystem, 0x23, nullptr, nullptr, InfoMetaPSF, this, 0, nullptr, nullptr) >= 0)
            {
                auto extPos = m_stream->GetName().find_last_of('.');
                if (extPos == std::string::npos || _stricmp(m_stream->GetName().c_str() + extPos + 1, "snsflib") != 0)
                {
                    if (psf_load(m_stream->GetName().c_str(), &m_psfFileSystem, 0x23, SnsfLoad, this, nullptr, nullptr, 0, nullptr, nullptr) >= 0)
                    {
                        m_mediaType.ext = m_hasLib ? eExtension::_minisnsf : eExtension::_snsf;
                        m_subsongs.Add({ 0, uint32_t(m_length) });
                    }
                }
            }
        }
        else
        {
            std::swap(m_stream, m_streamArchive);
            uint32_t fileIndex = 0;
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

                m_length = kDefaultSongDuration;
                if (psf_load(entryName.c_str(), &m_psfFileSystem, 0x23, nullptr, nullptr, InfoMetaPSF, this, 0, nullptr, nullptr) >= 0)
                {
                    auto extPos = entryName.find_last_of('.');
                    if (extPos == std::string::npos || _stricmp(entryName.c_str() + extPos + 1, "snsflib") != 0)
                    {
                        m_mediaType.ext = eExtension::_snsfPk;
                        m_subsongs.Add({ fileIndex, uint32_t(m_length) });
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

//         m_segaState = new uint8_t[sega_get_state_size(m_psfType - 0x10)];

        SetupMetadata(metadata);

        return this;
    }

    uint32_t ReplayHighlyCompetitive::Render(StereoSample* output, uint32_t numSamples)
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
        Snes9xRender(buf, numSamples);
        output->Convert(buf, numSamples);

        return numSamples;
    }

    void ReplayHighlyCompetitive::ResetPlayback()
    {
        auto subsongIndex = m_subsongIndex;
        if (m_streamArchive.IsValid() && m_currentSubsongIndex != subsongIndex)
        {
            m_loaderState.Clear();
            m_tags.Clear();
            m_title.clear();

            auto data = m_streamArchive->Read();

            auto* archive = archive_read_new();
            archive_read_support_format_all(archive);
            archive_read_open_memory(archive, data.Items(), data.Size());
            archive_entry* entry;
            for (uint32_t fileIndex = 0; fileIndex < m_subsongs[subsongIndex].index; ++fileIndex)
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

            psf_load(m_stream->GetName().c_str(), &m_psfFileSystem, 0x23, SnsfLoad, this, InfoMetaPSF, this, 0, nullptr, nullptr);

            archive_read_free(archive);
        }

        m_currentPosition = 0;
        m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
        m_currentSubsongIndex = subsongIndex;

        Snes9xRelease();
        Snes9xInit(m_loaderState.rom, m_loaderState.romSize, m_loaderState.sram, m_loaderState.sramSize);
    }

    void ReplayHighlyCompetitive::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        if (settings)
        {
            auto durations = settings->GetDurations();
            for (uint16_t i = 0; i <= settings->numSongsMinusOne; i++)
                m_subsongs[i].overriddenDuration = durations[i];
            m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
        }
        Snes9xSetInterpolationMethod((settings && settings->overrideInterpolation) ? settings->interpolation : *static_cast<int32_t*>(g_replayPlugin.globals));
    }

    void ReplayHighlyCompetitive::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayHighlyCompetitive::GetDurationMs() const
    {
        return m_subsongs[m_subsongIndex].overriddenDuration > 0 ? m_subsongs[m_subsongIndex].overriddenDuration : m_subsongs[m_subsongIndex].duration;
    }

    uint32_t ReplayHighlyCompetitive::GetNumSubsongs() const
    {
        return Max(1ul, m_subsongs.NumItems());
    }

    std::string ReplayHighlyCompetitive::GetSubsongTitle() const
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

    std::string ReplayHighlyCompetitive::GetExtraInfo() const
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

    std::string ReplayHighlyCompetitive::GetInfo() const
    {
        return "8 channels\nSuper Nintendo Sound Format\nHighly Competitive/snsf9x 0.05";
    }

    void ReplayHighlyCompetitive::SetupMetadata(CommandBuffer metadata)
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
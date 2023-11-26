#include "ReplayQuartet.h"

#include <Audio/Audiotypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <IO/StreamFile.h>
#include <IO/StreamMemory.h>
#include <ReplayDll.h>

#include "zingzong/src/zz_private.h"

ZZ_EXTERN_C zz_vfs_dri_t zz_ice_vfs(void);

namespace rePlayer
{
#   define ZingzongVersion "1.5.0.294"

    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Quartet, .isThreadSafe = false,
        .name = "zingzong " ZingzongVersion,
        .extensions = "4v;qts",
        .about = "zingzong " ZingzongVersion "\nCopyright (c) 2017-2018 Benjamin Gerard AKA Ben/OVR",
        .settings = "zingzong " ZingzongVersion,
        .init = ReplayQuartet::Init,
        .load = ReplayQuartet::Load,
        .displaySettings = ReplayQuartet::DisplaySettings,
        .editMetadata = ReplayQuartet::Settings::Edit,
        .globals = &ReplayQuartet::ms_globals
    };

    bool ReplayQuartet::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        if (&window != nullptr)
        {
            window.RegisterSerializedData(ms_globals.stereoSeparation, "ReplayQuartetStereoSeparation");
            window.RegisterSerializedData(ms_globals.surround, "ReplayQuartetSurround");
        }

        return false;
    }

    Replay* ReplayQuartet::Load(io::Stream* stream, CommandBuffer metadata)
    {
        auto data = stream->Read();
        if (data.Size() < 8)
            return nullptr;
        stream->Seek(0, io::Stream::kSeekBegin);
        ms_driver.isInternalQts = memcmp(data.Items(), "QTST-MOD", 8) == 0;
        ms_driver.stream = stream;

        zz_vfs_add(&ms_driver);
        zz_vfs_add(zz_ice_vfs());
        zz_play_t player = nullptr;
        zz_new(&player);

        zz_u8_t format;
        auto ecode = zz_load(player, stream->GetName().c_str(), nullptr, &format);
        if (ecode == 0)
        {
            return new ReplayQuartet(player, format, metadata);
        }

        zz_del(&player);
        zz_vfs_del(zz_ice_vfs());
        zz_vfs_del(&ms_driver);
        ms_driver.stream = nullptr;
        return nullptr;
    }

    bool ReplayQuartet::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_globals.stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_globals.surround, surround, _countof(surround));
        return changed;
    }

    void ReplayQuartet::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_globals.stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_globals.surround, "Output: Stereo", "Output: Surround");
        SliderOverride("Rate", GETSET(entry, overrideRate), GETSET(entry, rate),
            RATE_DEF, RATE_MIN, RATE_MAX, "Rate %d Hz");
        if (ImGui::IsItemHovered())
            ImGui::Tooltip("Applied only on reload :(");

        const float buttonSize = ImGui::GetFrameHeight();
        bool isEnabled = entry->duration != 0;
        uint32_t duration = isEnabled ? entry->duration : kDefaultSongDuration;
        auto pos = ImGui::GetCursorPosX();
        if (ImGui::Checkbox("##Checkbox", &isEnabled))
            duration = kDefaultSongDuration;
        ImGui::SameLine();
        ImGui::BeginDisabled(!isEnabled);
        auto width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 4 - buttonSize;
        ImGui::SetNextItemWidth(2.0f * width / 3.0f - ImGui::GetCursorPosX() + pos);
        ImGui::DragUint("##Duration", &duration, 1000.0f, 1, 0xffFFffFF, "Duration", ImGuiSliderFlags_NoInput, ImVec2(0.0f, 0.5f));
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
            context.subsongIndex = 0;
            context.isSongEndEditorEnabled = true;
        }
        else if (context.isSongEndEditorEnabled == false && context.duration != 0)
        {
            milliseconds = context.duration % 1000;
            seconds = (context.duration / 1000) % 60;
            minutes = context.duration / 60000;
            context.duration = 0;
        }
        if (ImGui::IsItemHovered())
            ImGui::Tooltip("Open Waveform Viewer");
        ImGui::EndDisabled();
        entry->duration = isEnabled ? uint32_t(minutes) * 60000 + uint32_t(seconds) * 1000 + uint32_t(milliseconds) : 0;

        context.metadata.Update(entry, entry->value == 0 && entry->duration == 0);
    }

    ReplayQuartet::Globals ReplayQuartet::ms_globals = {
        .stereoSeparation = 100,
        .surround = 1
    };

    ReplayQuartet::Driver ReplayQuartet::ms_driver = {
        "rePlayer",
        vfs_reg, vfs_unreg, vfs_ismine, vfs_create, vfs_destroy, vfs_uri,
        vfs_open, vfs_close, vfs_read, vfs_tell, vfs_size, vfs_seek,
        nullptr
    };

    ReplayQuartet::~ReplayQuartet()
    {
        zz_del(&m_player);
        zz_vfs_del(zz_ice_vfs());
        zz_vfs_del(&ms_driver);
        ms_driver.stream = nullptr;
    }

    ReplayQuartet::ReplayQuartet(zz_play_t player, zz_u8_t format, CommandBuffer metadata)
        : Replay(ms_driver.isInternalQts ? eExtension::_qts : format == ZZ_FORMAT_4V ? eExtension::_4v : eExtension::Unknown, eReplay::Quartet)
        , m_player(player)
        , m_surround(kSampleRate)
    {
        ApplySettings(metadata);
        zz_core_blend(0, 0, 256);
        zz_info(player, &m_info);
    }

    uint32_t ReplayQuartet::Render(StereoSample* output, uint32_t numSamples)
    {
        auto buf = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;
        auto ret = zz_play(m_player, buf, numSamples);
        if (ret > 0)
            output->Convert(m_surround, buf, ret, m_stereoSeparation);
        else
        {
            // reset the loop to continue playing
            m_player->done = 0;
            m_player->core.loop = 0;
            if (m_duration)
                m_player->ms_max += m_duration;
            else
                m_player->ms_len += m_info.len.ms;
        }
        return ret > 0 ? ret : 0;
    }

    uint32_t ReplayQuartet::Seek(uint32_t timeInMs)
    {
        if (m_player->ms_len > timeInMs)
            ResetPlayback();
        else
            m_surround.Reset();

        auto numSamples = uint64_t(timeInMs - m_player->ms_len);
        numSamples *= kSampleRate;
        numSamples /= 1000;
        zz_play(m_player, nullptr, uint32_t(numSamples));
        return timeInMs;
    }

    void ReplayQuartet::ResetPlayback()
    {
        m_surround.Reset();
        zz_close(m_player);
        zz_u8_t format;
        ms_driver.stream->Seek(0, io::Stream::kSeekBegin);
        zz_load(m_player, ms_driver.stream->GetName().c_str(), nullptr, &format);
        zz_init(m_player, m_rate, m_duration == 0 ? ZZ_EOF : m_duration);
        zz_setup(m_player, ZZ_MIXER_DEF, kSampleRate);
        zz_info(m_player, &m_info);
    }

    void ReplayQuartet::ApplySettings(const CommandBuffer metadata)
    {
        auto* globals = static_cast<Globals*>(g_replayPlugin.globals);
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : globals->stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : globals->surround);
        m_rate = (settings && settings->overrideRate) ? uint16_t(settings->rate) : 0;
        m_duration = settings ? settings->duration : 0;
        m_player->ms_max = m_duration ? m_duration : ZZ_EOF;
    }

    void ReplayQuartet::SetSubsong(uint16_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayQuartet::GetDurationMs() const
    {
        return m_info.len.ms;
    }

    uint32_t ReplayQuartet::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayQuartet::GetExtraInfo() const
    {
        std::string metadata;
        if (m_info.tag.title && m_info.tag.title[0])
        {
            metadata = "Title  : ";
            metadata += m_info.tag.title;
        }
        if (m_info.tag.artist && m_info.tag.artist[0])
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Artist : ";
            metadata += m_info.tag.artist;
        }
        if (m_info.tag.album && m_info.tag.album[0])
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Album  : ";
            metadata += m_info.tag.album;
        }
        if (m_info.tag.ripper && m_info.tag.ripper[0])
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Ripper : ";
            metadata += m_info.tag.ripper;
        }
        return metadata;
    }

    std::string ReplayQuartet::GetInfo() const
    {
        std::string info;
        info = "4 channels\n";
        info += "Quartet ";
        info += m_info.fmt.str;
        info += "\nzingzong " ZingzongVersion;
        return info;
    }

    zz_err_t ReplayQuartet::vfs_reg(zz_vfs_dri_t dri)
    {
        (void)dri;
        return 0;
    }

    zz_err_t ReplayQuartet::vfs_unreg(zz_vfs_dri_t dri)
    {
        (void)dri;
        return 0;
    }

    zz_u16_t ReplayQuartet::vfs_ismine(const char* uri)
    {
        return (!!*uri) << 10;
    }

    zz_vfs_t ReplayQuartet::vfs_create(const char* uri, va_list list)
    {
        (void)list;
        auto* fileHandle = new FileHandle();
        fileHandle->dri = &ms_driver;
        fileHandle->uri = uri;
        if (ms_driver.stream->GetName() == uri)
        {
            if (ms_driver.isInternalQts)
            {
                auto data = ms_driver.stream->Read();
                auto offset = *data.Items<const uint32_t>(8);
                fileHandle->stream = io::StreamMemory::Create(uri, data.Items(12), offset - 12, true);
            }
            else
                fileHandle->stream = ms_driver.stream;
        }
        else if (ms_driver.isInternalQts)
        {
            auto data = ms_driver.stream->Read();
            auto offset = *data.Items<const uint32_t>(8);
            fileHandle->stream = io::StreamMemory::Create(uri, data.Items(offset), data.NumItems() - offset, true);
        }
        else
            fileHandle->stream = io::StreamFile::Create(uri);
        return fileHandle;
    }

    void ReplayQuartet::vfs_destroy(zz_vfs_t vfs)
    {
        delete reinterpret_cast<FileHandle*>(vfs);
    }

    const char* ReplayQuartet::vfs_uri(zz_vfs_t vfs)
    {
        return reinterpret_cast<FileHandle*>(vfs)->uri.c_str();
    }

    zz_err_t ReplayQuartet::vfs_open(zz_vfs_t const vfs)
    {
        auto* fileHandle = reinterpret_cast<FileHandle*>(vfs);
        return fileHandle->stream.IsValid() ? 0 : -1;
    }

    zz_err_t ReplayQuartet::vfs_close(zz_vfs_t const vfs)
    {
        (void)vfs;
        return 0;
    }

    zz_u32_t ReplayQuartet::vfs_read(zz_vfs_t vfs, void* ptr, zz_u32_t n)
    {
        auto* fileHandle = reinterpret_cast<FileHandle*>(vfs);
        fileHandle->err = 0;
        return zz_u32_t(fileHandle->stream->Read(ptr, n));
    }

    zz_u32_t ReplayQuartet::vfs_tell(zz_vfs_t vfs)
    {
        auto* fileHandle = reinterpret_cast<FileHandle*>(vfs);
        fileHandle->err = 0;
        return zz_u32_t(fileHandle->stream->GetPosition());
    }

    zz_u32_t ReplayQuartet::vfs_size(zz_vfs_t vfs)
    {
        auto* fileHandle = reinterpret_cast<FileHandle*>(vfs);
        fileHandle->err = 0;
        return zz_u32_t(fileHandle->stream->GetSize());
    }

    zz_err_t ReplayQuartet::vfs_seek(zz_vfs_t vfs, zz_u32_t offset, zz_u8_t whence)
    {
        auto* fileHandle = reinterpret_cast<FileHandle*>(vfs);
        fileHandle->err = 0;
        fileHandle->stream->Seek(offset, whence == ZZ_SEEK_SET ? io::Stream::kSeekBegin : whence == ZZ_SEEK_CUR ? io::Stream::kSeekCurrent : io::Stream::kSeekEnd);
        return 0;
    }
}
// namespace rePlayer
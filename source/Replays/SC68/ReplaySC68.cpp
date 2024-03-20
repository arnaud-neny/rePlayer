#include "ReplaySC68.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

#include "sc68/libsc68/sc68/sc68.h"

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::sc68,
        .name = "sc68",
        .extensions = "sc68;sndh;snd",
        .about = "libsc68 3.0.0a\nCopyright (c) 1998-2023 Benjamin Gerard",
        .settings = sc68_versionstr() + 3,
        .init = ReplaySC68::Init,
        .release = ReplaySC68::Release,
        .load = ReplaySC68::Load,
        .displaySettings = ReplaySC68::DisplaySettings,
        .editMetadata = ReplaySC68::Settings::Edit
    };

    bool ReplaySC68::Init(SharedContexts* ctx, Window& window)
    {
        static char appname[] = "rePlayer";
        static char* argv[] = { appname };
        static sc68_init_t sc68Init = {};

        sc68Init.argc = sizeof(argv) / sizeof(*argv);
        sc68Init.argv = argv;
#ifdef DEBUG
        //s_init.msg_handler = (sc68_msg_t)msg;
#endif
        //config is a pain with sc68, can't have it per instance and seems some are not working
        //sc68Init.flags.no_load_config = 1;
        //sc68Init.flags.no_save_config = 1;

        sc68_init(&sc68Init);

        ctx->Init();

        window.RegisterSerializedData(ms_aSIDfier, "ReplaySC68aSIDfier");
        window.RegisterSerializedData(ms_ymEngineAndFilter, "ReplaySC68ymEngineAndFilter");
        window.RegisterSerializedData(ms_ymVolumeModel, "ReplaySC68ymVolumeModel");
        window.RegisterSerializedData(ms_paulaFilter, "ReplaySC68paulaFilter");
        window.RegisterSerializedData(ms_paulaBlending, "ReplaySC68paulaBlending");
        window.RegisterSerializedData(ms_surround, "ReplaySC68Surround");

        return false;
    }

    void ReplaySC68::Release()
    {
        sc68_shutdown();
    }

    Replay* ReplaySC68::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        if (stream->GetSize() > 1024 * 1024 * 128)
            return nullptr;
        ApplySettings();

        sc68_create_t create = {};
        create.log2mem = 23;
        create.name = "rePlayer";
        create.sampling_rate = kSampleRate;
        sc68_t* inst = sc68_create(&create);

        auto data = stream->Read();
        if (sc68_load_mem(inst, data.Items(), static_cast<int>(data.Size())) < 0)
        {
            sc68_destroy(inst);
            return nullptr;
        }
        sc68_process(inst, nullptr, 0);

        sc68_t* surroundInst = nullptr;
        sc68_music_info_t musicInfo = {};
        sc68_music_info(inst, &musicInfo, 1, 0);
        if (musicInfo.dsk.ym)
        {
            surroundInst = sc68_create(&create);
            sc68_ym_channels(surroundInst, 2);
            sc68_load_mem(surroundInst, data.Items(), static_cast<int>(data.Size()));
            sc68_process(surroundInst, nullptr, 0);
        }

        return new ReplaySC68(inst, surroundInst);
    }

    bool ReplaySC68::DisplaySettings()
    {
        bool changed = false;
        {
            const char* const aSIDfier[] = { "off", "on", "force"};
            changed |= ImGui::Combo("aSIDfier", &ms_aSIDfier, aSIDfier, _countof(aSIDfier));
        }
        {
            const char* const engines[] = { "blep", "pulse/2-poles", "pulse/mixed", "pulse/1-pole", "pulse/boxcar", "pulse/none"};
            changed |= ImGui::Combo("YM Engine & Filter", &ms_ymEngineAndFilter, engines, _countof(engines));
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Applied only on reload :(");
        }
        {
            const char* const volumeModels[] = { "atari", "linear" };
            changed |= ImGui::Combo("YM Volume Model", &ms_ymVolumeModel, volumeModels, _countof(volumeModels));
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Applied only on reload :(");
        }
        {
            const char* const paulaFilters[] = { "off", "on" };
            changed |= ImGui::Combo("Paula Filter", &ms_paulaFilter, paulaFilters, _countof(paulaFilters));
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Applied only on reload :(");
        }
        changed |= ImGui::SliderInt("Paula Blending Factor", &ms_paulaBlending, 0, 255, "%d", ImGuiSliderFlags_NoInput);
        if (ImGui::IsItemHovered())
            ImGui::Tooltip("Applied only on reload :(");
        const char* const surround[] = { "Default", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, _countof(surround));
        return changed;
    }

    void ReplaySC68::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Default", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplaySC68::ms_aSIDfier = 0;//off-on/force
    //int32_t ReplaySC68::ms_defaultTime = 180;//0..86399
    int32_t ReplaySC68::ms_ymEngineAndFilter = 1;//blep/pulse 2-poles/pulse mixed/pulse 1-pole/pulse boxcar/pulse none
    int32_t ReplaySC68::ms_ymVolumeModel = 0;//atari/linear
    //int32_t ReplaySC68::ms_paulaClock = 0;//pal/ntsc
    int32_t ReplaySC68::ms_paulaFilter = 0;//no-amiga-filter/amiga-filter
    int32_t ReplaySC68::ms_paulaBlending = 80;//0..255/128 = mono
    int32_t ReplaySC68::ms_surround = 1;

    ReplaySC68::~ReplaySC68()
    {
        for (auto sc68Instance : m_sc68Instances)
        {
            if (sc68Instance)
                sc68_destroy(sc68Instance);
        }
    }

    ReplaySC68::ReplaySC68(sc68_t* sc68Instance0, sc68_t* sc68Instance1)
        : Replay(GetExtension(sc68Instance0), eReplay::sc68)
        , m_sc68Instances{ sc68Instance0, sc68Instance1 }
        , m_surround(kSampleRate)
    {}

    uint32_t ReplaySC68::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_hasLooped)
        {
            m_hasLooped = false;
            return 0;
        }

        auto buf = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;
        {
            if (m_sc68Instances[1])
            {
                auto bufRight = reinterpret_cast<int16_t*>(output);
                auto n = numSamples;
                sc68_process(m_sc68Instances[1], bufRight, reinterpret_cast<int32_t*>(&n));
            }
            auto status = sc68_process(m_sc68Instances[0], buf, reinterpret_cast<int32_t*>(&numSamples));
            if (status & (SC68_LOOP | SC68_END) && numSamples > 0)
                m_hasLooped = true;
        }

        if (m_sc68Instances[1] && m_surround.IsEnabled())
        {
            auto src = reinterpret_cast<int16_t*>(output) + 1;
            auto dst = buf + 1;
            for (uint32_t i = 0; i < numSamples; i++)
            {
                *dst = *src;
                src += 2;
                dst += 2;
            }
            output->Convert(m_surround, buf, numSamples, 100, 1.333f);
        }
        else
            output->Convert(m_surround, buf, numSamples, 100);

        return numSamples;
    }

    void ReplaySC68::ResetPlayback()
    {
        for (auto sc68Instance : m_sc68Instances)
        {
            if (sc68Instance)
            {
                sc68_stop(sc68Instance);
                sc68_process(sc68Instance, nullptr, 0);
                sc68_play(sc68Instance, m_subsongIndex + 1, SC68_INF_LOOP);
                sc68_process(sc68Instance, nullptr, 0);
            }
        }
        m_surround.Reset();
    }

    void ReplaySC68::ApplySettings()
    {
        sc68_cntl(nullptr, SC68_SET_ASID, ms_aSIDfier);
        if (ms_ymEngineAndFilter == 0)
            sc68_cntl(nullptr, SC68_SET_OPT_INT, "ym-engine", 0);
        else
        {
            sc68_cntl(nullptr, SC68_SET_OPT_INT, "ym-engine", 1);
            sc68_cntl(nullptr, SC68_SET_OPT_INT, "ym-filter", ms_ymEngineAndFilter - 1);
        }
        sc68_cntl(nullptr, SC68_SET_OPT_INT, "ym-volmodel", ms_ymVolumeModel);
        sc68_cntl(nullptr, SC68_SET_OPT_INT, "amiga-filter", ms_paulaFilter);
        sc68_cntl(nullptr, SC68_SET_OPT_INT, "amiga-blend", ms_paulaBlending);
    }

    void ReplaySC68::ApplySettings(const CommandBuffer metadata)
    {
        ApplySettings();
        for (auto sc68Instance : m_sc68Instances)
        {
            if (sc68Instance)
                sc68_cntl(sc68Instance, SC68_SET_ASID, ms_aSIDfier);
        }
        auto settings = metadata.Find<Settings>();
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
        if (m_sc68Instances[1])
        {
            if (m_surround.IsEnabled())
                sc68_ym_channels(m_sc68Instances[0], 5);
            else
                sc68_ym_channels(m_sc68Instances[0], 7);
        }
    }

    void ReplaySC68::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    eExtension ReplaySC68::GetExtension(sc68_t* sc68Instance)
    {
        sc68_music_info_t musicInfo = {};
        sc68_music_info(sc68Instance, &musicInfo, 1, 0);
        if (strstr(musicInfo.format, "sndh"))
            return eExtension::_sndh;
        //else if (strstr(musicInfo.format, "sc68"))
            return eExtension::_sc68;
    }

    uint32_t ReplaySC68::GetDurationMs() const
    {
        sc68_stop(m_sc68Instances[0]);
        sc68_process(m_sc68Instances[0], nullptr, 0);
        sc68_play(m_sc68Instances[0], m_subsongIndex + 1, 0);
        sc68_process(m_sc68Instances[0], nullptr, 0);
        sc68_music_info_t musicInfo = {};
        sc68_music_info(m_sc68Instances[0], &musicInfo, m_subsongIndex + 1, 0);
        return musicInfo.trk.time_ms;
    }

    uint32_t ReplaySC68::GetNumSubsongs() const
    {
        sc68_music_info_t musicInfo = {};
        sc68_music_info(m_sc68Instances[0], &musicInfo, 1, 0);
        return uint32_t(musicInfo.tracks);
    }

    std::string ReplaySC68::GetExtraInfo() const
    {
        std::string info;
        sc68_music_info_t musicInfo = {};
        sc68_music_info(m_sc68Instances[0], &musicInfo, m_subsongIndex + 1, 0);
        info = "Title    : ";
        if (musicInfo.title)
            info += musicInfo.title;
        info += "\nArtist   : ";
        if (musicInfo.artist)
            info += musicInfo.artist;
        info += "\nDisk     : ";
        if (musicInfo.album)
            info += musicInfo.album;
        info += "\nGenre    : ";
        if (musicInfo.genre)
            info += musicInfo.genre;
        info += "\nYear     : ";
        if (musicInfo.year)
            info += musicInfo.year;
        info += "\nRipper   : ";
        if (musicInfo.ripper)
            info += musicInfo.ripper;
        info += "\nConverter: ";
        if (musicInfo.converter)
            info += musicInfo.converter;
        return info;
    }

    std::string ReplaySC68::GetInfo() const
    {
        std::string info;
        sc68_music_info_t musicInfo = {};
        sc68_music_info(m_sc68Instances[0], &musicInfo, m_subsongIndex + 1, 0);
        if (musicInfo.dsk.amiga || musicInfo.dsk.ste)
            info = "4 channels\n";
        else
            info = "3 channels\n";
        info += musicInfo.trk.hw;
        info += " / ";
        info += musicInfo.replay;
        info += "\n";
        info += sc68_versionstr();
        return info;
    }
}
// namespace rePlayer
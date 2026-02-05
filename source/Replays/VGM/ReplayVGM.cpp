// Some minor changes are done in libvgm:
// - emu/cores/2612intf.c
// - emu/cores/adlibemu.h
// - emu/cores/adlibemu_opl_inc.c
// - emu/cores/adlibemu_opl_inc.h
// - emu/cores/fmopn.c
// - emu/cores/fmopn.h
// - emu/cores/gb.c
// - emu/cores/Ootake_PSG.c
// - emu/cores/Ootake_PSG_private.h
// - emu/cores/oplintf.c
// - emu/cores/oplnintf.c
// - emu/cores/sn76496.c
// - player/droplayer.hpp
// - player/gymplayer.hpp
// - player/playerbase.hpp
// - player/s98player.hpp
// - player/vgmplayer.hpp
#include "ReplayVGM.h"

#include <Audio/AudioTypes.inl.h>
#include <Containers/Array.inl.h>
#include <Core/Log.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

#include <player/vgmplayer.hpp>
#include <player/s98player.hpp>
#include <player/droplayer.hpp>
#include <player/gymplayer.hpp>
#include <utils/MemoryLoader.h>

#include "yrw801.h"

#define LIBVGM_VERSION "@57585ea"

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::VGM,
        .name = "Video Game Music",
        .extensions = "vgm;vgz;s98;gym;dro",
        .about = "libvgm",
        .settings = "libvgm " LIBVGM_VERSION,
        .init = ReplayVGM::Init,
        .load = ReplayVGM::Load,
        .displaySettings = ReplayVGM::DisplaySettings,
        .editMetadata = ReplayVGM::Settings::Edit
    };

    bool ReplayVGM::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayVGMStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayVGMSurround");
        window.RegisterSerializedData(ms_droV2Opl3, "ReplayVGMDroV2Opl3");
        window.RegisterSerializedData(ms_vgmPlaybackHz, "ReplayVGMPlaybackHz");
        window.RegisterSerializedData(ms_vgmHardStopOld, "ReplayVGMHardStopOld");
        window.RegisterSerializedData(ms_forceStereo, "ReplayVGMForceStereo");

        return false;
    }

    Replay* ReplayVGM::Load(io::Stream* stream, CommandBuffer metadata)
    {
        if (stream->GetSize() > 1024 * 1024 * 128)
            return nullptr;
        auto settings = metadata.Find<Settings>();

        auto* droPlayer = new DROPlayer;
        DRO_PLAY_OPTIONS droOptions;
        droPlayer->GetPlayerOptions(droOptions);
        droOptions.v2opl3Mode = uint8_t((settings && settings->overrideDroV2Opl3) ? settings->droV2Opl3 : ms_droV2Opl3);
        droPlayer->SetPlayerOptions(droOptions);

        auto* vgmPlayer = new VGMPlayer;
        VGM_PLAY_OPTIONS vgmOptions;
        vgmPlayer->GetPlayerOptions(vgmOptions);
        static uint32_t hz[] = { 0, 50, 60 };
        vgmOptions.playbackHz = hz[(settings&& settings->overrideVgmPlaybackHz) ? settings->vgmPlaybackHz : ms_vgmPlaybackHz];
        vgmOptions.hardStopOld = uint8_t((settings && settings->overrideVgmHardStopOld) ? settings->vgmHardStopOld : ms_vgmHardStopOld);
        vgmPlayer->SetPlayerOptions(vgmOptions);

        auto* player = new PlayerA();
        player->RegisterPlayerEngine(new GYMPlayer);
        player->RegisterPlayerEngine(new S98Player);
        player->RegisterPlayerEngine(vgmPlayer);
        player->RegisterPlayerEngine(droPlayer);

        if (player->SetOutputSettings(kSampleRate, 2, 16, kSampleBufferLen))
        {
            Log::Error("[VGM] Unsupported sample rate %dHz\n", kSampleRate);
            delete player;
            return nullptr;
        }

        auto data = stream->Read();
        auto* replay = new ReplayVGM(stream, player, MemoryLoader_Init(data.Items(), uint32_t(data.Size())));
        player->SetFileReqCallback([](void* userParam, PlayerBase* /*player*/, const char* fileName)
        {
            DATA_LOADER* loader = nullptr;
            auto* replay = reinterpret_cast<ReplayVGM*>(userParam);
            if (strcmp(fileName, "yrw801.rom") == 0)
                loader = MemoryLoader_Init(s_yrw801_rom, uint32_t(sizeof(s_yrw801_rom)));
            else
            {
                auto newStream = replay->m_streams[0]->Open(fileName);
                if (newStream.IsValid())
                {
                    replay->m_streams.Add(newStream);
                    auto data = newStream->Read();
                    loader = MemoryLoader_Init(data.Items(), data.NumItems());
                }
            }
            if (DataLoader_Load(loader))
            {
                DataLoader_Deinit(loader);
                loader = nullptr;
            }
            return loader;
        }, replay);

        if (DataLoader_Load(replay->m_loader) || player->LoadFile(replay->m_loader))
        {
            delete replay;
            return nullptr;
        }

        player->SetLoopCount(0);
        player->SetEndSilenceSamples(kSampleRate / 4);
        player->Start();
        player->SetEventCallback(OnEvent, replay);

        replay->m_mediaType.ext = GetExtension(stream, player, replay->m_loader);

        return replay;
    }

    bool ReplayVGM::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        const char* const dro[] = { "Detect", "Header", "Forced"};
        changed |= ImGui::Combo("DRO v2 with Opl3", &ms_droV2Opl3, dro, NumItemsOf(dro));
        if (ImGui::IsItemHovered())
            ImGui::Tooltip("Applied only on reload :(");
        const char* const hz[] = { "Default", "PAL", "NTSC" };
        changed |= ImGui::Combo("VGM playback", &ms_vgmPlaybackHz, hz, NumItemsOf(hz));
        if (ImGui::IsItemHovered())
            ImGui::Tooltip("Applied only on reload :(");
        const char* const stop[] = { "Off", "On" };
        changed |= ImGui::Combo("Hard stop on old VGM", &ms_vgmHardStopOld, stop, NumItemsOf(stop));
        if (ImGui::IsItemHovered())
            ImGui::Tooltip("Applied only on reload :(");
        changed |= ImGui::Combo("Force Stereo", &ms_forceStereo, stop, NumItemsOf(stop));
        return changed;
    }

    void ReplayVGM::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find<Settings>(&dummy);
        if (entry->numEntries != dummy.numEntries) // check for older format as duration has been added after 0.6.0
        {
            dummy.value = entry->value;
            context.metadata.Update(entry, true);
            entry = &dummy;
        }

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");
        ComboOverride("DroV2Opl3", GETSET(entry, overrideDroV2Opl3), GETSET(entry, droV2Opl3),
            ms_droV2Opl3, "dro v2 with Opl3: Detect", "dro v2 with Opl3: Header", "dro v2 with Opl3: Forced");
        if (ImGui::IsItemHovered())
            ImGui::Tooltip("Applied only on reload :(");
        ComboOverride("VgmPlayback", GETSET(entry, overrideVgmPlaybackHz), GETSET(entry, vgmPlaybackHz),
            ms_vgmPlaybackHz, "VGM playback: Default", "VGM playback: PAL", "VGM playback: NTSC");
        if (ImGui::IsItemHovered())
            ImGui::Tooltip("Applied only on reload :(");
        ComboOverride("VgmHardStopOld", GETSET(entry, overrideVgmHardStopOld), GETSET(entry, vgmHardStopOld),
            ms_vgmHardStopOld, "Hard stop on old VGM: Off", "Hard stop on old VGM: On");
        if (ImGui::IsItemHovered())
            ImGui::Tooltip("Applied only on reload :(");
        ComboOverride("ForceStereo", GETSET(entry, overrideForceStereo), GETSET(entry, forceStereo),
            ms_forceStereo, "Default Panning", "Stereo Panning");
        Loops(context, &entry->loop, 1, kDefaultSongDuration);

        context.metadata.Update(entry, entry->value == 0 && !entry->loop.IsValid());
    }

    int32_t ReplayVGM::ms_stereoSeparation = 100;
    int32_t ReplayVGM::ms_surround = 1;
    int32_t ReplayVGM::ms_droV2Opl3 = DRO_V2OPL3_DETECT;
    int32_t ReplayVGM::ms_vgmPlaybackHz = 0;
    int32_t ReplayVGM::ms_vgmHardStopOld = 0;
    int32_t ReplayVGM::ms_forceStereo = 1;

    ReplayVGM::~ReplayVGM()
    {
        delete m_player;
        DataLoader_Deinit(m_loader);
    }

    ReplayVGM::ReplayVGM(io::Stream* stream, PlayerA* player, DATA_LOADER* loader)
        : Replay(eExtension::Unknown, eReplay::VGM)
        , m_player(player)
        , m_loader(loader)
        , m_surround(kSampleRate)
    {
        m_streams.Add(stream);
    }

    UINT8 ReplayVGM::OnEvent(PlayerBase* /*player*/, void* userParam, UINT8 evtType, void* /*evtParam*/)
    {
        switch (evtType)
        {
        case PLREVT_NONE:
        case PLREVT_START:
        case PLREVT_STOP:
            break;
        case PLREVT_LOOP:
            reinterpret_cast<ReplayVGM*>(userParam)->m_hasLooped = true;
            break;
        case PLREVT_END:
            reinterpret_cast<ReplayVGM*>(userParam)->m_hasEnded = true;
            break;
        }
        return 0;
    }

    uint32_t ReplayVGM::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_hasEnded)
            return 0;
        auto currentPosition = m_currentPosition;
        auto currentDuration = m_currentDuration;
        if (m_currentDuration != 0)
        {
            if ((currentPosition + numSamples) >= currentDuration)
            {
                numSamples = currentPosition < currentDuration ? uint32_t(currentDuration - currentPosition) : 0;
                if (numSamples == 0)
                {
                    m_currentPosition = 0;
                    return 0;
                }
            }
            m_hasLooped = false;
        }
        else if (m_hasLooped)
        {
            m_hasLooped = false;
            return 0;
        }
        m_currentPosition = currentPosition + numSamples;

        auto buf = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;
        numSamples = m_player->Render(numSamples * sizeof(int16_t) * 2, buf) / sizeof(int16_t) / 2;

        output->Convert(m_surround, buf, numSamples, m_stereoSeparation);

        return numSamples;
    }

    uint32_t ReplayVGM::Seek(uint32_t timeInMs)
    {
        uint64_t pos = timeInMs;
        pos *= kSampleRate;
        pos /= 1000;
        m_player->Seek(PLAYPOS_SAMPLE, uint32_t(pos));
        return timeInMs;
    }

    void ReplayVGM::ResetPlayback()
    {
        m_surround.Reset();
        m_player->Reset();
        m_hasEnded = m_hasLooped = false;
        m_currentPosition = 0;
        m_currentDuration = (uint64_t(m_loop.GetDuration()) * kSampleRate) / 1000;
    }

    void ReplayVGM::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
        m_loop = (settings && settings->loop.IsValid()) ? settings->loop : LoopInfo{};
        m_currentDuration = (uint64_t(m_loop.GetDuration()) * kSampleRate) / 1000;

        bool forceStereo = (settings && settings->overrideForceStereo) ? settings->forceStereo : ms_forceStereo;
        std::vector<PLR_DEV_INFO> devInfos;
        m_player->GetPlayer()->GetSongDeviceInfo(devInfos);
        for (auto& devInfo : devInfos)
        {
            PLR_DEV_OPTS devOpts;
            m_player->GetPlayer()->GetDeviceOptions(devInfo.id, devOpts);
            if (forceStereo) for (uint32_t i = 0; i < NumItemsOf(devOpts.panOpts.chnPan[0]); ++i)
                devOpts.panOpts.chnPan[1][i] = devOpts.panOpts.chnPan[0][i] = (i & 1) * 256 * 2 - 256;
            else for (uint32_t i = 0; i < NumItemsOf(devOpts.panOpts.chnPan[0]); ++i)
                devOpts.panOpts.chnPan[1][i] = devOpts.panOpts.chnPan[0][i] = 0;
            m_player->GetPlayer()->SetDeviceOptions(devInfo.id, devOpts);
        }
    }

    void ReplayVGM::SetSubsong(uint32_t /*subsongIndex*/)
    {}

    eExtension ReplayVGM::GetExtension(io::Stream* stream, PlayerA* player, DATA_LOADER* loader)
    {
        switch (player->GetPlayer()->GetPlayerType())
        {
        case FCC_VGM:
            return loader->_bytesTotal > stream->GetSize() ? eExtension::_vgz : eExtension::_vgm;
        case FCC_S98:
            return eExtension::_s98;
        case FCC_GYM:
            return eExtension::_gym;
        case FCC_DRO:
            return eExtension::_dro;
        }
        return eExtension::Unknown;
    }

    uint32_t ReplayVGM::GetDurationMs() const
    {
        m_player->SetLoopCount(1);
        m_player->Reset();
        auto duration = uint32_t(m_player->GetTotalTime(PLAYTIME_LOOP_INCL | PLAYTIME_TIME_FILE) * 1000ull);
        m_player->SetLoopCount(0);
        m_player->Reset();
        return duration;
    }

    uint32_t ReplayVGM::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayVGM::GetExtraInfo() const
    {
        std::string metadata;
        auto* tags = m_player->GetPlayer()->GetTags();
        while (*tags)
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += *tags++;
            metadata += " : ";
            metadata += *tags++;
        }

        auto& devs = m_player->GetPlayer()->GetDevNames();
        if (!devs.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "DEVICES : ";
            metadata += devs[0];
            for (size_t i = 1; i < devs.size(); ++i)
            {
                metadata += ";";
                metadata += devs[i];
            }
        }

        return metadata;
    }

    std::string ReplayVGM::GetInfo() const
    {
        std::string info;
        info = "2 channels\n";

        auto* tags = m_player->GetPlayer()->GetTags();
        while (*tags)
        {
            if (_stricmp(tags[0], "system") == 0)
            {
                if (tags[1][0] && tags[1][0] != '?')
                {
                    info += tags[1];
                    break;
                }
            }
            if (_stricmp(tags[0], "system") == 0)
            {
                if (tags[1][0] && tags[1][0] != '?')
                {
                    info += tags[1];
                    break;
                }
            }
            tags += 2;
        }
        if (*tags == nullptr)
        {
            if (m_player->GetPlayer()->GetPlayerType() == FCC_GYM)
                info += "Mega Drive GYM";
            else if (m_player->GetPlayer()->GetPlayerType() == FCC_DRO)
                info += "Adlib DOSBox";
            else
                info += m_player->GetPlayer()->GetPlayerName();
        }
        info += "\nlibvgm " LIBVGM_VERSION;
        return info;
    }
}
// namespace rePlayer
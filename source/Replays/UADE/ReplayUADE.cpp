/*
* Score has been customized to avoid stopping at the end of the song; instead, it notifies the end, but continue playing.
* changes are:
* - instead of end_song after the comment "* check if song has ended, ignore if songendbit ($12C) = 0", do this: bne.b	end_song2
* - and add this code before the end_song label:
* end_song2
    bsr	report_song_end
    lea	songendbit(pc),a0
    clr.l	(a0)
    bra nosongendcheck
*/
#include "ReplayUADE.h"
#include "ReplayUADE.inl.h"

extern "C"
{
#include <uade/uadestate.h>
#include <include/uadectl.h>
}
#include <Audio/AudioTypes.inl.h>
#include <Core/Log.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::UADE, .isThreadSafe = false,
        .name = "Unix Amiga Delitracker Emulator",
        .about = "UADE 3.0.3\nHeikki Orsila & Michael Doering",
        .settings = "Unix Amiga Delitracker Emulator 3.0.3",
        .init = ReplayUADE::Init,
        .release = ReplayUADE::Release,
        .load = ReplayUADE::Load,
        .displaySettings = ReplayUADE::DisplaySettings,
        .getFileFilter = ReplayUADE::GetFileFilters,
        .editMetadata = ReplayUADE::Settings::Edit,
        .globals = &ReplayUADE::ms_globals
    };

    bool ReplayUADE::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        if (&window != nullptr)
        {
            window.RegisterSerializedData(ms_globals.stereoSeparation, "ReplayUADEStereoSeparation");
            window.RegisterSerializedData(ms_globals.surround, "ReplayUADESurround");
            window.RegisterSerializedData(ms_globals.filter, "ReplayUADEFilter");
            window.RegisterSerializedData(ms_globals.isNtsc, "ReplayUADENtsc");

            auto file = fopen("eagleplayer.conf", "rb"); // fopen will fallback on stdioemu_fopen (inside our uade.7z)
            fseek(file, 0, SEEK_END);
            Array<char> conf;
            conf.Resize(ftell(file));
            fseek(file, 0, SEEK_SET);
            fread(conf.Items(), conf.NumItems<int>(), 1, file);
            fclose(file);
            auto buf = conf.Items();
            auto end = buf + conf.NumItems();

            std::string extensions;
            while (buf < end)
            {
                // eagleplayer name
                auto txt = buf;
                while (*txt != 9)
                    txt++;
                buf = strstr(txt + 1, "prefixes=") + sizeof("prefixes=") - 1;
                txt = buf;
                do
                {
                    if (!extensions.empty())
                        extensions += ';';
                    if (*txt == ',')
                        *txt++;
                    while (*txt != ',' && *txt != 9 && *txt != 0xa && *txt != 0xd)
                    {
                        extensions += *txt;
                        txt++;
                    }
                } while (*txt != 9 && *txt != 0xa && *txt != 0xd);
                while (*txt != 0xa)
                    txt++;
                buf = txt + 1;
            }
            extensions += ";tfm;ymst";
            auto ext = new char[extensions.size() + 1];
            memcpy(ext, extensions.c_str(), extensions.size() + 1);
            g_replayPlugin.extensions = ext;

            LoadPlayerNames();
        }

        return false;
    }

    void ReplayUADE::Release()
    {
        delete[] g_replayPlugin.extensions;
    }

    Replay* ReplayUADE::Load(io::Stream* stream, CommandBuffer metadata)
    {
        if (stream->GetSize() < 16 || stream->GetSize() > 1024 * 1024 * 128)
            return nullptr;

        auto buffer = stream->Read();

        // rePlayer extra formats
        ReplayOverride* replayOverride = nullptr;
        auto data = buffer.Items();
        for (auto& entry : ms_replayOverrides)
        {
            if (memcmp(data, entry.header, 8) == 0)
            {
                replayOverride = &entry;
                break;
            }
        }

        struct uade_config* config = uade_new_config();
        uade_config_set_option(config, UC_ONE_SUBSONG, NULL);
        uade_config_set_option(config, UC_IGNORE_PLAYER_CHECK, NULL);
        uade_config_set_option(config, UC_FREQUENCY, "48000");
        uade_config_set_option(config, UC_NO_PANNING, NULL);
        uade_config_set_option(config, UC_SUBSONG_TIMEOUT_VALUE, "-1");
        uade_config_set_option(config, UC_SILENCE_TIMEOUT_VALUE, "-1");

        uade_config_set_option(config, UC_BASE_DIR, "");

        auto* settings = metadata.Find<Settings>();
        bool isPlayerSet = false;
        if (settings && settings->overridePlayer)
        {
            auto* globals = static_cast<Globals*>(g_replayPlugin.globals);
            for (uint32_t i = 1; i < globals->players.NumItems(); i++)
            {
                if (globals->players[i].second == settings->data[0])
                {
                    std::string str = "/players/";
                    str += globals->strings.Items(globals->players[i].first);
                    uade_config_set_option(config, UC_PLAYER_FILE, str.c_str());
                    isPlayerSet = true;
                    break;
                }
            }
        }
        if (!isPlayerSet && replayOverride && replayOverride->player)
            uade_config_set_option(config, UC_PLAYER_FILE, replayOverride->player);

        uade_state* state = uade_new_state(config);
        free(config);
        if (state == nullptr)
            return nullptr;

        auto replay = new ReplayUADE(stream, state);
        uade_set_amiga_loader(ReplayUADE::AmigaLoader, replay, state);

        auto filepath = std::filesystem::path(stream->GetName());
        auto filename = filepath.filename().string();

        // rePlayer extra formats
        auto dataSize = buffer.Size();
        if (replayOverride)
        {
            replay->m_mediaType.ext = replayOverride->build(replay, data, filepath, dataSize);
            filename = filepath.filename().string();
        }

        if (uade_play_from_buffer(filename.c_str(), data, dataSize, -1, state) == 1)
        {
            replay->BuildMediaType();
            replay->BuildDurations(metadata);
            return replay;
        }

        delete replay;

        return nullptr;
    }

    struct uade_file* ReplayUADE::AmigaLoader(const char* name, const char* playerdir, void* context, struct uade_state* state)
    {
        // check if name is a directory
        auto nameLen = strlen(name);
        if (name[nameLen - 1] == '/')
        {
            struct uade_file* f = (struct uade_file*)calloc(1, sizeof(struct uade_file));
            f->name = strdup(name);
            f->size = 0;
            f->data = (char*)malloc(0);
            return f;
        }

        auto This = reinterpret_cast<ReplayUADE*>(context);

        auto loadFile = [](const char* name, const char* originalName)
        {
            struct uade_file* f = nullptr;
            auto file = fopen(name, "rb"); // fopen will fallback on stdioemu_fopen
            if (file)
            {
                f = (struct uade_file*)calloc(1, sizeof(struct uade_file));
                f->name = strdup(originalName);

                fseek(file, 0, SEEK_END);
                f->size = ftell(file);
                fseek(file, 0, SEEK_SET);
                f->data = (char*)malloc(f->size);
                fread(f->data, int(f->size), 1, file);
                fclose(file);
            }
            return f;
        };

        auto buffer = This->m_stream->Read();
        auto data = buffer.Items();
        auto dataSize = buffer.Size();
        if (This->m_stream->GetName().ends_with(name))
        {
            for (auto& replayOverride : ms_replayOverrides)
            {
                if (memcmp(data, replayOverride.header, 8) == 0)
                {
                    replayOverride.main(This, data, dataSize);
                    break;
                }
            }

            struct uade_file* f = (struct uade_file*)calloc(1, sizeof(struct uade_file));
            f->name = strdup(name);
            f->size = dataSize;
            f->data = (char*)malloc(dataSize);
            memcpy(f->data, data, dataSize);
            return f;
        }
        else if (_strnicmp(name, "ENV:", 4) == 0 && name[4] != '/' && name[4] != '\\')
        {
            std::string filename = playerdir;
            auto offset = filename.size();
            filename += name;
            filename[offset + 3] = '/';
            return loadFile(filename.c_str(), name);
        }
        else if (_strnicmp(name, "S:", 2) == 0 && name[2] != '/' && name[2] != '\\')
        {
            std::string filename = playerdir;
            auto offset = filename.size();
            filename += name;
            filename[offset + 1] = '/';
            return loadFile(filename.c_str(), name);
        }

        // rePlayer extra formats
        for (auto& replayOverride : ms_replayOverrides)
        {
            if (memcmp(data, replayOverride.header, 8) == 0 && replayOverride.load(This, data, name, dataSize))
            {
                struct uade_file* f = (struct uade_file*)calloc(1, sizeof(struct uade_file));
                f->name = strdup(name);
                f->size = dataSize;
                f->data = (char*)malloc(dataSize);
                memcpy(f->data, data, dataSize);
                return f;
            }
        }

        // load from the current stream
        if (auto newStream = This->m_stream->Open(name))
        {
            struct uade_file* f = (struct uade_file*)calloc(1, sizeof(struct uade_file));
            f->name = strdup(name);
            f->size = newStream->GetSize();
            f->data = (char*)malloc(f->size);
            newStream->Read(f->data, f->size);
            return f;
        }

        // check some extras
        std::string filename = "extras/";
        filename += name;
        if (auto f = loadFile(filename.c_str(), name))
            return f;

        // special cases
        if (strstr(name, "smp.set") && strstr(state->song.info.playername, "(soc)"))
            if (auto f = loadFile("extra/hippel_st.smp.set", name))
                return f;

        Log::Warning("UADE: can't load \"%s\"\n", name);

        // maybe we have to rename the file?
        return nullptr;
    }

    void ReplayUADE::LoadPlayerNames()
    {
        struct uade_config* config = uade_new_config();
        uade_config_set_option(config, UC_BASE_DIR, "");
        uade_state* state = uade_new_state(config);
        free(config);

        // simple hack to load the players: use our state as module
        uade_detection_info detectionInfo = {};
        uade_analyze_eagleplayer(&detectionInfo, state, sizeof(uade_state), nullptr, 0, state);

        ms_globals.players.Reserve(state->playerstore->nplayers);
        ms_globals.players.Add({ 0, 0 });
        ms_globals.strings.Add("Default", sizeof("Default"));
        for (size_t i = 0; i < state->playerstore->nplayers; i++)
        {
            bool isAlreadyAdded = false;
            for (auto& player : ms_globals.players)
            {
                if (isAlreadyAdded = strcmp(ms_globals.strings.Items(player.first), state->playerstore->players[i].playername) == 0)
                    break;
            }
            if (isAlreadyAdded)
                continue;

            // http://www.isthe.com/chongo/tech/comp/fnv/
            constexpr uint32_t fnvOffset = 2166136261ul;
            constexpr uint32_t fnvPrime = 16777619ul;
            uint32_t hash = fnvOffset;
            for (auto* str = state->playerstore->players[i].playername; *str; ++str)
                hash = static_cast<uint32_t>(static_cast<uint64_t>(hash ^ *str) * fnvPrime);

            ms_globals.players.Add({ ms_globals.strings.NumItems(), hash });
            ms_globals.strings.Add(state->playerstore->players[i].playername, strlen(state->playerstore->players[i].playername) + 1);
        }
        std::sort(ms_globals.players.begin() + 1, ms_globals.players.end(), [](auto& l, auto& r)
        {
            return stricmp(ms_globals.strings.Items(l.first), ms_globals.strings.Items(r.first)) < 0;
        });

        uade_cleanup_state(state);
    }

    bool ReplayUADE::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_globals.stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_globals.surround, surround, _countof(surround));
        const char* const filters[] = { "None", "A500", "A1200"};
        changed |= ImGui::Combo("Filter", &ms_globals.filter, filters, _countof(filters));
        const char* const region[] = { "PAL", "NTSC" };
        changed |= ImGui::Combo("Region", &ms_globals.isNtsc, region, _countof(region));
        return changed;
    }

    std::string ReplayUADE::GetFileFilters()
    {
        auto file = fopen("eagleplayer.conf", "rb"); // fopen will fallback on stdioemu_fopen (inside our uade.7z)
        fseek(file, 0, SEEK_END);
        Array<char> conf;
        conf.Resize(ftell(file));
        fseek(file, 0, SEEK_SET);
        fread(conf.Items(), conf.NumItems<int>(), 1, file);
        fclose(file);
        auto buf = conf.Items();
        auto end = buf + conf.NumItems();

        std::string fileFilter;
        while (buf < end)
        {
            if (!fileFilter.empty())
                fileFilter += ",";
            // eagleplayer name
            auto txt = buf;
            while (*txt != 9)
            {
                fileFilter += *txt;
                txt++;
            }
            fileFilter += " (*.";
            buf = strstr(txt + 1, "prefixes=") + sizeof("prefixes=") - 1;
            txt = buf;
            uint32_t count = 0;
            do
            {
                if (*txt == ',')
                {
                    *txt++;
                    if (++count == 5)
                    {
                        fileFilter += ";...";
                        break;
                    }
                    else
                        fileFilter += ";*.";
                }
                while (*txt != ',' && *txt != 9 && *txt != 0xa && *txt != 0xd)
                {
                    fileFilter += *txt;
                    txt++;
                }
            } while (*txt != 9 && *txt != 0xa && *txt != 0xd);
            fileFilter += "){.";
            txt = buf;
            do
            {
                if (*txt == ',')
                {
                    *txt++;
                    fileFilter += ",.";
                }
                while (*txt != ',' && *txt != 9 && *txt != 0xa && *txt != 0xd)
                {
                    fileFilter += *txt;
                    txt++;
                }
            } while (*txt != 9 && *txt != 0xa && *txt != 0xd);
            fileFilter += "}";
            while (*txt != 0xa)
                txt++;
            buf = txt + 1;
        }
        fileFilter += ",TFMX-MOD (*.tfx;*.tfm){.tfx,.tfm}";
        fileFilter += ",YMST (*.ymst){.ymst}";
        return fileFilter;
    }

    void ReplayUADE::Settings::Edit(ReplayMetadataContext& context)
    {
        uint32_t numSubsongs = context.lastSubsongIndex + 1;
        const auto settingsSize = sizeof(Settings) + (numSubsongs + 1u) * sizeof(uint32_t);
        auto* entry = new (_alloca(settingsSize)) Settings(numSubsongs);
        auto* oldEntry = context.metadata.Find<Settings>();
        if (oldEntry)
        {
            if (oldEntry->NumSubsongs() && (oldEntry->NumSubsongs() != numSubsongs))
                oldEntry = nullptr;
            else
            {
                if (oldEntry->overridePlayer)
                    entry->numEntries++;
                memcpy(&entry->value, &oldEntry->value, oldEntry->numEntries * sizeof(uint32_t));
            }
        }

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_globals.stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_globals.surround, "Output: Stereo", "Output: Surround");
        ComboOverride("Filter", GETSET(entry, overrideFilter), GETSET(entry, filter),
            ms_globals.filter, "Filter: None", "Filter: A500", "Filter: A1200");
        ComboOverride("NTSC", GETSET(entry, overrideNtsc), GETSET(entry, ntsc),
            ms_globals.isNtsc, "Region: PAL", "Region: NTSC");

        bool isPlayerOverridden = entry->overridePlayer;
        int32_t playerIndex = 0;
        {
            if (isPlayerOverridden)
            {
                playerIndex = ms_globals.players.FindIf<int32_t>([entry](auto& player)
                {
                    return player.second == entry->data[0];
                });
                if (playerIndex < 0)
                {
                    isPlayerOverridden = false;
                    playerIndex = 0;
                }
            }
            ImGui::PushID("Player");
            if (ImGui::Checkbox("##Checkbox", &isPlayerOverridden))
                playerIndex = 0;
            ImGui::SameLine();
            ImGui::BeginDisabled(!isPlayerOverridden);
            ImGui::SetNextItemWidth(-FLT_MIN);
            struct Label
            {
                static const char* Get(void* data, int idx)
                {
                    auto* This = reinterpret_cast<Label*>(data);
                    This->str = "Player: ";
                    This->str += ms_globals.strings.Items(ms_globals.players[idx].first);
                    return This->str.c_str();
                }
                std::string str;
            } label;
            ImGui::Combo("##Combo", &playerIndex, &Label::Get, &label, ms_globals.players.NumItems());
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Applied only on reload");
            ImGui::EndDisabled();
            ImGui::PopID();
            if (uint32_t(isPlayerOverridden) != entry->overridePlayer)
            {
                memmove(entry->data + (isPlayerOverridden ? 1 : 0), entry->GetDurations(), numSubsongs * sizeof(uint32_t));
                entry->numEntries += isPlayerOverridden ? 1 : -1;
                entry->overridePlayer = isPlayerOverridden;
            }
            if (isPlayerOverridden)
                entry->data[0] = ms_globals.players[playerIndex].second;
        }

        const float buttonSize = ImGui::GetFrameHeight();
        auto* durations = entry->GetDurations();
        bool isZero = true;
        for (uint16_t i = 0; i < entry->NumSubsongs(); i++)
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

            isZero &= durations[i] == 0;
        }
        if (isZero)
        {
            entry->_deprecated1 = 0; // old num subsong entry
            entry->numEntries -= uint16_t(numSubsongs);
        }

        context.metadata.Update(entry, isZero && entry->value == 0);
    }

    ReplayUADE::Globals ReplayUADE::ms_globals = {
        .stereoSeparation = 100,
        .surround = 1,
        .filter = 1,
        .isNtsc = 0
    };

    ReplayUADE::~ReplayUADE()
    {
        uade_cleanup_state(m_uadeState);
        delete[] m_durations;
    }

    ReplayUADE::ReplayUADE(io::Stream* stream, uade_state* uadeState)
        : Replay(eExtension::Unknown, eReplay::UADE)
        , m_stream(stream)
        , m_uadeState(uadeState)
        , m_surround(kSampleRate)
    {}

    uint32_t ReplayUADE::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_uadeState->hasended)
        {
            m_uadeState->hasended = 0;
            return 0;
        }
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
        auto rc = uade_read(buf, numSamples * sizeof(int16_t) * 2, m_uadeState);
        if (rc > 0)
        {
            numSamples = uint32_t(rc / sizeof(int16_t) / 2);
            output->Convert(m_surround, buf, numSamples, m_stereoSeparation);
        }
        else
        {
            m_uadeState->hasended = 0;
            numSamples = 0;
        }

        return numSamples;
    }

    void ReplayUADE::ResetPlayback()
    {
        auto uadeInfo = uade_get_song_info(m_uadeState);
        // check if we have to do the job
        if (uadeInfo->subsongbytes != 0 || m_lastSubsongIndex != m_subsongIndex)
        {
            auto subsongIndex = m_subsongIndex + (m_packagedSubsongNames.IsEmpty() ? uadeInfo->subsongs.min : 0);
            uade_stop(m_uadeState);
            auto buffer = m_stream->Read();
            auto filepath = std::filesystem::path(m_stream->GetName());
            auto filename = filepath.filename().string();

            // rePlayer extra formats
            auto data = buffer.Items();
            auto dataSize = buffer.Size();
            for (auto& replayOverride : ms_replayOverrides)
            {
                if (memcmp(data, replayOverride.header, 8) == 0)
                {
                    replayOverride.build(this, data, filepath, dataSize);
                    filename = filepath.filename().string();
                    break;
                }
            }

            uade_play_from_buffer(filename.c_str(), data, dataSize, subsongIndex, m_uadeState);
            m_lastSubsongIndex = m_subsongIndex;
        }
        m_surround.Reset();
        m_currentPosition = 0;
        m_currentDuration = (uint64_t(m_durations[m_subsongIndex]) * kSampleRate) / 1000;
    }

    void ReplayUADE::ApplySettings(const CommandBuffer metadata)
    {
        auto* globals = static_cast<Globals*>(g_replayPlugin.globals);
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : globals->stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : globals->surround);

        auto filter = (settings && settings->overrideFilter) ? settings->filter : globals->filter;
        if (filter == 0)
            m_uadeState->config.no_filter = 1;
        else
        {
            m_uadeState->config.no_filter = 0;
            m_uadeState->config.filter_type = char(filter);
        }
        uade_set_filter_state(m_uadeState, 0);
        uadecore_set_automatic_song_end(1);
        uadecore_set_ntsc((settings && settings->overrideNtsc) ? settings->ntsc : globals->isNtsc);

        if (settings && settings->NumSubsongs())
        {
            auto durations = settings->GetDurations();
            for (uint32_t i = 0, e = settings->NumSubsongs(); i < e; i++)
                m_durations[i] = durations[i];
        }
        else
        {
            for (uint32_t i = 0, e = GetNumSubsongs(); i < e; i++)
                m_durations[i] = 0;
        }
        m_currentDuration = (uint64_t(m_durations[m_subsongIndex]) * kSampleRate) / 1000;
    }

    void ReplayUADE::SetSubsong(uint16_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    void ReplayUADE::BuildMediaType()
    {
        if (m_mediaType.ext == eExtension::Unknown)
        {
            auto uadeInfo = uade_get_song_info(m_uadeState);
            MediaType mediaType(uadeInfo->detectioninfo.ext, eReplay::UADE);
            for (size_t i = 0; mediaType.ext == eExtension::Unknown && i < uadeInfo->detectioninfo.ep->nextensions; i++)
                mediaType = { uadeInfo->detectioninfo.ep->extensions[i], eReplay::UADE };
            if (mediaType.ext == eExtension::Unknown)
            {
                static const char* const extensionNames[] = { "emsv6", "TFMX7V", "TFMX1.5"};
                static eExtension extensions[] = { eExtension::_ems, eExtension::_mdat, eExtension::_mdat };
                for (auto& extensionName : extensionNames)
                {
                    if (_stricmp(uadeInfo->detectioninfo.ext, extensionName) == 0)
                    {
                        mediaType.ext = extensions[&extensionName - extensionNames];
                        break;
                    }
                }
            }
            else if (mediaType.ext == eExtension::_ym)
                mediaType.ext = eExtension::_ymst;
            else if (mediaType.ext == eExtension::_mdat && _stricmp(uadeInfo->playername, "TFMX ST") == 0)
                mediaType.ext = eExtension::_mdst;
            m_mediaType = mediaType;
        }
    }

    uint32_t ReplayUADE::GetDurationMs() const
    {
        return m_durations[m_subsongIndex];
    }

    uint32_t ReplayUADE::GetNumSubsongs() const
    {
        if (m_packagedSubsongNames.IsNotEmpty())
            return m_packagedSubsongNames.NumItems();
        auto uadeInfo = uade_get_song_info(m_uadeState);
        auto songMin = Clamp(uadeInfo->subsongs.min, 0, 255);
        auto songMax = Clamp(uadeInfo->subsongs.max, 0, 255);
        return uint32_t(songMax >= songMin ? songMax - songMin + 1 : 1);
    }

    std::string ReplayUADE::GetSubsongTitle() const
    {
        if (m_packagedSubsongNames.IsEmpty())
            return {};
        return m_packagedSubsongNames[m_subsongIndex];
    }

    std::string ReplayUADE::GetExtraInfo() const
    {
        std::string metadata;
        return metadata;
    }

    std::string ReplayUADE::GetInfo() const
    {
        std::string info;
        auto uadeInfo = uade_get_song_info(m_uadeState);
        info = "4 channels\n";
        info += uadeInfo->playername[0] ? uadeInfo->playername : uadeInfo->detectioninfo.custom ? "Custom" : uadeInfo->detectioninfo.ep->playername;
        info += " (";
        info += uadeInfo->detectioninfo.ext;
        info += ")\nUADE 3.0.3";
        return info;
    }

    void ReplayUADE::BuildDurations(CommandBuffer metadata)
    {
        auto uadeInfo = uade_get_song_info(m_uadeState);
        auto numSongs = m_packagedSubsongNames.IsEmpty() ? uadeInfo->subsongs.max >= uadeInfo->subsongs.min ? uint32_t(uadeInfo->subsongs.max - uadeInfo->subsongs.min + 1) : 1 : m_packagedSubsongNames.NumItems();
        m_durations = new uint32_t[numSongs];
        auto settings = metadata.Find<Settings>();
        if (settings && settings->NumSubsongs() == numSongs)
        {
            auto* durations = settings->GetDurations();
            for (uint32_t i = 0; i < numSongs; i++)
                m_durations[i] = durations[i];
            m_currentDuration = (uint64_t(durations[m_subsongIndex]) * kSampleRate) / 1000;
        }
        else
        {
            for (uint16_t i = 0; i < numSongs; i++)
                m_durations[i] = 0;
            m_currentDuration = 0;
        }
    }
}
// namespace rePlayer
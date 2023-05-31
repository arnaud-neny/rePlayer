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
#include <dllloader.h>

#include <Audio/AudioTypes.inl.h>
#include <Core/Log.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <IO/File.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::UADE,
        .name = "Unix Amiga Delitracker Emulator",
        .about = "UADE 3.0.2\nHeikki Orsila & Michael Doering",
        .settings = "Unix Amiga Delitracker Emulator 3.0.2",
        .init = ReplayUADE::Init,
        .release = ReplayUADE::Release,
        .load = ReplayUADE::Load,
        .displaySettings = ReplayUADE::DisplaySettings,
        .getFileFilter = ReplayUADE::GetFileFilters,
        .editMetadata = ReplayUADE::Settings::Edit
    };

    static DllManager* s_dllManager = nullptr;
    static bool s_isMainModule = false;
    struct DllEntry
    {
        std::wstring path;
        HMODULE handle;
        ReplayUADE* replay;
    };
    static Array<DllEntry> s_dlls;
    SharedContexts* s_sharedContexts = nullptr;

    typedef ReplayPlugin* (*GetReplayPlugin)();

    bool ReplayUADE::Init(SharedContexts* ctx, Window& window)
    {
        s_sharedContexts = ctx;
        ctx->Init();

        if (&window != nullptr)
        {
            window.RegisterSerializedData(ms_stereoSeparation, "ReplayUADEStereoSeparation");
            window.RegisterSerializedData(ms_surround, "ReplayUADESurround");
            window.RegisterSerializedData(ms_filter, "ReplayUADEFilter");
            window.RegisterSerializedData(ms_isNtsc, "ReplayUADENtsc");

            s_isMainModule = true;
            s_dlls.Reserve(8);

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
        }

        return false;
    }

    void ReplayUADE::Release()
    {
        for (auto& dllEntry : s_dlls)
        {
            ::FreeLibrary(dllEntry.handle);
            s_dllManager->UnsetDllFile(dllEntry.path.c_str());
        }
        s_dlls = {};
        delete s_dllManager;
        delete[] g_replayPlugin.extensions;
    }

    Replay* ReplayUADE::Load(io::Stream* stream, CommandBuffer metadata)
    {
        if (stream->Read().IsEmpty())
            return nullptr;

        if (s_isMainModule)
        {
            // UADE is a hard mess... to make it "thread safe" without rewriting everything, we duplicate the dll per song so all the data are in the module memory space.
            // Thanx to dll manager for that.
            // We still have to share some data from the main dll (replayUADE.dll) with the others such as the SharedContext, config params...

            // Load the main dll in memory
            char* pgrPath;
            _get_pgmptr(&pgrPath);
            auto mainPath = std::filesystem::path(pgrPath).remove_filename() / "replays/UADE.dll";
            mainPath = mainPath.lexically_normal(); // important for the dll loader

            Array<uint8_t> b;
            {
                auto f = io::File::OpenForRead(mainPath.c_str());
                b.Resize(f.GetSize());
                f.Read(b.Items(), b.Size());
            }

            // Create a unique name for it
            static uint32_t counter = 0;
            char dllName[32];
            sprintf(dllName, "UADE%08X.dll", counter++);

            // and load it through the dll manager (created only on the first load)
            auto freeIndex = s_dlls.NumItems();
            if (s_dllManager == nullptr)
            {
                s_dllManager = new DllManager();
                s_dllManager->EnableDllRedirection();
            }
            else for (uint32_t i = 0; i < s_dlls.NumItems(); i++)
            {
                auto& dllEntry = s_dlls[i];
                if (dllEntry.handle != nullptr)
                {
                    if (dllEntry.replay == nullptr)
                    {
                        ::FreeLibrary(dllEntry.handle);
                        s_dllManager->UnsetDllFile(dllEntry.path.c_str());
                        dllEntry.handle = nullptr;
                        freeIndex = i;
                    }
                }
                else
                    freeIndex = i;
            }

            mainPath.replace_filename(dllName);
            s_dllManager->SetDllFile(mainPath.c_str(), b.Items(), b.Size());

            auto dllHandle = s_dllManager->LoadLibrary(mainPath.c_str());
            ReplayUADE* replay = nullptr;
            if (dllHandle != 0)
            {
                // load the song though the new module
                auto g = reinterpret_cast<GetReplayPlugin>(GetProcAddress(dllHandle, "getReplayPlugin"));
                Window* w = nullptr;
                g()->init(s_sharedContexts, reinterpret_cast<Window&>(*w));
                replay = reinterpret_cast<ReplayUADE*>(g()->load(stream, metadata));
                if (replay)
                {
                    replay->SetSettings(ms_stereoSeparation, ms_surround, ms_filter, ms_isNtsc);
                    replay->m_dllIndex = freeIndex;
                    replay->m_dllEntries = &s_dlls;
                    if (freeIndex == s_dlls.NumItems())
                        s_dlls.Add({ std::move(mainPath), dllHandle, replay });
                    else
                        s_dlls[freeIndex] = { std::move(mainPath), dllHandle, replay };
                }
                else
                {
                    ::FreeLibrary(dllHandle);
                    s_dllManager->UnsetDllFile(mainPath.c_str());
                }
            }
            else
            {
                auto s = s_dllManager->GetLastError();
                s.clear();
            }

            return replay;
        }

        struct uade_config* config = uade_new_config();
        uade_config_set_option(config, UC_ONE_SUBSONG, NULL);
        uade_config_set_option(config, UC_IGNORE_PLAYER_CHECK, NULL);
        uade_config_set_option(config, UC_FREQUENCY, "48000");
        uade_config_set_option(config, UC_NO_PANNING, NULL);
        uade_config_set_option(config, UC_SUBSONG_TIMEOUT_VALUE, "-1");
        uade_config_set_option(config, UC_SILENCE_TIMEOUT_VALUE, "-1");

        uade_config_set_option(config, UC_BASE_DIR, "");
        uade_state* state = uade_new_state(config);
        free(config);
        if (state == nullptr)
            return nullptr;

        auto replay = new ReplayUADE(stream, state);
        uade_set_amiga_loader(ReplayUADE::AmigaLoader, replay, state);

        auto buffer = stream->Read();
        auto filepath = std::filesystem::path(stream->GetName());
        auto filename = filepath.filename().string();

        // rePlayer extra formats
        auto data = buffer.Items();
        auto dataSize = buffer.Size();
        for (auto& replayOverride : ms_replayOverrides)
        {
            if (memcmp(data, replayOverride.header, 8) == 0)
            {
                replay->m_mediaType.ext = replayOverride.build(data, filepath, dataSize);
                filename = filepath.filename().string();
                break;
            }
        }

        if (uade_play_from_buffer(filename.c_str(), data, dataSize, -1, state) == 1)
        {
            replay->BuildMediaType();
            replay->SetupMetadata(metadata);
            return replay;
        }

        delete replay;

        return nullptr;
    }

    struct uade_file* ReplayUADE::AmigaLoader(const char* name, const char* playerdir, void* context, struct uade_state* /*state*/)
    {
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
        auto originalPath = std::filesystem::path(This->m_stream->GetName());
        if (originalPath.filename().string() == name)
        {
            for (auto& replayOverride : ms_replayOverrides)
            {
                if (memcmp(data, replayOverride.header, 8) == 0)
                {
                    dataSize = *buffer.Items<const uint32_t>(8) - replayOverride.headerSize;
                    data += replayOverride.headerSize;
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
            if (memcmp(data, replayOverride.header, 8) == 0 && replayOverride.load(data, name, dataSize))
            {
                struct uade_file* f = (struct uade_file*)calloc(1, sizeof(struct uade_file));
                f->name = strdup(name);
                f->size = dataSize;
                f->data = (char*)malloc(dataSize);
                memcpy(f->data, data, dataSize);
                return f;
            }
        }

        originalPath.replace_filename(name);
        if (auto f = loadFile(originalPath.string().c_str(), name))
            return f;

        // check some extras
        std::string filename = "extras/";
        filename += name;
        if (auto f = loadFile(filename.c_str(), name))
            return f;

        Log::Warning("UADE: can't load \"%s\"\n", name);

        // maybe we have to rename the file?
        return nullptr;
    }

    bool ReplayUADE::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, _countof(surround));
        const char* const filters[] = { "None", "A500", "A1200"};
        changed |= ImGui::Combo("Filter", &ms_filter, filters, _countof(filters));
        const char* const region[] = { "PAL", "NTSC" };
        changed |= ImGui::Combo("Region", &ms_isNtsc, region, _countof(region));
        if (changed)
        {
            for (auto& dllEntry : s_dlls)
            {
                if (dllEntry.replay)
                    dllEntry.replay->SetSettings(ms_stereoSeparation, ms_surround, ms_filter, ms_isNtsc);
            }
        }
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
        auto* entry = context.metadata.Find<Settings>();
        if (entry == nullptr)
        {
            // ok, we are here because we never played this song in this player
            entry = context.metadata.Create<Settings>();
        }

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");
        ComboOverride("Filter", GETSET(entry, overrideFilter), GETSET(entry, filter),
            ms_filter, "Filter: None", "Filter: A500", "Filter: A1200");
        ComboOverride("NTSC", GETSET(entry, overrideNtsc), GETSET(entry, ntsc),
            ms_isNtsc, "Region: PAL", "Region: NTSC");
        const float buttonSize = ImGui::GetFrameHeight();
        auto durations = entry->GetDurations();
        for (uint16_t i = 0; i < entry->numSongs; i++)
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
                context.songIndex = i;
                context.isSongEndEditorEnabled = true;
            }
            else if (context.isSongEndEditorEnabled == false && context.duration != 0 && context.songIndex == i)
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

    int32_t ReplayUADE::ms_stereoSeparation = 100;
    int32_t ReplayUADE::ms_surround = 1;
    int32_t ReplayUADE::ms_filter = 1;
    int32_t ReplayUADE::ms_isNtsc = 0;

    ReplayUADE::~ReplayUADE()
    {
        uade_cleanup_state(m_uadeState);
        delete[] m_durations;
        if (m_dllEntries)
            (*m_dllEntries)[m_dllIndex].replay = nullptr;
    }

    ReplayUADE::ReplayUADE(io::Stream* stream, uade_state* uadeState)
        : Replay(eExtension::Unknown, eReplay::UADE)
        , m_stream(stream)
        , m_uadeState(uadeState)
        , m_surround(kSampleRate)
    {}

    void ReplayUADE::SetSettings(int32_t stereoSeparation, int32_t surround, int32_t filter, int32_t isNtsc)
    {
        ms_stereoSeparation = stereoSeparation;
        ms_surround = surround;
        ms_filter = filter;
        ms_isNtsc = isNtsc;
    }

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
            auto subsongIndex = uadeInfo->subsongs.min + m_subsongIndex;
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
                    replayOverride.build(data, filepath, dataSize);
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
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);

        auto filter = (settings && settings->overrideFilter) ? settings->filter : ms_filter;
        if (filter == 0)
            m_uadeState->config.no_filter = 1;
        else
        {
            m_uadeState->config.no_filter = 0;
            m_uadeState->config.filter_type = char(filter);
        }
        uade_set_filter_state(m_uadeState, 0);
        uadecore_set_automatic_song_end(1);
        uadecore_set_ntsc((settings && settings->overrideNtsc) ? settings->ntsc : ms_isNtsc);

        if (settings)
        {
            auto durations = settings->GetDurations();
            for (uint16_t i = 0; i < settings->numSongs; i++)
                m_durations[i] = durations[i];
            m_currentDuration = (uint64_t(m_durations[m_subsongIndex]) * kSampleRate) / 1000;
        }
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
            m_mediaType = mediaType;
        }
    }

    uint32_t ReplayUADE::GetDurationMs() const
    {
        return m_durations[m_subsongIndex] == 0 ? kDefaultSongDuration : m_durations[m_subsongIndex];
    }

    uint32_t ReplayUADE::GetNumSubsongs() const
    {
        auto uadeInfo = uade_get_song_info(m_uadeState);
        return uint32_t(uadeInfo->subsongs.max - uadeInfo->subsongs.min + 1);
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
        info += ")\nUADE 3.0.2";
        return info;
    }

    void ReplayUADE::SetupMetadata(CommandBuffer metadata)
    {
        auto uadeInfo = uade_get_song_info(m_uadeState);
        auto numSongs = uadeInfo->subsongs.max >= uadeInfo->subsongs.min ? uint32_t(uadeInfo->subsongs.max - uadeInfo->subsongs.min + 1) : 1;
        m_durations = new uint32_t[numSongs];
        auto settings = metadata.Find<Settings>();
        if (settings && settings->numSongs == numSongs)
        {
            auto durations = settings->GetDurations();
            for (uint32_t i = 0; i < settings->numSongs; i++)
                m_durations[i] = durations[i];
            m_currentDuration = (uint64_t(durations[m_subsongIndex]) * kSampleRate) / 1000;
        }
        else
        {
            auto value = settings ? settings->value : 0;
            settings = metadata.Create<Settings>(sizeof(Settings) + numSongs * sizeof(int32_t));
            settings->value = value;
            settings->numSongs = numSongs;
            auto durations = settings->GetDurations();
            for (uint16_t i = 0; i < numSongs; i++)
            {
                durations[i] = 0;
                m_durations[i] = 0;
            }
            m_currentDuration = 0;
        }
    }
}
// namespace rePlayer
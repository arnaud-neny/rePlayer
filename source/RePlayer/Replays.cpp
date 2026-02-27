#include "Replays.h"
#include "Settings.h"

#include <Core/Log.h>
#include <Core/String.h>
#include <Helpers/CommandBuffer.h>
#include <ImGui.h>
#include <IO/Stream.h>
#include <IO/StreamFile.h>
#include <RePlayer/Core.h>
#include <Replayer/Version.h>
#include <Replays/Replay.h>
#include <Replays/ReplayPlugin.h>

#include <dllloader.h>

#include <filesystem>

#include <windows.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin =  { .replayId = eReplay::Unknown };

    int16_t Replays::ms_priorities[uint16_t(eReplay::Count)] = {
        0x7fFF,
        #define REPLAY(a, b) b,
        #include "../../Replays/replays.inc"
        #undef REPLAY
    };

    typedef ReplayPlugin* (*GetReplayPlugin)();

    Replays::Replays()
        : m_plugins{ &g_replayPlugin, nullptr }
        , m_dllManager(new DllManager())
    {
        LoadPlugins();

        m_dllManager->EnableDllRedirection();

        // build the list of sorted extension and mapping tables
        for (int32_t i = 0; i < int32_t(eExtension::Count); i++)
            MediaType::sortedExtensions[i] = eExtension(i);
        std::sort(MediaType::sortedExtensions + 1, MediaType::sortedExtensions + int32_t(eExtension::Count), [this](eExtension l, eExtension r)
        {
            return strcmp(MediaType::extensionNames[int32_t(l)], MediaType::extensionNames[int32_t(r)]) < 0;
        });
        for (int32_t i = 0; i < int32_t(eExtension::Count); i++)
        {
            MediaType::mapSortedExtensions[int32_t(MediaType::sortedExtensions[i])] = i;
            MediaType::sortedExtensionNames[i] = MediaType::extensionNames[int32_t(MediaType::sortedExtensions[i])];
        }
        MediaType::sortedExtensionNames[0] = "";
    }

    Replays::~Replays()
    {
        FlushDlls();
        assert(m_dlls.IsEmpty());

        delete m_dllManager;

        if (m_fileFilters)
            core::Free(m_fileFilters->data);

        for (auto* plugin : m_plugins)
        {
            if (plugin)
            {
                plugin->release();
                free(plugin->dllName);
            }
        }
        //we should free all the plugin libraries here, but it's crashing after (in the ucrt trying to call an unloaded function)
    }

    Replay* Replays::Load(io::Stream* stream, CommandBuffer metadata, MediaType type)
    {
        // try the remap
        if (type.replay == eReplay::Unknown)
            type.replay = m_extensionToReplay[int(type.ext)];

        // default load
        if (auto plugin = m_plugins[int32_t(type.replay)])
        {
            stream->Rewind();
            if (auto replay = Load(plugin, stream, metadata))
                return replay;
        }

        // failed the default loader, then try to load using extension first
        ReplayPlugin* plugins[uint16_t(eReplay::Count)];
        memcpy(plugins, m_sortedPlugins, sizeof(m_sortedPlugins));
        auto baseReplayIndex = m_replayToIndex[int32_t(type.replay)];
        plugins[baseReplayIndex] = nullptr;

        if (type.ext != eExtension::Unknown)
        {
            auto currentExt = MediaType::extensionNames[int32_t(type.ext)];
            for (int16_t i = 0; i < uint16_t(eReplay::Count); i++)
            {
                auto replayIndex = (i + baseReplayIndex) % uint16_t(eReplay::Count);
                if (auto plugin = plugins[replayIndex])
                {
                    for (auto extensions = plugin->extensions;;)
                    {
                        auto nextExt = extensions + 1;
                        while (*nextExt != 0 && *nextExt != ';')
                            ++nextExt;
                        if (_strnicmp(extensions, currentExt, nextExt - extensions) == 0)
                        {
                            stream->Rewind();
                            if (auto replay = Load(plugin, stream, metadata))
                                return replay;
                            plugins[replayIndex] = nullptr;
                            break;
                        }
                        if (*nextExt)
                            extensions = nextExt + 1;
                        else
                            break;
                    }
                }
            }
        }

        // try to load with the remaining plugins
        for (int16_t i = 0; i < uint16_t(eReplay::Count); i++)
        {
            auto replayIndex = (i + baseReplayIndex) % uint16_t(eReplay::Count);
            if (auto plugin = plugins[replayIndex])
            {
                stream->Rewind();
                if (auto replay = Load(plugin, stream, metadata))
                    return replay;
            }
        }
        return nullptr;
    }

    Replayables Replays::Enumerate(io::Stream* stream)
    {
        Replayables replays;
        uint32_t numReplays = 0;
        Array<CommandBuffer::Command> commands;
        for (int16_t i = 0; i < uint16_t(eReplay::Count); i++)
        {
            if (auto plugin = m_plugins[i])
            {
                stream->Rewind();
                if (auto replay = Load(plugin, stream, commands))
                {
                    replays[numReplays++] = plugin->replayId;
                    delete replay;
                }
            }
        }
        replays[numReplays++] = eReplay::Unknown;
        return replays;
    }

    Replayables Replays::Enumerate(io::Stream* stream, MediaType type)
    {
        if (type.ext == eExtension::Unknown)
            return Enumerate(stream);

        ReplayPlugin* plugins[uint16_t(eReplay::Count)];
        memcpy(plugins, m_sortedPlugins, sizeof(m_sortedPlugins));
        auto baseReplayIndex = m_replayToIndex[int32_t(type.replay)];

        Replayables replays;
        uint32_t numReplays = 0;
        Array<CommandBuffer::Command> commands;

        // load by extension
        auto currentExt = MediaType::extensionNames[int32_t(type.ext)];
        for (int16_t i = 0; i < uint16_t(eReplay::Count); i++)
        {
            auto replayIndex = (i + baseReplayIndex) % uint16_t(eReplay::Count);
            if (auto plugin = plugins[replayIndex])
            {
                for (auto extensions = plugin->extensions;;)
                {
                    auto nextExt = extensions + 1;
                    while (*nextExt != 0 && *nextExt != ';')
                        ++nextExt;
                    if (_strnicmp(extensions, currentExt, nextExt - extensions) == 0)
                    {
                        stream->Rewind();
                        if (auto replay = Load(plugin, stream, commands))
                        {
                            replays[numReplays++] = plugin->replayId;
                            delete replay;
                        }
                        plugins[replayIndex] = nullptr;
                        break;
                    }
                    if (*nextExt)
                        extensions = nextExt + 1;
                    else
                        break;
                }
            }
        }

        // try to load with the remaining plugins
        for (int16_t i = 0; i < uint16_t(eReplay::Count); i++)
        {
            if (auto plugin = plugins[i])
            {
                stream->Rewind();
                if (auto replay = Load(plugin, stream, commands))
                {
                    replays[numReplays++] = plugin->replayId;
                    delete replay;
                }
            }
        }

        replays[numReplays++] = eReplay::Unknown;
        return replays;
    }

    MediaType Replays::Find(const char* extension) const
    {
        MediaType type(extension, eReplay::Unknown);
        // force the replay?
        type.replay = m_extensionToReplay[int32_t(type.ext)];
        if (extension != nullptr && type.replay == eReplay::Unknown)
        {
            // look for the first plugin allowing this extension
            auto length = strlen(extension);
            for (auto* plugin : m_sortedPlugins)
            {
                for (auto exts = plugin->extensions; *exts; exts++)
                {
                    auto ext = exts;
                    while (*exts && *exts != ';')
                        ++exts;

                    if (size_t(exts - ext) == length && _strnicmp(ext, extension, length) == 0)
                        return { type.ext, plugin->replayId };
                }
            }
        }
        return type;
    }

    bool Replays::DisplaySettings() const
    {
        bool changed = false;
        if (ImGui::CollapsingHeader("Replays", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
            ImGui::Combo("##Extension", &m_selectedExtension, MediaType::sortedExtensionNames + 1, int32_t(eExtension::Count) - 1);
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("extension vs replay");
            ImGui::SameLine();
            m_sortedReplayNames[0] = "- Default -";
            int32_t selectedReplay = m_mapSortedReplays[int32_t(m_extensionToReplay[int32_t(MediaType::sortedExtensions[m_selectedExtension + 1])])];
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::Combo("##Replay", &selectedReplay, m_sortedReplayNames, int32_t(eReplay::Count));
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("extension vs replay");
            m_extensionToReplay[int32_t(MediaType::sortedExtensions[m_selectedExtension + 1])] = m_sortedReplays[selectedReplay];
            m_sortedReplayNames[0] = "";

            struct
            {
                static const char* getter(void* data, int32_t index) { return reinterpret_cast<Replays*>(data)->m_settingsPlugins[index]->settings; }
            } cb;
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::Combo("##Replays", &m_selectedSettings, cb.getter, (void*)this, m_numSettings);
            changed = m_settingsPlugins[m_selectedSettings]->displaySettings();
        }
        return changed;
    }

    void Replays::DisplayAbout() const
    {
        for (uint16_t i = 1; i < uint16_t(eReplay::Count); i++)
        {
            if (auto about = m_plugins[i]->about)
            {
                ImGui::Bullet();
                ImGui::TextUnformatted(about);
            }
        }
    }

    bool Replays::LoadSettings(const char* line)
    {
        uint32_t ext = 0;
        uint32_t replay = 0;
        if (sscanf_s(line, "ExtensionVsReplay%u=%u", &ext, &replay) == 2)
        {
            if (ext > 0 && ext < uint32_t(eExtension::Count) && replay < uint32_t(eReplay::Count))
                m_extensionToReplay[ext] = eReplay(replay);
            return true;
        }
        return false;
    }

    void Replays::SaveSettings(ImGuiTextBuffer* buf)
    {
        for (uint32_t i = 1; i < uint32_t(eExtension::Count); i++)
            buf->appendf("ExtensionVsReplay%u=%u\n", i, uint32_t(m_extensionToReplay[i]));
    }

    const Replays::FileFilters& Replays::GetFileFilters() const
    {
        if (m_fileFilters == nullptr)
            const_cast<Replays*>(this)->BuildFileFilters();
        return *m_fileFilters;
    }

    void Replays::EditMetadata(eReplay replayId, ReplayMetadataContext& context) const
    {
        if (auto plugin = m_plugins[int32_t(replayId)])
            plugin->editMetadata(context);
    }

    const char* Replays::GetName(eReplay replay) const
    {
        if (auto plugin = m_plugins[int(replay)])
            return plugin->name;
        return "!!! Missing Plugin !!!";
    }

    void Replays::SetSelectedSettings(MediaType type)
    {
        if (type.replay != eReplay::Unknown)
        {
            for (int32_t selectedSettings = 0, e = m_numSettings; selectedSettings < e; selectedSettings++)
            {
                if (m_settingsPlugins[selectedSettings]->replayId == type.replay)
                {
                    m_selectedSettings = selectedSettings;
                    break;
                }
            }
        }
        if (type.ext != eExtension::Unknown)
            m_selectedExtension = MediaType::mapSortedExtensions[int32_t(type.ext)] - 1;
    }

    void Replays::LoadPlugins()
    {
        char* pgrPath;
        _get_pgmptr(&pgrPath);
        auto mainPath = std::filesystem::path(pgrPath).remove_filename() / "replays" REPLAYER_OS_STUB "/";

        for (const std::filesystem::directory_entry& dirEntry : std::filesystem::directory_iterator(mainPath))
        {
            if (dirEntry.path().extension() == ".dll")
            {
                auto hModule = LoadLibrary(LPCSTR(dirEntry.path().u8string().c_str()));
                if (!hModule)
                {
                    Log::Error("Replay: can't load \"%s\"\n", LPCSTR(dirEntry.path().filename().u8string().c_str()));
                    continue;
                }
                auto getReplayPlugin = reinterpret_cast<GetReplayPlugin>(GetProcAddress(hModule, "getReplayPlugin"));
                if (auto replayPlugin = getReplayPlugin ? getReplayPlugin() : nullptr)
                {
                    replayPlugin->dllName = _strdup(LPCSTR(dirEntry.path().stem().u8string().c_str()));
                    replayPlugin->download = [](const char* url)
                    {
                        return Core::Download("Replay", url);
                    };
                    replayPlugin->addJob = [](Replay* replay, void (*cb)(Replay*))
                    {
                        Core::AddJob([replay, cb]()
                        {
                            cb(replay);
                        });
                    };

                    if (m_plugins[int32_t(replayPlugin->replayId)] == nullptr)
                    {
                        replayPlugin->init(SharedContexts::ms_instance, Core::GetSettings());
                        m_plugins[int32_t(replayPlugin->replayId)] = replayPlugin;
                    }
                    else
                    {
                        Log::Error("Replay: can't register \"%s\", slot \"%s\" is already used\n", LPCSTR(dirEntry.path().filename().u8string().c_str()), MediaType(eExtension::Unknown, replayPlugin->replayId).GetReplay());
                        FreeLibrary(hModule);
                    }
                }
                else
                {
                    Log::Error("Replay: \"%s\" is not valid\n", LPCSTR(dirEntry.path().filename().u8string().c_str()));
                    FreeLibrary(hModule);
                }
            }
        }

        int16_t indices[uint16_t(eReplay::Count)];
        for (int16_t i = 0; i < uint16_t(eReplay::Count); i++)
            indices[i] = i;
        std::sort(indices + 1, indices + uint16_t(eReplay::Count), [](auto l, auto r)
        {
            return ms_priorities[l] > ms_priorities[r];
        });
        for (int16_t i = 0; i < uint16_t(eReplay::Count); i++)
        {
            m_replayToIndex[indices[i]] = uint8_t(i);
            m_sortedPlugins[i] = m_plugins[indices[i]];
            if (m_plugins[i] && m_plugins[i]->settings)
            {
                m_settingsPlugins[m_numSettings] = m_plugins[i];
                m_numSettings++;
            }
        }
        std::sort(m_settingsPlugins, m_settingsPlugins + m_numSettings, [](auto l, auto r)
        {
            if (l->settings == nullptr && r->settings == nullptr)
                return r < l;
            if (l->settings == nullptr && r->settings != nullptr)
                return false;
            if (l->settings != nullptr && r->settings == nullptr)
                return true;
            return _stricmp(l->settings, r->settings) < 0;
        });
        for (uint16_t i = 0; i < m_numSettings; i++)
        {
            if (m_settingsPlugins[i]->replayId == eReplay::OpenMPT)
            {
                m_selectedSettings = i;
                break;
            }
        }

        // build the list of sorted replays and mapping tables
        for (int32_t i = 0; i < int32_t(eReplay::Count); i++)
            m_sortedReplays[i] = eReplay(i);
        std::sort(m_sortedReplays + 1, m_sortedReplays + int32_t(eReplay::Count), [this](eReplay l, eReplay r)
        {
            const char* lName = "\x7f";
            const char* rName = "\x7f";
            if (m_plugins[int32_t(l)])
                lName = m_plugins[int32_t(l)]->name;
            if (m_plugins[int32_t(r)])
                rName = m_plugins[int32_t(r)]->name;

            return strcmp(lName, rName) < 0;
        });
        for (int32_t i = 0; i < int32_t(eReplay::Count); i++)
        {
            m_mapSortedReplays[int32_t(m_sortedReplays[i])] = i;
            m_sortedReplayNames[i] = m_plugins[int32_t(m_sortedReplays[i])] ? m_plugins[int32_t(m_sortedReplays[i])]->name : "";
        }
        m_sortedReplayNames[0] = "";
    }

    void Replays::BuildFileFilters()
    {
        Array<char> data;
        Array<uint32_t> filters;

        {
            data.Add("All Files (*.*)", sizeof("All Files (*.*)"));
            data.Resize((data.NumItems() + alignof(uint32_t) - 1) & ~(alignof(uint32_t) - 1));
            auto filterOffset = data.Push<uint32_t>(sizeof(FileFilters::Filter));
            filters.Add(filterOffset);
            auto* filter = data.Items<FileFilters::Filter>(filterOffset);
            filter->nameOffset = 0;
            filter->numExtensions = 0;
        }

        for (uint16_t i = 1; i < uint16_t(eReplay::Count) - 1; i++)
        {
            auto buildFileFilter = [&](const char* name, const char* exts)
            {
                auto nameSize = uint32_t(strlen(name));
                auto nameOffset = data.Push<uint32_t>(nameSize);
                memcpy(data.Items(nameOffset), name, nameSize);
                data.Add(" (", 2);

                Array<FileFilters::Extension> extensions;
                for (;;)
                {
                    auto ext = exts;
                    while (*exts && *exts != ';')
                        ++exts;

                    auto* extension = extensions.Push();
                    extension->size = exts - ext;

                    if (extensions.NumItems() < 5)
                    {
                        data.Add("*.", 2);
                        extension->offset = data.NumItems();
                        if (*exts)
                        {
                            data.Add(ext, extension->size + 1);
                            exts++;
                        }
                        else
                        {
                            data.Add(ext, extension->size);
                            data.Add(")", 2);
                            break;
                        }
                    }
                    else
                    {
                        if (extensions.NumItems() == 5)
                            data.Add("...)", sizeof("...)"));
                        extension->offset = data.NumItems();
                        data.Add(ext, extension->size);
                        if (*exts)
                            exts++;
                        else
                            break;
                    }
                }
                data.Resize((data.NumItems() + alignof(uint32_t) - 1) & ~(alignof(uint32_t) - 1));
                auto filterOffset = data.Push<uint32_t>(sizeof(FileFilters::Filter));
                filters.Add(filterOffset);
                auto* filter = data.Items<FileFilters::Filter>(filterOffset);
                filter->nameOffset = nameOffset;
                filter->numExtensions = extensions.NumItems();
                data.Add(extensions.Items<char>(), uint32_t(extensions.Size()));
            };

            if (auto plugin = m_sortedPlugins[i])
            {
                if (plugin->getFileFilter)
                {
                    auto pluginFilters = plugin->getFileFilter();

                    std::string superFilter;
                    for (auto& pluginFilter : pluginFilters)
                    {
                        if (!superFilter.empty())
                            superFilter += ";";
                        superFilter += pluginFilter.second;
                    }
                    buildFileFilter(plugin->name, superFilter.c_str());

                    for (auto& pluginFilter : pluginFilters)
                    {
                        std::string name = plugin->name;
                        name += ": ";
                        name += pluginFilter.first;
                        buildFileFilter(name.c_str(), pluginFilter.second.c_str());
                    }
                }
                else
                {
                    buildFileFilter(plugin->name, plugin->extensions);
                }
            }
        }
        data.Resize((data.NumItems() + alignof(FileFilters) - 1) & ~(alignof(FileFilters) - 1));
        auto fileFiltersOffset = data.Push<uint32_t>(sizeof(FileFilters) - sizeof(uint32_t));
        data.Add(filters.Items<char>(), uint32_t(filters.Size()));
        auto* fileFilters = data.Items<FileFilters>(fileFiltersOffset);
        fileFilters->data = data.Detach();
        fileFilters->numFilters = filters.NumItems();
        m_fileFilters = fileFilters;
    }

    Replay* Replays::Load(ReplayPlugin* plugin, io::Stream* stream, CommandBuffer metadata)
    {
        if (plugin->isThreadSafe)
            return plugin->load(stream, metadata);

        FlushDlls();

        char* pgrPath;
        _get_pgmptr(&pgrPath);
        auto mainPath = std::filesystem::path(pgrPath).remove_filename() / "replays" REPLAYER_OS_STUB / plugin->dllName;
        mainPath += ".dll";
        mainPath = mainPath.lexically_normal(); // important for the dll loader

        auto dllStream = io::StreamFile::Create(mainPath);
        auto dllData = dllStream->Read();

        char dllName[32];
        sprintf(dllName, "%s%04X.dll", plugin->dllName, m_dllIdGenerator++);

        mainPath.replace_filename(dllName);
        m_dllManager->SetDllFile(mainPath.c_str(), dllData.Items(), dllData.Size());

        auto dllHandle = m_dllManager->LoadLibrary(mainPath.c_str());
        if (dllHandle)
        {
            // load the song though the new module
            auto* replayPlugin = reinterpret_cast<GetReplayPlugin>(GetProcAddress(dllHandle, "getReplayPlugin"))();

            replayPlugin->onDelete = [](Replay* replay)
            {
                for (auto& dllEntry : Core::GetReplays().m_dlls)
                {
                    if (dllEntry.replay == replay)
                    {
                        dllEntry.replay = nullptr;
                        break;
                    }
                }
            };

            replayPlugin->globals = plugin->globals;
            Window* w = nullptr;
            replayPlugin->init(SharedContexts::ms_instance, reinterpret_cast<Window&>(*w));
            auto replay = replayPlugin->load(stream, metadata);
            if (replay)
            {
                m_dlls.Add({ mainPath, dllHandle, replay });
                return replay;
            }
            else
            {
                ::FreeLibrary(dllHandle);
                m_dllManager->UnsetDllFile(mainPath.c_str());
            }
        }
        else
        {
            auto s = m_dllManager->GetLastError();
            s.clear();
        }
        return nullptr;
    }

    void Replays::FlushDlls()
    {
        m_dlls.RemoveIf([this](auto& dllEntry)
        {
            if (!dllEntry.replay)
            {
                ::FreeLibrary(HMODULE(dllEntry.handle));
                m_dllManager->UnsetDllFile(dllEntry.path.c_str());
                return true;
            }
            return false;
        });
    }
}
// namespace rePlayer
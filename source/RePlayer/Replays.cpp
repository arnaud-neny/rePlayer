#include "Replays.h"
#include "Settings.h"

#include <Core/Log.h>
#include <Helpers/CommandBuffer.h>
#include <ImGui.h>
#include <IO/Stream.h>
#include <RePlayer/Core.h>
#include <Replays/Replay.h>
#include <Replays/ReplayPlugin.h>

#include <filesystem>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace rePlayer
{
    static ReplayPlugin s_dummyPlugin =  { .replayId = eReplay::Unknown };

    int16_t Replays::ms_priorities[uint16_t(eReplay::Count)] = {
        0x7fFF,
        #define REPLAY(a, b) b,
        #include "../../Replays/replays.inc"
        #undef REPLAY
    };

    typedef ReplayPlugin* (*GetReplayPlugin)();

    Replays::Replays()
        : m_plugins{ &s_dummyPlugin, nullptr }
    {
        LoadPlugins();
        BuildFileFilters();
    }

    Replays::~Replays()
    {
        delete[] m_fileFilters;

        for (auto* plugin : m_plugins)
        {
            if (plugin)
                plugin->release();
        }
        //we should free all the plugin libraries here, but it's crashing after (in the ucrt trying to call an unloaded function)
    }

    Replay* Replays::Load(io::Stream* stream, CommandBuffer metadata, MediaType type) const
    {
        // default load
        if (auto plugin = m_plugins[int32_t(type.replay)])
        {
            stream->Seek(0, io::Stream::kSeekBegin);
            if (auto replay = plugin->load(stream, metadata))
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
                            stream->Seek(0, io::Stream::kSeekBegin);
                            if (auto replay = plugin->load(stream, metadata))
                                return replay;
                            break;
                        }
                        if (*nextExt)
                            extensions = nextExt + 1;
                        else
                            break;
                    }

                    plugins[replayIndex] = nullptr;
                }
            }
        }

        // try to load with the remaining plugins
        for (int16_t i = 0; i < uint16_t(eReplay::Count); i++)
        {
            auto replayIndex = (i + baseReplayIndex) % uint16_t(eReplay::Count);
            if (auto plugin = plugins[replayIndex])
            {
                stream->Seek(0, io::Stream::kSeekBegin);
                if (auto replay = plugin->load(stream, metadata))
                    return replay;
            }
        }
        return nullptr;
    }

    Replayables Replays::Enumerate(io::Stream* stream) const
    {
        Replayables replays;
        uint32_t numReplays = 0;
        Array<CommandBuffer::Command> commands;
        for (int16_t i = 0; i < uint16_t(eReplay::Count); i++)
        {
            if (auto plugin = m_plugins[i])
            {
                stream->Seek(0, io::Stream::kSeekBegin);
                if (auto replay = plugin->load(stream, commands))
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
        if (extension != nullptr)
        {
            for (auto* plugin : m_sortedPlugins)
            {
                for (auto exts = plugin->extensions; *exts; exts++)
                {
                    auto ext = exts;
                    while (*exts && *exts != ';')
                        ++exts;

                    if (_strnicmp(ext, extension, exts - ext) == 0)
                        return { extension, plugin->replayId };
                }
            }
        }
        return MediaType(extension, eReplay::Unknown);
    }

    bool Replays::DisplaySettings() const
    {
        bool changed = false;
        if (ImGui::CollapsingHeader("Replays", ImGuiTreeNodeFlags_DefaultOpen))
        {
            struct
            {
                static bool getter(void* data, int32_t index, const char** outText) { *outText = reinterpret_cast<Replays*>(data)->m_settingsPlugins[index]->settings; return true; }
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

    const char* const Replays::GetFileFilters() const
    {
        return m_fileFilters;
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

    void Replays::SetSelectedSettings(eReplay replay)
    {
        if (replay != eReplay::Unknown)
        {
            for (int32_t selectedSettings = 0; selectedSettings < int32_t(eReplay::Count); selectedSettings++)
            {
                if (m_settingsPlugins[selectedSettings]->replayId == replay)
                {
                    m_selectedSettings = selectedSettings;
                    break;
                }
            }
        }
    }

    void Replays::LoadPlugins()
    {
        char* pgrPath;
        _get_pgmptr(&pgrPath);
        auto mainPath = std::filesystem::path(pgrPath).remove_filename() / "replays/";

        for (const std::filesystem::directory_entry& dirEntry : std::filesystem::directory_iterator(mainPath))
        {
            if (dirEntry.path().extension() == ".dll")
            {
                auto hModule = LoadLibrary(dirEntry.path().string().c_str());
                auto getReplayPlugin = reinterpret_cast<GetReplayPlugin>(GetProcAddress(hModule, "getReplayPlugin"));
                if (auto replayPlugin = getReplayPlugin())
                {
                    if (m_plugins[int32_t(replayPlugin->replayId)] == nullptr)
                    {
                        replayPlugin->init(SharedContexts::ms_instance, Core::GetSettings());
                        m_plugins[int32_t(replayPlugin->replayId)] = replayPlugin;
                    }
                    else
                    {
                        Log::Error("Replay: Can't register \"%s\", slot \"%s\" is already used\n", dirEntry.path().filename().string().c_str(), MediaType(eExtension::Unknown, replayPlugin->replayId).GetReplay());
                        FreeLibrary(hModule);
                    }
                }
                else if (hModule)
                    FreeLibrary(hModule);
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
            m_settingsPlugins[i] = m_plugins[i];
            if (m_plugins[i]->settings)
                m_numSettings++;
        }
        std::sort(m_settingsPlugins, m_settingsPlugins + uint16_t(eReplay::Count), [](auto l, auto r)
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
    }

    void Replays::BuildFileFilters()
    {
        std::string fileFilters;
        uint16_t i = 1;
        for (; i < uint16_t(eReplay::Count) - 1; i++)
        {
            if (auto plugin = m_sortedPlugins[i])
            {
                if (!fileFilters.empty())
                    fileFilters += ',';
                if (plugin->getFileFilter)
                    fileFilters += plugin->getFileFilter();
                else
                {
                    fileFilters += plugin->name;
                    fileFilters += " (";
                    std::string filter = "{";
                    int32_t count = 0;
                    for (auto exts = plugin->extensions;;)
                    {
                        auto ext = exts;
                        while (*exts && *exts != ';')
                            ++exts;

                        if (count < 5)
                        {
                            fileFilters += "*.";
                            fileFilters.insert(fileFilters.end(), ext, exts);
                        }

                        filter += ".";
                        filter.insert(filter.end(), ext, exts);

                        if (*exts++ == 0)
                            break;

                        if (++count >= 5)
                        {
                            if (count == 5)
                                fileFilters += ";...";
                        }
                        else
                            fileFilters += ';';
                        filter += ',';
                    }
                    fileFilters += ')';
                    filter += '}';
                    fileFilters += filter;
                }
            }
        }
        auto fileFiltersCopy = new char[fileFilters.size()];
        memcpy(fileFiltersCopy, fileFilters.c_str(), fileFilters.size());
        m_fileFilters = fileFiltersCopy;
    }
}
// namespace rePlayer
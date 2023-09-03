#pragma once

#include <Helpers/CommandBuffer.h>
#include <Replays/ReplayTypes.h>

#include <string>

class DllManager;

namespace core::io
{
    class Stream;
}
// namespace core::io

namespace rePlayer
{
    using namespace core;

    class Replay;

    struct ReplayMetadataContext;
    struct ReplayPlugin;

    class Replays
    {
    public:
        Replays();
        ~Replays();

        Replay* Load(io::Stream* stream, CommandBuffer metadata, MediaType type);
        Replayables Enumerate(io::Stream* stream);
        MediaType Find(const char* extension) const;

        bool DisplaySettings() const;
        void DisplayAbout() const;

        const char* const GetFileFilters() const;

        void EditMetadata(eReplay replayId, ReplayMetadataContext& context) const;

        const char* GetName(eReplay replay) const;

        void SetSelectedSettings(eReplay replay);

    private:
        struct DllEntry
        {
            std::wstring path;
            void* handle = nullptr;
            Replay* replay = nullptr;
        };

    private:
        void LoadPlugins();
        void BuildFileFilters();
        Replay* Load(ReplayPlugin* plugin, io::Stream* stream, CommandBuffer metadata);
        void FlushDlls();

    private:
        ReplayPlugin* m_plugins[uint16_t(eReplay::Count)];
        ReplayPlugin* m_sortedPlugins[uint16_t(eReplay::Count)];
        ReplayPlugin* m_settingsPlugins[uint16_t(eReplay::Count)];
        uint8_t m_replayToIndex[uint16_t(eReplay::Count)];
        uint16_t m_dllIdGenerator = 0;
        uint16_t m_numSettings = 0;
        mutable int32_t m_selectedSettings = 0;

        const char* m_fileFilters = nullptr;

        DllManager* m_dllManager = nullptr;
        Array<DllEntry> m_dlls;

        static int16_t ms_priorities[uint16_t(eReplay::Count)];
    };
}
// namespace rePlayer
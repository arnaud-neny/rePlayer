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
        class FileFilters
        {
            friend class Replays;

            struct Extension
            {
                uint32_t size : 8;
                uint32_t offset : 24;
            };
            struct Filter
            {
                uint32_t nameOffset;
                uint32_t numExtensions;
                Extension extensions[0];

                const char* Get(const FileFilters& fileFilter, uint32_t index) const { return fileFilter.data + extensions[index].offset; }
            };

        public:
            uint32_t Count() const { return numFilters; }
            const Filter& Get(uint32_t index) const { return *reinterpret_cast<const Filter*>(data + filters[index]); }
            const char* Label(uint32_t index) const { return data + Get(index).nameOffset; }

        private:
            const char* data;
            uint32_t numFilters;
            uint32_t filters[0];
        };

    public:
        Replays();
        ~Replays();

        Replay* Load(io::Stream* stream, CommandBuffer metadata, MediaType type);
        Replayables Enumerate(io::Stream* stream);
        Replayables Enumerate(io::Stream* stream, MediaType type);
        MediaType Find(const char* extension) const;

        bool DisplaySettings() const;
        void DisplayAbout() const;

        const FileFilters& GetFileFilters() const;

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

        FileFilters* m_fileFilters = nullptr;

        DllManager* m_dllManager = nullptr;
        Array<DllEntry> m_dlls;

        static int16_t ms_priorities[uint16_t(eReplay::Count)];
    };
}
// namespace rePlayer
#pragma once

#include <Containers/Array.h>
#include <Database/Types/Song.h>
#include <Library/Library.h>

#include <filesystem>

namespace rePlayer
{
    class Database;

    class Library::FileImport
    {
    public:
        void Scan(Array<std::filesystem::path>&& files);
        FileImport* Display();

    private:
        struct Entry
        {
            static constexpr uint32_t kMaxReplays = 6;
            std::filesystem::path path;
            std::string name;
            uint8_t currentReplay;
            uint8_t isSelected : 1 = false;
            uint8_t isChecked : 1 = false;
            uint8_t isArchive : 1 = false;
            uint8_t replays[kMaxReplays] = { 0 };
        };

    private:
        static MediaType GetMediaType(const std::filesystem::path& path);

    public:
        Array<Entry> m_entries;
        Array<uint32_t> m_sortedEntries;

        int m_lastSelected = -1;
        enum class ArtistMode
        {
            kNoArtist,
            kNewArtist,
            kRootPath,
            kParentPath
        } m_artistMode = ArtistMode::kNoArtist;

        bool m_isScanning = true;
        bool m_isImporting = false;
    };
}
// namespace rePlayer
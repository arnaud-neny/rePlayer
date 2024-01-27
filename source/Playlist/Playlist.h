#pragma once

#include <Containers/Array.h>
#include <Containers/SmartPtr.h>
#include <Core/Window.h>

#include <Database/Types/MusicID.h>
#include <RePlayer/CoreHeader.h>

namespace core::io
{
    class File;
    class Stream;
}
// namespace core::io

namespace rePlayer
{
    using namespace core;

    class Database;
    class DatabaseArtistsUI;
    class DatabaseSongsUI;
    class Player;

    class Playlist : public Window
    {
    public:
        Playlist();
        ~Playlist();

        void Save();

        void Clear();
        void Enqueue(MusicID musicId);
        void Discard(MusicID musicId);
        SmartPtr<core::io::Stream> GetStream(Song* song);
        SmartPtr<core::io::Stream> GetStream(const MusicID musicId);
        SmartPtr<Player> LoadSong(const MusicID musicId);
        void LoadPreviousSong(SmartPtr<Player>& currentPlayer, SmartPtr<Player>& nextPlayer);
        SmartPtr<Player> LoadCurrentSong();
        SmartPtr<Player> LoadNextSong(bool isAdvancing);
        void ValidateNextSong(SmartPtr<Player>& player);

        void Eject() { m_currentEntryIndex = -1; }

        uint32_t NumEntries() const { return m_cue.entries.NumItems(); }
        int32_t GetCurrentEntryIndex() const { return m_currentEntryIndex; }

        MusicID GetCurrentEntry() const;

        void FocusCurrentSong();

        void UpdateDragDropSource(uint8_t dropId);

    private:
        struct Cue
        {
            struct Entry : public MusicID
            {
                bool isSelected = false;

                Entry& operator=(const MusicID& musicId);
                bool operator==(PlaylistID other) const;
                bool IsAvailable() const;
            };

            Database& db;
            Array<char> paths; // paths are indexed in the sourceID of the song
            Array<Entry> entries;
            bool arePathsDirty = false;

            bool IsUrl(SourceID sourceId) const;
            const char* GetPath(SourceID sourceId, bool isUrl) const;
            const char* GetPath(SourceID sourceId) const;
        };

        static constexpr uint64_t kVersion = uint64_t(kMusicFileStamp) | (0ull << 32);
        struct Summary
        {
            uint32_t numSubsongs = 0;
            int32_t currentSong = -1;
            std::string name;

            static constexpr uint64_t kVersion = uint64_t(kMusicFileStamp) | (0ull << 32);

            void Load(io::File& file);
            void Save(io::File& file);
        };

        class DropTarget;
        class SongsUI;

    private:
        std::string OnGetWindowTitle() override;
        void OnDisplay() override;
        void OnEndUpdate() override;

        void MoveSelection(uint32_t draggedEntryIndex);

        void ProcessExternalDragAndDrop(int32_t droppedEntryIndex);

        Status LoadPlaylist(io::File& file, Cue& cue);
        void SavePlaylist(io::File& file, const Cue& cue);

        void ButtonUrl();
        void ButtonLoad();
        void ButtonSave();
        void ButtonClear();
        void ButtonSort();

        void AddFiles(const Array<std::string>& files, int32_t droppedEntryIndex, bool isAcceptingAll);
        void AddUrls(const Array<std::string>& urls, int32_t droppedEntryIndex);

        static std::string GetPlaylistFilename(const std::string& name);
        void SavePlaylistsToc();
        void PatchPlaylistsToc();

    private:
        Cue m_cue;

        SmartPtr<DropTarget> m_dropTarget;

        Array<Summary> m_playlists;

        std::string m_playlistName;

        PlaylistID m_lastSelectedEntry = PlaylistID::kInvalid;

        uint32_t m_firstDraggedEntryIndex = 0xffFFffFF;
        uint32_t m_lastDraggedEntryIndex = 0;
        uint32_t m_draggedEntryIndex = 0;

        int32_t m_oldCurrentEntryIndex = -1;
        int32_t m_currentEntryIndex = -1;
        bool m_isCurrentEntryFocus = true;

        std::string m_inputUrls;
        Array<std::string> m_urls;

        enum class OpenedTab : uint8_t
        {
            kNone,
            kArtists,
            kSongs
        };
        Serialized<OpenedTab> m_openedTab = { "OpenedTab", OpenedTab::kNone };

        DatabaseArtistsUI* m_artists = nullptr;
        SongsUI* m_songs = nullptr;

        MusicID m_currentRatingId;
        int32_t m_defaultRating = 0;

        PlaylistID m_uniqueIdGenerator = PlaylistID::kInvalid;

        static const char* const ms_fileName;
    };
}
// namespace rePlayer
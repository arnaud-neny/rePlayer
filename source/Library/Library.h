#pragma once

#include "Source.h"

#include <Containers/Array.h>
#include <Containers/SmartPtr.h>
#include <Core/Window.h>

namespace core::io
{
    class Stream;
}
// namespace core::io

namespace rePlayer
{
    class LibraryDatabase;
    class Player;
    class SongEditor;
    struct MusicID;

    class Library : public Window
    {
        friend class LibraryDatabase;
        friend class SongEditor;
    public:
        Library();
        ~Library();

        void DisplaySettings();

        void Validate();
        void Save();

        SmartPtr<core::io::Stream> GetStream(Song* song);
        SmartPtr<Player> LoadSong(const MusicID musicId);

    private:
        template <typename ParentDatabaseUI>
        class DatabaseUI;
        class ArtistsUI;
        class SongsUI;

        class FileImport;

    private:
        std::string OnGetWindowTitle() override;
        void OnDisplay() override;
        void OnEndUpdate() override;

        void SourceSelection();

        void UpdateImports();

        void UpdateImportArtists();
        void FindArtists();
        void ImportArtist(SourceID artistID, SourceResults& sourceResults);

        void ProcessImports();

        void Load();

        //DEBUG
        void ValidateArtist(const Artist* const artist) const;

    private:
        LibraryDatabase& m_db;
        ArtistsUI* m_artists = nullptr;
        SongsUI* m_songs = nullptr;

        Source* m_sources[SourceID::NumSourceIDs];
        static constexpr uint64_t kSelectableSources = ((1ull << SourceID::NumSourceIDs) - 1ull) & ~((1ull << SourceID::FileImportID) | (1ull << SourceID::URLImportID));
        uint64_t m_selectedSources = kSelectableSources;

        bool m_hasSongsBackup = false;
        bool m_hasArtistsBackup = false;

        bool m_isSongTabFirstCall = true; // Too many bugs with ImGuiTabItemFlags_SetSelected...
        Serialized<bool> m_isSongTabEnabled = { "SongTab", true };
        Serialized<bool> m_isMergingOnDownload = { "AutoMerge", false };

        bool m_isBusy = false;
        float m_busyTime = 0.0f;
        uint32_t m_busyColor;

        struct ImportArtists
        {
            struct State
            {
                bool isSelected;
                bool isImported;
                ArtistID id;
            };
            std::string searchName;
            Array<Source::ArtistsCollection::Artist> artists;
            Array<State> states;
            int lastSelected = -1;
            bool isOpened = false;
            bool isExternal = false;
        } m_importArtists;

        struct
        {
            std::string searchName;
            bool isOpened = false;
        } m_importSongs;

        struct
        {
            SourceResults sourceResults;
            Array<uint32_t> sortedSongs;
            Array<bool> selected;
            int lastSelected = -1;
            bool* isOpened = nullptr;
        } m_imports;

        FileImport* m_fileImport = nullptr;
        Serialized<std::string> m_lastFileDialogPath = "LastFileDialogPath";

        static const char* const ms_songsFilename;
        static const char* const ms_artistsFilename;
    };
}
// namespace rePlayer

#include "Library.inl"
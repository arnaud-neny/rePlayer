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
    class Database;
    class Player;
    class SongEditor;
    struct MusicID;

    class Library : public Window
    {
        friend class SongEditor;
    public:
        Library();
        ~Library();

        void Save();

        SmartPtr<core::io::Stream> GetStream(Song* song);
        SmartPtr<Player> LoadSong(const MusicID musicId);

    private:
        class ArtistsUI;
        class SongsUI;

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

        void ProcessFailedDeletes();

        //DEBUG
        void ValidateArtist(const Artist* const artist) const;

    private:
        Database& m_db;
        ArtistsUI* m_artists = nullptr;
        SongsUI* m_songs = nullptr;

        Source* m_sources[SourceID::NumSourceIDs];
        static constexpr uint64_t kSelectableSources = ((1ull << SourceID::NumSourceIDs) - 1ull) & ~((1ull << SourceID::FileImportID) | (1ull << SourceID::URLImportID));
        uint64_t m_selectedSources = (1ull << SourceID::AmigaMusicPreservationSourceID) | (1ull << SourceID::TheModArchiveSourceID) | (1ull << SourceID::ModlandSourceID);

        bool m_hasSongsBackup = false;
        bool m_hasArtistsBackup = false;

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

        Serialized<std::string> m_lastFileDialogPath = "LastFileDialogPath";

        static const char* const ms_songsFilename;
        static const char* const ms_artistsFilename;
    };
}
// namespace rePlayer

#include "Library.inl"
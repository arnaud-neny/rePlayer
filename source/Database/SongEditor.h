#pragma once

#include "Types/MusicID.h"

#include <Core/Window.h>

namespace rePlayer
{
    class SongEndEditor;

    class SongEditor : public Window
    {
    public:
        SongEditor();
        ~SongEditor();

        void OnSongSelected(MusicID musicId);

    private:
        std::string OnGetWindowTitle() override;
        void OnDisplay() override;

        void EditSubsongs();
        void EditSubsong(SubsongData<Blob::kIsDynamic>& subsong, uint32_t numValidSubsongs);
        void EditArtists();
        void EditTags();
        void EditPlayer();
        void EditMetadata();
        void EditSources();
        void SaveChanges(Song* currentSong);

    private:
        struct
        {
            SongSheet original;
            SongSheet edited;
        } m_song;
        MusicID m_musicId;
        bool m_isSubsongOpened = false;
        bool m_isMetadataOpened = false;
        uint16_t m_selectedSubsong = 0;
        Replayables m_playables;
        const char* m_sortedExtensionNames[int32_t(eExtension::Count)];
        eExtension m_sortedExtensions[int32_t(eExtension::Count)];
        eExtension m_mappedExtensions[int32_t(eExtension::Count)];

        SongEndEditor* m_songEndEditor = nullptr;
    };
}
// namespace rePlayer
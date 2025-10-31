#pragma once

#include "Types/MusicID.h"

#include <Containers/SmartPtr.h>
#include <Core/Window.h>

struct ImGuiTextFilter;

namespace rePlayer
{
    class BusySpinner;
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
        void OnEndUpdate() override;

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

        SongEndEditor* m_songEndEditor = nullptr;

        ArtistID m_selectedArtistID = ArtistID::Invalid;
        ImGuiTextFilter* m_artistFilter = nullptr;
        float m_artistMaxWidth = 0.0f;

        SmartPtr<BusySpinner> m_busySpinner;
    };
}
// namespace rePlayer
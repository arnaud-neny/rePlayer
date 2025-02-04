#pragma once

#include <Library/Library.h>

namespace rePlayer
{
    template <typename ParentDatabaseUI>
    class Library::DatabaseUI : public ParentDatabaseUI
    {
    public:
        DatabaseUI(Window& owner);
        ~DatabaseUI() override;

        void OnDisplay() override;

    protected:
        void OnSelectionContext() override;
        bool AddToPlaylistUI() override;

    protected:
        class SongMerger;

    protected:
        SongMerger* m_songMerger = nullptr;
    };
}
// namespace rePlayer
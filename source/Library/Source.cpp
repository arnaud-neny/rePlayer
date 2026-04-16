// Core
#include <ImGui.h>

// rePlayer
#include <Database/Database.h>
#include <RePlayer/Core.h>
#include "Source.h"

namespace rePlayer
{
    void Source::BrowserDisplayLibraryId(const BrowserEntry& entry, bool isSong) const
    {
        if (isSong)
        {
            if (entry.songId != SongID::Invalid)
                ImGui::Text("%08X %s", uint32_t(entry.songId), Core::GetDatabase(DatabaseID::kLibrary)[entry.songId]->GetName());
            else if (entry.isDiscarded)
                ImGui::TextUnformatted("-------- DISCARDED");
        }
        else
        {
            if (entry.artist.id != ArtistID::Invalid)
            {
                if (entry.artist.isFetched)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, 0xfff0d8c1);
                    ImGui::Text("%04X ", uint32_t(entry.artist.id), Core::GetDatabase(DatabaseID::kLibrary)[entry.artist.id]->GetHandle());
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::TextUnformatted(Core::GetDatabase(DatabaseID::kLibrary)[entry.artist.id]->GetHandle());
                }
                else
                    ImGui::Text("%04X %s", uint32_t(entry.artist.id), Core::GetDatabase(DatabaseID::kLibrary)[entry.artist.id]->GetHandle());
            }
        }
    }
}
// namespace rePlayer
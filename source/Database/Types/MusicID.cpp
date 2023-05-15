#include "MusicID.h"

#include <ImGui.h>
#include <IO/Stream.h>

#include <Database/Database.h>
#include <Deck/Deck.h>
#include <Library/Library.h>
#include <Playlist/Playlist.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>

namespace rePlayer
{
    bool MusicID::IsValid() const
    {
        return Core::GetDatabase(databaseId).IsValid(subsongId);
    }

    Song* MusicID::GetSong() const
    {
        return Core::GetDatabase(databaseId)[subsongId];
    }

    Artist* MusicID::GetArtist(ArtistID artistId) const
    {
        return Core::GetDatabase(databaseId)[artistId];
    }

    std::string MusicID::GetTitle() const
    {
        return Core::GetDatabase(databaseId).GetTitle(subsongId);
    }

    std::string MusicID::GetArtists() const
    {
        return Core::GetDatabase(databaseId).GetArtists(subsongId.songId);
    }

    std::string MusicID::GetFullpath() const
    {
        return Core::GetDatabase(databaseId).GetFullpath(subsongId.songId);
    }

    void MusicID::MarkForSave()
    {
        Core::GetDatabase(databaseId).Raise(Database::Flag::kSaveSongs);
    }

    void MusicID::Track() const
    {
        Core::GetDatabase(databaseId).TrackSubsong(subsongId);
    }

    SmartPtr<core::io::Stream> MusicID::GetStream()
    {
        SmartPtr<core::io::Stream> stream;
        if (databaseId == DatabaseID::kPlaylist)
            stream = Core::GetPlaylist().GetStream(*this);
        else
            stream = Core::GetLibrary().GetStream(GetSong());
        return stream;
    }

    void MusicID::DisplayArtists() const
    {
        auto& db = Core::GetDatabase(databaseId);
        auto* song = db[subsongId];
        bool firstArtist = true;
        for (auto& artistId : song->ArtistIds())
        {
            if (!firstArtist)
            {
                ImGui::SameLine();
                ImGui::TextUnformatted("&");
                ImGui::SameLine();
            }
            auto* artist = db[artistId];
            ImGui::TextUnformatted(artist->GetHandle());
            if (ImGui::IsItemHovered())
                artist->Tooltip();
            firstArtist = false;
        }
    }

    void MusicID::SongTooltip() const
    {
        auto& db = Core::GetDatabase(databaseId);
        auto* song = db[subsongId];

        ImGui::BeginTooltip();
        ImGui::Text("Title  : %s", song->GetName());
        auto subsongName = song->GetSubsongName(subsongId.index);
        if (*subsongName)
            ImGui::Text("         %s", subsongName);
        std::string txt = "Artist : ";
        for (int64_t i = 0, e = song->NumArtistIds(); i < e; i++)
        {
            if (i != 0)
                txt += " & ";
            txt += db[song->GetArtistId(i)]->GetHandle();
        }
        ImGui::TextUnformatted(txt.c_str());
        if (song->GetTags() != Tag::kNone)
        {
            txt = "Tags   : ";
            txt += song->GetTags().ToString();
            ImGui::TextUnformatted(txt.c_str());
        }
        if (song->GetType().replay != eReplay::Unknown)
            ImGui::Text("Replay : %s", Core::GetReplays().GetName(song->GetType().replay));
        ImGui::Text("Source : %s", song->GetSourceName());
        auto metadata = Core::GetDeck().GetMetadata(*this);
        if (metadata.size())
        {
            ImGui::Separator();
            ImGui::TextUnformatted(metadata.c_str());
        }

        ImGui::EndTooltip();
    }
}
// namespace rePlayer
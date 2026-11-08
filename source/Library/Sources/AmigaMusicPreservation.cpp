// Core
#include <Core/Log.h>
#include <Core/String.h>
#include <IO/File.h>
#include <IO/StreamMemory.h>
#include <JSON/json.hpp>

// rePlayer
#include <Database/Types/Countries.h>
#include <IO/Downloader.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>
#include <UI/BusySpinner.h>
#include "AmigaMusicPreservation.h"
#if __has_include("AmigaMusicPreservationKey.h")
#include "AmigaMusicPreservationKey.h"
#endif

// curl
#include <Curl/curl.h>

// zlib
#include <zlib.h>

// stl
#include <algorithm>

namespace rePlayer
{
    const char* const SourceAmigaMusicPreservation::ms_filename = MusicPath "AmigaMusicPreservation" MusicExt;

    class AMPHandler : public Downloader
    {
    public:
        virtual void OnReadJson(nlohmann::json& json) = 0;

        AMPHandler()
        {
            AddHTTPHeader("X-API-Key: " AmigaMusicPreservationKey);
        }

        Status OnDownloaded() override
        {
            nlohmann::json json;
            std::istringstream(Items<char>()) >> json;
            OnReadJson(json);
            return Status::kOk;
        }

        Status Fetch(const char* url) override
        {
            char newUrl[1024] = "https://amp.dascene.net/api/";
            strcat_s(newUrl, url);
            return Downloader::Fetch(newUrl);
        }

        using Downloader::Fetch;
    };

    struct SourceAmigaMusicPreservation::CollectorArtist : public AMPHandler
    {
        CollectorArtist() = default;
        CollectorArtist(ArtistSheet* otherArtist)
            : artist(otherArtist)
        {}

        ArtistSheet::Handles handles;
        std::string name;
        Array<std::string> countries;
        ArtistSheet::Groups groups;

        ArtistSheet* artist = new ArtistSheet;

        void OnReadJson(nlohmann::json& json) final;
    };

    void SourceAmigaMusicPreservation::CollectorArtist::OnReadJson(nlohmann::json& json)
    {
        if (!json.contains("message"))
        {
            artist->handles.Clear();
            artist->handles.Add(json["composer_handle"].get<std::string>());

            artist->realName = json["real_name"];

            for (auto& jsonAlt : json["alt_handle"])
                artist->handles.Add(jsonAlt.get<std::string>());

            artist->countries.Clear();
            for (auto& jsonCountries = json["country"]; auto& jsonCountry : jsonCountries)
            {
                auto country = jsonCountry["country_name"].get<std::string>();
                if (country != "R.I.P.")
                {
                    auto countryCode = Countries::GetCode(country.data());
                    if (countryCode == 0)
                        assert(0 && "todo find the right country");
                    else
                        artist->countries.Add(countryCode);
                }
            }

            artist->groups.Clear();
            for (auto& jsonGroups = json["groups"]; auto& jsonGroup : jsonGroups)
                artist->groups.Add(jsonGroup["group_name"].get<std::string>());
        }
    }

    struct SourceAmigaMusicPreservation::CollectorSong : public AMPHandler
    {
        struct Module
        {
            uint32_t id;
            std::string name;
            std::string ext;
            Array<uint32_t> artists;
        };
        Array<Module> songs;

        void OnReadJson(nlohmann::json& json) final;
    };

    void SourceAmigaMusicPreservation::CollectorSong::OnReadJson(nlohmann::json& json)
    {
        for (auto& jsonModule : json)
        {
            auto* song = songs.Push();

            song->id = jsonModule["module_id"].get<uint32_t>();

            song->name = jsonModule["module_name"].get<std::string>();
            song->ext = jsonModule["module_type"];

            for (auto& jsonAuthor : jsonModule["authors"])
                song->artists.Add(jsonAuthor["composer_id"].get<uint32_t>());
        }
    }

    struct SourceAmigaMusicPreservation::SearchArtist : public AMPHandler
    {
        Array<ArtistsCollection::Artist> artists;

        void OnReadJson(nlohmann::json& json) final;
    };

    void SourceAmigaMusicPreservation::SearchArtist::OnReadJson(nlohmann::json& json)
    {
        for (auto& jsonComposer : json)
        {
            auto* artist = artists.Push();

            artist->id = SourceID(kID, jsonComposer["composer_id"].get<uint32_t>());

            artist->name = jsonComposer["composer_handle"].get<std::string>();

            auto s = jsonComposer["real_name"].get<std::string>();
            if (!s.empty())
            {
                artist->description = "Real Name  : ";
                artist->description += s;
            }

            for (int i = 2; auto& jsonAlt : jsonComposer["alt_handle"])
            {
                if (i == 2)
                {
                    if (!artist->description.empty())
                        artist->description += "\n";
                    artist->description += "Alt Handles: ";
                }
                else if (i == 0)
                    artist->description += "\n             ";
                else
                    artist->description += ", ";
                artist->description += jsonAlt.get<std::string>();
                i = abs(i - 1);
            }


            for (int i = 2; auto& jsonCountry : jsonComposer["country"])
            {
                if (i == 2)
                {
                    if (!artist->description.empty())
                        artist->description += "\n";
                    artist->description += "Countries  : ";
                }
                else if (i == 0)
                    artist->description += "\n             ";
                else
                    artist->description += ", ";
                artist->description += jsonCountry["country_name"].get<std::string>();
                i = abs(i - 1);
            }

            for (int i = 2; auto& jsonGroup : jsonComposer["groups"])
            {
                if (i == 2)
                {
                    if (!artist->description.empty())
                        artist->description += "\n";
                    artist->description += "Groups     : ";
                }
                else if (i == 0)
                    artist->description += "\n             ";
                else
                    artist->description += ", ";
                artist->description += jsonGroup["group_name"].get<std::string>();
                i = abs(i - 1);
            }

            if (!artist->description.empty())
                artist->description += "\n";
            artist->description += "Modules    : ";
            artist->description += jsonComposer["module_count"].dump();
        }
    }

    struct SourceAmigaMusicPreservation::SearchSong : public AMPHandler
    {
        struct Song
        {
            uint32_t id;
            std::string name;
            std::string ext;
            Array<std::pair<uint32_t, std::string>> artist;
        };
        Array<Song> songs;

        std::string ext;
        uint32_t page = 1;
        uint32_t numPages = 0;

        void OnReadJson(nlohmann::json& json) final;
    };

    void SourceAmigaMusicPreservation::SearchSong::OnReadJson(nlohmann::json& json)
    {
        page = json["current_page"].get<uint32_t>();
        if (numPages == 0)
            numPages = json["total_page"].get<uint32_t>();
        for (auto& jsonModule : json["modules"])
        {
            auto* song = songs.Push();

            song->id = jsonModule["module_id"].get<uint32_t>();

            song->name = jsonModule["module_name"].get<std::string>();
            song->ext = jsonModule["module_type"];

            for (auto& jsonAuthor : jsonModule["authors"])
                song->artist.Add({ jsonAuthor["composer_id"].get<uint32_t>(), jsonAuthor["composer_handle"].get<std::string>() });
        }
    }

    SourceAmigaMusicPreservation::SourceAmigaMusicPreservation()
        : Source(false)
    {}

    SourceAmigaMusicPreservation::~SourceAmigaMusicPreservation()
    {}

    void SourceAmigaMusicPreservation::FindArtists(ArtistsCollection& artists, const char* name, BusySpinner& busySpinner)
    {
        SearchArtist search;

        if (strlen(name) <= 1)
        {
            busySpinner.Error("Search too short, need at least 2 characters");
        }
        else
        {
            busySpinner.Info("looking for handle");
            search.Fetch("search_by_handle?search=%s&match=false", search.Escape(name).c_str());
            if (!search.m_error.empty())
                busySpinner.Error(search.m_error);
            artists.matches = std::move(search.artists);

            busySpinner.Info("looking for alias");
            search.Fetch("search_composers?search=*%s*&field=oldhandles", search.Escape(name).c_str());
            if (!search.m_error.empty())
                busySpinner.Error(search.m_error);
            for (auto& altArtist : search.artists)
            {
                // check if it's not already in the matches
                if (artists.matches.FindIf([&altArtist](auto& artist) {
                    return artist.id == altArtist.id;
                }))
                    continue;
                artists.alternatives.Add(std::move(altArtist));
            }
        }
    }

    void SourceAmigaMusicPreservation::ImportArtist(SourceID importedArtistID, SourceResults& results, BusySpinner& busySpinner)
    {
        assert(importedArtistID.sourceId == kID);
        results.importedArtists.Add(importedArtistID);

        CollectorSong collectorSongs;
        collectorSongs.Fetch("get_modules_by_author?index=%u", importedArtistID.internalId);

        for (auto& collectedSong : collectorSongs.songs)
        {
            SourceID songSourceId(kID, collectedSong.id);
            if (results.IsSongAvailable(songSourceId))
                continue;

            auto* song = new SongSheet();

            SourceResults::State state;
            if (auto sourceSong = FindSong(collectedSong.id))
            {
                song->id = sourceSong->songId;
                if (sourceSong->isDiscarded)
                    state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kDiscarded : SourceResults::kMerged);
                else
                    song->id == SongID::Invalid ? state.SetSongStatus(SourceResults::kNew).SetChecked(true) : state.SetSongStatus(SourceResults::kOwned);
            }
            else
            {
                state.SetSongStatus(SourceResults::kNew).SetChecked(true);
            }

            song->name = collectedSong.name;
            song->SetType(Core::GetReplays().Find(collectedSong.ext.c_str()));
            if (song->ext == eExtension::Unknown)
            {
                if (collectedSong.ext == "FST")
                    song->SetType(Core::GetReplays().Find("mod"));
                else if (collectedSong.ext == "OSS")
                    song->SetType(Core::GetReplays().Find("med"));
                else
                {
                    song->name.String() += ".";
                    song->name.String() += collectedSong.ext;
                }
            }
            song->sourceIds.Add(songSourceId);

            for (auto artistId : collectedSong.artists)
            {
                SourceID artistSourceId(kID, artistId);
                auto artistIndex = results.GetArtistIndex(artistSourceId);
                if (artistIndex < 0)
                {
                    artistIndex = results.artists.NumItems<int32_t>();

                    CollectorArtist collectedArtist;
                    collectedArtist.Fetch("get_author_details?index=%u", artistId);
                    if (collectedArtist.artist->handles.IsEmpty())
                    {
                        // Shit happens (server failure?)
                        char buf[32];
                        sprintf(buf, "#Error:ID%u", artistSourceId.value);
                        collectedArtist.artist->handles.Add(buf);
                        busySpinner.Error("Fail to fetch artist details");
                    }
                    collectedArtist.artist->sources.Add(artistSourceId);

                    results.artists.Add(collectedArtist.artist);
                }
                if (artistId == importedArtistID.internalId)//main artist
                    song->artistIds.Insert(0, static_cast<ArtistID>(artistIndex));
                else
                    song->artistIds.Add(static_cast<ArtistID>(artistIndex));
            }

            results.songs.Add(song);
            results.states.Add(state);
        }
    }

    void SourceAmigaMusicPreservation::FindSongs(const char* name, SourceResults& collectedSongs, BusySpinner& busySpinner)
    {
        busySpinner.Info("looking for modules");
        SearchSong search;

        do 
        {
            search.Fetch("search_modules_paged?search=*%s*&page=%u", search.Escape(name).c_str(), search.page);
            if (!search.m_error.empty())
                busySpinner.Error(search.m_error);
        } while (++search.page <= search.numPages);

        for (auto& searchSong : search.songs)
        {
            auto song = new SongSheet();

            SourceResults::State state;
            if (auto sourceSong = FindSong(searchSong.id))
            {
                song->id = sourceSong->songId;
                if (sourceSong->isDiscarded)
                    state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kDiscarded : SourceResults::kMerged);
                else
                    state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kNew : SourceResults::kOwned);
            }
            else
            {
                state.SetSongStatus(SourceResults::kNew);
            }

            song->name = searchSong.name;
            song->SetType(Core::GetReplays().Find(searchSong.ext.c_str()));
            if (song->ext == eExtension::Unknown)
            {
                if (searchSong.ext == "FST")
                    song->SetType(Core::GetReplays().Find("mod"));
                else if (searchSong.ext == "OSS")
                    song->SetType(Core::GetReplays().Find("med"));
                else
                {
                    song->name.String() += ".";
                    song->name.String() += searchSong.ext;
                }
            }
            song->sourceIds.Add(SourceID(kID, searchSong.id));
            for (auto& searchArtist : searchSong.artist)
            {
                auto it = std::find_if(collectedSongs.artists.begin(), collectedSongs.artists.end(), [id = searchArtist.first](auto entry)
                {
                    return entry->sources[0].id.sourceId == kID && entry->sources[0].id.internalId == id;
                });
                song->artistIds.Add(static_cast<ArtistID>(it - collectedSongs.artists.begin()));
                if (it == collectedSongs.artists.end())
                {
                    auto artist = new ArtistSheet();
                    artist->sources.Add(SourceID(kID, searchArtist.first));
                    artist->handles.Add(searchArtist.second.c_str());
                    collectedSongs.artists.Add(artist);
                }
            }
            collectedSongs.songs.Add(song);
            collectedSongs.states.Add(state);
        }
    }

    Source::Import SourceAmigaMusicPreservation::ImportSong(SourceID sourceId, const std::string& path)
    {
        thread::ScopedSpinLock lock(m_mutex);
        SourceID sourceToDownload = sourceId;
        assert(sourceToDownload.sourceId == kID);

        Downloader downloader("Amiga Music Preservation: downloading \"%s\"...");
        auto status = downloader.Fetch("https://amp.dascene.net/module/%u", sourceToDownload.internalId);

        SmartPtr<io::Stream> stream;
        if (status == Status::kOk&& downloader.IsNotEmpty())
        {
            // There are issues sometimes
            if (memcmp(downloader.begin(), "<!DOCTYPE html", sizeof("<!DOCTYPE html") - 1) == 0)
            {
                Log::Error("Amiga Music Preservation: file \"%s\" not found\n", downloader.m_urlLog.c_str());
            }
            else
            {
                Log::Message("OK...");

                SongSource* songAMP = FindSong(sourceToDownload.internalId);
                songAMP->crc = crc32(0L, Z_NULL, 0);
                songAMP->crc = crc32_z(songAMP->crc, downloader.Items(), downloader.Size());
                songAMP->size = downloader.NumItems();
                m_isDirty = true;

                stream = io::StreamMemory::Create(path, downloader.Items(), downloader.Size(), false);
                downloader.Detach();

                if (stream.IsInvalid())
                    Log::Error("Amiga Music Preservation: failed to process package\n");
                else
                    Log::Message("Unpacked\n");
            }
        }

        return { stream, false };
    }

    void SourceAmigaMusicPreservation::OnArtistUpdate(ArtistSheet* artist)
    {
        assert(artist->sources[0].id.sourceId == kID);

        CollectorArtist collector(artist);
        collector.Fetch("get_author_details?index=%u", artist->sources[0].id.internalId);
        if (artist->handles.IsEmpty())
        {
            // Shit happens (server failure?)
            char buf[32];
            sprintf(buf, "#Error:ID%u", artist->sources[0].id.value);
            artist->handles.Add(buf);
        }
    }

    void SourceAmigaMusicPreservation::OnSongUpdate(const Song* const song)
    {
        assert(song->GetSourceId(0).sourceId == kID);
        auto foundSong = FindSong(song->GetSourceId(0).internalId);
        if (foundSong == nullptr)
            foundSong = AddSong(song->GetSourceId(0).internalId);
        foundSong->songId = song->GetId();
        foundSong->isDiscarded = false;
        m_isDirty = true;
    }

    void SourceAmigaMusicPreservation::DiscardSong(SourceID sourceId, SongID newSongId)
    {
        thread::ScopedSpinLock lock(m_mutex);
        assert(sourceId.sourceId == kID);
        auto foundSong = FindSong(sourceId.internalId);
        if (foundSong)
        {
            foundSong->songId = newSongId;
            foundSong->isDiscarded = true;
            m_isDirty = true;
        }
    }

    void SourceAmigaMusicPreservation::InvalidateSong(SourceID sourceId, SongID newSongId)
    {
        thread::ScopedSpinLock lock(m_mutex);
        assert(sourceId.sourceId == kID);
        auto foundSong = FindSong(sourceId.internalId);
        if (foundSong)
        {
            foundSong->songId = newSongId;
            foundSong->isDiscarded = false;
            m_isDirty = true;
        }
    }

    void SourceAmigaMusicPreservation::Load()
    {
        auto file = io::File::OpenForRead(ms_filename);
        if (file.IsValid())
        {
            if (file.Read<uint32_t>() != kMusicFileStamp || file.Read<uint32_t>() > Core::GetVersion())
            {
                assert(0 && "file read error");
                return;
            }
            file.Read<uint32_t>(m_songs);
        }
    }

    void SourceAmigaMusicPreservation::Save() const
    {
        if (m_isDirty)
        {
            if (!m_hasBackup)
            {
                std::string backupFileame = ms_filename;
                backupFileame += ".bak";
                io::File::Copy(ms_filename, backupFileame.c_str());
                m_hasBackup = true;
            }
            auto file = io::File::OpenForWrite(ms_filename);
            if (file.IsValid())
            {
                file.Write(kMusicFileStamp);
                file.Write(Core::GetVersion());
                file.Write<uint32_t>(m_songs);
                m_isDirty = false;
            }
        }
    }

    SourceAmigaMusicPreservation::SongSource* SourceAmigaMusicPreservation::AddSong(uint32_t id)
    {
        auto* song = std::lower_bound(m_songs.begin(), m_songs.end(), id, [](auto& song, auto id)
        {
            return song.id < id;
        });
        thread::ScopedSpinLock lock(m_mutex);
        return m_songs.Insert(song - m_songs, SongSource(id));
    }

    SourceAmigaMusicPreservation::SongSource* SourceAmigaMusicPreservation::FindSong(uint32_t id) const
    {
        auto* song = std::lower_bound(m_songs.begin(), m_songs.end(), id, [](auto& song, auto id)
        {
            return song.id < id;
        });
        return song != m_songs.end() && song->id == id ? const_cast<SongSource*>(song) : nullptr;
    }
}
// namespace rePlayer
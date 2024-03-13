// Core
#include <Core/Log.h>
#include <IO/File.h>
#include <IO/StreamMemory.h>

// rePlayer
#include <Library/Sources/XmlHandler.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>
#include "TheModArchive.h"
#if __has_include("TheModArchiveKey.h")
#include "TheModArchiveKey.h"
#endif

// zlib
#include <zlib.h>

// stl
#include <algorithm>
#include <string>

namespace rePlayer
{
    const char* const SourceTheModArchive::ms_filename = MusicPath "TheModArchive" MusicExt;

    struct SourceTheModArchive::Collector : public XmlHandler
    {
        struct Song
        {
            uint32_t id;
            MediaType type;
            std::string name;
            Array<std::pair<uint32_t, std::string>> artists;
        };
        Array<Song> songs;

        uint32_t numPages = 0;
        uint32_t mainArtistId = 0;
        std::string name;

        void OnReadNode(xmlNode* node) final;
    };

    void SourceTheModArchive::Collector::OnReadNode(xmlNode* node)
    {
        auto* root = FindNode(node, "modarchive");
        if (!root)
            return;

        uint32_t numSongs = 0;
        if (node = FindChildNode(root, "results"))
            numSongs = UnsignedText(node);

        if (node = FindChildNode(root, "totalpages"))
            numPages = UnsignedText(node);

        for (auto* moduleNode = FindChildNode(root, "module"); moduleNode; moduleNode = NextNode(moduleNode, "module"))
        {
            Song song;

            song.id = UnsignedText(FindChildNode(moduleNode, "id"));
            if (song.id != 0) // sometimes, modarchive returns empty ids... (eg: artist "reed")
            {
                song.type = Core::GetReplays().Find(Text(FindChildNode(moduleNode, "format")).c_str());
                song.name = Text(FindChildNode(moduleNode, "songtitle"));
                song.name += "@";
                song.name += Text(FindChildNode(moduleNode, "filename"));

                auto* artistsNode = FindChildNode(moduleNode, "artist_info");
                for (node = FindChildNode(artistsNode, "artist"); node; node = NextNode(node, "artist"))
                {
                    std::pair<uint32_t, std::string> artist;
                    artist.first = UnsignedText(FindChildNode(node, "id"));
                    artist.second = Text(FindChildNode(node, "alias"));
                    if (artist.first == mainArtistId)
                        song.artists.Insert(0, std::move(artist));
                    else
                        song.artists.Add(std::move(artist));
                }
                for (node = FindChildNode(artistsNode, "guessed_artist"); node; node = NextNode(node, "guessed_artist"))
                {
                    std::pair<uint32_t, std::string> artist;
                    artist.first = 0xffFFff;
                    artist.second = Text(FindChildNode(node, "alias"));
                    if (artist.second == name)
                        song.artists.Insert(0, std::move(artist));
                    else
                        song.artists.Add(std::move(artist));
                }

                songs.Add(std::move(song));
            }

            numSongs--;
        }
        assert(numSongs == 0);
    }

    struct SourceTheModArchive::SearchArtist : public XmlHandler
    {
        Array<ArtistsCollection::Artist> artists;

        uint32_t numPages = 0;

        void OnReadNode(xmlNode* node);
    };

    void SourceTheModArchive::SearchArtist::OnReadNode(xmlNode* node)
    {
        auto root = FindNode(node, "modarchive");
        if (!root)
            return;

        uint32_t numArtists = 0;
        if (node = FindChildNode(root, "results"))
            numArtists = UnsignedText(node);

        if (node = FindChildNode(root, "totalpages"))
            numPages = UnsignedText(node);

        if (auto* itemsNode = FindChildNode(root, "items"))
        {
            for (auto* itemNode = FindChildNode(itemsNode, "item"); itemNode; itemNode = NextNode(itemNode, "item"))
            {
                artists.Push();
                artists.Last().id = SourceID(kID, UnsignedText(FindChildNode(itemNode, "id")));
                artists.Last().name = Text(FindChildNode(itemNode, "alias"));
                artists.Last().description = "Our Roster";

                numArtists--;
            }
            assert(numArtists == 0);
        }
    }

    struct SourceTheModArchive::SearchGuessedArtist : public XmlHandler
    {
        Array<std::string> artists;

        uint32_t numPages = 0;

        void OnReadNode(xmlNode* node);
    };

    void SourceTheModArchive::SearchGuessedArtist::OnReadNode(xmlNode* node)
    {
        auto* root = FindNode(node, "modarchive");
        if (!root)
            return;

        uint32_t numResults = 0;
        if (node = FindChildNode(root, "results"))
            numResults = UnsignedText(node);

        if (node = FindChildNode(root, "totalpages"))
            numPages = UnsignedText(node);

        for (auto moduleNode = FindChildNode(root, "module"); moduleNode; moduleNode = NextNode(moduleNode, "module"))
        {
            auto artistsNode = FindChildNode(moduleNode, "artist_info");
            if (artistsNode)
            {
                for (node = FindChildNode(artistsNode, "guessed_artist"); node; node = NextNode(node, "guessed_artist"))
                    artists.AddOnce(Text(FindChildNode(node, "alias")));
            }
            numResults--;
        }
        assert(numResults == 0);
    }

    struct SourceTheModArchive::SearchSong : public XmlHandler
    {
        SearchSong(const char* n)
            : name(ToLower(n))
        {}

        struct Song
        {
            uint32_t id;
            std::string name;
            Array<std::pair<uint32_t, std::string>> artists;
            MediaType type;
        };
        Array<Song> songs;
        std::string name;

        uint32_t numPages = 0;

        void OnReadNode(xmlNode* node);
    };

    void SourceTheModArchive::SearchSong::OnReadNode(xmlNode* node)
    {
        auto* root = FindNode(node, "modarchive");
        if (!root)
            return;

        uint32_t numSongs = 0;
        if (node = FindChildNode(root, "results"))
            numSongs = UnsignedText(node);

        if (node = FindChildNode(root, "totalpages"))
            numPages = UnsignedText(node);
        for (auto* moduleNode = FindChildNode(root, "module"); moduleNode; moduleNode = NextNode(moduleNode, "module"))
        {
            Song song;

            song.type = Core::GetReplays().Find(Text(FindChildNode(moduleNode, "format")).c_str());
            song.id = UnsignedText(FindChildNode(moduleNode, "id"));
            song.name = Text(FindChildNode(moduleNode, "songtitle"));

            std::string lName = ToLower(song.name);
            song.name += "@";
            song.name += Text(FindChildNode(moduleNode, "filename"));

            auto* artistsNode = FindChildNode(moduleNode, "artist_info");
            if (artistsNode)
            {
                for (node = FindChildNode(artistsNode, "artist"); node; node = NextNode(node, "artist"))
                {
                    std::pair<uint32_t, std::string> artist;
                    artist.first = UnsignedText(FindChildNode(node, "id"));
                    artist.second = Text(FindChildNode(node, "alias"));
                    song.artists.Add(std::move(artist));
                }
                for (node = FindChildNode(artistsNode, "guessed_artist"); node; node = NextNode(node, "guessed_artist"))
                {
                    std::pair<uint32_t, std::string> artist;
                    artist.first = 0xffFFff;
                    artist.second = Text(FindChildNode(node, "alias"));
                    song.artists.Add(std::move(artist));
                }
            }

            songs.Add(std::move(song));

            numSongs--;
        }
        assert(numSongs == 0);
    }

    SourceTheModArchive::SourceTheModArchive()
    {}

    SourceTheModArchive::~SourceTheModArchive()
    {}

    void SourceTheModArchive::FindArtists(ArtistsCollection& artists, const char* name)
    {
#ifdef TheModArchiveKey
        SearchArtist search;
        for (uint32_t i = 1;;)
        {
            search.Fetch("https://modarchive.org/data/xml-tools.php?key=" TheModArchiveKey "&request=search_artist&query=%s&page=%u", search.Escape(name).c_str(), i);
            i++;
            if (i > search.numPages)
                break;
        }

        {
            SearchGuessedArtist searchGuessed;
            for (uint32_t i = 1;;)
            {
                searchGuessed.Fetch("https://modarchive.org/data/xml-tools.php?key=" TheModArchiveKey "&request=view_modules_by_guessed_artist&query=*%s*&page=%u", search.Escape(name).c_str(), i);
                i++;
                if (i > searchGuessed.numPages)
                    break;
            }
            std::string lName = ToLower(name);
            for (auto& artist : searchGuessed.artists)
            {
                std::string lArtist = ToLower(artist);
                if (strstr(lArtist.c_str(), lName.c_str()))
                {
                    if (std::find_if(search.artists.begin(), search.artists.end(), [&artist](auto& entry)
                    {
                        if (entry.id.internalId & 0x800000)
                            return entry.name == artist;
                        return false;
                    }) == search.artists.end())
                    {
                        auto* newArtist = search.artists.Push();
                        newArtist->id = SourceID(kID, FindGuessedArtist(artist) | 0x800000);
                        newArtist->name = std::move(artist);
                        newArtist->description = "Guessed Artist";
                    }
                }
            }
        }
        for (auto& artist : search.artists)
            artists.matches.Add(std::move(artist));
#endif
    }

    void SourceTheModArchive::ImportArtist(SourceID importedArtistID, SourceResults& results)
    {
#ifdef TheModArchiveKey
        assert(importedArtistID.sourceId == kID);
        results.importedArtists.Add(importedArtistID);
        auto artistId = importedArtistID.internalId;

        std::string guessedArtistName;
        Collector collector;
        if (artistId & 0x800000)
        {
            auto& guessedArtist = m_guessedArtists[artistId & 0x7fFFff];
            guessedArtistName = m_strings.Items(guessedArtist.nameOffset);
            m_areStringsDirty |= guessedArtist.isNotRegistered;
            m_isDirty |= guessedArtist.isNotRegistered;
            guessedArtist.isNotRegistered = false;
            collector.name = guessedArtistName;
            for (uint32_t i = 1;;)
            {
                collector.Fetch("https://modarchive.org/data/xml-tools.php?key=" TheModArchiveKey "&request=view_modules_by_guessed_artist&query=%s&page=%u", collector.Escape(guessedArtistName.c_str()).c_str(), i);
                i++;
                if (i > collector.numPages)
                    break;
            }
        }
        else
        {
            collector.mainArtistId = artistId;
            for (uint32_t i = 1;;)
            {
                collector.Fetch("https://modarchive.org/data/xml-tools.php?key=" TheModArchiveKey "&request=view_modules_by_artistid&query=%u&page=%u", artistId, i);
                i++;
                if (i > collector.numPages)
                    break;
            }
        }

        for (auto& collectedSong : collector.songs)
        {
            // guessed artist search is not case sensitive, so skip the song without the case sensitive match
            if (artistId & 0x800000)
            {
                bool isGuessedArtistFound = false;
                for (auto& artist : collectedSong.artists)
                {
                    if (artist.first & 0x800000)
                    {
                        isGuessedArtistFound = artist.second == guessedArtistName;
                        if (isGuessedArtistFound)
                            break;
                    }
                }
                if (!isGuessedArtistFound)
                    continue;
            }

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
            song->type = collectedSong.type;
            song->sourceIds.Add(SourceID(kID, collectedSong.id));

            for (auto& artist : collectedSong.artists)
            {
                if (artist.first == 0xffFFff)
                {
                    auto it = std::find_if(results.artists.begin(), results.artists.end(), [&artist](auto entry)
                    {
                        if (entry->sources[0].id.sourceId == kID && entry->sources[0].id.internalId & 0x800000)
                            return entry->handles[0] == artist.second;
                        return false;
                    });
                    if (it == results.artists.end())
                    {
                        auto rplArtist = new ArtistSheet();
                        rplArtist->handles.Add(artist.second.c_str());
                        auto guessedArtistIndex = FindGuessedArtist(artist.second);
                        auto& guessedArtist = m_guessedArtists[guessedArtistIndex];
                        rplArtist->sources.Add(SourceID(kID, guessedArtistIndex | 0x800000));
                        m_areStringsDirty |= guessedArtist.isNotRegistered;
                        m_isDirty |= guessedArtist.isNotRegistered;
                        guessedArtist.isNotRegistered = false;
                        it = results.artists.Add(rplArtist);
                    }
                    song->artistIds.Add(static_cast<ArtistID>(it - results.artists.begin()));
                }
                else
                {
                    auto it = std::find_if(results.artists.begin(), results.artists.end(), [&artist](auto entry)
                    {
                        return entry->sources[0].id == SourceID(kID, artist.first);
                    });
                    if (it == results.artists.end())
                    {
                        auto rplArtist = new ArtistSheet();
                        rplArtist->handles.Add(artist.second.c_str());
                        rplArtist->sources.Add(SourceID(kID, artist.first));
                        it = results.artists.Add(rplArtist);
                    }
                    song->artistIds.Add(static_cast<ArtistID>(it - results.artists.begin()));
                }
            }
            results.songs.Add(song);
            results.states.Add(state);
        }
#endif
    }

    void SourceTheModArchive::FindSongs(const char* name, SourceResults& collectedSongs)
    {
#ifdef TheModArchiveKey
        SearchSong search(name);
        for (uint32_t i = 1;;)
        {
            search.Fetch("https://modarchive.org/data/xml-tools.php?key=" TheModArchiveKey "&request=search&type=filename_or_songtitle&query=%s&page=%u", search.Escape(name).c_str(), i);
            i++;
            if (i > search.numPages)
                break;
        }

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
            song->type = searchSong.type;
            song->sourceIds.Add(SourceID(kID, searchSong.id));
            for (auto& searchArtist : searchSong.artists)
            {
                auto artistIt = std::find_if(collectedSongs.artists.begin(), collectedSongs.artists.end(), [&searchArtist](auto entry)
                {
                    if (entry->sources[0].id.sourceId != kID)
                        return false;
                    if (searchArtist.first == 0xffFFff && entry->sources[0].id.internalId & 0x800000)
                        return _stricmp(searchArtist.second.c_str(), entry->handles[0].Items()) == 0;
                    return entry->sources[0].id.internalId == searchArtist.first;
                });
                song->artistIds.Add(static_cast<ArtistID>(artistIt - collectedSongs.artists.begin()));
                if (artistIt == collectedSongs.artists.end())
                {
                    auto artist = new ArtistSheet();
                    if (searchArtist.first != 0xffFFff)
                        artist->sources.Add(SourceID(kID, searchArtist.first));
                    else
                        artist->sources.Add(SourceID(kID, FindGuessedArtist(searchArtist.second) | 0x800000));
                    artist->handles.Add(searchArtist.second.c_str());
                    collectedSongs.artists.Add(artist);
                }
            }
            collectedSongs.songs.Add(song);
            collectedSongs.states.Add(state);
        }
#endif
    }

    std::pair<SmartPtr<io::Stream>, bool> SourceTheModArchive::ImportSong(SourceID sourceId, const std::string& path)
    {
        thread::ScopedSpinLock lock(m_mutex);
        SourceID sourceToDownload = sourceId;
        assert(sourceToDownload.sourceId == kID);

        CURL* curl = curl_easy_init();

        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
        char url[128];
        sprintf(url, "https://api.modarchive.org/downloads.php?moduleid=%u", sourceToDownload.internalId);
        Log::Message("The Mod Archive: downloading \"%s\"...", url);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

        struct Buffer : public Array<uint8_t>
        {
            static size_t Writer(const uint8_t* data, size_t size, size_t nmemb, Buffer* buffer)
            {
                buffer->Add(data, size * nmemb);
                return size * nmemb;
            }
        } buffer;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Buffer::Writer);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        auto curlError = curl_easy_perform(curl);
        SmartPtr<io::Stream> stream;
        bool isEntryMissing = false;
        if (curlError == CURLE_OK && buffer.IsNotEmpty())
        {
            if (memcmp(buffer.begin(), "Invalid ID Error", sizeof("Invalid ID Error")) == 0)
            {
                isEntryMissing = true;
                auto* song = FindSong(sourceToDownload.internalId);
                m_songs.RemoveAt(song - m_songs);

                Log::Error("The Mod Archive: file \"%s\" not found\n", url);
            }
            else
            {
                auto* songSource = FindSong(sourceToDownload.internalId);
                songSource->crc = crc32(0L, Z_NULL, 0);
                songSource->crc = crc32_z(songSource->crc, buffer.Items(), buffer.Size());
                songSource->size = static_cast<uint32_t>(buffer.Size());

                stream = io::StreamMemory::Create(path, buffer.Items(), buffer.Size(), false);
                buffer.Detach();

                Log::Message("OK\n");
            }
            m_isDirty = true;
        }
        else
            Log::Error("The Mod Archive: %s\n", curl_easy_strerror(curlError));

        curl_easy_cleanup(curl);

        return { stream, isEntryMissing };
    }

    void SourceTheModArchive::OnArtistUpdate(ArtistSheet* artist)
    {
        assert(artist->sources[0].id.sourceId == kID);
        if (artist->sources[0].id.internalId & 0x800000)
        {
            auto& guessedArtist = m_guessedArtists[artist->sources[0].id.internalId & 0x7fFFff];
            m_areStringsDirty |= guessedArtist.isNotRegistered;
            m_isDirty |= guessedArtist.isNotRegistered;
            guessedArtist.isNotRegistered = false;
        }
    }

    void SourceTheModArchive::OnSongUpdate(const Song* const song)
    {
        assert(song->GetSourceId(0).sourceId == kID);
        auto foundSong = FindSong(song->GetSourceId(0).internalId);
        if (foundSong == nullptr)
            foundSong = AddSong(song->GetSourceId(0).internalId);
        foundSong->songId = song->GetId();
        foundSong->isDiscarded = false;
        m_isDirty = true;
    }

    void SourceTheModArchive::DiscardSong(SourceID sourceId, SongID newSongId)
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

    void SourceTheModArchive::InvalidateSong(SourceID sourceId, SongID newSongId)
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

    void SourceTheModArchive::Load()
    {
        auto file = io::File::OpenForRead(ms_filename);
        if (file.IsValid())
        {
            if (file.Read<uint64_t>() != kVersion)
            {
                assert(0 && "file read error");
                return;
            }
            file.Read<uint32_t>(m_strings);
            file.Read<uint32_t>(m_guessedArtists);
            file.Read<uint32_t>(m_songs);
            m_availableArtistIds = {};
            for (uint32_t i = 0, e = m_guessedArtists.NumItems(); i < e; i++)
            {
                if (m_guessedArtists[i].isNotRegistered)
                {
                    m_availableArtistIds.nextId = i;
                    break;
                }
            }
        }
    }

    void SourceTheModArchive::Save() const
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
                file.Write(kVersion);
                if (m_areStringsDirty)
                {
                    // remove discarded artists at the end of the array
                    Array<GuessedArtistSource> guessedArtists(m_guessedArtists);
                    for (int64_t i = guessedArtists.NumItems<int64_t>() - 1; i >= 0; --i)
                    {
                        if (!guessedArtists[i].isNotRegistered)
                        {
                            guessedArtists.Resize(i + 1);
                            break;
                        }
                    }
                    // re-pack artists
                    Array<char> strings(0ull, m_strings.NumItems<size_t>());
                    uint32_t lastArtist = 0xffFFffFF;
                    for (uint32_t i = 0, e = guessedArtists.NumItems(); i < e; i++)
                    {
                        auto& artist = guessedArtists[i];
                        if (!artist.isNotRegistered)
                        {
                            auto name = m_strings.Items(artist.nameOffset);
                            artist.nameOffset = strings.NumItems();
                            strings.Add(name, strlen(name) + 1);
                        }
                        else
                        {
                            guessedArtists[i].value = 0xffFFffFF;
                            if (lastArtist != 0xffFFffFF)
                                guessedArtists[lastArtist].nextId = i;
                            lastArtist = i;
                        }
                    }
                    file.Write<uint32_t>(strings);
                    file.Write<uint32_t>(guessedArtists);
                }
                else
                {
                    file.Write<uint32_t>(m_strings);
                    file.Write<uint32_t>(m_guessedArtists);
                }
                file.Write<uint32_t>(m_songs);
                m_isDirty = false;
            }
        }
    }

    SourceTheModArchive::SongSource* SourceTheModArchive::AddSong(uint32_t id)
    {
        auto* song = std::lower_bound(m_songs.begin(), m_songs.end(), id, [](auto& song, auto id)
        {
            return song.id < id;
        });
        thread::ScopedSpinLock lock(m_mutex);
        return m_songs.Insert(song - m_songs, SongSource(id));
    }

    SourceTheModArchive::SongSource* SourceTheModArchive::FindSong(uint32_t id) const
    {
        auto* song = std::lower_bound(m_songs.begin(), m_songs.end(), id, [](auto& song, auto id)
        {
            return song.id < id;
        });
        return song != m_songs.end() && song->id == id ? const_cast<SongSource*>(song) : nullptr;
    }

    uint32_t SourceTheModArchive::FindGuessedArtist(const std::string& name)
    {
        auto guessedArtist = m_guessedArtists.FindIf([&](auto& artist)
        {
            if (artist.value == 0xffFFffFF)
                return false;
            return name == m_strings.Items(artist.nameOffset);
        });
        if (guessedArtist == nullptr)
        {
            thread::ScopedSpinLock lock(m_mutex);
            if (m_availableArtistIds.value != 0xffFFffFF)
            {
                guessedArtist = m_guessedArtists.Items(m_availableArtistIds.nextId);
                m_availableArtistIds = *guessedArtist;
            }
            else
                guessedArtist = m_guessedArtists.Push();
            guessedArtist->nameOffset = m_strings.NumItems();
            m_strings.Add(name.c_str(), name.size() + 1);
            m_areStringsDirty = true;
        }
        return guessedArtist - m_guessedArtists;
    }
}
// namespace rePlayer
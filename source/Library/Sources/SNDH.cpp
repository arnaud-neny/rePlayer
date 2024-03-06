// Core
#include <Core/Log.h>
#include <IO/File.h>
#include <IO/StreamMemory.h>

// rePlayer
#include "SNDH.h"
#include "WebHandler.h"

// zlib
#include <zlib.h>

// stl
#include <algorithm>

namespace rePlayer
{
    const char* const SourceSNDH::ms_filename = MusicPath "SNDH" MusicExt;

    struct SourceSNDH::Collector : public WebHandler
    {
        uint32_t numSongs = 0;
        struct Song
        {
            std::string name;
            uint32_t id;
            uint32_t year;
        };
        Array<Song> songs;

        enum
        {
            kStateInit = 0,
            kStateSongBegin = kStateInit,
            kStateSongID,
            kStateSongName,
            kStateSongYearBegin,
            kStateSongYearStep,
            kStateSongYearEnd,
            kStateEnd
        } state = kStateInit;

        void OnReadNode(xmlNode* node) final;
    };

    void SourceSNDH::Collector::OnReadNode(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (node->type == XML_ELEMENT_NODE)
            {
                if (state == kStateSongYearStep && xmlStrcmp(node->name, BAD_CAST"td") == 0)
                    state = kStateSongYearEnd;
                else for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (state == kStateSongID)
                    {
                        if (xmlStrstr(propChild->content, BAD_CAST"/?ID="))
                        {
                            songs.Push();
                            sscanf_s((const char*)propChild->content, "/?ID=%u", &songs.Last().id);
                            state = kStateSongName;
                        }
                    }
                }
            }
            else switch (state)
            {
            case kStateSongBegin:
                if (xmlStrstr(node->content, BAD_CAST"SNDH-files in listing"))
                {
                    if (sscanf_s((const char*)node->content, "%u SNDH-files in listing", &numSongs) == 1)
                        songs.Reserve(numSongs);
                    state = kStateSongID;
                }
                break;
            case kStateSongName:
                ConvertString(node->content, songs.Last().name);
                state = kStateSongYearBegin;
                break;
            case kStateSongYearBegin:
                if (xmlStrstr(node->content, BAD_CAST"Hertz"))
                    state = kStateSongYearStep;
                break;
            case kStateSongYearEnd:
                if (!xmlStrstr(node->content, BAD_CAST"n/a"))
                    sscanf_s((const char*)node->content, "%u", &songs.Last().year);
                state = numSongs == songs.NumItems() ? kStateEnd : kStateSongID;
                break;
            default:
                break;
            }
            if (state == kStateEnd)
                return;
            OnReadNode(node->children);
        }
    }

    struct SourceSNDH::Search : public WebHandler
    {
        uint32_t numArtists = 0;
        Array<std::pair<std::string, std::string>> foundArtists;

        struct Song
        {
            std::string name;
            std::pair<std::string, std::string> artist;
            uint32_t id;
        };
        uint32_t numSongs = 0;
        Array<Song> foundSongs;

        enum
        {
            kStateInit = 0,
            kStateArtistsBegin = kStateInit,
            kStateArtistUrl,
            kStateArtistName,
            kStateSongsBegin,
            kStateSongID,
            kStateSongStep,
            kStateSongName,
            kStateSongArtistUrl,
            kStateSongArtistName,
            kStateEnd
        } state = kStateInit;

        static std::string FixUmlaut(std::string txt);
        void OnReadNode(xmlNode* node) final;
    };

    std::string SourceSNDH::Search::FixUmlaut(std::string txt)
    {
        // impossible to make tidy not converting this...
        for (size_t pos = 0;;)
        {
            pos = txt.find("%C3%BC", pos, sizeof("%C3%BC") - 1);
            if (pos == std::string::npos)
                break;
            txt.erase(pos, sizeof("%C3%BC") - 2);
            txt[pos] = 'ü';
        }
        return txt;
    }

    void SourceSNDH::Search::OnReadNode(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (node->type == XML_ELEMENT_NODE)
            {
                if (state == kStateSongStep && xmlStrcmp(node->name, BAD_CAST"a") == 0)
                    state = kStateSongName;
                else for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (state == kStateArtistUrl)
                    {
                        if (xmlStrstr(propChild->content, BAD_CAST"/?p=composer&name="))
                        {
                            foundArtists.Push();
                            foundArtists.Last().first = FixUmlaut(Escape((const char*)propChild->content + sizeof("/?p=composer&name=") - 1).c_str());//FixUmlaut((const char*)propChild->content + sizeof("/?p=composer&name=") - 1);
                            state = kStateArtistName;
                        }
                    }
                    else if (state == kStateSongID)
                    {
                        if (xmlStrstr(propChild->content, BAD_CAST"dl.php?ID="))
                        {
                            foundSongs.Push();
                            sscanf_s((const char*)propChild->content, "dl.php?ID=%u", &foundSongs.Last().id);
                            state = kStateSongStep;
                        }
                    }
                    else if (state == kStateSongArtistUrl)
                    {
                        if (xmlStrstr(propChild->content, BAD_CAST"/?p=composer&name="))
                        {
                            foundSongs.Last().artist.first = FixUmlaut(Escape((const char*)propChild->content + sizeof("/?p=composer&name=") - 1).c_str());//FixUmlaut((const char*)propChild->content + sizeof("/?p=composer&name=") - 1);
                            state = kStateSongArtistName;
                        }
                    }
                }
            }
            else switch (state)
            {
            case kStateArtistsBegin:
                if (xmlStrstr(node->content, BAD_CAST"Composers found ("))
                {
                    if (sscanf_s((const char*)node->content, "Composers found (%u)", &numArtists) == 1 && numArtists > 0)
                    {
                        foundArtists.Reserve(numArtists);
                        state = kStateArtistUrl;
                    }
                    else
                        state = kStateSongsBegin;
                }
                break;
            case kStateArtistName:
                ConvertString(node->content, foundArtists.Last().second);
                foundArtists.Last().second = foundArtists.Last().second;
                state = foundArtists.NumItems() == numArtists ? kStateSongsBegin : kStateArtistUrl;
                break;
            case kStateSongsBegin:
                if (xmlStrstr(node->content, BAD_CAST"Tunes found ("))
                {
                    if (sscanf_s((const char*)node->content, "Tunes found (%u)", &numSongs) == 1 && numSongs > 0)
                    {
                        foundSongs.Reserve(numSongs);
                        state = kStateSongID;
                    }
                    else
                        state = kStateEnd;
                }
                break;
            case kStateSongName:
                ConvertString(node->content, foundSongs.Last().name);
                state = kStateSongArtistUrl;
                break;
            case kStateSongArtistName:
                ConvertString(node->content, foundSongs.Last().artist.second);
                state = foundSongs.NumItems() == numSongs ? kStateEnd : kStateSongID;
                break;
            default:
                break;
            }
            if (state == kStateEnd)
                return;
            OnReadNode(node->children);
        }
    }

    SourceSNDH::SourceSNDH()
    {}

    SourceSNDH::~SourceSNDH()
    {}

    void SourceSNDH::FindArtists(ArtistsCollection& artists, const char* name)
    {
        Search search;
        search.Fetch("https://sndh.atari.org/?p=searchdone&searchword=%s", search.Escape(name).c_str());

        for (auto& searchedArtist : search.foundArtists)
        {
            auto* newArtist = artists.matches.Push();
            newArtist->name = searchedArtist.second;
            newArtist->id = SourceID(kID, FindArtist(searchedArtist.first, searchedArtist.second));
        }
    }

    void SourceSNDH::ImportArtist(SourceID importedArtistID, SourceResults& results)
    {
        assert(importedArtistID.sourceId == kID);
        results.importedArtists.Add(importedArtistID);

        Collector collector;
        collector.Fetch("https://sndh.atari.org/?p=composer&name=%s", m_strings.Items(m_artists[importedArtistID.internalId].urlOffset));

        auto artistIndex = results.GetArtistIndex(importedArtistID);
        if (artistIndex < 0)
        {
            artistIndex = results.artists.NumItems<int32_t>();

            auto* rplArtist = new ArtistSheet();
            rplArtist->handles.Add(m_strings.Items(m_artists[importedArtistID.internalId].nameOffset));
            rplArtist->sources.Add(importedArtistID);
            results.artists.Add(rplArtist);
        }

        for (auto& collectedSong : collector.songs)
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
            song->type = { eExtension::_sndh, eReplay::SNDHPlayer };
            song->releaseYear = uint16_t(collectedSong.year);
            song->sourceIds.Add(songSourceId);
            song->artistIds.Add(static_cast<ArtistID>(artistIndex));

            results.songs.Add(song);
            results.states.Add(state);
        }
    }

    void SourceSNDH::FindSongs(const char* name, SourceResults& collectedSongs)
    {
        Search search;
        search.Fetch("https://sndh.atari.org/?p=searchdone&searchword=%s", search.Escape(name).c_str());

        for (auto& searchedSong : search.foundSongs)
        {
            auto song = new SongSheet();

            SourceResults::State state;
            if (auto sourceSong = FindSong(searchedSong.id))
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

            song->name = searchedSong.name;
            song->type = { eExtension::_sndh, eReplay::SNDHPlayer };
            song->sourceIds.Add(SourceID(kID, searchedSong.id));

            if (searchedSong.artist.second != "Unknown Composer")
            {
                auto artistId = FindArtist(searchedSong.artist.first, searchedSong.artist.second);
                auto artistIt = std::find_if(collectedSongs.artists.begin(), collectedSongs.artists.end(), [&artistId](auto entry)
                {
                    if (entry->sources[0].id.sourceId != kID)
                        return false;
                    return entry->sources[0].id.internalId == artistId;
                });
                song->artistIds.Add(static_cast<ArtistID>(artistIt - collectedSongs.artists.begin()));
                if (artistIt == collectedSongs.artists.end())
                {
                    auto artist = new ArtistSheet();
                    artist->sources.Add(SourceID(kID, artistId));
                    artist->handles.Add(searchedSong.artist.second.c_str());
                    collectedSongs.artists.Add(artist);
                }
            }

            collectedSongs.songs.Add(song);
            collectedSongs.states.Add(state);
        }
    }

    std::pair<SmartPtr<io::Stream>, bool> SourceSNDH::ImportSong(SourceID sourceId, const std::string& path)
    {
        SourceID sourceToDownload = sourceId;
        assert(sourceToDownload.sourceId == kID);

        CURL* curl = curl_easy_init();

        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
        char url[128];
        sprintf(url, "https://sndh.atari.org/dl.php?ID=%u", sourceToDownload.internalId);
        Log::Message("SNDH: downloading \"%s\"...", url);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_REFERER, "https://sndh.atari.org/");

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
        if (curlError == CURLE_OK)
        {
            if (buffer.Size() < 128 || strstr(buffer.Items<char>(), "<html>"))
            {
                isEntryMissing = true;
                auto* song = FindSong(sourceToDownload.internalId);
                m_songs.RemoveAt(song - m_songs);
                m_isDirty = true;

                Log::Error("SNDH: file \"%s\" not found\n", url);
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
        }
        else
            Log::Error("SNDH: %s\n", curl_easy_strerror(curlError));

        curl_easy_cleanup(curl);

        return { stream, isEntryMissing };
    }

    void SourceSNDH::OnArtistUpdate(ArtistSheet* artist)
    {
        assert(artist->sources[0].id.sourceId == kID);
        auto& sourceArtist = m_artists[artist->sources[0].id.internalId];
        m_areStringsDirty |= sourceArtist.isNotRegistered;
        m_isDirty |= sourceArtist.isNotRegistered;
        sourceArtist.isNotRegistered = false;
    }

    void SourceSNDH::OnSongUpdate(const Song* const song)
    {
        assert(song->GetSourceId(0).sourceId == kID);
        auto foundSong = FindSong(song->GetSourceId(0).internalId);
        if (foundSong == nullptr)
            foundSong = AddSong(song->GetSourceId(0).internalId);
        foundSong->songId = song->GetId();
        foundSong->isDiscarded = false;
        m_isDirty = true;
    }

    void SourceSNDH::DiscardSong(SourceID sourceId, SongID newSongId)
    {
        assert(sourceId.sourceId == kID);
        auto foundSong = FindSong(sourceId.internalId);
        if (foundSong)
        {
            foundSong->songId = newSongId;
            foundSong->isDiscarded = true;
            m_isDirty = true;
        }
    }

    void SourceSNDH::InvalidateSong(SourceID sourceId, SongID newSongId)
    {
        assert(sourceId.sourceId == kID);
        auto foundSong = FindSong(sourceId.internalId);
        if (foundSong)
        {
            foundSong->songId = newSongId;
            foundSong->isDiscarded = false;
            m_isDirty = true;
        }
    }

    void SourceSNDH::Load()
    {
        auto file = io::File::OpenForRead(ms_filename);
        if (file.IsValid())
        {
            if (file.Read<uint64_t>() != SongSource::kVersion)
            {
                assert(0 && "file read error");
                return;
            }
            file.Read<uint32_t>(m_strings);
            file.Read<uint32_t>(m_artists);
            file.Read<uint32_t>(m_songs);
            m_availableArtistIds = {};
            for (uint32_t i = 0, e = m_artists.NumItems(); i < e; i++)
            {
                if (m_artists[i].isNotRegistered)
                {
                    m_availableArtistIds.nextId = i;
                    break;
                }
            }
        }
    }

    void SourceSNDH::Save() const
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
                file.Write(SongSource::kVersion);
                if (m_areStringsDirty)
                {
                    // remove discarded artists at the end of the array
                    Array<ArtistSource> artists(m_artists);
                    for (int64_t i = artists.NumItems<int64_t>() - 1; i >= 0; --i)
                    {
                        if (!artists[i].isNotRegistered)
                        {
                            artists.Resize(i + 1);
                            break;
                        }
                    }
                    // re-pack artists
                    Array<char> strings(0ull, m_strings.NumItems<size_t>());
                    uint32_t lastArtist = 0xffFFffFF;
                    for (uint32_t i = 0, e = artists.NumItems(); i < e; i++)
                    {
                        auto& artist = artists[i];
                        if (!artist.isNotRegistered)
                        {
                            auto* name = m_strings.Items(artist.nameOffset);
                            auto* url = m_strings.Items(artist.urlOffset);
                            artist.nameOffset = strings.NumItems();
                            strings.Add(name, strlen(name) + 1);
                            if (name != url)
                            {
                                artist.urlOffset = strings.NumItems();
                                strings.Add(url, strlen(url) + 1);
                            }
                            else
                                artist.urlOffset = artist.nameOffset;
                        }
                        else
                        {
                            artists[i].value = 0xffFFffFF;
                            if (lastArtist != 0xffFFffFF)
                                artists[lastArtist].nextId = i;
                            lastArtist = i;
                        }
                    }
                    file.Write<uint32_t>(strings);
                    file.Write<uint32_t>(artists);
                }
                else
                {
                    file.Write<uint32_t>(m_strings);
                    file.Write<uint32_t>(m_artists);
                }
                file.Write<uint32_t>(m_songs);
                m_isDirty = false;
            }
        }
    }

    SourceSNDH::SongSource* SourceSNDH::AddSong(uint32_t id)
    {
        auto* song = std::lower_bound(m_songs.begin(), m_songs.end(), id, [](auto& song, auto id)
        {
            return song.id < id;
        });
        return m_songs.Insert(song - m_songs, SongSource(id));
    }

    SourceSNDH::SongSource* SourceSNDH::FindSong(uint32_t id) const
    {
        auto* song = std::lower_bound(m_songs.begin(), m_songs.end(), id, [](auto& song, auto id)
        {
            return song.id < id;
        });
        return song != m_songs.end() && song->id == id ? const_cast<SongSource*>(song) : nullptr;
    }

    uint32_t SourceSNDH::FindArtist(const std::string& url, const std::string& name)
    {
        auto artist = m_artists.FindIf([&](auto& artist)
        {
            if (artist.value == 0xffFFffFF)
                return false;
            return url == m_strings.Items(artist.urlOffset);
        });
        if (artist == nullptr)
        {
            if (m_availableArtistIds.value != 0xffFFffFF)
            {
                artist = m_artists.Items(m_availableArtistIds.nextId);
                m_availableArtistIds = *artist;
            }
            else
                artist = m_artists.Push();
            artist->nameOffset = m_strings.NumItems();
            m_strings.Add(name.c_str(), name.size() + 1);
            if (url != name)
            {
                artist->urlOffset = m_strings.NumItems();
                m_strings.Add(url.c_str(), url.size() + 1);
            }
            else
                artist->urlOffset = artist->nameOffset;
            m_areStringsDirty = true;
        }
        return artist - m_artists;
    }
}
// namespace rePlayer
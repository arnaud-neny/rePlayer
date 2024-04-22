#include "HighVoltageSIDCollection.h"

// Core
#include <Core/Log.h>
#include <Core/String.h>
#include <IO/File.h>
#include <IO/StreamMemory.h>

// zlib
#include <zlib.h>

// curl
#include <Curl/curl.h>

namespace rePlayer
{
    static inline int Download(CURL* curl)
    {
        auto curlError = curl_easy_perform(curl);
        if (curlError == CURLE_OK)
        {
            return 0;
        }
        else if (curlError == CURLE_HTTP_RETURNED_ERROR)
        {
            long responseCode = 0;
            if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode) == CURLE_OK && responseCode == 404)
                return 1;
        }
        Log::Error("High Voltage SID Collection: %s\n", curl_easy_strerror(curlError));
        return -1;
    }

    const char* const SourceHighVoltageSIDCollection::ms_filename = MusicPath "HighVoltageSIDCollection" MusicExt;
    const char* const SourceHighVoltageSIDCollection::ms_urls[] = {
        "https://hvsc.de/download/C64Music/",
        "https://hvsc.etv.cx/C64Music/"
    };

    inline const char* SourceHighVoltageSIDCollection::Chars::operator()(const Array<char>& blob) const
    {
        return blob.Items() + offset;
    }

    inline void SourceHighVoltageSIDCollection::Chars::Set(Array<char>& blob, const char* otherString)
    {
        offset = blob.NumItems();
        blob.Add(otherString, strlen(otherString) + 1);
    }

    inline void SourceHighVoltageSIDCollection::Chars::Set(Array<char>& blob, const std::string& otherString)
    {
        offset = blob.NumItems();
        blob.Add(otherString.c_str(), otherString.size() + 1);
    }

    template <typename T>
    inline void SourceHighVoltageSIDCollection::Chars::Copy(const Array<char>& blob, Array<T>& otherblob) const
    {
        auto src = blob.Items() + offset;
        otherblob.Copy(src, strlen(src) + 1);
    }

    inline bool SourceHighVoltageSIDCollection::Chars::IsSame(const Array<char>& blob, const char* otherString) const
    {
        auto src = blob.Items() + offset;
        while (*src && *otherString)
        {
            if (*src != *otherString)
                return false;
            src++;
            otherString++;
        }
        return *src == *otherString;
    }

    SourceHighVoltageSIDCollection::SourceHighVoltageSIDCollection()
    {
        m_artists.Push();
        m_roots.Push();
        m_songs.Push();
        m_strings.Add('\0'); // 0 == empty string == invalid offset
        m_data.Copy(&m_areStringsDirty, 4); // add 4 zeros
    }

    SourceHighVoltageSIDCollection::~SourceHighVoltageSIDCollection()
    {}

    void SourceHighVoltageSIDCollection::FindArtists(ArtistsCollection& artists, const char* name)
    {
        if (DownloadDatabase())
            return;

        std::string lName = ToLower(name);
        for (auto& dbArtist : m_db.artists)
        {
            std::string lArtist = ToLower(dbArtist.name(m_db.strings));
            if (strstr(lArtist.c_str(), lName.c_str()))
            {
                artists.matches.Push();
                auto& newArtist = artists.matches.Last();
                newArtist.id = SourceID(kID, FindArtist(dbArtist.name(m_db.strings)));
                newArtist.name = dbArtist.name(m_db.strings);
                char buf[32];
                sprintf(buf, "%d %s", dbArtist.numSongs, dbArtist.numSongs <= 1 ? "song" : "songs");
                newArtist.description = buf;
            }
        }
    }

    void SourceHighVoltageSIDCollection::ImportArtist(SourceID importedArtistID, SourceResults& results)
    {
        assert(importedArtistID.sourceId == kID);
        results.importedArtists.Add(importedArtistID);

        if (DownloadDatabase())
            return;

        uint16_t dbImportedArtistId = 0;
        {
            auto* name = m_artists[importedArtistID.internalId].name(m_strings);
            for (auto& dbArtist : m_db.artists)
            {
                if (dbArtist.name.IsSame(m_db.strings, name))
                {
                    dbImportedArtistId = uint16_t(&dbArtist - m_db.artists.Items());
                    break;
                }
            }

            assert(dbImportedArtistId != 0); // has the artist disappeared?
            if (dbImportedArtistId == 0)
                Log::Error("High Voltage SID Collection: can't find artist \"%s\"\n", name);
        }

        for (uint32_t dbSongId = m_db.artists[dbImportedArtistId].songs; dbSongId; dbSongId = m_db.songs[dbSongId].nextSong)
        {
            const auto& dbSong = m_db.songs[dbSongId];
            auto songSourceId = FindSong(dbSong);
            if (results.IsSongAvailable(SourceID(kID, songSourceId)))
                continue;

            std::string dbSongName(dbSong.name(m_db.strings));
            for (auto* c = dbSongName.data(); c = strchr(c, '_');)
                *c = ' ';

            auto song = new SongSheet();
            auto songSource = GetSongSource(songSourceId);

            SourceResults::State state;
            song->id = songSource->songId;
            if (songSource->isDiscarded)
                state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kDiscarded : SourceResults::kMerged);
            else
                song->id == SongID::Invalid ? state.SetSongStatus(SourceResults::kNew).SetChecked(true) : state.SetSongStatus(SourceResults::kOwned);

            uint32_t artistId = 0;
            if (songSource->artist)
            {
                SourceID artistSourceId(kID, songSource->artist);
                auto* artistIt = results.artists.FindIf([artistSourceId](auto entry)
                {
                    return entry->sources[0].id == artistSourceId;
                });
                if (artistIt == nullptr)
                {
                    artistId = results.artists.NumItems();
                    auto artist = new ArtistSheet();
                    artist->handles.Add(m_db.artists[dbSong.artist].name(m_db.strings));
                    for (auto* c = artist->handles[0].String().data(); c = strchr(c, '_');)
                        *c = ' ';
                    artist->sources.Add(artistSourceId);
                    results.artists.Add(artist);
                }
                else
                    artistId = artistIt - results.artists;
            }

            song->type = { eExtension::_sid, eReplay::SidPlay };
            song->name = dbSongName;
            if (dbSong.artist)
                song->artistIds.Add(static_cast<ArtistID>(artistId));
            song->sourceIds.Add(SourceID(kID, songSourceId));
            results.songs.Add(song);
            results.states.Add(state);
        }

        m_isDirty |= results.songs.IsNotEmpty();
    }

    void SourceHighVoltageSIDCollection::FindSongs(const char* name, SourceResults& collectedSongs)
    {
        if (DownloadDatabase())
            return;

        std::string lName = ToLower(name);

        for (uint32_t i = 0, e = m_db.songs.NumItems(); i < e; i++)
        {
            const auto& dbSong = m_db.songs[i];
            std::string dbSongName(dbSong.name(m_db.strings));
            for (auto* c = dbSongName.data(); c = strchr(c, '_');)
                *c = ' ';

            std::string rName = ToLower(dbSongName);
            if (strstr(rName.c_str(), lName.c_str()) == nullptr)
                continue;

            auto songSourceId = FindSong(dbSong);
            auto song = new SongSheet();
            auto songSource = GetSongSource(songSourceId);

            SourceResults::State state;
            song->id = songSource->songId;
            if (songSource->isDiscarded)
                state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kDiscarded : SourceResults::kMerged);
            else
                state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kNew : SourceResults::kOwned);

            uint32_t artistId = 0;
            if (songSource->artist)
            {
                SourceID artistSourceId(kID, songSource->artist);
                auto artistIt = collectedSongs.artists.FindIf([artistSourceId](auto entry)
                {
                    return entry->sources[0].id == artistSourceId;
                });
                if (artistIt == nullptr)
                {
                    artistId = collectedSongs.artists.NumItems();
                    auto artist = new ArtistSheet();
                    artist->handles.Add(m_db.artists[dbSong.artist].name(m_db.strings));
                    for (auto* c = artist->handles[0].String().data(); c = strchr(c, '_');)
                        *c = ' ';
                    artist->sources.Add(artistSourceId);
                    collectedSongs.artists.Add(artist);
                }
                else
                    artistId = artistIt - collectedSongs.artists;
            }

            song->type = { eExtension::_sid, eReplay::SidPlay };
            song->name = dbSongName;
            if (dbSong.artist)
                song->artistIds.Add(static_cast<ArtistID>(artistId));
            song->sourceIds.Add(SourceID(kID, songSourceId));
            collectedSongs.songs.Add(song);
            collectedSongs.states.Add(state);
        }
    }

    Source::Import SourceHighVoltageSIDCollection::ImportSong(SourceID sourceId, const std::string& path)
    {
        thread::ScopedSpinLock lock(m_mutex);
        assert(sourceId.sourceId == kID);
        auto* songSource = GetSongSource(sourceId.internalId);

        CURL* curl = curl_easy_init();

        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);

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
        SmartPtr<io::Stream> stream;
        bool isEntryMissing = false;

        std::string url;
        int32_t downloadStatus = -1;
        for (uint32_t i = 0; i < _countof(ms_urls) && downloadStatus != 0; i++)
        {
            url = SetupUrl(curl, songSource, ms_urls[m_currentUrl]);
            downloadStatus = Download(curl);
            if (downloadStatus != 0)
            {
                buffer.Clear();
                m_currentUrl = (m_currentUrl + 1) % _countof(ms_urls);
            }
        }
        if (downloadStatus >= 0)
        {
            if (buffer.IsEmpty() || downloadStatus > 0)
            {
                isEntryMissing = true;
                if (songSource->artist)
                    m_areStringsDirty |= --m_artists[songSource->artist].refcount == 0;
                new (songSource) SourceSong();
                m_areDataDirty = true;
                m_availableSongIds.Add(sourceId.internalId);

                Log::Error("High Voltage SID Collection: file \"%s\" not found\n", url.c_str());
            }
            else
            {
                songSource->crc = crc32(0L, Z_NULL, 0);
                songSource->crc = crc32_z(songSource->crc, buffer.Items(), buffer.Size());
                songSource->size = buffer.NumItems();

                stream = io::StreamMemory::Create(path, buffer.Items(), buffer.Size(), false);
                buffer.Detach();

                Log::Message("OK\n");
            }
            m_isDirty = true;
        }
        else
            Log::Error("High Voltage SID Collection: can't connect\n");

        curl_easy_cleanup(curl);

        return { stream, isEntryMissing };
    }

    void SourceHighVoltageSIDCollection::OnArtistUpdate(ArtistSheet* artist)
    {
        (void)artist;
    }

    void SourceHighVoltageSIDCollection::OnSongUpdate(const Song* const song)
    {
        assert(song->GetSourceId(0).sourceId == kID);
        auto songSource = GetSongSource(song->GetSourceId(0).internalId);
        if (!songSource->IsValid())
        {
            if (songSource->artist)
                m_artists[songSource->artist].refcount++;
        }
        songSource->songId = song->GetId();
        songSource->isDiscarded = false;
        m_isDirty = true;
    }

    void SourceHighVoltageSIDCollection::DiscardSong(SourceID sourceId, SongID newSongId)
    {
        thread::ScopedSpinLock lock(m_mutex);
        assert(sourceId.sourceId == kID);
        auto songSource = GetSongSource(sourceId.internalId);
        songSource->songId = newSongId;
        songSource->isDiscarded = true;
        m_isDirty = true;
    }

    void SourceHighVoltageSIDCollection::InvalidateSong(SourceID sourceId, SongID newSongId)
    {
        thread::ScopedSpinLock lock(m_mutex);
        assert(sourceId.sourceId == kID && newSongId != SongID::Invalid);
        auto songSource = GetSongSource(sourceId.internalId);
        songSource->songId = newSongId;
        songSource->isDiscarded = false;
        m_isDirty = true;
    }

    void SourceHighVoltageSIDCollection::Load()
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
            file.Read<uint32_t>(m_roots);
            file.Read<uint32_t>(m_artists);
            file.Read<uint32_t>(m_data);
            file.Read<uint32_t>(m_songs);
            m_availableArtistIds.Clear();
            for (uint32_t i = 1, e = m_artists.NumItems(); i < e; i++)
            {
                if (m_artists[i].refcount == 0)
                    m_availableArtistIds.Add(uint16_t(i));
            }
            m_availableSongIds.Clear();
            for (uint32_t i = 1, e = m_songs.NumItems(); i < e; i++)
            {
                auto* song = GetSongSource(i);
                if (!song->IsValid())
                    m_availableSongIds.Add(i);
            }
        }
    }

    void SourceHighVoltageSIDCollection::Save() const
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
                    Array<char> strings(0ull, m_strings.NumItems());
                    strings.Add('\0');
                    // re-pack roots
                    Array<SourceRoot> roots(m_roots);
                    for (uint32_t i = 1, e = roots.NumItems(); i < e; i++)
                        roots[i].name.Set(strings, roots[i].name(m_strings));
                    // remove discarded artists at the end of the array
                    Array<SourceArtist> artists(m_artists);
                    for (auto i = artists.NumItems<int64_t>() - 1; i > 0; i--)
                    {
                        if (artists[i].refcount != 0)
                            break;
                        artists.Pop();
                    }
                    // re-pack the artists
                    for (uint32_t i = 1, e = artists.NumItems(); i < e; i++)
                    {
                        if (artists[i].refcount == 0)
                        {
                            artists[i].name = {};
                            continue;
                        }
                        artists[i].name.Set(strings, artists[i].name(m_strings));
                    }
                    // store the new blob
                    file.Write<uint32_t>(strings);
                    file.Write<uint32_t>(roots);
                    file.Write<uint32_t>(artists);
                }
                else
                {
                    file.Write<uint32_t>(m_strings);
                    file.Write<uint32_t>(m_roots);
                    file.Write<uint32_t>(m_artists);
                }
                if (m_areDataDirty)
                {
                    Array<uint8_t> data(0ull, m_data.NumItems<size_t>());
                    data.Add(m_data.Items(), 4);
                    // remove discarded songs at the end of the array
                    Array<uint32_t> songs(m_songs);
                    for (auto i = m_songs.NumItems<int64_t>() - 1; i > 0; i--)
                    {
                        auto* song = GetSongSource(i);
                        if (song->songId != SongID::Invalid || song->isDiscarded)
                            break;
                        songs.Pop();
                    }
                    // re-pack the songs
                    for (uint32_t i = 1, e = songs.NumItems(); i < e; i++)
                    {
                        if (songs[i] == 0)
                            continue;
                        auto* song = GetSongSource(i);
                        if (song->songId == SongID::Invalid && !song->isDiscarded)
                        {
                            songs[i] = 0;
                            continue;
                        }

                        songs[i] = data.NumItems();
                        data.Copy(song, offsetof(SourceSong, name));
                        data.Copy(song->name, strlen(song->name) + 1);
                        data.Resize((data.NumItems() + alignof(SourceSong) - 1) & ~(alignof(SourceSong) - 1));
                    }
                    // store the new blob
                    file.Write<uint32_t>(data);
                    file.Write<uint32_t>(songs);
                }
                else
                {
                    file.Write<uint32_t>(m_data);
                    file.Write<uint32_t>(m_songs);
                }
                m_isDirty = false;
            }
        }
    }

    SourceHighVoltageSIDCollection::SourceSong* SourceHighVoltageSIDCollection::GetSongSource(size_t index) const
    {
        return m_data.Items<SourceSong>(m_songs[index]);
    }

    uint16_t SourceHighVoltageSIDCollection::FindArtist(const char* const name)
    {
        // look in our database
        for (uint32_t i = 1, e = m_artists.NumItems(); i < e; i++)
        {
            if (m_artists[i].name.IsSame(m_strings, name))
                return uint16_t(i);
        }
        // add a new one
        thread::ScopedSpinLock lock(m_mutex);
        SourceArtist* artist;
        if (m_availableArtistIds.IsEmpty())
            artist = m_artists.Push();
        else
            artist = m_artists.Items(m_availableArtistIds.Pop());
        artist->refcount = 0;
        artist->name.Set(m_strings, name);
        m_areStringsDirty = true; // this artist may never be used
        return uint16_t(artist - m_artists);
    }

    uint32_t SourceHighVoltageSIDCollection::FindSong(const HvscSong& dbSong)
    {
        // look in our database (this is ugly/slow)
        for (uint32_t i = 1, e = m_songs.NumItems(); i < e; i++)
        {
            if (m_songs[i] == 0)
                continue;
            auto song = GetSongSource(i);
            if (dbSong.name.IsSame(m_db.strings, song->name))
            {
                if (!m_db.roots[dbSong.root].name.IsSame(m_db.strings, m_roots[song->root].name(m_strings)))
                    continue;
                if (song->artist)
                {
                    if (!dbSong.artist)
                        continue;
                    if (!m_db.artists[dbSong.artist].name.IsSame(m_db.strings, m_artists[song->artist].name(m_strings)))
                        continue;
                }
                else if (dbSong.artist)
                    continue;

                return uint16_t(i);
            }
        }
        // add a new one
        thread::ScopedSpinLock lock(m_mutex);
        uint32_t id = m_availableSongIds.IsEmpty() ? m_songs.Push<uint32_t>() : m_availableSongIds.Pop();
        m_songs[id] = m_data.NumItems();
        auto* songSource = new (m_data.Push(offsetof(SourceSong, name))) SourceSong();
        // look for the root in our database
        for (uint32_t i = 1, e = m_roots.NumItems(); i < e; i++)
        {
            if (m_db.roots[dbSong.root].name.IsSame(m_db.strings, m_roots[i].name(m_strings)))
            {
                songSource->root = uint16_t(i);
                break;
            }
        }
        if (songSource->root == 0)
        {
            // add a new root
            songSource->root = m_roots.NumItems<uint16_t>();
            auto root = m_roots.Push();
            root->name.Set(m_strings, m_db.roots[dbSong.root].name(m_db.strings));
        }
        // get artists
        if (dbSong.artist)
            songSource->artist = FindArtist(m_db.artists[dbSong.artist].name(m_db.strings));
        // name
        dbSong.name.Copy(m_db.strings, m_data);
        m_data.Resize((m_data.NumItems() + alignof(SourceSong) - 1) & ~(alignof(SourceSong) - 1));

        m_areDataDirty = true;

        return id;
    }

    std::string SourceHighVoltageSIDCollection::SetupUrl(CURL* curl, SourceSong* songSource, std::string url) const
    {
        auto unescape = [&url, curl](const char* str)
        {
            for (;;)
            {
                auto start = str;
                while (*str != 0 && *str != '/')
                    ++str;

                auto e = curl_easy_escape(curl, start, static_cast<int>(str - start));
                url += e;
                curl_free(e);
                if (*str++)
                    url += '/';
                else
                    break;
            }
        };

        unescape(m_roots[songSource->root].name(m_strings));
        url += '/';
        if (songSource->artist != 0)
        {
            unescape(m_artists[songSource->artist].name(m_strings));
            url += '/';
        }
        unescape(songSource->name);
        url += ".sid";

        Log::Message("High Voltage SID Collection: downloading \"%s\"...", url.c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

        return url;
    }

    bool SourceHighVoltageSIDCollection::DownloadDatabase()
    {
        if (m_db.songs.IsEmpty())
        {
            CURL* curl = curl_easy_init();

            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
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

            int32_t downloadStatus = -1;
            for (uint32_t i = 0; i < _countof(ms_urls) && downloadStatus != 0; i++)
            {
                auto url = std::string(ms_urls[m_currentUrl]) + "DOCUMENTS/Songlengths.md5";
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                downloadStatus = Download(curl);
                if (downloadStatus != 0)
                {
                    buffer.Clear();
                    m_currentUrl = (m_currentUrl + 1) % _countof(ms_urls);
                }
            }

            curl_easy_cleanup(curl);

            if (downloadStatus == 0)
                DecodeDatabase(buffer.Items<char>(), buffer.Items<char>() + buffer.NumItems());
        }
        return m_db.songs.IsEmpty();
    }

    void SourceHighVoltageSIDCollection::DecodeDatabase(char* bufBegin, const char* bufEnd)
    {
        //first item is ignore as index 0 is null
        m_db.roots.Resize(1);
        m_db.artists.Resize(1);
        m_db.songs.Resize(1);
        m_db.strings.Add('\0');

        auto buf = bufBegin;
        char* lineEnd = nullptr;
        for (auto* line = buf; line < bufEnd; line = lineEnd + 1)
        {
            // go to song entry
            while (line < bufEnd && *line != ';')
                line++;
            lineEnd = line;

            // find end of line
            while (lineEnd < bufEnd && *lineEnd != '\n' && *lineEnd != '\r')
                lineEnd++;
            lineEnd[0] = 0;

            if (line < bufEnd)
            {
                line += 3; // skip "; /"
                std::string link(line);

                // get the root
                auto rootIndex = FindDatabaseRoot(line);
                if (rootIndex != 0)
                {
                    // get the artist if any
                    line += strlen(m_db.roots[rootIndex].name(m_db.strings)) + 1;
                    uint16_t artist = 0;
                    if (m_db.roots[rootIndex].hasArtist)
                    {
                        auto newArtist = line;
                        while (*line != '/')
                            line++;
                        *line++ = 0;

                        artist = FindDatabaseArtist(newArtist);
                    }

                    // song name
                    auto newSong = line;
                    line = lineEnd;

                    // strip the sid extension
                    while (*--line != '.');
                    *line = 0;

                    auto songIndex = m_db.songs.NumItems();
                    m_db.songs.Push();
                    m_db.songs.Last().name.Set(m_db.strings, newSong);
                    m_db.songs.Last().root = rootIndex;
                    if (m_db.songs.Last().artist = artist)
                    {
                        m_db.songs.Last().nextSong = m_db.artists[artist].songs;
                        m_db.artists[artist].songs = songIndex;
                        m_db.artists[artist].numSongs++;
                    }
                }
            }
        }
    }

    uint16_t SourceHighVoltageSIDCollection::FindDatabaseRoot(const char* newRoot)
    {
        std::string theRoot = newRoot;
        auto ofs = theRoot.find_last_of('.');
        if (ofs == theRoot.npos || _stricmp(theRoot.c_str() + ofs, ".sid") != 0)
        {
            Log::Warning("High Voltage SID Collection: missing sid extension for \"%s\"\n", newRoot);
            return 0;
        }

        // keep this unique path as part of our song name, otherwise, just use the two first directories as root
        if (_strnicmp(newRoot, "DEMOS/Commodore/", sizeof("DEMOS/Commodore/") - 1) == 0)
            theRoot = "DEMOS/";
        else
        {
            ofs = theRoot.find_first_of('/', theRoot.find_first_of('/') + 1);
            theRoot.resize(ofs);
        }

        auto numRoots = m_db.roots.NumItems();
        for (auto i = numRoots - 1; i > 0; i--)
        {
            auto& root = m_db.roots[i];

            if (theRoot == root.name(m_db.strings))
                return uint16_t(i);
        }

        m_db.roots.Push();
        m_db.roots.Last().name.Set(m_db.strings, theRoot);
        m_db.roots.Last().hasArtist = _strnicmp(newRoot, "MUSICIANS/", sizeof("MUSICIANS/") - 1) == 0;
        return uint16_t(numRoots);
    }

    uint16_t SourceHighVoltageSIDCollection::FindDatabaseArtist(const char* newArtist)
    {
        auto numArtists = m_db.artists.NumItems();
        for (auto i = numArtists - 1; i > 0; i--)
        {
            auto& artist = m_db.artists[i];
            if (artist.name.IsSame(m_db.strings, newArtist))
                return static_cast<uint16_t>(i);
        }
        m_db.artists.Push();
        m_db.artists.Last().name.Set(m_db.strings, newArtist);
        return uint16_t(numArtists);
    }
}
// namespace rePlayer
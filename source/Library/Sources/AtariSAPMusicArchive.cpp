#include "AtariSAPMusicArchive.h"

// Core
#include <Core/Log.h>
#include <Core/String.h>
#include <IO/File.h>
#include <IO/StreamMemory.h>
#include <JSON/json.hpp>

// rePlayer
#include <Database/Types/Countries.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>

// zlib
#include <zlib.h>

// curl
#include <Curl/curl.h>

// stl
#include <filesystem>

namespace rePlayer
{
    const char* const SourceAtariSAPMusicArchive::ms_filename = MusicPath "ASMA" MusicExt;

    inline const char* SourceAtariSAPMusicArchive::Chars::operator()(const Array<char>& blob) const
    {
        return blob.Items() + offset;
    }

    inline void SourceAtariSAPMusicArchive::Chars::Set(Array<char>& blob, const char* otherString)
    {
        offset = blob.NumItems();
        blob.Add(otherString, strlen(otherString) + 1);
    }

    inline void SourceAtariSAPMusicArchive::Chars::Set(Array<char>& blob, const std::string& otherString)
    {
        offset = blob.NumItems();
        blob.Add(otherString.c_str(), otherString.size() + 1);
    }

    template <typename T>
    inline void SourceAtariSAPMusicArchive::Chars::Copy(const Array<char>& blob, Array<T>& otherblob) const
    {
        auto src = blob.Items() + offset;
        otherblob.Copy(src, strlen(src) + 1);
    }

    inline bool SourceAtariSAPMusicArchive::Chars::IsSame(const Array<char>& blob, const char* otherString) const
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

    inline bool SourceAtariSAPMusicArchive::Chars::IsSame(const Array<char>& blob, const std::string otherString) const
    {
        return IsSame(blob, otherString.c_str());
    }

    SourceAtariSAPMusicArchive::SourceAtariSAPMusicArchive()
    {
        m_strings.Add('\0'); // 0 == empty string == invalid offset
        m_data.Add(uint8_t(0), alignof(SourceSong));
    }

    void SourceAtariSAPMusicArchive::FindArtists(ArtistsCollection& artists, const char* name)
    {
        if (DownloadDatabase())
            return;

        std::string lName = ToLower(name);
        for (auto& dbArtist : m_db.artists)
        {
            std::string lArtist = ToLower(dbArtist.name(m_db.strings));
            bool isFound = strstr(lArtist.c_str(), lName.c_str());
            for (uint32_t i = 0; !isFound && i < dbArtist.numHandles; i++)
            {
                lArtist = ToLower(m_db.handles[dbArtist.handleIndex + i](m_db.strings));
                if (strstr(lArtist.c_str(), lName.c_str()))
                    isFound = true;
            }
            if (isFound)
            {
                artists.matches.Push();
                auto& newArtist = artists.matches.Last();
                newArtist.id = SourceID(kID, FindArtist(dbArtist));
                newArtist.name = m_db.handles[dbArtist.handleIndex](m_db.strings);
                char buf[32];
                sprintf(buf, "%u %s", dbArtist.numSongs, dbArtist.numSongs <= 1 ? "song" : "songs");
                newArtist.description = buf;
                if (dbArtist.name.offset && dbArtist.name.offset != m_db.handles[dbArtist.handleIndex].offset)
                {
                    newArtist.description += "\nName   : ";
                    newArtist.description += dbArtist.name(m_db.strings);
                }
                if (dbArtist.numHandles > 1)
                {
                    newArtist.description += "\nHandles: ";
                    for (uint32_t i = 1; i < dbArtist.numHandles; i++)
                    {
                        if (i > 1)
                            newArtist.description += ", ";
                        newArtist.description += m_db.handles[dbArtist.handleIndex + i](m_db.strings);
                    }
                }
                if (dbArtist.country)
                {
                    newArtist.description += "\nCountry: ";
                    newArtist.description += Countries::GetName(dbArtist.country);
                }
            }
        }
    }

    void SourceAtariSAPMusicArchive::ImportArtist(SourceID importedArtistID, SourceResults& results)
    {
        assert(importedArtistID.sourceId == kID);
        results.importedArtists.Add(importedArtistID);

        if (DownloadDatabase())
            return;

        int32_t dbImportedArtistId = -1;
        {
            auto* name = m_artists[importedArtistID.internalId].name(m_strings);
            for (auto& dbArtist : m_db.artists)
            {
                if (GetArtistName(dbArtist) == name)
                {
                    dbImportedArtistId = int32_t(&dbArtist - m_db.artists.Items());
                    break;
                }
            }

            assert(dbImportedArtistId >= 0); // has the artist disappeared from ASMA?
            if (dbImportedArtistId < 0)
                Log::Error("Atari SAP Music Archive: can't find artist \"%s\"\n", name);
        }

        for (auto dbSongId = m_db.artists[dbImportedArtistId].songs; dbSongId;)
        {
            const auto& dbSong = DbSong(dbSongId);
            auto songSourceId = FindSong(dbSong);
            if (!results.IsSongAvailable(SourceID(kID, songSourceId)))
            {
                auto* song = new SongSheet();
                auto* songSource = GetSongSource(songSourceId);

                SourceResults::State state;
                song->id = songSource->songId;
                if (songSource->isDiscarded)
                    state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kDiscarded : SourceResults::kMerged);
                else
                    song->id == SongID::Invalid ? state.SetSongStatus(SourceResults::kNew).SetChecked(true) : state.SetSongStatus(SourceResults::kOwned);

                for (uint16_t i = 0; i < songSource->numArtists; i++)
                {
                    SourceID artistId(kID, songSource->artists[i]);
                    auto artistIdx = results.artists.FindIf<int32_t>([artistId](auto entry)
                    {
                        return entry->sources[0].id == artistId;
                    });
                    if (artistIdx < 0)
                    {
                        auto& dbArtist = m_db.artists[dbSong.artists[i].id];
                        artistIdx = results.artists.NumItems<int32_t>();
                        auto artist = new ArtistSheet();
                        for (uint16_t j = 0; j < dbArtist.numHandles; j++)
                            artist->handles.Add(m_db.handles[dbArtist.handleIndex + j](m_db.strings));
                        artist->realName = dbArtist.name(m_db.strings);
                        artist->countries.Add(dbArtist.country);
                        artist->sources.Add(artistId);
                        results.artists.Add(artist);
                    }
                    song->artistIds.Add(static_cast<ArtistID>(artistIdx));
                }

                song->type = Core::GetReplays().Find(std::filesystem::path(dbSong.path(m_db.strings)).extension().generic_string().c_str() + 1);
                song->name = dbSong.name(m_db.strings);
                song->releaseYear = dbSong.date;

                song->sourceIds.Add(SourceID(kID, songSourceId));
                results.songs.Add(song);
                results.states.Add(state);
            }

            for (uint16_t i = 0; i < dbSong.numArtists; i++)
            {
                if (dbSong.artists[i].id == uint16_t(dbImportedArtistId))
                {
                    dbSongId = dbSong.artists[i].nextSong;
                    break;
                }
            }
        }

        m_isDirty |= results.songs.IsNotEmpty();
    }

    void SourceAtariSAPMusicArchive::FindSongs(const char* name, SourceResults& collectedSongs)
    {
        if (DownloadDatabase())
            return;

        std::string lName = ToLower(name);

        for (auto songOffset : m_db.songs)
        {
            const auto& dbSong = DbSong(songOffset);
            std::string dbSongName(dbSong.name(m_db.strings));

            std::string rName = ToLower(dbSongName);
            if (strstr(rName.c_str(), lName.c_str()) == nullptr)
                continue;

            auto songSourceId = FindSong(dbSong);
            auto* song = new SongSheet();
            auto* songSource = GetSongSource(songSourceId);

            SourceResults::State state;
            song->id = songSource->songId;
            if (songSource->isDiscarded)
                state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kDiscarded : SourceResults::kMerged);
            else
                state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kNew : SourceResults::kOwned);

            for (uint16_t i = 0; i < songSource->numArtists; i++)
            {
                SourceID artistId(kID, songSource->artists[i]);
                auto artistIdx = collectedSongs.artists.FindIf<int32_t>([artistId](auto entry)
                {
                    return entry->sources[0].id == artistId;
                });
                if (artistIdx < 0)
                {
                    auto& dbArtist = m_db.artists[dbSong.artists[i].id];
                    artistIdx = collectedSongs.artists.NumItems<int32_t>();
                    auto artist = new ArtistSheet();
                    for (uint16_t j = 0; j < dbArtist.numHandles; j++)
                        artist->handles.Add(m_db.handles[dbArtist.handleIndex + j](m_db.strings));
                    artist->realName = dbArtist.name(m_db.strings);
                    artist->countries.Add(dbArtist.country);
                    artist->sources.Add(artistId);
                    collectedSongs.artists.Add(artist);
                }
                song->artistIds.Add(static_cast<ArtistID>(artistIdx));
            }

            song->type = Core::GetReplays().Find(std::filesystem::path(dbSong.path(m_db.strings)).extension().generic_string().c_str() + 1);
            song->name = dbSongName;
            song->releaseYear = dbSong.date;

            song->sourceIds.Add(SourceID(kID, songSourceId));
            collectedSongs.songs.Add(song);
            collectedSongs.states.Add(state);
        }
    }

    Source::Import SourceAtariSAPMusicArchive::ImportSong(SourceID sourceId, const std::string& path)
    {
        assert(sourceId.sourceId == kID);
        auto* songSource = GetSongSource(sourceId.internalId);

        CURL* curl = curl_easy_init();

        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);

        auto url = SetupUrl(curl, songSource);

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
            if (buffer.Size() < 256 && strstr((const char*)buffer.begin(), "404 Not Found"))
            {
                isEntryMissing = true;
                for (uint16_t i = 0; i < songSource->numArtists; i++)
                    m_areStringsDirty |= --m_artists[songSource->artists[i]].refcount == 0;
                m_areStringsDirty |= --m_roots[songSource->root].refcount == 0;
                *songSource = SourceSong();
                m_areDataDirty = true;
                m_availableSongIds.Add(sourceId.internalId);

                Log::Error("Atari SAP Music Archive: file \"%s\" not found\n", url.c_str());
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
            Log::Error("Atari SAP Music Archive: %s\n", curl_easy_strerror(curlError));

        curl_easy_cleanup(curl);

        return { stream, isEntryMissing };
    }

    void SourceAtariSAPMusicArchive::OnArtistUpdate(ArtistSheet* artist)
    {
        (void)artist;
    }

    void SourceAtariSAPMusicArchive::OnSongUpdate(const Song* const song)
    {
        assert(song->GetSourceId(0).sourceId == kID);
        auto* songSource = GetSongSource(song->GetSourceId(0).internalId);
        if (!songSource->IsValid())
        {
            for (uint16_t i = 0; i < songSource->numArtists; i++)
                m_artists[songSource->artists[i]].refcount++;
            m_roots[songSource->root].refcount++;
        }
        songSource->songId = song->GetId();
        songSource->isDiscarded = false;
        m_isDirty = true;
    }

    void SourceAtariSAPMusicArchive::DiscardSong(SourceID sourceId, SongID newSongId)
    {
        thread::ScopedSpinLock lock(m_mutex);
        assert(sourceId.sourceId == kID);
        auto* songSource = GetSongSource(sourceId.internalId);
        songSource->songId = newSongId;
        songSource->isDiscarded = true;
        m_isDirty = true;
    }

    void SourceAtariSAPMusicArchive::InvalidateSong(SourceID sourceId, SongID newSongId)
    {
        thread::ScopedSpinLock lock(m_mutex);
        assert(sourceId.sourceId == kID && newSongId != SongID::Invalid);
        auto songSource = GetSongSource(sourceId.internalId);
        songSource->songId = newSongId;
        songSource->isDiscarded = false;
        m_isDirty = true;
    }

    void SourceAtariSAPMusicArchive::Load()
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
            m_availableRootIds.Clear();
            for (uint32_t i = 0, e = m_roots.NumItems(); i < e; i++)
            {
                if (m_roots[i].refcount == 0)
                    m_availableRootIds.Add(uint16_t(i));
            }
            m_availableArtistIds.Clear();
            for (uint32_t i = 0, e = m_artists.NumItems(); i < e; i++)
            {
                if (m_artists[i].refcount == 0)
                    m_availableArtistIds.Add(uint16_t(i));
            }
            m_availableSongIds.Clear();
            for (uint32_t i = 0, e = m_songs.NumItems(); i < e; i++)
            {
                auto* song = GetSongSource(i);
                if (!song->IsValid())
                    m_availableSongIds.Add(i);
            }
        }
    }

    void SourceAtariSAPMusicArchive::Save() const
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
                    Array<char> strings(size_t(0), m_strings.NumItems());
                    strings.Add('\0');
                    // remove discarded roots at the end of the array
                    Array<SourceRoot> roots(m_roots);
                    for (auto i = roots.NumItems<int64_t>() - 1; i >= 0; i--)
                    {
                        if (roots[i].refcount != 0)
                            break;
                        roots.Pop();
                    }
                    // re-pack roots
                    for (uint32_t i = 0, e = roots.NumItems(); i < e; i++)
                    {
                        if (roots[i].refcount == 0)
                        {
                            roots[i] = {};
                            continue;
                        }
                        if (roots[i].label.offset)
                            roots[i].label.Set(strings, roots[i].label(m_strings));
                    }
                    // remove discarded artists at the end of the array
                    Array<SourceArtist> artists(m_artists);
                    for (auto i = artists.NumItems<int64_t>() - 1; i >= 0; i--)
                    {
                        if (artists[i].refcount != 0)
                            break;
                        artists.Pop();
                    }
                    // re-pack the artists
                    for (uint32_t i = 0, e = artists.NumItems(); i < e; i++)
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
                    Array<uint8_t> data(size_t(0), m_data.NumItems<size_t>());
                    data.Add(m_data.Items(), alignof(SourceSong));
                    // remove discarded songs at the end of the array
                    Array<uint32_t> songs(m_songs);
                    for (auto i = m_songs.NumItems<int64_t>() - 1; i >= 0; i--)
                    {
                        auto* song = GetSongSource(i);
                        if (song->songId != SongID::Invalid || song->isDiscarded)
                            break;
                        songs.Pop();
                    }
                    // re-pack the songs
                    for (uint32_t i = 0, e = songs.NumItems(); i < e; i++)
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
                        data.Copy(song, offsetof(SourceSong, artists) + song->numArtists * sizeof(uint16_t));
                        data.Copy(song->name(), strlen(song->name()) + 1);
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

    std::string SourceAtariSAPMusicArchive::GetArtistName(const ASMAArtist& dbArtist) const
    {
        std::string name = dbArtist.name(m_db.strings);
        for (uint32_t i = dbArtist.name.offset != 0 && dbArtist.name.offset == m_db.handles[dbArtist.handleIndex].offset ? 1 : 0; i < Min(dbArtist.numHandles, 2u); i++)
            name += m_db.handles[dbArtist.handleIndex + i](m_db.strings);
        return name;
    }

    uint16_t SourceAtariSAPMusicArchive::FindArtist(const ASMAArtist& dbArtist)
    {
        auto name = GetArtistName(dbArtist);
        // look in our database
        for (uint32_t i = 0, e = m_artists.NumItems(); i < e; i++)
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
        artist->name.Set(m_strings, name);
        m_areStringsDirty = true; // this artist may never be used
        return uint16_t(artist - m_artists);
    }

    uint32_t SourceAtariSAPMusicArchive::FindSong(const ASMASong& dbSong)
    {
        // look in our database (this is ugly/slow)
        for (uint32_t i = 0, e = m_songs.NumItems(); i < e; i++)
        {
            if (m_songs[i] == 0)
                continue;
            auto& song = SrcSong(m_songs[i]);
            if (dbSong.path.IsSame(m_db.strings, song.name()))
            {
                if (!m_db.roots[dbSong.root].IsSame(m_db.strings, m_roots[song.root].id, m_roots[song.root].label(m_strings)))
                    continue;
                if (dbSong.numArtists != song.numArtists)
                    continue;
                uint16_t artistIndex = 0;
                for (; artistIndex < dbSong.numArtists; artistIndex++)
                {
                    if (!m_artists[song.artists[artistIndex]].name.IsSame(m_strings, GetArtistName(m_db.artists[dbSong.artists[artistIndex].id])))
                        break;
                }
                if (artistIndex != dbSong.numArtists)
                    continue;

                return i;
            }
        }

        // add a new one
        thread::ScopedSpinLock lock(m_mutex);
        uint32_t id = m_availableSongIds.IsEmpty() ? m_songs.Push<uint32_t>() : m_availableSongIds.Pop();
        m_songs[id] = m_data.NumItems();
        auto* songSource = new (m_data.Push(offsetof(SourceSong, artists) + dbSong.numArtists * sizeof(uint16_t))) SourceSong();
        // look for the root in our database
        auto rootId = m_roots.FindIf<int32_t>([&](auto& root)
        {
            return m_db.roots[dbSong.root].IsSame(m_db.strings, root.id, root.label(m_strings));
        });
        if (rootId < 0)
        {
            // add a new root
            rootId = m_availableRootIds.IsEmpty() ? m_roots.Push<int32_t>() : m_availableRootIds.Pop();
            auto* root = m_roots.Items(rootId);
            root->id = m_db.roots[dbSong.root].id;
            if (m_db.roots[dbSong.root].label)
            {
                root->label.Set(m_strings, m_db.roots[dbSong.root].Label(m_db.strings));
                m_areStringsDirty = true;
            }
        }
        songSource->root = uint16_t(rootId);
        // artists
        songSource->numArtists = dbSong.numArtists;
        for (uint16_t i = 0; i < dbSong.numArtists; i++)
            songSource->artists[i] = FindArtist(m_db.artists[dbSong.artists[i].id]);
        // name
        dbSong.path.Copy(m_db.strings, m_data);
        m_data.Resize((m_data.NumItems() + alignof(SourceSong) - 1) & ~(alignof(SourceSong) - 1));

        m_areDataDirty = true;

        return id;
    }

    std::string SourceAtariSAPMusicArchive::SetupUrl(CURL* curl, SourceSong* songSource) const
    {
        std::string url("https://asma.atari.org/asma/");
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

        static const char* const roots[] = {
            "", "", "Composers/", "Games/", "Groups/", "Misc/", "Unknown/"
        };

        auto& root = m_roots[songSource->root];
        if (root.id > RootID::kNone)
            url += roots[size_t(root.id)];
        if (root.label.offset)
        {
            unescape(root.label(m_strings));
            url += "/";
        }
        unescape(songSource->name());

        Log::Message("Atari SAP Music Archive: downloading \"%s\"...", url.c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

        return url;
    }

    bool SourceAtariSAPMusicArchive::DownloadDatabase()
    {
        if (m_db.songs.IsEmpty())
        {
            // download database
            CURL* curl = curl_easy_init();

            char errorBuffer[CURL_ERROR_SIZE];
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
            curl_easy_setopt(curl, CURLOPT_URL, "https://asma.atari.org/asmadb/asmadb.js");
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
            curl_easy_perform(curl);
            curl_easy_cleanup(curl);

            if (buffer.IsNotEmpty() && strstr(buffer.Items<char>(), "const asma =") == buffer.Items<char>())
            {
                // add null entries
                m_db.strings.Add('\0');
                m_db.data.Add(uint8_t(0), alignof(ASMASong));

                // add default roots
                m_db.roots.Add({ RootID::kNone, 0 });
                m_db.roots.Add({ RootID::kGames, 0 });
                m_db.roots.Add({ RootID::kMisc, 0 });
                m_db.roots.Add({ RootID::kUnknown, 0 });

                auto json = nlohmann::json::parse(buffer.begin() + sizeof("const asma ="), buffer.end());

                // parse db composers
                auto& composerInfos = json["composerInfos"];
                m_db.artists.Reserve(composerInfos.size());
                for (auto& composerInfo : composerInfos)
                {
                    auto* artist = m_db.artists.Push();
                    auto name = composerInfo["lastName"].get<std::string>();
                    if (!name.empty() && name != "<?>")
                    {
                        name = composerInfo["firstName"].get<std::string>() + std::string(" ") + name;
                        artist->name.Set(m_db.strings, name);
                    }
                    else
                        artist->name = {};

                    artist->handleIndex = m_db.handles.NumItems<uint16_t>();
                    auto handles = composerInfo["handles"].empty() ? std::string() : composerInfo["handles"].get<std::string>();
                    if (handles.empty())
                    {
                        auto* h = m_db.handles.Push();
                        *h = artist->name;
                        artist->numHandles = 1;
                    }
                    else
                    {
                        artist->numHandles = 0;
                        size_t offset = 0;
                        for (;;)
                        {
                            auto* h = m_db.handles.Push();
                            auto nextOffset = handles.find(", ", offset);
                            auto handle = handles.substr(offset, nextOffset);
                            h->Set(m_db.strings, handle);
                            artist->numHandles++;
                            if (nextOffset == handles.npos)
                                break;
                            offset = nextOffset + 2;
                        }
                    }

                    auto country = composerInfo["country"].empty() ? std::string() : composerInfo["country"].get<std::string>();
                    if (!country.empty())
                    {
                        artist->country = Countries::GetCode(country.c_str());
                        if (artist->country == 0)
                            assert(0 && "todo find the right country");
                    }
                }

                // parse db songs
                auto& fileInfos = json["fileInfos"];
                m_db.songs.Reserve(fileInfos.size());
                for (auto& fileInfo : fileInfos)
                {
                    auto songOffset = m_db.data.Push<uint32_t>(sizeof(ASMASong));
                    m_db.songs.Add(songOffset);

                    auto filePath = fileInfo["filePath"].get<std::string>();
                    DbSong(songOffset).root = FindDatabaseRoot(filePath);
                    DbSong(songOffset).path.Set(m_db.strings, filePath);
                    DbSong(songOffset).name.Set(m_db.strings, fileInfo["title"].get<std::string>());
                    FindDatabaseArtists(fileInfo["author"].get<std::string>(), songOffset);

                    DbSong(songOffset).date = 0;
                    auto date = fileInfo["date"].get<std::string>();
                    if (!date.empty())
                    {
                        if (date.find(" <?>") == date.npos)
                            sscanf_s(date.c_str() + date.size() - 4, "%hu", &DbSong(songOffset).date);
                        else
                            sscanf_s(date.c_str() + date.size() - 8, "%hu <?>", &DbSong(songOffset).date);
                    }
                }
            }
        }
        return m_db.songs.IsEmpty();
    }

    uint32_t SourceAtariSAPMusicArchive::FindDatabaseRoot(std::string& filePath)
    {
        RootID id = RootID::kNone;
        struct
        {
            RootID id;
            uint32_t size;
            const char* const label;
        } static constexpr roots[] = {
            { RootID::kComposers, sizeof("Composers/") - 1, "Composers/" },
            { RootID::kGames, sizeof("Games/") - 1, "Games/"},
            { RootID::kGroups, sizeof("Groups/") - 1, "Groups/" },
            { RootID::kMisc, sizeof("Misc/") - 1, "Misc/" },
            { RootID::kUnknown, sizeof("Unknown/") - 1, "Unknown/" },
        };
        for (auto& r : roots)
        {
            if (strstr(filePath.c_str(), r.label) == filePath.c_str())
            {
                id = r.id;
                filePath = filePath.substr(r.size);
                break;
            }
        }
        if (id == RootID::kNone)
            return 0;
        else if (id == RootID::kGames)
            return 1;
        else if (id == RootID::kMisc)
            return 2;
        else if (id == RootID::kUnknown)
            return 3;

        std::filesystem::path path(filePath);
        path.remove_filename();
        auto label = path.generic_string();
        label.pop_back();

        auto* root = m_db.roots.FindIf([&](auto& root)
        {
            return root.IsSame(m_db.strings, id, label.c_str());
        });
        if (root == nullptr)
        {
            root = m_db.roots.Push();
            root->id = id;
            Chars c;
            c.Set(m_db.strings, label);
            root->label = c.offset;
        }
        filePath = std::filesystem::path(filePath).filename().generic_string();

        return root - m_db.roots;
    }

    uint32_t SourceAtariSAPMusicArchive::FindDatabaseArtist(const std::string& author)
    {
        auto covered = [](std::string& name)
        {
            auto offset = name.find("/ covered by ");
            if (offset != name.npos)
                name = name.substr(offset + sizeof("/ covered by ") - 1);
        };

        std::string name1, name2;
        auto offset = author.find(" (");
        if (offset != author.npos)
        {
            name1 = author.substr(0, offset);
            offset += 2;
            if (name1 == "<?>")
                name1 = author.substr(offset, author.find(')', offset + 1) - offset);
            else
            {
                name2 = author.substr(offset, author.find(')', offset + 1) - offset);
                covered(name2);
            }
            covered(name1);
        }
        else
        {
            name1 = author;
            covered(name1);
        }

        auto* artist = m_db.artists.FindIf([&](auto& artist)
        {
            uint8_t isFound = name2.empty() ? 2 : 0;
            if (artist.name.IsSame(m_db.strings, name1))
                isFound |= 1;
            if (artist.name.IsSame(m_db.strings, name2))
                isFound |= 2;
            for (uint32_t i = 0; i < artist.numHandles; i++)
            {
                if (m_db.handles[artist.handleIndex + i].IsSame(m_db.strings, name1))
                    isFound |= 1;
                if (m_db.handles[artist.handleIndex + i].IsSame(m_db.strings, name2))
                    isFound |= 2;
            }
            return isFound == 3;
        });
        if (artist == nullptr)
        {
            artist = m_db.artists.Push();
            artist->handleIndex = m_db.handles.NumItems<uint16_t>();
            artist->numHandles = name2.empty() ? 1 : 2;
            if (!name2.empty())
                m_db.handles.Push()->Set(m_db.strings, name2);
            m_db.handles.Push()->Set(m_db.strings, name1);
            artist->numSongs = 0;
            artist->songs = 0;
        }
        return artist - m_db.artists;
    }

    void SourceAtariSAPMusicArchive::FindDatabaseArtists(std::string author, uint32_t songOffset)
    {
        DbSong(songOffset).numArtists = 0;
        if (author.empty())
            return;

        size_t offset = 0;
        for (;;)
        {
            m_db.data.Push(sizeof(ASMASong::Artist));
            auto nextOffset = author.find(" & ", offset);
            auto artistIndex = FindDatabaseArtist(author.substr(offset, nextOffset));
            DbSong(songOffset).artists[DbSong(songOffset).numArtists].id = artistIndex;
            DbSong(songOffset).artists[DbSong(songOffset).numArtists].nextSong = m_db.artists[artistIndex].songs;
            DbSong(songOffset).numArtists++;
            m_db.artists[artistIndex].songs = songOffset;
            m_db.artists[artistIndex].numSongs++;
            if (nextOffset == author.npos)
                break;
            offset = nextOffset + 3;
        }
    }
}
// namespace rePlayer
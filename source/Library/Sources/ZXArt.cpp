#include "ZXArt.h"
#include "WebHandler.h"

// Core
#include <Core/Log.h>
#include <IO/File.h>
#include <IO/StreamMemory.h>
#include <JSON/json.hpp>

// rePlayer
#include <Database/Types/Countries.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>

// zlib
#include <zlib.h>

// stl
#include <algorithm>

namespace rePlayer
{
    const char* const SourceZXArt::ms_filename = MusicPath "ZXArt" MusicExt;

    inline const char* SourceZXArt::Chars::operator()(const Array<char>& blob) const
    {
        return blob.Items() + offset;
    }

    inline void SourceZXArt::Chars::Set(Array<char>& blob, const char* otherString)
    {
        offset = blob.NumItems();
        blob.Add(otherString, strlen(otherString) + 1);
    }

    inline void SourceZXArt::Chars::Set(Array<char>& blob, const std::string& otherString)
    {
        offset = blob.NumItems();
        blob.Add(otherString.c_str(), otherString.size() + 1);
    }

    template <typename T>
    inline void SourceZXArt::Chars::Copy(const Array<char>& blob, Array<T>& otherblob) const
    {
        auto src = blob.Items() + offset;
        otherblob.Add(src, strlen(src) + 1);
    }

    inline bool SourceZXArt::Chars::IsSame(const Array<char>& blob, const char* otherString) const
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

    inline bool SourceZXArt::Chars::IsSame(const Array<char>& blob, const std::string otherString) const
    {
        return IsSame(blob, otherString.c_str());
    }

    SourceZXArt::SourceZXArt()
    {}

    SourceZXArt::~SourceZXArt()
    {}

    void SourceZXArt::FindArtists(ArtistsCollection& artists, const char* name)
    {
        if (DownloadDatabase())
            return;

        std::string lName = ToLower(name);
        for (auto& artist : m_db.artists)
        {
            bool isFound = false;
            bool isMatch = false;
            Chars handle = artist.handles;
            for (uint16_t i = 0; i < artist.numHandles; i++)
            {
                auto lSearch = ToLower(handle(m_db.strings));
                if (isFound = strstr(lSearch.c_str(), lName.c_str()))
                {
                    isMatch = i == 0;
                    break;
                }
                handle.offset += uint32_t(lSearch.size()) + 1;
            }
            if (!isFound)
            {
                auto lSearch = ToLower(artist.realName(m_db.strings));
                isFound = strstr(lSearch.c_str(), lName.c_str());
            }
            if (isFound)
            {
                auto* newArtist = isMatch ? artists.matches.Push() : artists.alternatives.Push();
                newArtist->id = { kID, artist.id };
                newArtist->name = artist.handles(m_db.strings);
                if (artist.numHandles > 1)
                {
                    handle = artist.handles;
                    handle.offset += uint32_t(newArtist->name.size()) + 1;
                    for (uint16_t i = 1; i < artist.numHandles; i++)
                    {
                        if (i == 1)
                            newArtist->description = "Alias    : ";
                        else
                            newArtist->description += "\n           ";
                        auto size = newArtist->description.size();
                        newArtist->description += handle(m_db.strings);
                        handle.offset += uint32_t(newArtist->description.size() - size) + 1;
                    }
                }
                if (artist.realName.offset)
                {
                    if (!newArtist->description.empty())
                        newArtist->description += "\n";
                    newArtist->description += "Real Name: ";
                    newArtist->description += artist.realName(m_db.strings);
                }
                if (artist.country)
                {
                    if (!newArtist->description.empty())
                        newArtist->description += "\n";
                    newArtist->description += "Country  : ";
                    newArtist->description += Countries::GetName(artist.country);
                }
                if (!newArtist->description.empty())
                    newArtist->description += "\n";
                newArtist->description += "Songs    : ";
                char txt[16];
                sprintf(txt, "%u", artist.numSongs);
                newArtist->description += txt;
            }
        }
    }

    void SourceZXArt::ImportArtist(SourceID importedArtistID, SourceResults& results)
    {
        assert(importedArtistID.sourceId == kID);
        results.importedArtists.Add(importedArtistID);

        CURL* curl = curl_easy_init();

        char url[128];
        sprintf(url, "https://zxart.ee/api/export:zxMusic/language:eng/filter:authorId=%u", importedArtistID.internalId);

        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
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
        curl_easy_perform(curl);

        GetSongs(results, buffer, true, curl);

        curl_easy_cleanup(curl);
    }

    void SourceZXArt::FindSongs(const char* name, SourceResults& collectedSongs)
    {
        CURL* curl = curl_easy_init();

        std::string url = "https://zxart.ee/api/types:zxMusic/export:zxMusic/language:eng/filter:zxMusicTitleSearch=";
        auto e = curl_easy_escape(curl, name, int(strlen(name)));
        url += e;
        curl_free(e);

        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
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

        GetSongs(collectedSongs, buffer, false, curl);

        curl_easy_cleanup(curl);
    }

    std::pair<SmartPtr<io::Stream>, bool> SourceZXArt::ImportSong(SourceID sourceId, const std::string& path)
    {
        SourceID sourceToDownload = sourceId;
        assert(sourceToDownload.sourceId == kID);

        CURL* curl = curl_easy_init();

        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
        char url[128];
        sprintf(url, "https://zxart.ee/file/id:%u/", sourceToDownload.internalId);
        Log::Message("ZX-Art: downloading \"%s\"...", url);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, Core::GetLabel());

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
            if (buffer.IsEmpty())
            {
                isEntryMissing = true;
                auto* song = FindSong(sourceToDownload.internalId);
                m_songs.RemoveAt(song - m_songs);
                m_isDirty = true;

                Log::Error("ZX-Art: file \"%s\" not found\n", url);
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
            Log::Error("ZX-Art: %s\n", curl_easy_strerror(curlError));

        curl_easy_cleanup(curl);

        return { stream, isEntryMissing };
    }

    void SourceZXArt::OnArtistUpdate(ArtistSheet* artist)
    {
        assert(artist->sources[0].id.sourceId == kID);

        CURL* curl = curl_easy_init();

        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

        SmartPtr<ArtistSheet> newArtist = GetArtist(artist->sources[0].id.internalId, curl);
        artist->handles = std::move(newArtist->handles);
        artist->realName = std::move(newArtist->realName);
        artist->countries = std::move(newArtist->countries);

        curl_easy_cleanup(curl);
    }

    void SourceZXArt::OnSongUpdate(const Song* const song)
    {
        assert(song->GetSourceId(0).sourceId == kID);
        auto foundSong = FindSong(song->GetSourceId(0).internalId);
        if (foundSong == nullptr)
            foundSong = AddSong(song->GetSourceId(0).internalId);
        foundSong->songId = song->GetId();
        foundSong->isDiscarded = false;
        m_isDirty = true;
    }

    void SourceZXArt::DiscardSong(SourceID sourceId, SongID newSongId)
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

    void SourceZXArt::InvalidateSong(SourceID sourceId, SongID newSongId)
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

    void SourceZXArt::Load()
    {
        auto file = io::File::OpenForRead(ms_filename);
        if (file.IsValid())
        {
            if (file.Read<uint64_t>() != SongSource::kVersion)
            {
                assert(0 && "file read error");
                return;
            }
            file.Read<uint32_t>(m_songs);
        }
    }

    void SourceZXArt::Save() const
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
                file.Write<uint32_t>(m_songs);
                m_isDirty = false;
            }
        }
    }

    SourceZXArt::SongSource* SourceZXArt::AddSong(uint32_t id)
    {
        auto* song = std::lower_bound(m_songs.begin(), m_songs.end(), id, [](auto& song, auto id)
        {
            return song.id < id;
        });
        return m_songs.Insert(song - m_songs, SongSource(id));
    }

    SourceZXArt::SongSource* SourceZXArt::FindSong(uint32_t id) const
    {
        auto* song = std::lower_bound(m_songs.begin(), m_songs.end(), id, [](auto& song, auto id)
        {
            return song.id < id;
        });
        return song != m_songs.end() && song->id == id ? const_cast<SongSource*>(song) : nullptr;
    }

    void SourceZXArt::GetSongs(SourceResults& collectedSongs, const Array<uint8_t>& buffer, bool isCheckable, CURL* curl) const
    {
        if (buffer.IsNotEmpty())
        {
            auto json = nlohmann::json::parse(buffer.begin(), buffer.end());
            for (auto& zxMusic : json["responseData"]["zxMusic"])
            {
                auto song = new SongSheet();

                auto searchSongId = zxMusic["id"].get<uint32_t>();
                song->sourceIds.Add(SourceID(kID, searchSongId));

                SourceResults::State state;
                if (auto sourceSong = FindSong(searchSongId))
                {
                    song->id = sourceSong->songId;
                    if (sourceSong->isDiscarded)
                        state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kDiscarded : SourceResults::kMerged);
                    else
                        song->id == SongID::Invalid ? state.SetSongStatus(SourceResults::kNew).SetChecked(isCheckable) : state.SetSongStatus(SourceResults::kOwned);
                }
                else
                {
                    state.SetSongStatus(SourceResults::kNew).SetChecked(isCheckable);
                }

                song->name = ConvertEntities(zxMusic["title"].get<std::string>());
                if (zxMusic.contains("internalTitle"))
                    song->name.String() += " / " + ConvertEntities(zxMusic["internalTitle"].get<std::string>());
                if (zxMusic.contains("year"))
                {
                    uint32_t year = 0;
                    sscanf(zxMusic["year"].get<std::string>().c_str(), "%u", &year);
                    song->releaseYear = uint16_t(year);
                }
                song->type = Core::GetReplays().Find(zxMusic["type"].get<std::string>().c_str());
                if (song->type.ext == eExtension::Unknown)
                    song->name.String() += std::string(".") + zxMusic["type"].get<std::string>();
                for (auto& author : zxMusic["authorIds"])
                {
                    auto authorId = author.get<uint32_t>();
                    auto it = std::find_if(collectedSongs.artists.begin(), collectedSongs.artists.end(), [authorId](auto entry)
                    {
                        return entry->sources[0].id.sourceId == kID && entry->sources[0].id.internalId == authorId;
                    });
                    auto newArtistId = static_cast<ArtistID>(it - collectedSongs.artists.begin());
                    if (it == collectedSongs.artists.end())
                    {
                        if (auto newArtist = GetArtist(authorId, curl))
                        {
                            collectedSongs.artists.Add(newArtist);
                            song->artistIds.Add(newArtistId);
                        }
                    }
                    else
                        song->artistIds.Add(newArtistId);
                }
                collectedSongs.songs.Add(song);
                collectedSongs.states.Add(state);
            }
        }
    }

    ArtistSheet* SourceZXArt::GetArtist(uint32_t id, CURL* curl) const
    {
        ArtistSheet* artist = nullptr;

        char url[128];
        sprintf(url, "http://zxart.ee/api/export:author/language:eng/filter:authorId=%u", id);
        curl_easy_setopt(curl, CURLOPT_URL, url);

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
        if (buffer.IsNotEmpty())
        {
            auto json = nlohmann::json::parse(buffer.begin(), buffer.end());
            if (json["totalAmount"].get<uint32_t>() != 0)
            {
                artist = new ArtistSheet();
                artist->sources.Add(SourceID(kID, id));

                auto& author = json["responseData"]["author"][0];
                artist->handles.Add(author["title"].get<std::string>());
                if (author.contains("realName"))
                    artist->realName = author["realName"].get<std::string>();
                if (author.contains("country"))
                {
                    auto country = author["country"].get<std::string>();
                    if (country != "Information removed")
                    {
                        auto countryCode = Countries::GetCode(country.c_str());
                        if (countryCode == 0)
                            assert(0 && "todo find the right country");
                        else
                            artist->countries.Add(countryCode);
                    }
                }
                if (author.contains("aliases"))
                {
                    for (auto& alias : author["aliases"])
                    {
                        sprintf(url, "http://zxart.ee/api/export:authorAlias/language:eng/filter:authorAliasId=%u", alias.get<uint32_t>());
                        curl_easy_setopt(curl, CURLOPT_URL, url);

                        buffer.Clear();
                        curl_easy_perform(curl);

                        if (buffer.IsNotEmpty())
                        {
                            auto jsonAlias = nlohmann::json::parse(buffer.begin(), buffer.end());
                            auto aliases = jsonAlias["responseData"]["authorAlias"];
                            for (auto& aliasObject : aliases)
                            {
                                if (aliasObject.contains("title"))
                                    artist->handles.Add(aliasObject["title"].get<std::string>());
                            }
                        }
                    }
                }
            }
        }
        return artist;
    }

    bool SourceZXArt::DownloadDatabase()
    {
        if (m_db.artists.IsEmpty())
        {
            CURL* curl = curl_easy_init();

            char errorBuffer[CURL_ERROR_SIZE];
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
            curl_easy_setopt(curl, CURLOPT_URL, "https://zxart.ee/api/export:authorAlias/language:eng/filter:authorAliasAll");
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
            if (buffer.IsNotEmpty())
            {
                Array<std::pair<uint32_t, std::string>> aliases;
                {
                    auto jsonAllAliases = nlohmann::json::parse(buffer.begin(), buffer.end());
                    aliases.Reserve(jsonAllAliases["responseData"]["authorAlias"].size());
                    for (auto& jsonAlias : jsonAllAliases["responseData"]["authorAlias"])
                    {
                        if (jsonAlias.contains("title"))
                        {
                            auto* alias = aliases.Push();
                            alias->first = jsonAlias["id"].get<uint32_t>();
                            alias->second = jsonAlias["title"].get<std::string>();
                        }
                    }
                }

                buffer.Clear();
                curl_easy_setopt(curl, CURLOPT_URL, "https://zxart.ee/api/export:author/language:eng/filter:authorAll");
                curl_easy_perform(curl);
                if (buffer.IsNotEmpty())
                {
                    auto jsonAllAuthors = nlohmann::json::parse(buffer.begin(), buffer.end());
                    for (auto& author : jsonAllAuthors["responseData"]["author"])
                    {
                        if (!author.contains("tunesQuantity"))
                            continue;

                        auto* artist = m_db.artists.Push();
                        artist->id = author["id"].get<uint32_t>();
                        sscanf(author["tunesQuantity"].get<std::string>().c_str(), "%u", &artist->numSongs);

                        auto title = author["title"].get<std::string>();
                        artist->handles.Set(m_db.strings, title);

                        if (author.contains("aliases"))
                        {
                            artist->numHandles += uint16_t(author["aliases"].size());
                            for (auto& authorAlias : author["aliases"])
                            {
                                auto aliasId = authorAlias.get<uint32_t>();
                                if (auto* alias = aliases.FindIf([aliasId](auto& item)
                                {
                                    return item.first == aliasId;
                                }))
                                {
                                    Chars c;
                                    c.Set(m_db.strings, alias->second);
                                    aliases.RemoveAtFast(alias - aliases);
                                }
                            }
                        }
                        if (author.contains("realName"))
                        {
                            auto realName = author["realName"].get<std::string>();
                            artist->realName.Set(m_db.strings, realName);
                        }
                        if (author.contains("country"))
                        {
                            auto country = author["country"].get<std::string>();
                            if (country != "Information removed")
                            {
                                auto countryCode = Countries::GetCode(country.c_str());
                                if (countryCode == 0)
                                    assert(0 && "todo find the right country");
                                else
                                    artist->country = countryCode;
                            }
                        }
                    }
                }
            }
            curl_easy_cleanup(curl);
        }
        return m_db.artists.IsEmpty();
    }
}
// namespace rePlayer
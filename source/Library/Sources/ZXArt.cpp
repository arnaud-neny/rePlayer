// Core
#include <Core/Log.h>
#include <Core/String.h>
#include <ImGui.h>
#include <IO/File.h>
#include <IO/StreamMemory.h>
#include <JSON/json.hpp>

// rePlayer
#include <Database/Database.h>
#include <Database/Types/Countries.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>
#include <UI/BusySpinner.h>
#include "ZXArt.h"

// curl
#include <Curl/curl.h>

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
        blob.Add(otherString, uint32_t(strlen(otherString) + 1));
    }

    inline void SourceZXArt::Chars::Set(Array<char>& blob, const std::string& otherString)
    {
        offset = blob.NumItems();
        blob.Add(otherString.c_str(), uint32_t(otherString.size() + 1));
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
        : Source(true)
    {}

    SourceZXArt::~SourceZXArt()
    {}

    void SourceZXArt::FindArtists(ArtistsCollection& artists, const char* name, BusySpinner& busySpinner)
    {
        if (DownloadDatabase(busySpinner))
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

    void SourceZXArt::ImportArtist(SourceID importedArtistID, SourceResults& results, BusySpinner& busySpinner)
    {
        assert(importedArtistID.sourceId == kID);
        results.importedArtists.Add(importedArtistID);

        CURL* curl = curl_easy_init();

        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, Core::GetLabel());

        struct Buffer : public Array<uint8_t>
        {
            static size_t Writer(const uint8_t* data, size_t size, size_t nmemb, Buffer* buffer)
            {
                buffer->Add(data, uint32_t(size * nmemb));
                return size * nmemb;
            }
        } buffer;

        auto* message = busySpinner.Info("importing artist at %u", 0);
        CURLcode curlError = CURLE_OK;
        for (uint32_t start = 0; curlError == CURLE_OK;)
        {
            busySpinner.UpdateMessageParam(message, start);
            buffer.Clear();

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Buffer::Writer);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

            char url[1024];
            sprintf(url, "https://zxart.ee/api/export:zxMusic/language:eng/start:%u/filter:authorId=%u", start, importedArtistID.internalId);
            curl_easy_setopt(curl, CURLOPT_URL, url);
            Log::Message("ZXArt: fetching author %u at %u\n", uint32_t(importedArtistID.internalId), start);
            curlError = curl_easy_perform(curl);
            if (curlError == CURLE_OK)
            {
                if (GetSongs(results, buffer, true, curl, start))
                    break;
            }
        }

        curl_easy_cleanup(curl);
    }

    void SourceZXArt::FindSongs(const char* name, SourceResults& collectedSongs, BusySpinner& busySpinner)
    {
        CURL* curl = curl_easy_init();

        auto curlName = curl_easy_escape(curl, name, int(strlen(name)));

        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, Core::GetLabel());

        struct Buffer : public Array<uint8_t>
        {
            static size_t Writer(const uint8_t* data, size_t size, size_t nmemb, Buffer* buffer)
            {
                buffer->Add(data, uint32_t(size * nmemb));
                return size * nmemb;
            }
        } buffer;

        auto* message = busySpinner.Info("looking music title at %u", 0);
        CURLcode curlError = CURLE_OK;
        for (uint32_t start = 0; curlError == CURLE_OK;)
        {
            busySpinner.UpdateMessageParam(message, start);
            buffer.Clear();

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Buffer::Writer);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

            char url[1024];
            sprintf(url, "https://zxart.ee/api/types:zxMusic/export:zxMusic/language:eng/start:%u/filter:zxMusicTitleSearch=%s", start, curlName);
            curl_easy_setopt(curl, CURLOPT_URL, url);
            Log::Message("ZXArt: fetching zxMusicTitleSearch %s at %u\n", name, start);
            curlError = curl_easy_perform(curl);
            if (curlError == CURLE_OK)
            {
                if (GetSongs(collectedSongs, buffer, false, curl, start))
                    break;
            }
        }

        curl_free(curlName);

        curl_easy_cleanup(curl);
    }

    Source::Import SourceZXArt::ImportSong(SourceID sourceId, const std::string& path)
    {
        thread::ScopedSpinLock lock(m_mutex);
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
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, Core::GetLabel());

        struct Buffer : public Array<uint8_t>
        {
            static size_t Writer(const uint8_t* data, size_t size, size_t nmemb, Buffer* buffer)
            {
                buffer->Add(data, uint32_t(size * nmemb));
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

                Log::Error("ZX-Art: file \"%s\" not found\n", url);
            }
            else
            {
                auto* songSource = FindSong(sourceToDownload.internalId);
                songSource->crc = crc32(0L, Z_NULL, 0);
                songSource->crc = crc32_z(songSource->crc, buffer.Items(), buffer.Size<size_t>());
                songSource->size = buffer.Size<uint32_t>();

                stream = io::StreamMemory::Create(path, buffer.Items(), buffer.Size(), false);
                buffer.Detach();

                Log::Message("OK\n");
            }
            m_isDirty = true;
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
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, Core::GetLabel());

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

    void SourceZXArt::InvalidateSong(SourceID sourceId, SongID newSongId)
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

    void SourceZXArt::Load()
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
                file.Write(kMusicFileStamp);
                file.Write(Core::GetVersion());
                file.Write<uint32_t>(m_songs);
                m_isDirty = false;
            }
        }
    }

    void SourceZXArt::BrowserInit(BrowserContext& context)
    {
        if (m_db.artists.IsEmpty())
        {
            context.busySpinner.New(ImGui::GetColorU32(ImGuiCol_ButtonHovered));
            Core::AddJob([this, &context]()
            {
                DownloadDatabase(*context.busySpinner);

                Core::FromJob([this, &context]()
                {
                    context.busySpinner.Reset();
                    if (m_db.artists.IsEmpty())
                        context.Invalidate();
                });
            });
        }
        static const char* columnNames[] = { "Name", "Artist", "Year", "ID" };
        context.numColumns = NumItemsOf(columnNames);
        context.columnNames = columnNames;
    }

    void SourceZXArt::BrowserPopulate(BrowserContext& context, const ImGuiTextFilter& filter)
    {
        Array<BrowserEntry> entries;
        if (context.stage.id == kStageArtists.id)
        {
            // disable artist column
            context.disabledColumns = 3 << 1;
            context.stage = kStageArtists;
            for (uint32_t i = 0, e = m_db.artists.NumItems(); i < e; i++)
            {
                auto* artistName = m_db.artists[i].handles(m_db.strings);
                if (filter.PassFilter(artistName))
                {
                    bool isSelected = false;
                    if (auto* entry = context.entries.Find(i))
                    {
                        isSelected = entry->isSelected;
                        context.entries.RemoveAtFast(entry - context.entries.Items());
                    }
                    ArtistID artistId = ArtistID::Invalid;
                    auto sourceId = SourceID(kID, m_db.artists[i].id);
                    for (auto* rplArtist : Core::GetDatabase(DatabaseID::kLibrary).Artists())
                    {
                        for (uint16_t j = 0, n = rplArtist->NumSources(); j < n; j++)
                        {
                            if (rplArtist->GetSource(j).id == sourceId)
                            {
                                artistId = rplArtist->GetId();
                                break;
                            }
                        }
                        if (artistId != ArtistID::Invalid)
                            break;
                    }
                    entries.Add({
                        .dbIndex = i,
                        .isSong = false,
                        .isSelected = isSelected,
                        .artistId = artistId
                        });
                }
            }
        }
        else
        {
            // no column disabled
            context.disabledColumns = 0;
            context.stage = kStageSongs;

            auto& dbArtist = m_db.artists[context.stageDbIndex];
            DownloadArtist(dbArtist);

            for (uint32_t i = 0, songOffset = dbArtist.songs; i < dbArtist.numSongs; ++i)
            {
                const auto* dbSong = m_db.songs.Items<ZxArtSong>(songOffset);
                if (filter.PassFilter(dbSong->name(m_db.strings)))
                {
                    bool isSelected = false;
                    bool isDiscarded = false;
                    if (auto* entry = context.entries.Find(songOffset))
                    {
                        isSelected = entry->isSelected;
                        context.entries.RemoveAtFast(entry - context.entries.Items());
                    }
                    SongID songId = SongID::Invalid;
                    if (auto* songSource = FindSong(dbSong->id))
                    {
                        songId = songSource->songId;
                        isDiscarded = songSource->isDiscarded;
                    }
                    entries.Add({
                        .dbIndex = songOffset,
                        .isSong = true,
                        .isSelected = isSelected,
                        .isDiscarded = isDiscarded,
                        .songId = songId
                        });
                }
                songOffset += AlignUp(uint32_t(ZxArtSong::Size() + dbSong->numArtists * sizeof(uint16_t)), alignof(uint32_t));
            }
        }
        context.entries = std::move(entries);
    }

    int64_t SourceZXArt::BrowserCompare(const BrowserContext& context, const BrowserEntry& lEntry, const BrowserEntry& rEntry, BrowserColumn column) const
    {
        UnusedArg(context);
        int64_t delta = 0;
        if (lEntry.isSong)
        {
            auto& lDbEntry = *m_db.songs.Items<ZxArtSong>(lEntry.dbIndex);
            auto& rDbEntry = *m_db.songs.Items<ZxArtSong>(rEntry.dbIndex);
            if (column == kColumnName)
                delta = CompareStringMixed(lDbEntry.name(m_db.strings), rDbEntry.name(m_db.strings));
            else if (column == kColumnArtist)
            {
                std::string lArtist, rArtist;
                for (uint16_t i = 0; i < lDbEntry.numArtists; ++i)
                    lArtist += m_db.artists[lDbEntry.artists[i]].handles(m_db.strings);
                for (uint16_t i = 0; i < rDbEntry.numArtists; ++i)
                    rArtist += m_db.artists[rDbEntry.artists[i]].handles(m_db.strings);
                delta = CompareStringMixed(lArtist.c_str(), rArtist.c_str());
            }
            else if (column == kColumnYear)
                delta = int64_t(lDbEntry.year) - int64_t(rDbEntry.year);
            else if (column == kColumnId)
            {
                auto* dName = "\xff";
                auto* lName = lEntry.songId != SongID::Invalid ? Core::GetDatabase(DatabaseID::kLibrary)[lEntry.songId]->GetName() : dName;
                auto* rName = rEntry.songId != SongID::Invalid ? Core::GetDatabase(DatabaseID::kLibrary)[rEntry.songId]->GetName() : dName;
                delta = CompareStringMixed(lName, rName);
                if (delta == 0)
                {
                    delta = ((int64_t(rEntry.isDiscarded) - int64_t(lEntry.isDiscarded)) << 32)
                        + int64_t(lEntry.songId) - int64_t(rEntry.songId);
                }
            }
        }
        else
        {
            auto& lDbEntry = m_db.artists[lEntry.dbIndex];
            auto& rDbEntry = m_db.artists[rEntry.dbIndex];
            if (column == kColumnName)
                delta = CompareStringMixed(lDbEntry.handles(m_db.strings), rDbEntry.handles(m_db.strings));
            else if (column == kColumnId)
            {
                auto* dName = "\xff";
                auto* lName = lEntry.artistId != ArtistID::Invalid ? Core::GetDatabase(DatabaseID::kLibrary)[lEntry.artistId]->GetHandle() : dName;
                auto* rName = rEntry.artistId != ArtistID::Invalid ? Core::GetDatabase(DatabaseID::kLibrary)[rEntry.artistId]->GetHandle() : dName;
                delta = CompareStringMixed(lName, rName);
                if (delta == 0)
                    delta = int32_t(lEntry.artistId) - int32_t(rEntry.artistId);
            }
        }
        return delta;
    }

    void SourceZXArt::BrowserDisplay(const BrowserContext& context, const BrowserEntry& entry, BrowserColumn column) const
    {
        UnusedArg(context);
        if (column == kColumnName)
        {
            if (entry.isSong)
            {
                auto& dbSong = *m_db.songs.Items<ZxArtSong>(entry.dbIndex);
                if (dbSong.type.ext != eExtension::Unknown)
                    ImGui::Text(ImGuiIconFile "%s.%s", dbSong.name(m_db.strings), MediaType::extensionNames[int(dbSong.type.ext)]);
                else
                    ImGui::Text(ImGuiIconFile "%s", dbSong.name(m_db.strings));
            }
            else
            {
                ImGui::Text(ImGuiIconFolder "%s", m_db.artists[entry.dbIndex].handles(m_db.strings));
            }
        }
        else if (column == kColumnArtist)
        {
            if (entry.isSong)
            {
                auto& dbSong = *m_db.songs.Items<ZxArtSong>(entry.dbIndex);
                ImGui::TextUnformatted(m_db.artists[dbSong.artists[0]].handles(m_db.strings));
                for (uint16_t i = 1; i < dbSong.numArtists; ++i)
                {
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::Text(" & %s", m_db.artists[dbSong.artists[i]].handles(m_db.strings));
                }
            }
        }
        else if (column == kColumnYear)
        {
            if (entry.isSong)
            {
                auto& dbSong = *m_db.songs.Items<ZxArtSong>(entry.dbIndex);
                if (dbSong.year)
                    ImGui::Text("%04d", dbSong.year);
                else
                    ImGui::TextUnformatted("n/a");
            }
        }
        else if (column == kColumnId)
        {
            if (entry.isSong)
            {
                if (entry.songId != SongID::Invalid)
                    ImGui::Text("%08X %s", uint32_t(entry.songId), Core::GetDatabase(DatabaseID::kLibrary)[entry.songId]->GetName());
                else if (entry.isDiscarded)
                    ImGui::TextUnformatted("-------- DISCARDED");
            }
            else
            {
                if (entry.artistId != ArtistID::Invalid)
                    ImGui::Text("%04X %s", uint32_t(entry.artistId), Core::GetDatabase(DatabaseID::kLibrary)[entry.artistId]->GetHandle());
            }
        }
    }

    void SourceZXArt::BrowserImport(const BrowserContext& context, const BrowserEntry& entry, SourceResults& collectedSongs)
    {
        UnusedArg(context);
        if (entry.isSong)
        {
            AddSong(*m_db.songs.Items<ZxArtSong>(entry.dbIndex), collectedSongs);
        }
        else
        {
            auto& dbArtist = m_db.artists[entry.dbIndex];
            DownloadArtist(dbArtist);
            for (uint32_t i = 0, songOffset = dbArtist.songs; i < dbArtist.numSongs; ++i)
            {
                auto& dbSong = *m_db.songs.Items<ZxArtSong>(songOffset);
                AddSong(dbSong, collectedSongs);
                songOffset += AlignUp(uint32_t(ZxArtSong::Size() + dbSong.numArtists * sizeof(uint16_t)), alignof(uint32_t));
            }
        }
    }

    std::string SourceZXArt::BrowserGetStageName(const BrowserEntry& entry, BrowserStage stage) const
    {
        UnusedArg(stage);
        return m_db.artists[entry.dbIndex].handles(m_db.strings);
    }

    core::Array<rePlayer::BrowserSong> SourceZXArt::BrowserFetchSongs(const BrowserContext& context, const BrowserEntry& entry)
    {
        UnusedArg(context);
        Array<BrowserSong> songs;
        auto addSong = [&](const ZxArtSong& dbSong)
        {
            auto* song = songs.Push();
            char url[128];
            sprintf(url, "https://zxart.ee/file/id:%u/", dbSong.id);
            song->url = url;
            song->name = dbSong.name(m_db.strings);
            song->type = dbSong.type;
            for (uint16_t i = 0; i < dbSong.numArtists; ++i)
                song->artists.Add(m_db.artists[dbSong.artists[i]].handles(m_db.strings));
        };
        if (entry.isSong)
        {
            addSong(*m_db.songs.Items<ZxArtSong>(entry.dbIndex));
        }
        else
        {
            auto& dbArtist = m_db.artists[entry.dbIndex];
            DownloadArtist(dbArtist);
            for (uint32_t i = 0, songOffset = dbArtist.songs; i < dbArtist.numSongs; ++i)
            {
                auto& dbSong = *m_db.songs.Items<ZxArtSong>(songOffset);
                addSong(dbSong);
                songOffset += AlignUp(uint32_t(ZxArtSong::Size() + dbSong.numArtists * sizeof(uint16_t)), alignof(uint32_t));
            }
        }
        return songs;
    }

    SourceZXArt::SongSource* SourceZXArt::AddSong(uint32_t id)
    {
        auto* song = std::lower_bound(m_songs.begin(), m_songs.end(), id, [](auto& song, auto id)
        {
            return song.id < id;
        });
        thread::ScopedSpinLock lock(m_mutex);
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

    bool SourceZXArt::GetSongs(SourceResults& collectedSongs, const Array<uint8_t>& buffer, bool isCheckable, CURL* curl, uint32_t& start) const
    {
        auto json = nlohmann::json::parse(buffer.begin(), buffer.end());
        uint32_t totalAmount = json.contains("totalAmount") ? json["totalAmount"].get<uint32_t>() : 0;
        auto& jsonEntries = json["responseData"]["zxMusic"];
        for (auto& zxMusic : jsonEntries)
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
                auto& zxYear = zxMusic["year"];
                if (zxYear.is_string())
                {
                    uint32_t year = 0;
                    sscanf_s(zxYear.get<std::string>().c_str(), "%u", &year);
                    song->releaseYear = uint16_t(year);
                }
                else if (zxYear.is_number())
                    song->releaseYear = uint16_t(zxYear.get<uint32_t>());
            }
            if (zxMusic.contains("type"))
            {
                song->type = Core::GetReplays().Find(zxMusic["type"].get<std::string>().c_str());
                if (song->type.ext == eExtension::Unknown)
                    song->name.String() += std::string(".") + zxMusic["type"].get<std::string>();
            }
            else if (zxMusic.contains("originalFileName"))
            {
                auto filename = zxMusic["originalFileName"].get<std::string>();
                auto pos = filename.rfind('.');
                if (pos != filename.npos)
                {
                    song->type = Core::GetReplays().Find(filename.c_str() + pos + 1);
                    if (song->type.ext == eExtension::Unknown)
                        song->name.String() += filename.c_str() + pos;
                }
            }
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
        start += uint32_t(jsonEntries.size());
        return start >= totalAmount;
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
                buffer->Add(data, uint32_t(size * nmemb));
                return size * nmemb;
            }
        } buffer;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Buffer::Writer);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

        CURLcode curlError = curl_easy_perform(curl);
        if (curlError == CURLE_OK)
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
                        Log::Message("ZXArt: fetching authorAlias %u\n", alias.get<uint32_t>());
                        curlError = curl_easy_perform(curl);
                        if (curlError == CURLE_OK)
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

    bool SourceZXArt::DownloadDatabase(BusySpinner& busySpinner)
    {
        if (m_db.artists.IsEmpty())
        {
            if (m_db.songs.IsEmpty())
                m_db.songs.Add(uint8_t(0), 4);

            CURL* curl = curl_easy_init();

            char errorBuffer[CURL_ERROR_SIZE];
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, Core::GetLabel());

            struct Buffer : public Array<uint8_t>
            {
                static size_t Writer(const uint8_t* data, size_t size, size_t nmemb, Buffer* buffer)
                {
                    buffer->Add(data, uint32_t(size * nmemb));
                    return size * nmemb;
                }
            } buffer;

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Buffer::Writer);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

            CURLcode curlError = CURLE_OK;
            Array<std::pair<uint32_t, std::string>> aliases;
            auto* message = busySpinner.Info("downloading aliases database at %u", 0);
            for (uint32_t start = 0; curlError == CURLE_OK;)
            {
                busySpinner.UpdateMessageParam(message, start);

                buffer.Clear();
                char url[1024];
                sprintf(url, "https://zxart.ee/api/export:authorAlias/language:eng/start:%u/filter:authorAliasAll", start);
                curl_easy_setopt(curl, CURLOPT_URL, url);
                Log::Message("ZXArt: fetching authorAliasAll at %u\n", start);
                curlError = curl_easy_perform(curl);
                if (curlError == CURLE_OK)
                {
                    auto jsonAllAliases = nlohmann::json::parse(buffer.begin(), buffer.end());
                    uint32_t totalAmount = jsonAllAliases.contains("totalAmount") ? jsonAllAliases["totalAmount"].get<uint32_t>() : 0;
                    auto& jsonEntries = jsonAllAliases["responseData"]["authorAlias"];
                    aliases.Reserve(uint32_t(jsonEntries.size()));
                    for (auto& jsonAlias : jsonAllAliases["responseData"]["authorAlias"])
                    {
                        // Sanitize messy ZXArt database
                        if (jsonAlias.contains("title"))
                        {
                            auto entry = aliases.AddOnce({ jsonAlias["id"].get<uint32_t>(), jsonAlias["title"].get<std::string>() });
                            if (!entry.second)
                                Log::Warning("ZXArt: duplicated author alias %u (\"%s\")\n", entry.first->first, entry.first->second.c_str());
                        }
                        else
                            Log::Warning("ZXArt: missing title for author alias %u\n", jsonAlias["id"].get<uint32_t>());
                    }
                    start += uint32_t(jsonEntries.size());
                    if (start >= totalAmount)
                        break;
                }
            }
            if (curlError == CURLE_OK && aliases.IsNotEmpty())
            {
                message = busySpinner.Info("downloading authors database at %u", 0);
                for (uint32_t start = 0; curlError == CURLE_OK;)
                {
                    busySpinner.UpdateMessageParam(message, start);

                    buffer.Clear();
                    char url[1024];
                    sprintf(url, "https://zxart.ee/api/export:author/language:eng/start:%u/filter:authorAll", start);
                    curl_easy_setopt(curl, CURLOPT_URL, url);
                    Log::Message("ZXArt: fetching authorAll at %u\n", start);
                    curlError = curl_easy_perform(curl);
                    if (curlError == CURLE_OK)
                    {
                        auto jsonAllAuthors = nlohmann::json::parse(buffer.begin(), buffer.end());
                        uint32_t totalAmount = jsonAllAuthors.contains("totalAmount") ? jsonAllAuthors["totalAmount"].get<uint32_t>() : 0;
                        auto& jsonEntries = jsonAllAuthors["responseData"]["author"];
                        for (auto& author : jsonEntries)
                        {
                            if (!author.contains("tunesQuantity"))
                                continue;

                            // ZXArt database requests are messy, so check for duplicates
                            uint32_t artistId = author["id"].get<uint32_t>();
                            auto* entry = m_db.artists.FindIf([artistId](auto& artist) { return artist.id == artistId; });
                            if (entry != nullptr)
                            {
                                Log::Warning("ZXArt: duplicated author %u (\"%s\")\n", artistId, author["title"].get<std::string>().c_str());
                                continue;
                            }

                            auto* artist = m_db.artists.Push();
                            artist->id = artistId;
                            sscanf_s(author["tunesQuantity"].get<std::string>().c_str(), "%u", &artist->numSongs);

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
                        start += uint32_t(jsonEntries.size());
                        if (start >= totalAmount)
                            break;
                    }
                }
            }
            if (curlError != CURLE_OK)
                Log::Error("ZXArt: %s\n", curl_easy_strerror(curlError));
            curl_easy_cleanup(curl);
        }
        return m_db.artists.IsEmpty();
    }

    void SourceZXArt::DownloadArtist(ZxArtArtist& artist)
    {
        if (artist.songs != 0)
            return;

        CURL* curl = curl_easy_init();

        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, Core::GetLabel());

        struct Buffer : public Array<uint8_t>
        {
            static size_t Writer(const uint8_t* data, size_t size, size_t nmemb, Buffer* buffer)
            {
                buffer->Add(data, uint32_t(size * nmemb));
                return size * nmemb;
            }
        } buffer;

        artist.songs = m_db.songs.NumItems();
        CURLcode curlError = CURLE_OK;
        artist.numSongs = 0;
        for (; curlError == CURLE_OK;)
        {
            buffer.Clear();

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Buffer::Writer);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

            char url[1024];
            sprintf(url, "https://zxart.ee/api/export:zxMusic/language:eng/start:%u/filter:authorId=%u", artist.numSongs, artist.id);
            curl_easy_setopt(curl, CURLOPT_URL, url);
            Log::Message("ZXArt: fetching author %u at %u\n", artist.id, artist.numSongs);
            curlError = curl_easy_perform(curl);
            if (curlError == CURLE_OK)
            {
                if (GetDbSongs(buffer, artist.numSongs))
                    break;
            }
        }

        curl_easy_cleanup(curl);
    }

    bool SourceZXArt::GetDbSongs(const Array<uint8_t>& buffer, uint32_t& start)
    {
        auto json = nlohmann::json::parse(buffer.begin(), buffer.end());
        uint32_t totalAmount = json.contains("totalAmount") ? json["totalAmount"].get<uint32_t>() : 0;
        auto& jsonEntries = json["responseData"]["zxMusic"];
        for (auto& zxMusic : jsonEntries)
        {
            auto* song = m_db.songs.Push<ZxArtSong*>(ZxArtSong::Size());

            song->id = zxMusic["id"].get<uint32_t>();

            auto name = ConvertEntities(zxMusic["title"].get<std::string>());
            if (zxMusic.contains("internalTitle"))
                name += " / " + ConvertEntities(zxMusic["internalTitle"].get<std::string>());
            song->name.Set(m_db.strings, name);

            uint32_t year = 0;
            if (zxMusic.contains("year"))
            {
                auto& zxYear = zxMusic["year"];
                if (zxYear.is_string())
                    sscanf_s(zxYear.get<std::string>().c_str(), "%u", &year);
                else if (zxYear.is_number())
                    year = zxYear.get<uint32_t>();
            }
            song->year = uint16_t(year);

            song->type = {};
            if (zxMusic.contains("type"))
            {
                song->type = Core::GetReplays().Find(zxMusic["type"].get<std::string>().c_str());
                if (song->type.ext == eExtension::Unknown)
                {
                    m_db.strings.Last() = '.';
                    auto type = zxMusic["type"].get<std::string>();
                    m_db.strings.Add(type.c_str(), uint32_t(type.size() + 1));
                }
            }
            else if (zxMusic.contains("originalFileName"))
            {
                auto filename = zxMusic["originalFileName"].get<std::string>();
                auto pos = filename.rfind('.');
                if (pos != filename.npos)
                {
                    song->type = Core::GetReplays().Find(filename.c_str() + pos + 1);
                    if (song->type.ext == eExtension::Unknown)
                        m_db.strings.Add(filename.c_str() + pos, uint32_t(filename.size() - pos + 1));
                }
            }

            song->numArtists = uint16_t(zxMusic["authorIds"].size());
            for (auto& author : zxMusic["authorIds"])
            {
                auto authorId = author.get<uint32_t>();
                auto it = m_db.artists.FindIf([authorId](auto& entry)
                {
                    return entry.id == authorId;
                });
                auto newArtistId = uint16_t(it - m_db.artists.begin());
                m_db.songs.Add(pcCast<uint8_t>(&newArtistId), sizeof(uint16_t));
            }
            m_db.songs.Resize(AlignUp(m_db.songs.NumItems(), alignof(uint32_t)));
        }
        start += uint32_t(jsonEntries.size());
        return start >= totalAmount;
    }

    void SourceZXArt::AddSong(const ZxArtSong& dbSong, SourceResults& collectedSongs)
    {
        auto song = new SongSheet();

        song->sourceIds.Add(SourceID(kID, dbSong.id));

        SourceResults::State state;
        if (auto sourceSong = FindSong(dbSong.id))
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

        song->name = dbSong.name(m_db.strings);
        song->releaseYear = dbSong.year;
        song->type = dbSong.type;
        for (uint16_t i = 0; i < dbSong.numArtists; ++i)
        {
            auto& dbArtist = m_db.artists[dbSong.artists[i]];
            auto it = std::find_if(collectedSongs.artists.begin(), collectedSongs.artists.end(), [&](auto entry)
            {
                return entry->sources[0].id.sourceId == kID && entry->sources[0].id.internalId == dbArtist.id;
            });
            auto newArtistId = static_cast<ArtistID>(it - collectedSongs.artists.begin());
            if (it == collectedSongs.artists.end())
            {
                auto* newArtist = new ArtistSheet();
                newArtist->sources.Add(SourceID(kID, dbArtist.id));
                for (uint32_t j = 0, ofs = dbArtist.handles.offset; j < dbArtist.numHandles; ++j)
                {
                    newArtist->handles.Add(m_db.strings.Items(ofs));
                    ofs += uint32_t(newArtist->handles[j].String().size() + 1);
                }

                newArtist->realName = dbArtist.realName(m_db.strings);
                if (dbArtist.country)
                    newArtist->countries.Add(dbArtist.country);

                collectedSongs.artists.Add(newArtist);
            }
            song->artistIds.Add(newArtistId);
        }
        collectedSongs.songs.Add(song);
        collectedSongs.states.Add(state);
    }
}
// namespace rePlayer
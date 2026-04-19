// Core
#include <Core/Log.h>
#include <ImGui.h>
#include <IO/File.h>
#include <IO/StreamMemory.h>

// rePlayer
#include <Database/Database.h>
#include <RePlayer/Core.h>
#include <UI/BusySpinner.h>
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

        Collector() : WebHandler("UTF-8") {}
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
            uint32_t year = 0; // not available in this mode :(
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

        Search() : WebHandler("UTF-8") {}
        void OnReadNode(xmlNode* node) final;
    };

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
                            foundArtists.Last().first = Escape((const char*)propChild->content + sizeof("/?p=composer&name=") - 1);
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
                            foundSongs.Last().artist.first = Escape((const char*)propChild->content + sizeof("/?p=composer&name=") - 1);
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

    struct SourceSNDH::Composers : public WebHandler
    {
        uint32_t strSize = 0;
        struct Composer
        {
            std::string url;
            std::string name;
        };
        Array<Composer> entries;

        Composers() : WebHandler("UTF-8") {}
        void OnReadNode(xmlNode* node) final;
    };

    void SourceSNDH::Composers::OnReadNode(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            bool doChildren = true;
            if (node->type == XML_ELEMENT_NODE)
            {
                for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (xmlStrstr(propChild->content, BAD_CAST"/?p=composer&name="))
                    {
                        auto* entry = entries.Push();
                        entry->url = Escape(pcCast<char>(propChild->content) + sizeof("/?p=composer&name=") - 1);
                        strSize += uint32_t(entry->url.size() + 1);
                        if (node->children && node->children->content)
                        {
                            std::string name = pcCast<char>(node->children->content);
                            if (name != entry->url)
                            {
                                entry->name = name;
                                strSize += uint32_t(name.size() + 1);
                            }
                        }
                        doChildren = false;
                        break;
                    }
                }
            }
            if (doChildren)
                OnReadNode(node->children);
        }
    }

    SourceSNDH::SourceSNDH()
        : Source(true)
    {}

    SourceSNDH::~SourceSNDH()
    {}

    void SourceSNDH::FindArtists(ArtistsCollection& artists, const char* name, BusySpinner& busySpinner)
    {
        if (DownloadDatabase(busySpinner))
            return;

        std::string lName = ToLower(name);
        for (auto& composer : m_db.composers)
        {
            std::string lArtist = ToLower(m_db.strings.Items(composer.name));
            if (strstr(lArtist.c_str(), lName.c_str()))
            {
                auto* newArtist = artists.matches.Push();
                newArtist->name = m_db.strings.Items(composer.name);
                newArtist->id = SourceID(kID, FindArtist(m_db.strings.Items(composer.url), m_db.strings.Items(composer.name)));
            }
        }
    }

    void SourceSNDH::ImportArtist(SourceID importedArtistID, SourceResults& results, BusySpinner& busySpinner)
    {
        busySpinner.Info(m_strings.Items(m_artists[importedArtistID.internalId].urlOffset));

        assert(importedArtistID.sourceId == kID);
        results.importedArtists.Add(importedArtistID);

        Collector collector;
        collector.Fetch("https://sndh.atari.org/?p=composer&name=%s", m_strings.Items(m_artists[importedArtistID.internalId].urlOffset));
        if (!collector.error.empty())
            busySpinner.Error(collector.error);

        for (auto& collectedSong : collector.songs)
            AddSong(collectedSong
                , m_strings.Items(m_artists[importedArtistID.internalId].urlOffset)
                , m_strings.Items(m_artists[importedArtistID.internalId].nameOffset)
                , results
                , true);
    }

    void SourceSNDH::FindSongs(const char* name, SourceResults& collectedSongs, BusySpinner& busySpinner)
    {
        Search search;
        search.Fetch("https://sndh.atari.org/?p=searchdone&searchword=%s", search.Escape(name).c_str());
        if (!search.error.empty())
            busySpinner.Error(search.error);

        for (auto& searchedSong : search.foundSongs)
            AddSong(searchedSong
                , searchedSong.artist.first.c_str()
                , searchedSong.artist.second.c_str()
                , collectedSongs
                , false);
    }

    Source::Import SourceSNDH::ImportSong(SourceID sourceId, const std::string& path)
    {
        thread::ScopedSpinLock lock(m_mutex);
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
            if (buffer.Size() < 128 || strstr(buffer.Items<char>(), "<html>"))
            {
                isEntryMissing = true;
                auto* song = FindSong(sourceToDownload.internalId);
                m_songs.RemoveAt(song - m_songs);

                Log::Error("SNDH: file \"%s\" not found\n", url);
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

    void SourceSNDH::InvalidateSong(SourceID sourceId, SongID newSongId)
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

    std::string SourceSNDH::GetArtistStub(SourceID artistId) const
    {
        return m_strings.Items(m_artists[artistId.internalId].urlOffset);
    }

    bool SourceSNDH::IsValidationEnabled() const
    {
        // validation is only possible when the database has been downloaded
        return m_db.composers.IsNotEmpty();
    }

    Status SourceSNDH::Validate(SourceID sourceId, ArtistID artistId)
    {
        UnusedArg(artistId);
        if (sourceId.internalId >= m_artists.NumItems())
            return Status::kFail;

        auto* url = m_strings.Items(m_artists[sourceId.internalId].urlOffset);
        for (auto& composer : m_db.composers)
        {
            if (strcmp(m_db.strings.Items(composer.url), url) == 0)
                return Status::kOk;
        }

        m_isDirty = m_areStringsDirty = true;

        // try to find and patch or remove artists with missing escape characters from old db
        std::string tmp;
        if (auto* c = strchr(url, 'ü'))
        {
            // hardcore patch for umlaut
            tmp.append(url, c);
            tmp += "%C3%BC";
            tmp += c + 1;
            url = tmp.data();
        }

        auto* curl = curl_easy_init();
        url = curl_easy_unescape(curl, url, 0, nullptr);
        auto s = Scope([](){}
            , [&]()
            {
                curl_free(url);
                curl_easy_cleanup(curl);
            });
        for (auto& composer : m_db.composers)
        {
            auto composerUrl = curl_easy_unescape(curl, m_db.strings.Items(composer.url), 0, nullptr);
            auto isFound = strcmp(composerUrl, url) == 0;
            curl_free(composerUrl);
            if (isFound)
            {
                for (auto& artist : m_artists)
                {
                    if (&artist == &m_artists[sourceId.internalId] || artist.isNotRegistered)
                        continue;
                    // has it already been fetched with the fixed url
                    if (strcmp(m_strings.Items(artist.urlOffset), m_db.strings.Items(composer.url)) == 0)
                    {
                        // discard
                        m_artists[sourceId.internalId].isNotRegistered = true;
                        return Status::kFail;
                    }
                }
                // patch with the fixed url and name
                m_artists[sourceId.internalId].nameOffset = m_artists[sourceId.internalId].urlOffset = m_strings.NumItems();
                m_strings.Add(m_db.strings.Items(composer.url), uint32_t(strlen(m_db.strings.Items(composer.url)) + 1));
                if (composer.url != composer.name)
                {
                    m_artists[sourceId.internalId].nameOffset = m_strings.NumItems();
                    m_strings.Add(m_db.strings.Items(composer.name), uint32_t(strlen(m_db.strings.Items(composer.name)) + 1));
                }
                return Status::kOk;
            }
        }

        // can't find a match, discard
        m_artists[sourceId.internalId].isNotRegistered = true;
        return Status::kFail;
    }

    void SourceSNDH::Load()
    {
        auto file = io::File::OpenForRead(ms_filename);
        if (file.IsValid())
        {
            auto stamp = file.Read<uint32_t>();
            auto version = file.Read<uint32_t>();
            if (stamp != kMusicFileStamp || version > Core::GetVersion())
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
                    if (m_availableArtistIds.value == 0xffFFffFF)
                        m_availableArtistIds.nextId = i;
                }
            }
            static constexpr uint32_t kVersionFix = (0 << 28) | (22 << 14); // fix wrong value check
            if (version < kVersionFix)
            {
                if (m_strings.IsEmpty())
                    m_strings.Add(0);
                for (uint32_t i = 0, e = m_artists.NumItems(); i < e; i++)
                {
                    if (m_artists[i].isNotRegistered)
                        m_artists[i].urlOffset = m_strings.NumItems() - 1;
                }
                m_isDirty = m_areStringsDirty = true;
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
                file.Write(kMusicFileStamp);
                file.Write(Core::GetVersion());
                if (m_areStringsDirty)
                {
                    // remove discarded artists at the end of the array
                    Array<ArtistSource> artists(m_artists);
                    for (int64_t i = artists.NumItems<int64_t>() - 1; i >= 0; --i)
                    {
                        if (!artists[i].isNotRegistered)
                        {
                            artists.Resize(uint32_t(i + 1));
                            break;
                        }
                    }
                    // re-pack artists
                    Array<char> strings(0u, m_strings.NumItems());
                    strings.Add(0);
                    uint32_t lastArtist = 0xffFFffFF;
                    for (uint32_t i = 0, e = artists.NumItems(); i < e; i++)
                    {
                        auto& artist = artists[i];
                        if (!artist.isNotRegistered)
                        {
                            auto* name = m_strings.Items(artist.nameOffset);
                            auto* url = m_strings.Items(artist.urlOffset);
                            artist.nameOffset = strings.NumItems();
                            strings.Add(name, uint32_t(strlen(name) + 1));
                            if (name != url)
                            {
                                artist.urlOffset = strings.NumItems();
                                strings.Add(url, uint32_t(strlen(url) + 1));
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
                            artists[i].urlOffset = 0;
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

    void SourceSNDH::BrowserInit(BrowserContext& context)
    {
        if (m_db.composers.IsEmpty())
        {
            context.busySpinner.New(ImGui::GetColorU32(ImGuiCol_ButtonHovered));
            Core::AddJob([this, &context]()
            {
                DownloadDatabase(*context.busySpinner);

                Core::FromJob([this, &context]()
                {
                    context.busySpinner.Reset();
                    if (m_db.composers.IsEmpty())
                        context.Invalidate();
                });
            });
        }
        static const char* columnNames[] = { "Name", "Artist", "Year", "ID" };
        context.numColumns = NumItemsOf(columnNames);
        context.columnNames = columnNames;
    }

    void SourceSNDH::BrowserPopulate(BrowserContext& context, const ImGuiTextFilter& filter)
    {
        Array<BrowserEntry> entries;
        if (context.stage.id == kStageArtists.id)
        {
            // disable artist column
            context.disabledColumns = 3 << 1;
            context.stage = kStageArtists;
            for (uint32_t i = 0, e = m_db.composers.NumItems(); i < e; i++)
            {
                auto* composerName = m_db.strings.Items(m_db.composers[i].name);
                if (filter.PassFilter(composerName))
                {
                    bool isSelected = false;
                    if (auto* entry = context.entries.Find(i))
                    {
                        isSelected = entry->isSelected;
                        context.entries.RemoveAtFast(entry - context.entries.Items());
                    }
                    ArtistID artistId = ArtistID::Invalid;
                    bool hasNewEntries = m_db.composers[i].songs == 0;
                    if (auto* sourceArtist = m_artists.FindIf([&](auto& entry)
                    {
                        return strcmp(m_db.strings.Items(m_db.composers[i].url), m_strings.Items(entry.urlOffset)) == 0;
                    }))
                    {
                        auto sourceId = SourceID(kID, uint32_t(sourceArtist - m_artists.Items()));
                        for (auto* rplArtist : Core::GetDatabase(DatabaseID::kLibrary).Artists())
                        {
                            for (uint16_t j = 0, n = rplArtist->NumSources(); j < n; j++)
                            {
                                if (rplArtist->GetSource(j).id == sourceId)
                                {
                                    artistId = rplArtist->GetId();

                                    for (auto dbSongId = m_db.composers[i].songs; dbSongId;)
                                    {
                                        const auto& dbSong = m_db.songs[dbSongId];
                                        if (!FindSong(dbSong.id))
                                        {
                                            hasNewEntries = true;
                                            break;
                                        }
                                        dbSongId = dbSong.next;
                                    }
                                    break;
                                }
                            }
                            if (artistId != ArtistID::Invalid)
                                break;
                        }
                    }
                    entries.Add({
                        .dbIndex = i,
                        .isSong = false,
                        .isSelected = isSelected,
                        .artist = {
                            .id = artistId,
                            .isFetched = !hasNewEntries
                        }
                    });
                }
            }
        }
        else
        {
            // no column disabled
            context.disabledColumns = 0;
            context.stage = kStageSongs;

            auto& dbComposer = m_db.composers[context.stageDbIndex];
            DownloadComposer(dbComposer);

            for (auto dbSongId = dbComposer.songs; dbSongId;)
            {
                const auto& dbSong = m_db.songs[dbSongId];
                if (filter.PassFilter(m_db.strings.Items(dbSong.name)))
                {
                    bool isSelected = false;
                    bool isDiscarded = false;
                    if (auto* entry = context.entries.Find(dbSongId))
                    {
                        isSelected = entry->isSelected;
                        context.entries.RemoveAtFast(entry - context.entries.Items());
                    }
                    SongID songId = SongID::Invalid;
                    if (auto* songSource = FindSong(dbSong.id))
                    {
                        songId = songSource->songId;
                        isDiscarded = songSource->isDiscarded;
                    }
                    entries.Add({
                        .dbIndex = dbSongId,
                        .isSong = true,
                        .isSelected = isSelected,
                        .isDiscarded = isDiscarded,
                        .songId = songId
                    });
                }
                dbSongId = dbSong.next;
            }
        }
        context.entries = std::move(entries);
    }

    int64_t SourceSNDH::BrowserCompare(const BrowserContext& context, const BrowserEntry& lEntry, const BrowserEntry& rEntry, BrowserColumn column) const
    {
        UnusedArg(context);
        int64_t delta = 0;
        if (lEntry.isSong)
        {
            auto& lDbEntry = m_db.songs[lEntry.dbIndex];
            auto& rDbEntry = m_db.songs[rEntry.dbIndex];
            if (column == kColumnName)
                delta = CompareStringMixed(m_db.strings.Items(lDbEntry.name), m_db.strings.Items(rDbEntry.name));
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
            auto& lDbEntry = m_db.composers[lEntry.dbIndex];
            auto& rDbEntry = m_db.composers[rEntry.dbIndex];
            if (column == kColumnName)
                delta = CompareStringMixed(m_db.strings.Items(lDbEntry.name), m_db.strings.Items(rDbEntry.name));
            else if (column == kColumnId)
            {
                auto* dName = "\xff";
                auto* lName = lEntry.artist.id != ArtistID::Invalid ? Core::GetDatabase(DatabaseID::kLibrary)[lEntry.artist.id]->GetHandle() : dName;
                auto* rName = rEntry.artist.id != ArtistID::Invalid ? Core::GetDatabase(DatabaseID::kLibrary)[rEntry.artist.id]->GetHandle() : dName;
                delta = CompareStringMixed(lName, rName);
                if (delta == 0)
                    delta = int32_t(lEntry.artist.id) - int32_t(rEntry.artist.id);
            }
        }
        return delta;
    }

    void SourceSNDH::BrowserDisplay(const BrowserContext& context, const BrowserEntry& entry, BrowserColumn column) const
    {
        UnusedArg(context);
        if (column == kColumnName)
        {
            if (entry.isSong)
            {
                auto& dbSong = m_db.songs[entry.dbIndex];
                ImGui::Text(ImGuiIconFile "%s.sndh", m_db.strings.Items(dbSong.name));
            }
            else
            {
                ImGui::Text(ImGuiIconFolder "%s", m_db.strings.Items(m_db.composers[entry.dbIndex].name));
            }
        }
        else if (column == kColumnArtist)
        {
            if (entry.isSong)
            {
                auto& dbArtist = m_db.composers[context.stageDbIndex];
                ImGui::TextUnformatted(m_db.strings.Items(dbArtist.name));
            }
        }
        else if (column == kColumnYear)
        {
            if (entry.isSong)
            {
                auto& dbSong = m_db.songs[entry.dbIndex];
                if (dbSong.year)
                    ImGui::Text("%04d", dbSong.year);
                else
                    ImGui::TextUnformatted("n/a");
            }
        }
        else if (column == kColumnId)
            BrowserDisplayLibraryId(entry, entry.isSong);
    }

    void SourceSNDH::BrowserImport(const BrowserContext& context, const BrowserEntry& entry, SourceResults& collectedSongs)
    {
        if (entry.isSong)
        {
            AddSong(m_db.songs[entry.dbIndex]
                , m_db.strings.Items(m_db.composers[context.stageDbIndex].url)
                , m_db.strings.Items(m_db.composers[context.stageDbIndex].name)
                , collectedSongs
                , true);
        }
        else
        {
            auto& dbComposer = m_db.composers[entry.dbIndex];
            DownloadComposer(dbComposer);
            for (auto dbSongId = dbComposer.songs; dbSongId;)
            {
                const auto& dbSong = m_db.songs[dbSongId];
                AddSong(dbSong
                    , m_db.strings.Items(dbComposer.url)
                    , m_db.strings.Items(dbComposer.name)
                    , collectedSongs
                    , true);
                dbSongId = dbSong.next;
            }
        }
    }

    std::string SourceSNDH::BrowserGetStageName(const BrowserEntry& entry, BrowserStage stage) const
    {
        UnusedArg(stage);
        return m_db.strings.Items(m_db.composers[entry.dbIndex].name);
    }

    core::Array<rePlayer::BrowserSong> SourceSNDH::BrowserFetchSongs(const BrowserContext& context, const BrowserEntry& entry)
    {
        Array<BrowserSong> songs;
        auto addSong = [&](const SndhSong& dbSong, uint32_t artistId)
        {
            auto* song = songs.Push();
            char url[128];
            sprintf(url, "https://sndh.atari.org/dl.php?ID=%u", dbSong.id);
            song->url = url;
            song->name = m_db.strings.Items(dbSong.name);
            if (strcmp(m_db.strings.Items(m_db.composers[artistId].name), "Unknown Composer"))
                song->artists.Add(m_db.strings.Items(m_db.composers[artistId].name));
            song->type = { eExtension::_sndh, eReplay::SNDHPlayer };
        };
        if (entry.isSong)
        {
            addSong(m_db.songs[entry.dbIndex], context.stageDbIndex);
        }
        else
        {
            DownloadComposer(m_db.composers[entry.dbIndex]);
            for (auto dbSongId = m_db.composers[entry.dbIndex].songs; dbSongId;)
            {
                const auto& dbSong = m_db.songs[dbSongId];
                addSong(dbSong, entry.dbIndex);
                dbSongId = dbSong.next;
            }
        }
        return songs;
    }

    SourceSNDH::SongSource* SourceSNDH::AddSong(uint32_t id)
    {
        auto* song = std::lower_bound(m_songs.begin(), m_songs.end(), id, [](auto& song, auto id)
        {
            return song.id < id;
        });
        thread::ScopedSpinLock lock(m_mutex);
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

    uint32_t SourceSNDH::FindArtist(const char* url, const char* name)
    {
        auto artist = m_artists.FindIf([&](auto& artist)
        {
            return strcmp(url, m_strings.Items(artist.urlOffset)) == 0;
        });
        if (artist == nullptr)
        {
            thread::ScopedSpinLock lock(m_mutex);
            if (m_availableArtistIds.value != 0xffFFffFF)
            {
                artist = m_artists.Items(m_availableArtistIds.nextId);
                m_availableArtistIds = *artist;
            }
            else
                artist = m_artists.Push();
            artist->nameOffset = m_strings.NumItems();
            m_strings.Add(name, uint32_t(strlen(name) + 1));
            if (url != name)
            {
                artist->urlOffset = m_strings.NumItems();
                m_strings.Add(url, uint32_t(strlen(url) + 1));
            }
            else
                artist->urlOffset = artist->nameOffset;
            m_areStringsDirty = true;
        }
        return artist - m_artists;
    }

    template <typename T>
    void SourceSNDH::AddSong(const T& dbSong, const char* artistUrl, const char* artistName, SourceResults& collectedSongs, bool isNewChecked)
    {
        SourceID songSourceId(kID, dbSong.id);
        if (collectedSongs.IsSongAvailable(songSourceId))
            return;

        auto* song = new SongSheet();

        SourceResults::State state;
        if (auto sourceSong = FindSong(dbSong.id))
        {
            song->id = sourceSong->songId;
            if (sourceSong->isDiscarded)
                state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kDiscarded : SourceResults::kMerged);
            else
                song->id == SongID::Invalid ? state.SetSongStatus(SourceResults::kNew).SetChecked(isNewChecked) : state.SetSongStatus(SourceResults::kOwned);
        }
        else
        {
            state.SetSongStatus(SourceResults::kNew).SetChecked(isNewChecked);
        }

        if constexpr (std::is_same<decltype(dbSong.name), uint32_t>::value)
            song->name = m_db.strings.Items(dbSong.name);
        else
            song->name = dbSong.name;
        song->type = { eExtension::_sndh, eReplay::SNDHPlayer };
        song->releaseYear = uint16_t(dbSong.year);
        song->sourceIds.Add(songSourceId);

        if (strcmp(artistName, "Unknown Composer"))
        {
            auto artistId = FindArtist(artistUrl, artistName);
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
                artist->handles.Add(artistName);
                collectedSongs.artists.Add(artist);
            }
        }

        collectedSongs.songs.Add(song);
        collectedSongs.states.Add(state);

    }

    bool SourceSNDH::DownloadDatabase(BusySpinner& busySpinner)
    {
        if (m_db.composers.IsEmpty())
        {
            busySpinner.Info("downloading database");

            Composers composers;
            composers.Fetch("https://sndh.atari.org/?p=composers");
            if (composers.strSize)
            {
                m_db.composers.Resize(composers.entries.NumItems());
                m_db.strings.Resize(composers.strSize + 1);
                m_db.strings[0] = 0;
                uint32_t offset = 1;
                for (uint32_t i = 0; i < composers.entries.NumItems(); ++i)
                {
                    auto& composer = composers.entries[i];
                    m_db.composers[i].url = m_db.composers[i].name = offset;
                    memcpy(m_db.strings.Items(offset), composer.url.c_str(), composer.url.size() + 1);
                    offset += uint32_t(composer.url.size() + 1);
                    if (!composer.name.empty())
                    {
                        m_db.composers[i].name = offset;
                        memcpy(m_db.strings.Items(offset), composer.name.c_str(), composer.name.size() + 1);
                        offset += uint32_t(composer.name.size() + 1);
                    }
                    m_db.composers[i].songs = 0;
                }
                m_db.songs.Add({});
            }
            else
            {
                Log::Error("SNDH: database failure\n");
                busySpinner.Error("database failure");
            }
        }
        return m_db.composers.IsEmpty();
    }

    void SourceSNDH::DownloadComposer(SndhComposer& dbComposer)
    {
        if (dbComposer.songs == 0)
        {
            Collector collector;
            collector.Fetch("https://sndh.atari.org/?p=composer&name=%s", m_db.strings.Items(dbComposer.url));
//             if (!collector.error.empty())
//                 busySpinner.Error(collector.error);
            for (auto& song : collector.songs)
            {
                auto songIdx = m_db.songs.NumItems();
                m_db.songs.Push();
                m_db.songs.Last().id = song.id;
                m_db.songs.Last().name = m_db.strings.NumItems();
                m_db.songs.Last().year = song.year;
                m_db.songs.Last().next = dbComposer.songs;
                m_db.strings.Add(song.name.c_str(), uint32_t(song.name.size() + 1));
                dbComposer.songs = songIdx;
            }
        }
    }
}
// namespace rePlayer
// Core
#include <Core/Log.h>
#include <IO/File.h>
#include <IO/StreamMemory.h>

// rePlayer
#include <Database/Types/Countries.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>
#include "AmigaMusicPreservation.h"
#include "WebHandler.h"

// zlib
#include <zlib.h>

// stl
#include <algorithm>

namespace rePlayer
{
    const char* const SourceAmigaMusicPreservation::ms_filename = MusicPath "AmigaMusicPreservation" MusicExt;

    struct SourceAmigaMusicPreservation::CollectorArtist
    {
        ArtistSheet::Handles handles;
        std::string name;
        Array<std::string> countries;
        ArtistSheet::Groups groups;
    };

    struct SourceAmigaMusicPreservation::CollectorSong
    {
        uint32_t id;
        std::string name;
        std::string ext;
        Array<uint32_t> artists;
    };

    struct SourceAmigaMusicPreservation::Collector : public WebHandler
    {
        SourceAmigaMusicPreservation::CollectorArtist artist;
        Array<SourceAmigaMusicPreservation::CollectorSong> songs;

        enum
        {
            kStateInit = 0,
            kStateArtistHandleBegin = kStateInit,
            kStateArtistHandleStep,
            kStateArtistHandleEnd,
            kStateArtistNameBegin,
            kStateArtistNameStep,
            kStateArtistNameEnd,
            kStateArtistCountries,
            kStateArtistCountry,
            kStateArtistExHandlesBegin,
            kStateArtistExHandlesStep,
            kStateArtistExHandlesEnd,
            kStateArtistGroups,
            kStateArtistGroup,
            kStateSongs,
            kStateSong = 0x10,
            kStateSongArtistsBegin,
            kStateSongArtistsEnd
        } state = kStateInit;
        int32_t stack = 0;

        std::string ext;

        void OnReadNode(xmlNode* node) final;
    };

    void SourceAmigaMusicPreservation::Collector::OnReadNode(xmlNode* node)
    {
        if (state & kStateSong)
            stack++;
        for (; node; node = node->next)
        {
            if (node->type == XML_ELEMENT_NODE)
            {
                if (state == kStateArtistHandleStep && xmlStrcmp(node->name, BAD_CAST"td") == 0)
                    state = kStateArtistHandleEnd;
                else if (state == kStateArtistNameStep && xmlStrcmp(node->name, BAD_CAST"td") == 0)
                    state = kStateArtistNameEnd;
                else if (state == kStateArtistExHandlesStep && xmlStrcmp(node->name, BAD_CAST"td") == 0)
                    state = kStateArtistExHandlesEnd;
                else for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (state == kStateArtistCountry)
                    {
                        if (xmlStrstr(node->name, BAD_CAST"img") && xmlStrstr(property->name, BAD_CAST"title"))
                            artist.countries.Add((const char*)propChild->content);
                    }
                    else if (state == kStateArtistGroups)
                    {
                        if (xmlStrstr(propChild->content, BAD_CAST"newresult.php?request=groupid&search="))
                            state = kStateArtistGroup;
                    }
                    else if (state == kStateSongArtistsBegin || state == kStateSongArtistsEnd)
                    {
                        if (xmlStrstr(propChild->content, BAD_CAST"detail.php?view="))
                        {
                            uint32_t id;
                            sscanf_s((const char*)propChild->content, "detail.php?view=%u", &id);
                            songs.Last().artists.Add(id);
                        }
                    }
                    else if (state == kStateSongs)
                    {
                        if (xmlStrstr(propChild->content, BAD_CAST"downmod.php?index="))
                        {
                            songs.Add(SourceAmigaMusicPreservation::CollectorSong());
                            stack = 0;
                            state = kStateSong;
                            sscanf_s((const char*)propChild->content, "downmod.php?index=%u", &songs.Last().id);
                        }
                    }
                }
            }
            else if (node->type == XML_TEXT_NODE && node->content) switch (state)
            {
            case kStateArtistHandleBegin:
                if (xmlStrstr(node->content, BAD_CAST"Handle:"))
                    state = kStateArtistHandleStep;
                break;
            case kStateArtistHandleEnd:
                {
                    std::string str;
                    ConvertString(node->content, str);
                    artist.handles.Add(std::move(str));
                }
                state = kStateArtistNameBegin;
                break;
            case kStateArtistNameBegin:
                if (xmlStrstr(node->content, BAD_CAST"Real\xc2\xa0Name:"))
                    state = kStateArtistNameStep;
                break;
            case kStateArtistNameEnd:
                ConvertString(node->content, artist.name);
                state = kStateArtistCountries;
                break;
            case kStateArtistCountries:
                if (xmlStrstr(node->content, BAD_CAST"Lived\xc2\xa0in:"))
                    state = kStateArtistCountry;
                break;
            case kStateArtistCountry:
                if (xmlStrstr(node->content, BAD_CAST"Ex.Handles:"))
                    state = kStateArtistExHandlesStep;
                break;
            case kStateArtistExHandlesEnd:
                {
                    std::string str;
                    ConvertString(node->content, str);
                    auto txt = str.data();
                    while (1)
                    {
                        auto next = strstr(txt, ", ");
                        if (next)
                            next[0] = 0;
                        if (strcmp(txt, "n/a") != 0)
                            artist.handles.Add(txt);
                        if (next)
                            txt = next + 2;
                        else
                            break;
                    }
                }
                state = kStateArtistGroups;
                break;
            case kStateArtistGroups:
                if (xmlStrstr(node->content, BAD_CAST"Modules:"))
                    state = kStateSongs;
                break;
            case kStateArtistGroup:
                {
                    std::string str;
                    ConvertString(node->content, str);
                    artist.groups.Add(std::move(str));
                }
                state = kStateArtistGroups;
                break;
            case kStateSong:
                ConvertString(node->content, songs.Last().name);
                state = kStateSongArtistsBegin;
                break;
            case kStateSongArtistsBegin:
                if (stack == 0)
                {
                    int32_t modSize;
                    char kb[3];
                    if (sscanf_s((const char*)node->content, "%d%s", &modSize, kb, uint32_t(sizeof(kb))) != 2 || kb[0] != 'K' || kb[1] != 'b' || kb[2] != 0)
                        ConvertString(node->content, ext);
                    else
                        state = kStateSongArtistsEnd;
                }
                break;
            default:
                break;
            }
            OnReadNode(node->children);
        }
        if (state & kStateSong)
        {
            stack--;
            if (stack == -2)
            {
                songs.Last().ext = ext;
                state = kStateSongs;
            }
        }
    }

    struct SourceAmigaMusicPreservation::SearchArtist : public WebHandler
    {
        Array<ArtistsCollection::Artist> artists;
        bool hasNextPage;
        bool isFirstEntry;

        enum
        {
            kStateInit = 0,
            kStateArtistHandleBegin = kStateInit,
            kStateArtistHandleEnd,
            kStateArtistNameBegin,
            kStateArtistNameEnd,
            kStateArtistCountries,
            kStateArtistCountry,
            kStateArtistGroups,
            kStateArtistGroup
        } state = kStateInit;

        void OnReadNode(xmlNode* node) final;
    };

    void SourceAmigaMusicPreservation::SearchArtist::OnReadNode(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (node->type == XML_ELEMENT_NODE)
            {
                for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (state == kStateArtistHandleBegin)
                    {
                        if (xmlStrstr(node->name, BAD_CAST"img") && xmlStrstr(property->name, BAD_CAST"src"))
                            hasNextPage |= xmlStrcmp(propChild->content, BAD_CAST"images/right.gif") == 0;
                    }
                    else if (state == kStateArtistHandleEnd)
                    {
                        if (xmlStrstr(propChild->content, BAD_CAST"detail.php?view="))
                        {
                            uint32_t id;
                            sscanf_s((const char*)propChild->content, "detail.php?view=%u", &id);
                            artists.Last().id = SourceID(kID, id);
                        }
                    }
                    else if (state == kStateArtistCountry)
                    {
                        if (xmlStrstr(node->name, BAD_CAST"img") && xmlStrstr(property->name, BAD_CAST"title"))
                        {
                            if (isFirstEntry)
                            {
                                artists.Last().description += "\nCountries: ";
                                isFirstEntry = false;
                            }
                            else
                                artists.Last().description += ", ";
                            artists.Last().description += (const char*)propChild->content;
                        }
                    }
                    else if (state == kStateArtistGroups)
                    {
                        if (xmlStrstr(propChild->content, BAD_CAST"newresult.php?request=groupid&search="))
                            state = kStateArtistGroup;
                    }
                }
            }
            else if (node->type == XML_TEXT_NODE && node->content) switch (state)
            {
            case kStateArtistHandleBegin:
                if (xmlStrstr(node->content, BAD_CAST"Handle:"))
                {
                    artists.Push();
                    state = kStateArtistHandleEnd;
                }
                break;
            case kStateArtistHandleEnd:
                ConvertString(node->content, artists.Last().name);
                state = kStateArtistNameBegin;
                break;
            case kStateArtistNameBegin:
                if (xmlStrstr(node->content, BAD_CAST"Real Name:"))
                    state = kStateArtistNameEnd;
                break;
            case kStateArtistNameEnd:
                {
                    std::string str;
                    ConvertString(node->content, str);
                    artists.Last().description += "Real Name: ";
                    artists.Last().description += str;
                }
                state = kStateArtistCountries;
                break;
            case kStateArtistCountries:
                if (xmlStrstr(node->content, BAD_CAST"Country:"))
                {
                    isFirstEntry = true;
                    state = kStateArtistCountry;
                }
                break;
            case kStateArtistCountry:
                if (xmlStrstr(node->content, BAD_CAST"Groups:"))
                {
                    isFirstEntry = true;
                    state = kStateArtistGroups;
                }
                break;
            case kStateArtistGroups:
                if (xmlStrstr(node->content, BAD_CAST"Handle:"))
                {
                    artists.Push();
                    state = kStateArtistHandleEnd;
                }
                break;
            case kStateArtistGroup:
                {
                    std::string str;
                    ConvertString(node->content, str);
                    if (isFirstEntry)
                    {
                        artists.Last().description += "\nGroups   : ";
                        isFirstEntry = false;
                    }
                    else
                        artists.Last().description += "\n         : ";
                    artists.Last().description += str;
                }
                state = kStateArtistGroups;
                break;
            default:
                break;
            }
            OnReadNode(node->children);
        }
    }

    struct SourceAmigaMusicPreservation::SearchSong : public WebHandler
    {
        struct Song
        {
            uint32_t id;
            std::string name;
            std::string ext;
            Array<std::pair<uint32_t, std::string>> artist;
        };
        Array<Song> songs;
        bool hasNextPage;

        std::string ext;

        enum
        {
            kStateInit = 0,
            kStateSongBegin = kStateInit,
            kStateSongEnd,
            kStateArtistBegin,
            kStateArtistContinue,
            kStateArtistEnd,
        } state = kStateInit;

        void OnReadNode(xmlNode* node) final;
    };

    void SourceAmigaMusicPreservation::SearchSong::OnReadNode(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (node->type == XML_ELEMENT_NODE)
            {
                for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (state == kStateSongBegin)
                    {
                        if (songs.IsEmpty() && xmlStrstr(node->name, BAD_CAST"img") && xmlStrstr(property->name, BAD_CAST"src"))
                            hasNextPage |= xmlStrcmp(propChild->content, BAD_CAST"images/right.gif") == 0;

                        uint32_t id;
                        if (sscanf_s((const char*)propChild->content, "downmod.php?index=%u", &id) == 1)
                        {
                            songs.Push();
                            songs.Last().id = id;
                            state = kStateSongEnd;
                        }
                    }
                    else if (state == kStateArtistBegin || state == kStateArtistContinue)
                    {
                        uint32_t id;
                        if (sscanf_s((const char*)propChild->content, "detail.php?view=%u", &id) == 1)
                        {
                            songs.Last().artist.Push();
                            songs.Last().artist.Last().first = id;
                            state = kStateArtistEnd;
                        }
                        else if (xmlStrstr(propChild->content, BAD_CAST"analyzer2.php?idx="))
                        {
                            songs.Last().ext = ext;
                            state = kStateSongBegin;
                        }
                    }
                }
            }
            else if (node->type == XML_TEXT_NODE && node->content) switch (state)
            {
            case kStateSongEnd:
                ConvertString(node->content, songs.Last().name);
                state = kStateArtistBegin;
                break;
            case kStateArtistBegin:
                {
                    int32_t modSize;
                    char kb[3];
                    if (sscanf_s((const char*)node->content, "%d%s", &modSize, kb, uint32_t(sizeof(kb))) != 2 || kb[0] != 'K' || kb[1] != 'b' || kb[2] != 0)
                        ConvertString(node->content, ext);
                    else
                        state = kStateArtistContinue;
                }
                break;
            case kStateArtistEnd:
                ConvertString(node->content, songs.Last().artist.Last().second);
                state = kStateArtistBegin;
                break;
            default:
                break;
            }
            OnReadNode(node->children);
        }
    }

    SourceAmigaMusicPreservation::SourceAmigaMusicPreservation()
    {}

    SourceAmigaMusicPreservation::~SourceAmigaMusicPreservation()
    {}

    void SourceAmigaMusicPreservation::FindArtists(ArtistsCollection& artists, const char* name)
    {
        SearchArtist search;
        for (uint32_t i = 0;; i += 50)
        {
            search.hasNextPage = false;
            search.Fetch("https://amp.dascene.net/newresult.php?request=handle&search=%s&position=%u", search.Escape(name).c_str(), i);
            if (!search.hasNextPage)
                break;
            search.state = SearchArtist::kStateInit;
        }

        auto newArtists = std::move(search.artists);
        search.hasNextPage = false;
        for (uint32_t i = 0;; i += 50)
        {
            search.hasNextPage = false;
            search.Fetch("https://amp.dascene.net/newresult.php?request=oldhandles&search=%s&position=%u", search.Escape(name).c_str(), i);
            if (!search.hasNextPage)
                break;
            search.state = SearchArtist::kStateInit;
        }
        for (auto& artist : search.artists)
        {
            if (std::find_if(newArtists.begin(), newArtists.end(), [id = artist.id](auto& entry)
            {
                return entry.id == id;
            }) == newArtists.end())
                artists.alternatives.Add(std::move(artist));
        }
        for (auto& artist : newArtists)
            artists.matches.Add(std::move(artist));
    }

    void SourceAmigaMusicPreservation::ImportArtist(SourceID importedArtistID, SourceResults& results)
    {
        assert(importedArtistID.sourceId == kID);
        results.importedArtists.Add(importedArtistID);

        Collector collector = Collect(importedArtistID.internalId);

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
            song->type = Core::GetReplays().Find(collectedSong.ext.c_str());
            if (song->type.ext == eExtension::Unknown)
            {
                if (collectedSong.ext == "FST")
                    song->type = { eExtension::_mod, eReplay::OpenMPT };
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

                    auto createArtist = [artistSourceId](const Collector& collector)
                    {
                        auto* rplArtist = new ArtistSheet();
                        if (collector.artist.name != "n/a" && collector.artist.name != "currently not public")
                            rplArtist->realName = collector.artist.name;
                        rplArtist->handles = collector.artist.handles;
                        rplArtist->groups = collector.artist.groups;
                        rplArtist->sources.Add(artistSourceId);
                        for (auto& country : collector.artist.countries)
                        {
                            if (country != "R.I.P.")
                            {
                                auto countryCode = Countries::GetCode(country.data());
                                if (countryCode == 0)
                                    assert(0 && "todo find the right country");
                                rplArtist->countries.Add(countryCode);
                            }
                        }

                        return rplArtist;
                    };

                    auto* rplArtist = createArtist(artistId == importedArtistID.internalId ? collector : Collect(artistId));
                    results.artists.Add(rplArtist);
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

    void SourceAmigaMusicPreservation::FindSongs(const char* name, SourceResults& collectedSongs)
    {
        SearchSong search;
        for (uint32_t i = 0;; i += 50)
        {
            search.hasNextPage = false;
            search.Fetch("https://amp.dascene.net/newresult.php?request=module&search=%s&position=%u", search.Escape(name).c_str(), i);
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
                song->type = Core::GetReplays().Find(searchSong.ext.c_str());
                if (song->type.ext == eExtension::Unknown)
                {
                    if (searchSong.ext == "FST")
                        song->type = { eExtension::_mod, eReplay::OpenMPT };
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
            if (!search.hasNextPage)
                break;
            search.songs.Clear();
            search.state = SearchSong::kStateInit;
        }
    }

    Source::Import SourceAmigaMusicPreservation::ImportSong(SourceID sourceId, const std::string& path)
    {
        thread::ScopedSpinLock lock(m_mutex);
        SourceID sourceToDownload = sourceId;
        assert(sourceToDownload.sourceId == kID);

        CURL* curl = curl_easy_init();

        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
        char url[128];
        sprintf(url, "https://amp.dascene.net/downmod.php?index=%u", sourceToDownload.internalId);
        Log::Message("Amiga Music Preservation: downloading \"%s\"...", url);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
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
        if (curlError == CURLE_OK && buffer.IsNotEmpty())
        {
            if (memcmp(buffer.begin(), "<!DOCTYPE html", sizeof("<!DOCTYPE html") - 1) == 0)
            {
                if (IsInvalidIndex(buffer.Items<char>(), buffer.NumItems()))
                {
                    isEntryMissing = true;
                    auto* song = FindSong(sourceToDownload.internalId);
                    m_songs.RemoveAt(song - m_songs);
                    m_isDirty = true;
                }

                Log::Error("Amiga Music Preservation: file \"%s\" not found\n", url);
            }
            else
            {
                Log::Message("OK...");

                z_stream strm = {};
                strm.next_in = reinterpret_cast<Bytef*>(buffer.Items());
                strm.avail_in = static_cast<uint32_t>(buffer.Size());

                if (inflateInit2(&strm, (16 + MAX_WBITS)) == Z_OK)
                {
                    gz_header gzhdr = {};
                    char filename[1024] = {};
                    gzhdr.name = reinterpret_cast<Bytef*>(filename);
                    gzhdr.name_max = 1024;
                    inflateGetHeader(&strm, &gzhdr);

                    Array<uint8_t> unpackedData;
                    unpackedData.Resize(buffer[buffer.NumItems() - 4] | (buffer[buffer.NumItems() - 3] << 8) | (buffer[buffer.NumItems() - 2] << 16) | (buffer[buffer.NumItems() - 1] << 24));//gzip file last 2 dwords are crc & decompressed size

                    for (;;)
                    {
                        if (strm.total_out >= unpackedData.NumItems())//just in case something is wrong with the gzip file
                            unpackedData.Resize(unpackedData.NumItems() + 65536);

                        strm.next_out = reinterpret_cast<Bytef*>(unpackedData.Items() + strm.total_out);
                        strm.avail_out = static_cast<uint32_t>(unpackedData.Size()) - strm.total_out;

                        auto zError = inflate(&strm, Z_SYNC_FLUSH);
                        if (zError == Z_STREAM_END)
                        {
                            unpackedData.Resize(strm.total_out);
                            break;
                        }
                        else if (zError != Z_OK)
                        {
                            unpackedData.Reset();
                            break;
                        }
                    }

                    if (unpackedData.IsNotEmpty())
                    {
                        SongSource* songAMP = FindSong(sourceToDownload.internalId);
                        songAMP->crc = buffer[buffer.NumItems() - 8] | (buffer[buffer.NumItems() - 7] << 8) | (buffer[buffer.NumItems() - 6] << 16) | (buffer[buffer.NumItems() - 5] << 24);
                        songAMP->size = buffer.NumItems();
                        m_isDirty = true;

                        stream = io::StreamMemory::Create(path, unpackedData.Items(), unpackedData.Size(), false);
                        unpackedData.Detach();
                    }
                }

                if (stream.IsInvalid())
                    Log::Error("Amiga Music Preservation: failed to process package\n");
                else
                    Log::Message("Unpacked\n");

                inflateEnd(&strm);
            }
        }
        else
            Log::Error("Amiga Music Preservation: %s\n", curl_easy_strerror(curlError));

        curl_easy_cleanup(curl);

        return { stream, isEntryMissing };
    }

    void SourceAmigaMusicPreservation::OnArtistUpdate(ArtistSheet* artist)
    {
        assert(artist->sources[0].id.sourceId == kID);

        Collector collector = Collect(artist->sources[0].id.internalId);
        if (collector.artist.name != "n/a" && collector.artist.name != "currently not public")
            artist->realName = std::move(collector.artist.name);
        else
            artist->realName = {};
        artist->handles = std::move(collector.artist.handles);
        artist->groups = std::move(collector.artist.groups);
        artist->countries.Clear();
        for (auto& country : collector.artist.countries)
        {
            if (country != "R.I.P.")
            {
                auto countryCode = Countries::GetCode(country.data());
                if (countryCode == 0)
                    assert(0 && "todo find the right country");
                artist->countries.Add(countryCode);
            }
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
            if (file.Read<uint64_t>() != SongSource::kVersion)
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
                file.Write(SongSource::kVersion);
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

    SourceAmigaMusicPreservation::Collector SourceAmigaMusicPreservation::Collect(uint32_t artistID) const
    {
        Collector collector;

        collector.Fetch("https://amp.dascene.net/detail.php?detail=modules&view=%u", artistID);

        return collector;
    }

    bool SourceAmigaMusicPreservation::IsInvalidIndex(const char* buffer, uint32_t size) const
    {
        if (size > (sizeof("<!DOCTYPE ") + sizeof("lalala<BR/>invalid index !<BR/>")))
            return strstr(buffer + sizeof("<!DOCTYPE "), "lalala<BR/>invalid index !<BR/>");
        return false;
    }
}
// namespace rePlayer
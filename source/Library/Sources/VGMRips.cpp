// Core
#include <Containers/HashMap.h>
#include <Core/Log.h>
#include <ImGui.h>
#include <IO/File.h>
#include <IO/StreamMemory.h>
#include <Thread/Thread.h>

// rePlayer
#include <Database/Database.h>
#include <UI/BusySpinner.h>
#include "VGMRips.h"
#include "WebHandler.h"

// zlib
#include <zlib.h>

// stl
#include <algorithm>

namespace rePlayer
{
    const char* const SourceVGMRips::ms_filename = MusicPath "VGMRips" MusicExt;

    inline const char* SourceVGMRips::Chars::operator()(const Array<char>& blob) const
    {
        return blob.Items() + offset;
    }

    inline void SourceVGMRips::Chars::Set(Array<char>& blob, const char* otherString)
    {
        auto len = strlen(otherString);
        if (len)
        {
            offset = blob.NumItems();
            blob.Add(otherString, uint32_t(len + 1));
        }
    }

    inline void SourceVGMRips::Chars::Set(Array<char>& blob, const std::string& otherString)
    {
        if (otherString.size())
        {
            offset = blob.NumItems();
            blob.Add(otherString.c_str(), uint32_t(otherString.size() + 1));
        }
    }

    template <typename T>
    inline void SourceVGMRips::Chars::Copy(const Array<char>& blob, Array<T>& otherblob) const
    {
        auto src = blob.Items() + offset;
        otherblob.Copy(src, strlen(src) + 1);
    }

    template <bool isCaseSensitive>
    inline bool SourceVGMRips::Chars::IsSame(const Array<char>& blob, const char* otherString) const
    {
        auto src = blob.Items() + offset;
        while (*src && *otherString)
        {
            if constexpr (isCaseSensitive)
            {
                if (*src != *otherString)
                    return false;
            }
            else if (::tolower(*src) != ::tolower(*otherString))
                return false;
            src++;
            otherString++;
        }
        return *src == *otherString;
    }

    struct SourceVGMRips::ArtistsCollector : public WebHandler
    {
        SourceVGMRips::DB& db;

        ArtistsCollector(SourceVGMRips::DB& other);
        void OnReadNode(xmlNode* node) final;
        void ReadArtist(xmlNode* node);
    };

    SourceVGMRips::ArtistsCollector::ArtistsCollector(SourceVGMRips::DB& other)
        : db(other)
    {}

    void SourceVGMRips::ArtistsCollector::OnReadNode(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (node->type == XML_ELEMENT_NODE)
            {
                if (xmlStrcmp(node->name, BAD_CAST"li") == 0) for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (xmlStrcmp(propChild->content, BAD_CAST"composer") == 0)
                    {
                        ReadArtist(node->children);
                        break;
                    }
                }
            }
            OnReadNode(node->children);
        }
    }

    void SourceVGMRips::ArtistsCollector::ReadArtist(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (xmlStrcmp(node->name, BAD_CAST"a") == 0) for (auto* property = node->properties; property; property = property->next)
            {
                if (xmlStrcmp(property->name, BAD_CAST"href") == 0) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (propChild->type == XML_TEXT_NODE)
                    {
                        if (auto* artistUrl = xmlStrstr(propChild->content, BAD_CAST"https://vgmrips.net/packs/composer/"))
                        {
                            auto dataOffset = db.data.NumItems();
                            artistUrl += sizeof("https://vgmrips.net/packs/composer");
                            auto* artist = db.artists.Push();
                            artist->url.offset = artist->name.offset = dataOffset;
                            artist->packs = 0;
                            artist->isComplete = 0;
                            db.data.Add(reinterpret_cast<const char*>(artistUrl), xmlStrlen(artistUrl) + 1);

                            for (node = node->children; node; node = node->next)
                            {
                                if (node->prev && xmlStrcmp(node->name, BAD_CAST"br") == 0)
                                {
                                    node = node->prev;
                                    auto* name = reinterpret_cast<const char*>(node->content);
                                    while (*name && (*name == '\r' || *name == '\n' || *name == '\t'))
                                        name++;
                                    auto* nameEnd = name;
                                    while (*nameEnd && *nameEnd != '\r' && *nameEnd != '\n' && *nameEnd != '\t')
                                        nameEnd++;
                                    if (name != nameEnd)
                                    {
                                        artist->name.offset = db.data.NumItems();
                                        db.data.Add(name, uint32_t(nameEnd - name));
                                        db.data.Add(0);
                                    }
                                    break;
                                }
                            }
                            return;
                        }
                    }
                }
            }
        }
    }

    struct SourceVGMRips::ArtistCollector : public WebHandler
    {
        SourceVGMRips::DB& db;

        std::string name;
        std::string url;

        enum
        {
            kStateInit = 0,
            kStatePack,
            kStateChips,
            kStateSystems,
            kStateArtists,
            kStateEnd
        } state = kStateInit;
        bool isSkipped = false;
        bool isDone = false;

        ArtistCollector(SourceVGMRips::DB& other);
        void OnReadNode(xmlNode* node) final;
        void ReadPack(xmlNode* node);
        void ReadChips(xmlNode* node);
        void ReadSystems(xmlNode* node);
        void ReadArtists(xmlNode* node);
    };

    SourceVGMRips::ArtistCollector::ArtistCollector(SourceVGMRips::DB& other)
        : db(other)
    {}

    void SourceVGMRips::ArtistCollector::OnReadNode(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (state == kStateInit && node->type == XML_TEXT_NODE)
            {
                if (auto* total = xmlStrstr(node->content, BAD_CAST"Packs "))
                {
                    uint32_t a, b, c;
                    if (sscanf_s(reinterpret_cast<const char*>(total), "Packs %u to %u of %u total", &a, &b, &c) == 3)
                    {
                        state = kStatePack;
                        isDone = b == c;
                    }
                }
            }
            else if (state == kStatePack)
            {
                if (xmlStrcmp(node->name, BAD_CAST"h2") == 0) for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (xmlStrcmp(propChild->content, BAD_CAST"clearfix title") == 0)
                    {
                        ReadPack(node->children);
                        break;
                    }
                }
            }
            else if (state == kStateChips)
            {
                if (xmlStrcmp(node->name, BAD_CAST"tr") == 0) for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (xmlStrcmp(propChild->content, BAD_CAST"chips") == 0)
                    {
                        if (!isSkipped)
                            ReadChips(node->children);
                        state = kStateSystems;
                        break;
                    }
                }
            }
            else if (state == kStateSystems)
            {
                if (xmlStrcmp(node->name, BAD_CAST"tr") == 0) for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (xmlStrcmp(propChild->content, BAD_CAST"systems") == 0)
                    {
                        if (!isSkipped)
                        {
                            ReadSystems(node->children);

                            auto urlOffset = db.data.NumItems();
                            db.data.Add(url.c_str(), uint32_t(url.size() + 1));
                            auto nameOffset = db.data.NumItems();
                            db.data.Add(name.c_str(), uint32_t(name.size() + 1));
                            db.data.Resize(AlignUp(db.data.NumItems(), alignof(VgmRipsPack)));
                            db.packs.Add(db.data.NumItems());
                            auto* pack = db.data.Push<VgmRipsPack*>(sizeof(VgmRipsPack));
                            pack->url.offset = urlOffset;
                            pack->name.offset = nameOffset;
                            pack->songsUrl.offset = 0;
                            pack->songs = 0;
                            pack->year = 0;
                            pack->numArtists = 0;
                        }

                        state = kStateArtists;
                        break;
                    }
                }
            }
            else if (state == kStateArtists)
            {
                if (xmlStrcmp(node->name, BAD_CAST"tr") == 0) for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (xmlStrcmp(propChild->content, BAD_CAST"composers") == 0)
                    {
                        if (!isSkipped)
                            ReadArtists(node->children);
                        state = kStatePack;
                        break;
                    }
                }
            }
            OnReadNode(node->children);
        }
    }

    void SourceVGMRips::ArtistCollector::ReadPack(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (node->type == XML_ELEMENT_NODE)
            {
                if (xmlStrcmp(node->name, BAD_CAST"a") == 0) for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (propChild->type == XML_TEXT_NODE && xmlStrstr(propChild->content, BAD_CAST"#autoplay") == nullptr)
                    {
                        if (auto* packUrl = xmlStrstr(propChild->content, BAD_CAST"https://vgmrips.net/packs/pack/"))
                        {
                            packUrl += sizeof("https://vgmrips.net/packs/pack");
                            url = reinterpret_cast<const char*>(packUrl);
                            name.clear();
                            for (node = node->children; node; node = node->next)
                            {
                                if (node->type == XML_TEXT_NODE)
                                {
                                    name = reinterpret_cast<const char*>(node->content);
                                    break;
                                }
                            }
                            isSkipped = false;
                            for (auto packOffset : db.packs)
                            {
                                auto* pack = db.data.Items<VgmRipsPack>(packOffset);
                                if (url == pack->url(db.data))
                                {
                                    isSkipped = true;
                                    break;
                                }
                            }

                            state = kStateChips;
                            return;
                        }
                    }
                }
            }
        }
    }

    void SourceVGMRips::ArtistCollector::ReadChips(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (node->type == XML_ELEMENT_NODE)
            {
                if (xmlStrcmp(node->name, BAD_CAST"a") == 0) for (auto* property = node->properties; property; property = property->next)
                {
                    if (xmlStrcmp(property->name, BAD_CAST"href") == 0) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                    {
                        if (propChild->type == XML_TEXT_NODE)
                        {
                            if (xmlStrstr(propChild->content, BAD_CAST"https://vgmrips.net/packs/chip/"))
                            {
                                for (auto* child = node->children; child; child = child->next)
                                {
                                    if (child->type == XML_TEXT_NODE)
                                    {
                                        name += ';';
                                        name += reinterpret_cast<const char*>(child->content);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            ReadChips(node->children);
        }
    }

    void SourceVGMRips::ArtistCollector::ReadSystems(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (node->type == XML_ELEMENT_NODE)
            {
                if (xmlStrcmp(node->name, BAD_CAST"a") == 0) for (auto* property = node->properties; property; property = property->next)
                {
                    if (xmlStrcmp(property->name, BAD_CAST"href") == 0) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                    {
                        if (propChild->type == XML_TEXT_NODE)
                        {
                            if (xmlStrstr(propChild->content, BAD_CAST"https://vgmrips.net/packs/system/"))
                            {
                                for (auto* child = node->children; child; child = child->next)
                                {
                                    if (child->type == XML_TEXT_NODE)
                                    {
                                        name += ';';
                                        name += reinterpret_cast<const char*>(child->content);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            ReadSystems(node->children);
        }
    }

    void SourceVGMRips::ArtistCollector::ReadArtists(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (node->type == XML_ELEMENT_NODE)
            {
                if (xmlStrcmp(node->name, BAD_CAST"a") == 0) for (auto* property = node->properties; property; property = property->next)
                {
                    if (xmlStrcmp(property->name, BAD_CAST"href") == 0) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                    {
                        if (propChild->type == XML_TEXT_NODE)
                        {
                            if (auto* artistUrl = xmlStrstr(propChild->content, BAD_CAST"https://vgmrips.net/packs/composer/"))
                            {
                                artistUrl += sizeof("https://vgmrips.net/packs/composer");
                                bool isFound = false;
                                for (uint32_t i = 0, e = db.artists.NumItems(); i < e; i++)
                                {
                                    if (_stricmp(db.artists[i].url(db.data), pcCast<char>(artistUrl)) == 0)
                                    {
                                        auto& packArtist = db.data.Push<decltype(VgmRipsPack::artists[0])&>(sizeof(VgmRipsPack::artists[0]));
                                        packArtist.index = uint16_t(i);
                                        db.data.Items<VgmRipsPack>(db.packs.Last())->numArtists++;
                                        packArtist.nextPack = db.artists[i].packs;
                                        db.artists[i].packs = db.packs.Last();
                                        isFound = true;
                                        break;
                                    }
                                }
                                if (!isFound)
                                    Log::Warning("VGMRips: artist \"%s\" is missing from database\n", artistUrl);
                            }
                        }
                    }
                }
            }
            ReadArtists(node->children);
        }
    }

    struct SourceVGMRips::PacksCollector : public WebHandler
    {
        SourceVGMRips::DB& db;

        std::string name;
        std::string url;

        enum
        {
            kStateInit = 0,
            kStatePack,
            kStateChips,
            kStateSystems,
            kStateArtists,
            kStateEnd
        } state = kStateInit;
        bool isSkipped = false;
        bool isDone = false;

        PacksCollector(SourceVGMRips::DB& other);
        void OnReadNode(xmlNode* node) final;
        void ReadPack(xmlNode* node);
        void ReadChips(xmlNode* node);
        void ReadSystems(xmlNode* node);
        void ReadArtists(xmlNode* node);
    };

    SourceVGMRips::PacksCollector::PacksCollector(SourceVGMRips::DB& other)
        : db(other)
    {}

    void SourceVGMRips::PacksCollector::OnReadNode(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (state == kStateInit && node->type == XML_TEXT_NODE)
            {
                if (auto* total = xmlStrstr(node->content, BAD_CAST"Packs "))
                {
                    uint32_t a, b, c;
                    if (sscanf_s(reinterpret_cast<const char*>(total), "Packs %u to %u of %u total", &a, &b, &c) == 3)
                    {
                        state = kStatePack;
                        isDone = b == c;
                    }
                }
            }
            else if (state == kStatePack)
            {
                if (xmlStrcmp(node->name, BAD_CAST"h2") == 0) for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (xmlStrcmp(propChild->content, BAD_CAST"clearfix title") == 0)
                    {
                        ReadPack(node->children);
                        break;
                    }
                }
            }
            else if (state == kStateChips)
            {
                if (xmlStrcmp(node->name, BAD_CAST"tr") == 0) for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (xmlStrcmp(propChild->content, BAD_CAST"chips") == 0)
                    {
                        if (!isSkipped)
                            ReadChips(node->children);
                        state = kStateSystems;
                        break;
                    }
                }
            }
            else if (state == kStateSystems)
            {
                if (xmlStrcmp(node->name, BAD_CAST"tr") == 0) for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (xmlStrcmp(propChild->content, BAD_CAST"systems") == 0)
                    {
                        if (!isSkipped)
                        {
                            ReadSystems(node->children);

                            auto urlOffset = db.data.NumItems();
                            db.data.Add(url.c_str(), uint32_t(url.size() + 1));
                            auto nameOffset = db.data.NumItems();
                            db.data.Add(name.c_str(), uint32_t(name.size() + 1));
                            db.data.Resize(AlignUp(db.data.NumItems(), alignof(VgmRipsPack)));
                            db.packs.Add(db.data.NumItems());
                            auto* pack = db.data.Push<VgmRipsPack*>(sizeof(VgmRipsPack));
                            pack->url.offset = urlOffset;
                            pack->name.offset = nameOffset;
                            pack->songsUrl.offset = 0;
                            pack->songs = 0;
                            pack->year = 0;
                            pack->numArtists = 0;
                        }

                        state = kStateArtists;
                        break;
                    }
                }
            }
            else if (state == kStateArtists)
            {
                if (xmlStrcmp(node->name, BAD_CAST"tr") == 0) for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (xmlStrcmp(propChild->content, BAD_CAST"composers") == 0)
                    {
                        if (!isSkipped)
                            ReadArtists(node->children);
                        state = kStatePack;
                        break;
                    }
                }
            }
            OnReadNode(node->children);
        }
    }

    void SourceVGMRips::PacksCollector::ReadPack(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (node->type == XML_ELEMENT_NODE)
            {
                if (xmlStrcmp(node->name, BAD_CAST"a") == 0) for (auto* property = node->properties; property; property = property->next) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                {
                    if (propChild->type == XML_TEXT_NODE && xmlStrstr(propChild->content, BAD_CAST"#autoplay") == nullptr)
                    {
                        if (auto* packUrl = xmlStrstr(propChild->content, BAD_CAST"https://vgmrips.net/packs/pack/"))
                        {
                            packUrl += sizeof("https://vgmrips.net/packs/pack");
                            url = reinterpret_cast<const char*>(packUrl);
                            name.clear();
                            for (node = node->children; node; node = node->next)
                            {
                                if (node->type == XML_TEXT_NODE)
                                {
                                    name = reinterpret_cast<const char*>(node->content);
                                    break;
                                }
                            }
                            isSkipped = false;
                            for (auto packOffset : db.packs)
                            {
                                auto* pack = db.data.Items<VgmRipsPack>(packOffset);
                                if (url == pack->url(db.data))
                                {
                                    isSkipped = true;
                                    break;
                                }
                            }
                            state = kStateChips;
                            return;
                        }
                    }
                }
            }
        }
    }

    void SourceVGMRips::PacksCollector::ReadChips(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (node->type == XML_ELEMENT_NODE)
            {
                if (xmlStrcmp(node->name, BAD_CAST"a") == 0) for (auto* property = node->properties; property; property = property->next)
                {
                    if (xmlStrcmp(property->name, BAD_CAST"href") == 0) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                    {
                        if (propChild->type == XML_TEXT_NODE)
                        {
                            if (xmlStrstr(propChild->content, BAD_CAST"https://vgmrips.net/packs/chip/"))
                            {
                                for (auto* child = node->children; child; child = child->next)
                                {
                                    if (child->type == XML_TEXT_NODE)
                                    {
                                        name += ';';
                                        name += reinterpret_cast<const char*>(child->content);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            ReadChips(node->children);
        }
    }

    void SourceVGMRips::PacksCollector::ReadSystems(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (node->type == XML_ELEMENT_NODE)
            {
                if (xmlStrcmp(node->name, BAD_CAST"a") == 0) for (auto* property = node->properties; property; property = property->next)
                {
                    if (xmlStrcmp(property->name, BAD_CAST"href") == 0) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                    {
                        if (propChild->type == XML_TEXT_NODE)
                        {
                            if (xmlStrstr(propChild->content, BAD_CAST"https://vgmrips.net/packs/system/"))
                            {
                                for (auto* child = node->children; child; child = child->next)
                                {
                                    if (child->type == XML_TEXT_NODE)
                                    {
                                        name += ';';
                                        name += reinterpret_cast<const char*>(child->content);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            ReadSystems(node->children);
        }
    }

    void SourceVGMRips::PacksCollector::ReadArtists(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            if (node->type == XML_ELEMENT_NODE)
            {
                if (xmlStrcmp(node->name, BAD_CAST"a") == 0) for (auto* property = node->properties; property; property = property->next)
                {
                    if (xmlStrcmp(property->name, BAD_CAST"href") == 0) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                    {
                        if (propChild->type == XML_TEXT_NODE)
                        {
                            if (auto* artistUrl = xmlStrstr(propChild->content, BAD_CAST"https://vgmrips.net/packs/composer/"))
                            {
                                artistUrl += sizeof("https://vgmrips.net/packs/composer");
                                bool isFound = false;
                                for (uint32_t i = 0, e = db.artists.NumItems(); i < e; i++)
                                {
                                    if (_stricmp(db.artists[i].url(db.data), pcCast<char>(artistUrl)) == 0)
                                    {
                                        auto& packArtist = db.data.Push<decltype(VgmRipsPack::artists[0])&>(sizeof(VgmRipsPack::artists[0]));
                                        packArtist.index = uint16_t(i);
                                        db.data.Items<VgmRipsPack>(db.packs.Last())->numArtists++;
                                        packArtist.nextPack = db.artists[i].packs;
                                        db.artists[i].packs = db.packs.Last();
                                        db.artists[i].isComplete = true;
                                        isFound = true;
                                        break;
                                    }
                                }
                                if (!isFound)
                                    Log::Warning("VGMRips: artist \"%s\" is missing from database\n", artistUrl);
                            }
                        }
                    }
                }
            }
            ReadArtists(node->children);
        }
    }

    struct SourceVGMRips::PackCollector : public WebHandler
    {
        SourceVGMRips::DB& db;

        const uint32_t packOffset;
        uint32_t songOffset;

        enum
        {
            kStateInit = 0,
            kStateYear,
            kStateSongs,
            kStateEnd
        } state = kStateInit;

        PackCollector(SourceVGMRips::DB& other, uint32_t packOffset);
        void OnReadNode(xmlNode* node) final;
    };

    SourceVGMRips::PackCollector::PackCollector(SourceVGMRips::DB& other, uint32_t packOffset)
        : db(other)
        , packOffset(packOffset)
    {}

    void SourceVGMRips::PackCollector::OnReadNode(xmlNode* node)
    {
        for (; node; node = node->next)
        {
            bool skipChildren = false;
            if (state == kStateInit && node->type == XML_TEXT_NODE)
            {
                if (auto* total = xmlStrstr(node->content, BAD_CAST"Release date:"))
                {
                    state = kStateYear;
                    skipChildren = true;
                }
            }
            else if (state == kStateYear && node->type == XML_TEXT_NODE)
            {
                uint32_t year = 0;
                sscanf_s(reinterpret_cast<const char*>(node->content), "%u", &year);
                db.data.Items<VgmRipsPack>(packOffset)->year = uint16_t(year);
                state = kStateSongs;
                skipChildren = true;
            }
            else if (state == kStateSongs && node->type == XML_ELEMENT_NODE)
            {
                if (xmlStrcmp(node->name, BAD_CAST"tr") == 0) for (auto* property = node->properties; property; property = property->next)
                {
                    if (xmlStrcmp(property->name, BAD_CAST"data-vgmurl") == 0) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                    {
                        if (auto* packUrl = xmlStrstr(propChild->content, BAD_CAST"https://vgmrips.net/packs/vgm/"))
                        {
                            packUrl += sizeof("https://vgmrips.net/packs/vgm");
                            auto* songUrl = packUrl;
                            for (auto* c = songUrl; *c; c++)
                            {
                                if (*c == '/')
                                    songUrl = c + 1;
                            }
                            if (db.data.Items<VgmRipsPack>(packOffset)->songsUrl.offset == 0)
                            {
                                db.data.Items<VgmRipsPack>(packOffset)->songsUrl.offset = db.data.NumItems();
                                db.data.Add(pcCast<char>(packUrl), uint32_t(songUrl - packUrl));
                                db.data.Last() = 0;
                            }
                            db.data.Resize(AlignUp(db.data.NumItems(), alignof(VgmRipsSong)));
                            songOffset = db.data.NumItems();
                            db.data.Push(sizeof(VgmRipsSong));
                            db.data.Items<VgmRipsSong>(songOffset)->url.Set(db.data, pcCast<char>(songUrl));
                            db.data.Items<VgmRipsSong>(songOffset)->nextSong = db.data.Items<VgmRipsPack>(packOffset)->songs;
                            db.data.Items<VgmRipsPack>(packOffset)->songs = songOffset;
                        }
                    }
                }
                else if (xmlStrcmp(node->name, BAD_CAST"td") == 0) for (auto* property = node->properties; property; property = property->next)
                {
                    if (xmlStrcmp(property->name, BAD_CAST"class") == 0) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                    {
                        if (xmlStrcmp(propChild->content, BAD_CAST"number") == 0)
                        {
                            auto* child = node->children;
                            while (child->children)
                                child = child->children;
                            db.data.Items<VgmRipsSong>(songOffset)->name.Set(db.data, pcCast<char>(child->content));
                            db.data.Pop();
                        }
                    }
                }
                else if (xmlStrcmp(node->name, BAD_CAST"a") == 0) for (auto* property = node->properties; property; property = property->next)
                {
                    if (xmlStrcmp(property->name, BAD_CAST"class") == 0) for (auto* propChild = property->children; propChild; propChild = propChild->next)
                    {
                        if (xmlStrcmp(propChild->content, BAD_CAST"beginPlay") == 0)
                        {
                            auto* child = node->children;
                            while (child->children)
                                child = child->children;
                            db.data.Add(pcCast<char>(child->content), xmlStrlen(child->content) + 1);
                            skipChildren = true;
                        }
                    }
                }
            }
            if (!skipChildren)
                OnReadNode(node->children);
        }
    }

    SourceVGMRips::SourceVGMRips()
        : Source(true)
    {}

    SourceVGMRips::~SourceVGMRips()
    {}

    void SourceVGMRips::FindArtists(ArtistsCollection& artists, const char* name, BusySpinner& busySpinner)
    {
        if (DownloadArtists(busySpinner))
            return;

        // look for the artist
        auto lName = ToLower(name);
        for (auto& artist : m_db.artists)
        {
            if (strstr(ToLower(artist.name(m_db.data)).c_str(), lName.c_str()))
            {
                auto* newArtist = artists.matches.Push();
                newArtist->name = artist.name(m_db.data);
                newArtist->id = SourceID(kID, FindArtist(artist.url(m_db.data))->id);
            }
        }
    }

    void SourceVGMRips::ImportArtist(SourceID importedArtistID, SourceResults& results, BusySpinner& busySpinner)
    {
        assert(importedArtistID.sourceId == kID);
        results.importedArtists.Add(importedArtistID);

        auto artistSourceIndex = m_artists.FindIf<uint32_t>([&](auto& item)
        {
            return item.id == importedArtistID.internalId;
        });
        busySpinner.Info(m_artists[artistSourceIndex].url(m_data));

        if (DownloadArtists(busySpinner))
            return;

        // validation (has the artist been removed or renamed)
        auto* dbArtist = m_db.artists.FindIf([&](auto& item)
        {
            return item.url.IsSame(m_db.data, m_artists[artistSourceIndex].url(m_data));
        });
        if (dbArtist == nullptr)
        {
            // has the artist disappeared from VGMRips?
            Log::Error("VGMRips: can't find artist \"%s\"\n", m_artists[artistSourceIndex].url(m_data));
            return;
        }

        // collect all packs
        if (dbArtist->isComplete == false)
        {
            auto* message = busySpinner.Info("downloading artist packs database at %u", 0);
            ArtistCollector collector(m_db);
            for (uint32_t i = 0; !collector.isDone; i++)
            {
                busySpinner.UpdateMessageParam(message, i);

                collector.state = ArtistCollector::kStateInit;
                if (collector.Fetch("https://vgmrips.net/packs/composer/%s?p=%u", m_artists[artistSourceIndex].url(m_data), i) != Status::kOk)
                    collector.isDone = true;
            }
            dbArtist->isComplete = true;
        }

        // collect all songs
        auto* message = busySpinner.Info("downloading artist songs database from %s", "");
        for (auto packOffset = dbArtist->packs, nextOffset = 0u; packOffset; packOffset = nextOffset)
        {
            auto getPack = [&]() { return m_db.data.Items<VgmRipsPack>(packOffset); };

            if (getPack()->songs == 0)
            {
                busySpinner.UpdateMessageParam(message, getPack()->url(m_db.data));

                PackCollector packCollector(m_db, packOffset);
                if (packCollector.Fetch("https://vgmrips.net/packs/pack/%s", getPack()->url(m_db.data)) != Status::kOk)
                    break;
            }

            // build pack
            auto sourcePackOffset = m_songs.FindIf<int64_t>([&](auto& song)
            {
                auto* packSource = m_data.Items<PackSource>(song.pack);
                return song.pack && packSource->url.IsSame(m_data, getPack()->songsUrl(m_db.data));
            });
            if (sourcePackOffset < 0)
            {
                m_data.Resize(AlignUp(m_data.NumItems(), alignof(PackSource)));
                sourcePackOffset = m_data.NumItems();
                m_data.Push(sizeof(PackSource) + getPack()->numArtists * sizeof(uint32_t));
                m_data.Items<PackSource>(uint32_t(sourcePackOffset))->url.Set(m_data, getPack()->songsUrl(m_db.data));
                m_data.Items<PackSource>(uint32_t(sourcePackOffset))->numArtists = getPack()->numArtists;
            }
            else
                sourcePackOffset = m_songs[sourcePackOffset].pack;

            // remap artists
            for (uint32_t i = 0; i < getPack()->numArtists; i++)
            {
                auto* newArtistSource = FindArtist(m_db.artists[getPack()->artists[i].index].url(m_db.data));
                m_data.Items<PackSource>(uint32_t(sourcePackOffset))->artists[i] = newArtistSource->id;
                SourceID artistId(kID, newArtistSource->id);
                auto* artistIt = results.artists.FindIf([&](auto& entry)
                {
                    return entry->sources[0].id == artistId;
                });
                if (artistIt == nullptr)
                {
                    auto artist = new ArtistSheet();
                    artist->handles.Add(m_db.artists[getPack()->artists[i].index].name(m_db.data));
                    artist->sources.Add(artistId);
                    getPack()->artists[i].remap = results.artists.NumItems<uint16_t>();
                    results.artists.Add(artist);
                }
                else
                    getPack()->artists[i].remap = uint16_t(artistIt - results.artists);

                // find also next pack
                if (getPack()->artists[i].index == uint32_t(dbArtist - m_db.artists))
                    nextOffset = getPack()->artists[i].nextPack;
            }

            // collect all songs from the pack
            for (auto songOffset = getPack()->songs; songOffset; songOffset = m_db.data.Items<VgmRipsSong>(songOffset)->nextSong)
            {
                auto* songSource = FindSong(uint32_t(sourcePackOffset), m_db.data.Items<VgmRipsSong>(songOffset)->url(m_db.data));
                SourceID songSourceId(kID, songSource->id);
                if (results.IsSongAvailable(songSourceId))
                    continue;

                auto* song = new SongSheet();

                SourceResults::State state;
                song->id = songSource->songId;
                if (songSource->isDiscarded)
                    state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kDiscarded : SourceResults::kMerged);
                else
                    song->id == SongID::Invalid ? state.SetSongStatus(SourceResults::kNew).SetChecked(true) : state.SetSongStatus(SourceResults::kOwned);

                song->name = getPack()->name(m_db.data);
                song->name.String() += '/';
                song->name.String() += m_db.data.Items<VgmRipsSong>(songOffset)->name(m_db.data);
                song->type = { eExtension::_vgz, eReplay::VGM };
                song->releaseYear = getPack()->year;
                song->sourceIds.Add(songSourceId);
                for (uint32_t i = 0; i < getPack()->numArtists; i++)
                    song->artistIds.Add(ArtistID(getPack()->artists[i].remap));

                results.songs.Add(song);
                results.states.Add(state);
            }
        }
    }

    void SourceVGMRips::FindSongs(const char* name, SourceResults& collectedSongs, BusySpinner& busySpinner)
    {
        if (DownloadArtists(busySpinner))
            return;

        // download packs database
        if (!m_db.IsFullPacks)
        {
            auto* message = busySpinner.Info("downloading packs database at %u", 0);

            PacksCollector collector(m_db);
            for (uint32_t i = 0; !collector.isDone; i++)
            {
                busySpinner.UpdateMessageParam(message, i);

                // throttle to avoid flooding website
                if ((i & 15) == 15)
                    thread::Sleep(256 + (rand() & 0x1ff));

                collector.state = PacksCollector::kStateInit;
                if (collector.Fetch("https://vgmrips.net/packs/latest?p=%u", i) != Status::kOk)
                    collector.isDone = true;
                else
                    m_db.IsFullPacks = true;
            }
        }

        // collect all songs
        auto* message = busySpinner.Info("downloading songs database from %s", "");
        auto lName = ToLower(name);
        for (uint32_t packIdx = 0; packIdx < m_db.packs.NumItems(); packIdx++)
        {
            auto packOffset = m_db.packs[packIdx];
            auto getPack = [&]() { return m_db.data.Items<VgmRipsPack>(packOffset); };
            if (strstr(ToLower(getPack()->name(m_db.data)).c_str(), lName.c_str()))
            {
                char txt[1024];
                sprintf(txt, "%s / %u%%", getPack()->url(m_db.data), uint32_t(((packIdx + 1) * 100ull) / m_db.packs.NumItems()));
                busySpinner.UpdateMessageParam(message, txt);

                if (getPack()->songs == 0)
                {
                    PackCollector packCollector(m_db, packOffset);
                    if (packCollector.Fetch("https://vgmrips.net/packs/pack/%s", getPack()->url(m_db.data)) != Status::kOk)
                        break;
                }

                // throttle to avoid flooding website
                if ((packIdx & 31) == 31)
                    thread::Sleep(256 + (rand() & 0x1ff));

                // build pack
                auto sourcePackOffset = m_songs.FindIf<int64_t>([&](auto& song)
                {
                    auto* packSource = m_data.Items<PackSource>(song.pack);
                    return song.pack && packSource->url.IsSame(m_data, getPack()->songsUrl(m_db.data));
                });
                if (sourcePackOffset < 0)
                {
                    m_data.Resize(AlignUp(m_data.NumItems(), alignof(PackSource)));
                    sourcePackOffset = m_data.NumItems();
                    m_data.Push(sizeof(PackSource) + getPack()->numArtists * sizeof(uint32_t));
                    m_data.Items<PackSource>(uint32_t(sourcePackOffset))->url.Set(m_data, getPack()->songsUrl(m_db.data));
                    m_data.Items<PackSource>(uint32_t(sourcePackOffset))->numArtists = getPack()->numArtists;
                }
                else
                    sourcePackOffset = m_songs[sourcePackOffset].pack;

                // remap artists
                for (uint32_t i = 0; i < getPack()->numArtists; i++)
                {
                    auto* newArtistSource = FindArtist(m_db.artists[getPack()->artists[i].index].url(m_db.data));
                    m_data.Items<PackSource>(uint32_t(sourcePackOffset))->artists[i] = newArtistSource->id;
                    SourceID artistId(kID, newArtistSource->id);
                    auto* artistIt = collectedSongs.artists.FindIf([&](auto& entry)
                    {
                        return entry->sources[0].id == artistId;
                    });
                    if (artistIt == nullptr)
                    {
                        auto artist = new ArtistSheet();
                        artist->handles.Add(m_db.artists[getPack()->artists[i].index].name(m_db.data));
                        artist->sources.Add(artistId);
                        getPack()->artists[i].remap = collectedSongs.artists.NumItems<uint16_t>();
                        collectedSongs.artists.Add(artist);
                    }
                    else
                        getPack()->artists[i].remap = uint16_t(artistIt - collectedSongs.artists);
                }

                // collect all songs from the pack
                for (auto songOffset = getPack()->songs; songOffset; songOffset = m_db.data.Items<VgmRipsSong>(songOffset)->nextSong)
                {
                    auto* songSource = FindSong(uint32_t(sourcePackOffset), m_db.data.Items<VgmRipsSong>(songOffset)->url(m_db.data));
                    SourceID songSourceId(kID, songSource->id);
                    if (collectedSongs.IsSongAvailable(songSourceId))
                        continue;

                    auto* song = new SongSheet();

                    SourceResults::State state;
                    song->id = songSource->songId;
                    if (songSource->isDiscarded)
                        state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kDiscarded : SourceResults::kMerged);
                    else
                        song->id == SongID::Invalid ? state.SetSongStatus(SourceResults::kNew) : state.SetSongStatus(SourceResults::kOwned);

                    song->name = getPack()->name(m_db.data);
                    song->name.String() += '/';
                    song->name.String() += m_db.data.Items<VgmRipsSong>(songOffset)->name(m_db.data);
                    song->type = { eExtension::_vgz, eReplay::VGM };
                    song->releaseYear = getPack()->year;
                    song->sourceIds.Add(songSourceId);
                    for (uint32_t i = 0; i < getPack()->numArtists; i++)
                        song->artistIds.Add(ArtistID(getPack()->artists[i].remap));

                    collectedSongs.songs.Add(song);
                    collectedSongs.states.Add(state);
                }
            }
        }
    }

    Source::Import SourceVGMRips::ImportSong(SourceID sourceId, const std::string& path)
    {
        thread::ScopedSpinLock lock(m_mutex);
        SourceID sourceToDownload = sourceId;
        assert(sourceToDownload.sourceId == kID);

        auto* songSource = m_songs.FindIf([&](auto& item)
        {
            return item.id == sourceToDownload.internalId;
        });
        auto* packSource = m_data.Items<PackSource>(songSource->pack);

        CURL* curl = curl_easy_init();

        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
        std::string url = "https://vgmrips.net/packs/vgm/";
        url += packSource->url(m_data);
        url += '/';
        url += songSource->url(m_data);
        Log::Message("VGMRips: downloading \"%s\"...", url.c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_REFERER, "https://vgmrips.net/");

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
            songSource->crc = crc32(0L, Z_NULL, 0);
            songSource->crc = crc32_z(songSource->crc, buffer.Items(), buffer.Size<size_t>());
            songSource->size = buffer.Size<uint32_t>();

            stream = io::StreamMemory::Create(path, buffer.Items(), buffer.Size(), false);
            buffer.Detach();
            m_isDirty = true;

            Log::Message("OK\n");
        }
        else if (curlError == CURLE_HTTP_RETURNED_ERROR)
        {
            long responseCode = 0;
            if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode) == CURLE_OK && responseCode == 404)
            {
                Log::Error("VGMRips: file \"%s\" not found\n", url.c_str());

                isEntryMissing = true;
                songSource->crc = 0;
                songSource->size  = 0;
                songSource->isDiscarded = 0;
                songSource->songId = SongID::Invalid;
                songSource->pack = 0;
                songSource->url.offset = 0;
                m_isDirty = true;
                m_areDataDirty = true;
            }
            else
                Log::Error("VGMRips: %s\n", curl_easy_strerror(curlError));
        }
        else
            Log::Error("VGMRips: %s\n", curl_easy_strerror(curlError));

        curl_easy_cleanup(curl);

        return { stream, isEntryMissing };
    }

    void SourceVGMRips::OnArtistUpdate(ArtistSheet* artist)
    {
        (void)artist;
    }

    void SourceVGMRips::OnSongUpdate(const Song* const song)
    {
        assert(song->GetSourceId(0).sourceId == kID);
        auto* foundSong = m_songs.FindIf([&](auto& item)
        {
            return item.id == song->GetSourceId(0).internalId;
        });
        foundSong->songId = song->GetId();
        foundSong->isDiscarded = false;
        m_isDirty = true;
    }

    void SourceVGMRips::DiscardSong(SourceID sourceId, SongID newSongId)
    {
        thread::ScopedSpinLock lock(m_mutex);
        assert(sourceId.sourceId == kID);
        auto* foundSong = m_songs.FindIf([&](auto& item)
        {
            return item.id == sourceId.internalId;
        });
        if (foundSong)
        {
            foundSong->songId = newSongId;
            foundSong->isDiscarded = true;
            m_isDirty = true;
        }
    }

    void SourceVGMRips::InvalidateSong(SourceID sourceId, SongID newSongId)
    {
        thread::ScopedSpinLock lock(m_mutex);
        assert(sourceId.sourceId == kID && newSongId != SongID::Invalid);
        auto* foundSong = m_songs.FindIf([&](auto& item)
        {
            return item.id == sourceId.internalId;
        });
        if (foundSong)
        {
            foundSong->songId = newSongId;
            foundSong->isDiscarded = false;
            m_isDirty = true;
        }
    }

    std::string SourceVGMRips::GetArtistStub(SourceID artistId) const
    {
        auto artistSourceIndex = m_artists.FindIf<uint32_t>([&](auto& item)
        {
            return item.id == artistId.internalId;
        });
        return m_artists[artistSourceIndex].url(m_data);
    }

    void SourceVGMRips::Load()
    {
        auto file = io::File::OpenForRead(ms_filename);
        if (file.IsValid())
        {
            if (file.Read<uint32_t>() != kMusicFileStamp || file.Read<uint32_t>() > Core::GetVersion())
            {
                assert(0 && "file read error");
                return;
            }
            file.Read<uint32_t>(m_data);
            file.Read<uint32_t>(m_artists);
            file.Read<uint32_t>(m_songs);

            uint32_t nextId = 0;
            for (uint32_t i = 0, e = m_artists.NumItems(); i < e; i++, nextId++)
            {
                auto id = m_artists[i].id;
                for (; nextId < id; nextId++)
                    m_availableArtistIds.Add(nextId);
            }
            nextId = 0;
            for (uint32_t i = 0, e = m_songs.NumItems(); i < e; i++, nextId++)
            {
                auto id = m_songs[i].id;
                for (; nextId < id; nextId++)
                    m_availableSongIds.Add(nextId);
            }
        }
        else
        {
            m_data.Clear();
            m_data.Add('\0');
        }
    }

    void SourceVGMRips::Save() const
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
                if (m_areDataDirty)
                {
                    HashMap<uint32_t, uint32_t> dataUsage;

                    Array<SongSource> songs;
                    Array<char> data;
                    Array<ArtistSource> artists;
                    data.Add('\0');

                    // first pass rebuild strings and artists
                    for (auto& song : m_songs)
                    {
                        if (song.IsValid())
                        {
                            auto* newSong = songs.Add(song);
                            newSong->url.Set(data, song.url(m_data));

                            auto* pack = m_data.Items<PackSource>(song.pack);
                            auto& packUsage = dataUsage[pack->url.offset];
                            if (packUsage == 0)
                            {
                                packUsage = data.NumItems();
                                data.Add(m_data.Items(pack->url.offset), uint32_t(strlen(pack->url(m_data)) + 1));
                            }

                            for (uint32_t i = 0; i < pack->numArtists; i++)
                            {
                                auto* artist = m_artists.FindIf([&](auto& item)
                                {
                                    return item.id == pack->artists[i];
                                });
                                auto& artistUsage = dataUsage[artist->url.offset];
                                if (artistUsage == 0)
                                {
                                    auto* newArtist = artists.Add(*artist);
                                    newArtist->url.Set(data, artist->url(m_data));
                                    artistUsage = newArtist->url.offset;
                                }
                            }
                        }
                    }

                    // second pass rebuild packs
                    uint32_t songIndex = 0;
                    data.Resize(AlignUp(data.NumItems(), alignof(PackSource)));
                    for (auto& song : m_songs)
                    {
                        if (song.IsValid())
                        {
                            auto* newSong = songs.Items(songIndex++);

                            auto& packUsage = dataUsage[song.pack];
                            if (packUsage == 0)
                            {
                                auto* pack = m_data.Items<PackSource>(song.pack);
                                packUsage = data.NumItems();
                                auto* newPack = data.Push<PackSource*>(sizeof(PackSource) + pack->numArtists * sizeof(uint32_t));
                                newPack->url.offset = dataUsage[pack->url.offset];
                                newPack->numArtists = pack->numArtists;
                                for (uint32_t i = 0; i < pack->numArtists; i++)
                                    newPack->artists[i] = pack->artists[i];
                            }
                            newSong->pack = packUsage;
                        }
                    }

                    // sort everything to build available ids on the next load
                    std::sort(artists.begin(), artists.end(), [](auto& l, auto& r)
                    {
                        return l.id < r.id;
                    });
                    std::sort(songs.begin(), songs.end(), [](auto& l, auto& r)
                    {
                        return l.id < r.id;
                    });

                    file.Write<uint32_t>(data);
                    file.Write<uint32_t>(artists);
                    file.Write<uint32_t>(songs);
                }
                else
                {
                    file.Write<uint32_t>(m_data);
                    file.Write<uint32_t>(m_artists);
                    file.Write<uint32_t>(m_songs);
                }
                m_isDirty = false;
            }
        }
    }

    void SourceVGMRips::BrowserInit(BrowserContext& context)
    {
        if (m_db.artists.IsEmpty())
        {
            context.busySpinner.New(ImGui::GetColorU32(ImGuiCol_ButtonHovered));
            Core::AddJob([this, &context]()
            {
                DownloadArtists(*context.busySpinner);

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

    void SourceVGMRips::BrowserPopulate(BrowserContext& context, const ImGuiTextFilter& filter)
    {
        Array<BrowserEntry> entries;
        if (context.stage.id == kStageArtists.id)
        {
            // disable artist/year columns
            context.disabledColumns = 3 << 1;
            context.stage = kStageArtists;
            for (uint32_t i = 0, e = m_db.artists.NumItems(); i < e; i++)
            {
                auto* artistName = m_db.artists[i].name(m_db.data);
                if (filter.PassFilter(artistName))
                {
                    bool isSelected = false;
                    if (auto* entry = context.entries.Find(i))
                    {
                        isSelected = entry->isSelected;
                        context.entries.RemoveAtFast(entry - context.entries.Items());
                    }
                    ArtistID artistId = ArtistID::Invalid;
                    bool hasNewEntries = !m_db.artists[i].isComplete;
                    auto* artistUrl = m_db.artists[i].url(m_db.data);
                    if (auto* sourceArtist = m_artists.FindIf([&](auto& entry)
                    {
                        if (entry.url.offset == 0)
                            return false;
                        return strcmp(artistUrl, entry.url(m_data)) == 0;
                    }))
                    {
                        auto sourceId = SourceID(kID, sourceArtist->id);
                        for (auto* rplArtist : Core::GetDatabase(DatabaseID::kLibrary).Artists())
                        {
                            for (uint16_t j = 0, n = rplArtist->NumSources(); j < n; j++)
                            {
                                if (rplArtist->GetSource(j).id == sourceId)
                                {
                                    artistId = rplArtist->GetId();

                                    if (!hasNewEntries) for (uint32_t packOffset = m_db.artists[i].packs; packOffset;)
                                    {
                                        const auto& dbPack = *m_db.data.Items<VgmRipsPack>(packOffset);

                                        auto sourcePackOffset = m_songs.FindIf<int64_t>([&](auto& song)
                                        {
                                            auto* packSource = m_data.Items<PackSource>(song.pack);
                                            return song.pack && packSource->url.IsSame(m_data, dbPack.songsUrl(m_db.data));
                                        });
                                        if (sourcePackOffset < 0)
                                        {
                                            hasNewEntries = true;
                                            break;
                                        }
                                        for (auto songOffset = dbPack.songs; songOffset; songOffset = m_db.data.Items<VgmRipsSong>(songOffset)->nextSong)
                                        {
                                            const auto& dbSong = *m_db.data.Items<VgmRipsSong>(songOffset);
                                            if (!m_songs.FindIf([&](auto& song)
                                            {
                                                if (song.url.offset == 0)
                                                    return false;
                                                return m_songs[sourcePackOffset].pack == song.pack && strcmp(dbSong.url(m_db.data), song.url(m_data)) == 0;
                                            }))
                                            {
                                                hasNewEntries = true;
                                                break;
                                            }
                                        }

                                        for (uint16_t k = 0; k < dbPack.numArtists; k++)
                                        {
                                            if (dbPack.artists[k].index == i)
                                            {
                                                packOffset = dbPack.artists[k].nextPack;
                                                break;
                                            }
                                        }
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
        else if (context.stage.id == kStagePacks.id)
        {
            // disable id column
            context.disabledColumns = 1 << 3;
            context.stage = kStagePacks;

            // collect all packs
            auto& dbArtist = m_db.artists[context.stageDbIndex];
            if (dbArtist.isComplete == false)
            {
                ArtistCollector collector(m_db);
                for (uint32_t i = 0; !collector.isDone; i++)
                {
                    collector.state = ArtistCollector::kStateInit;
                    if (collector.Fetch("https://vgmrips.net/packs/composer/%s?p=%u", dbArtist.url(m_db.data), i) != Status::kOk)
                        collector.isDone = true;
                }
                dbArtist.isComplete = true;
            }

            for (uint32_t packOffset = dbArtist.packs; packOffset;)
            {
                const auto* dbPack = m_db.data.Items<VgmRipsPack>(packOffset);
                if (filter.PassFilter(dbPack->name(m_db.data)))
                {
                    bool isSelected = false;
                    if (auto* entry = context.entries.Find(packOffset))
                    {
                        isSelected = entry->isSelected;
                        context.entries.RemoveAtFast(entry - context.entries.Items());
                    }
                    entries.Add({
                        .dbIndex = packOffset,
                        .isSong = false,
                        .isSelected = isSelected,
                        .isDiscarded = false,
                        .artist = { .id = ArtistID::Invalid }
                    });
                }
                for (uint16_t i = 0; i < dbPack->numArtists; i++)
                {
                    if (dbPack->artists[i].index == context.stageDbIndex)
                    {
                        packOffset = dbPack->artists[i].nextPack;
                        break;
                    }
                }
            }
        }
        else
        {
            // no column disabled
            context.disabledColumns = 0;
            context.stage = kStagePacks;

            auto getPack = [&]() { return m_db.data.Items<VgmRipsPack>(context.stageDbIndex); };
            if (getPack()->songs == 0)
            {
                PackCollector packCollector(m_db, context.stageDbIndex);
                packCollector.Fetch("https://vgmrips.net/packs/pack/%s", getPack()->url(m_db.data));
            }

            auto sourcePackOffset = m_songs.FindIf<int64_t>([&](auto& song)
            {
                auto* packSource = m_data.Items<PackSource>(song.pack);
                return song.pack && packSource->url.IsSame(m_data, getPack()->songsUrl(m_db.data));
            });

            for (auto songOffset = getPack()->songs; songOffset; songOffset = m_db.data.Items<VgmRipsSong>(songOffset)->nextSong)
            {
                const auto& dbSong = *m_db.data.Items<VgmRipsSong>(songOffset);
                if (filter.PassFilter(dbSong.name(m_db.data)))
                {
                    bool isSelected = false;
                    bool isDiscarded = false;
                    if (auto* entry = context.entries.Find(songOffset))
                    {
                        isSelected = entry->isSelected;
                        context.entries.RemoveAtFast(entry - context.entries.Items());
                    }
                    SongID songId = SongID::Invalid;
                    if (sourcePackOffset >= 0)
                    {
                        if (auto* songSource = m_songs.FindIf([&](auto& song)
                        {
                            if (song.url.offset == 0)
                                return false;
                            return m_songs[sourcePackOffset].pack == song.pack && strcmp(dbSong.url(m_db.data), song.url(m_data)) == 0;
                        }))
                        {
                            songId = songSource->songId;
                            isDiscarded = songSource->isDiscarded;
                        }
                    }
                    entries.Add({
                        .dbIndex = songOffset,
                        .isSong = true,
                        .isSelected = isSelected,
                        .isDiscarded = isDiscarded,
                        .songId = songId
                    });
                }
            }
        }
        context.entries = std::move(entries);
    }

    int64_t SourceVGMRips::BrowserCompare(const BrowserContext& context, const BrowserEntry& lEntry, const BrowserEntry& rEntry, BrowserColumn column) const
    {
        UnusedArg(context);
        int64_t delta = 0;
        if (lEntry.isSong)
        {
            auto& lDbEntry = *m_db.data.Items<VgmRipsSong>(lEntry.dbIndex);
            auto& rDbEntry = *m_db.data.Items<VgmRipsSong>(rEntry.dbIndex);
            if (column == kColumnName)
                delta = CompareStringMixed(lDbEntry.name(m_db.data), rDbEntry.name(m_db.data));
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
        else if (context.stage == kStagePacks)
        {
            auto& lDbEntry = *m_db.data.Items<VgmRipsPack>(lEntry.dbIndex);
            auto& rDbEntry = *m_db.data.Items<VgmRipsPack>(rEntry.dbIndex);
            if (column == kColumnName)
                delta = CompareStringMixed(lDbEntry.name(m_db.data), rDbEntry.name(m_db.data));
            else if (column == kColumnArtist)
            {
                std::string lArtist, rArtist;
                for (uint16_t i = 0; i < lDbEntry.numArtists; ++i)
                    lArtist += m_db.artists[lDbEntry.artists[i].index].name(m_db.data);
                for (uint16_t i = 0; i < rDbEntry.numArtists; ++i)
                    rArtist += m_db.artists[rDbEntry.artists[i].index].name(m_db.data);
                delta = CompareStringMixed(lArtist.c_str(), rArtist.c_str());
            }
            else if (column == kColumnYear)
                delta = int64_t(lDbEntry.year) - int64_t(rDbEntry.year);
        }
        else
        {
            auto& lDbEntry = m_db.artists[lEntry.dbIndex];
            auto& rDbEntry = m_db.artists[rEntry.dbIndex];
            if (column == kColumnName)
                delta = CompareStringMixed(lDbEntry.name(m_db.data), rDbEntry.name(m_db.data));
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

    void SourceVGMRips::BrowserDisplay(const BrowserContext& context, const BrowserEntry& entry, BrowserColumn column) const
    {
        UnusedArg(context);
        if (column == kColumnName)
        {
            if (entry.isSong)
            {
                auto& dbSong = *m_db.data.Items<VgmRipsSong>(entry.dbIndex);
                ImGui::Text(ImGuiIconFile "%s.vgz", dbSong.name(m_db.data));
            }
            else if (context.stage == kStagePacks)
            {
                ImGui::Text(ImGuiIconFolder "%s", m_db.data.Items<VgmRipsPack>(entry.dbIndex)->name(m_db.data));
            }
            else
            {
                ImGui::Text(ImGuiIconFolder "%s", m_db.artists[entry.dbIndex].name(m_db.data));
            }
        }
        else if (column == kColumnArtist)
        {
            if (context.stage != kStageArtists)
            {
                auto& dbPack = entry.isSong ? *m_db.data.Items<VgmRipsPack>(context.stageDbIndex) : *m_db.data.Items<VgmRipsPack>(entry.dbIndex);
                if (dbPack.numArtists)
                {
                    ImGui::TextUnformatted(m_db.artists[dbPack.artists[0].index].name(m_db.data));
                    for (uint16_t i = 1; i < dbPack.numArtists; ++i)
                    {
                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::Text(" & %s", m_db.artists[dbPack.artists[i].index].name(m_db.data));
                    }
                }
            }
        }
        else if (column == kColumnYear)
        {
            if (context.stage != kStageArtists)
            {
                auto& dbPack = entry.isSong ? *m_db.data.Items<VgmRipsPack>(context.stageDbIndex) : *m_db.data.Items<VgmRipsPack>(entry.dbIndex);
                if (dbPack.year)
                    ImGui::Text("%04d", dbPack.year);
                else
                    ImGui::TextUnformatted("n/a");
            }
        }
        else if (column == kColumnId)
            BrowserDisplayLibraryId(entry, entry.isSong);
    }

    void SourceVGMRips::BrowserImport(const BrowserContext& context, const BrowserEntry& entry, SourceResults& collectedSongs)
    {
        UnusedArg(context);

        auto addPack = [&](VgmRipsPack& dbPack)
        {
            // build pack
            auto sourcePackOffset = m_songs.FindIf<int64_t>([&](auto& song)
            {
                auto* packSource = m_data.Items<PackSource>(song.pack);
                return song.pack && packSource->url.IsSame(m_data, dbPack.songsUrl(m_db.data));
            });
            if (sourcePackOffset < 0)
            {
                m_data.Resize(AlignUp(m_data.NumItems(), alignof(PackSource)));
                sourcePackOffset = m_data.NumItems();
                m_data.Push(sizeof(PackSource) + dbPack.numArtists * sizeof(uint32_t));
                m_data.Items<PackSource>(uint32_t(sourcePackOffset))->url.Set(m_data, dbPack.songsUrl(m_db.data));
                m_data.Items<PackSource>(uint32_t(sourcePackOffset))->numArtists = dbPack.numArtists;
            }
            else
                sourcePackOffset = m_songs[sourcePackOffset].pack;

            // remap artists
            for (uint32_t i = 0; i < dbPack.numArtists; i++)
            {
                auto* newArtistSource = FindArtist(m_db.artists[dbPack.artists[i].index].url(m_db.data));
                m_data.Items<PackSource>(uint32_t(sourcePackOffset))->artists[i] = newArtistSource->id;
                SourceID artistId(kID, newArtistSource->id);
                auto* artistIt = collectedSongs.artists.FindIf([&](auto& entry)
                {
                    return entry->sources[0].id == artistId;
                });
                if (artistIt == nullptr)
                {
                    auto artist = new ArtistSheet();
                    artist->handles.Add(m_db.artists[dbPack.artists[i].index].name(m_db.data));
                    artist->sources.Add(artistId);
                    dbPack.artists[i].remap = collectedSongs.artists.NumItems<uint16_t>();
                    collectedSongs.artists.Add(artist);
                }
                else
                    dbPack.artists[i].remap = uint16_t(artistIt - collectedSongs.artists);
            }
            return uint32_t(sourcePackOffset);
        };
        auto addSong = [&](uint32_t sourcePackOffset, const VgmRipsPack& dbPack, const VgmRipsSong& dbSong)
        {
            auto* songSource = FindSong(sourcePackOffset, dbSong.url(m_db.data));
            SourceID songSourceId(kID, songSource->id);
            if (collectedSongs.IsSongAvailable(songSourceId))
                return;

            auto* song = new SongSheet();

            SourceResults::State state;
            song->id = songSource->songId;
            if (songSource->isDiscarded)
                state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kDiscarded : SourceResults::kMerged);
            else
                song->id == SongID::Invalid ? state.SetSongStatus(SourceResults::kNew).SetChecked(true) : state.SetSongStatus(SourceResults::kOwned);

            song->name = dbPack.name(m_db.data);
            song->name.String() += '/';
            song->name.String() += dbSong.name(m_db.data);
            song->type = { eExtension::_vgz, eReplay::VGM };
            song->releaseYear = dbPack.year;
            song->sourceIds.Add(songSourceId);
            for (uint32_t i = 0; i < dbPack.numArtists; i++)
                song->artistIds.Add(ArtistID(dbPack.artists[i].remap));

            collectedSongs.songs.Add(song);
            collectedSongs.states.Add(state);
        };

        if (entry.isSong)
        {
            auto& dbPack = *m_db.data.Items<VgmRipsPack>(context.stageDbIndex);
            auto& dbSong = *m_db.data.Items<VgmRipsSong>(entry.dbIndex);

            auto sourcePackOffset = addPack(dbPack);
            addSong(sourcePackOffset, dbPack, dbSong);
        }
        else if (context.stage == kStagePacks)
        {
            auto getPack = [&]() { return m_db.data.Items<VgmRipsPack>(entry.dbIndex); };
            if (getPack()->songs == 0)
            {
                PackCollector packCollector(m_db, entry.dbIndex);
                packCollector.Fetch("https://vgmrips.net/packs/pack/%s", getPack()->url(m_db.data));
            }

            auto sourcePackOffset = addPack(*getPack());
            for (auto songOffset = getPack()->songs; songOffset; songOffset = m_db.data.Items<VgmRipsSong>(songOffset)->nextSong)
                addSong(sourcePackOffset, *getPack(), *m_db.data.Items<VgmRipsSong>(songOffset));
        }
        else
        {
            // collect all packs
            auto& dbArtist = m_db.artists[entry.dbIndex];
            if (dbArtist.isComplete == false)
            {
                ArtistCollector collector(m_db);
                for (uint32_t i = 0; !collector.isDone; i++)
                {
                    collector.state = ArtistCollector::kStateInit;
                    if (collector.Fetch("https://vgmrips.net/packs/composer/%s?p=%u", dbArtist.url(m_db.data), i) != Status::kOk)
                        collector.isDone = true;
                }
                dbArtist.isComplete = true;
            }

            for (uint32_t packOffset = dbArtist.packs; packOffset;)
            {
                auto getPack = [&]() { return m_db.data.Items<VgmRipsPack>(packOffset); };
                if (getPack()->songs == 0)
                {
                    PackCollector packCollector(m_db, packOffset);
                    packCollector.Fetch("https://vgmrips.net/packs/pack/%s", getPack()->url(m_db.data));
                }

                auto sourcePackOffset = addPack(*getPack());
                for (auto songOffset = getPack()->songs; songOffset; songOffset = m_db.data.Items<VgmRipsSong>(songOffset)->nextSong)
                    addSong(sourcePackOffset, *getPack(), *m_db.data.Items<VgmRipsSong>(songOffset));

                for (uint16_t i = 0; i < getPack()->numArtists; i++)
                {
                    if (getPack()->artists[i].index == entry.dbIndex)
                    {
                        packOffset = getPack()->artists[i].nextPack;
                        break;
                    }
                }
            }
        }
    }

    std::string SourceVGMRips::BrowserGetStageName(const BrowserEntry& entry, BrowserStage stage) const
    {
        if (stage == kStageArtists)
            return m_db.artists[entry.dbIndex].name(m_db.data);
        return m_db.data.Items<VgmRipsPack>(entry.dbIndex)->name(m_db.data);
    }

    core::Array<rePlayer::BrowserSong> SourceVGMRips::BrowserFetchSongs(const BrowserContext& context, const BrowserEntry& entry)
    {
        Array<BrowserSong> songs;
        auto addSong = [&](const VgmRipsPack& dbPack, const VgmRipsSong& dbSong)
        {
            auto* song = songs.Push();
            song->url = "https://vgmrips.net/packs/vgm/";
            song->url += dbPack.songsUrl(m_db.data);
            song->url += '/';
            song->url += dbSong.url(m_db.data);
            song->name = dbPack.name(m_db.data);
            song->name += '/';
            song->name += dbSong.name(m_db.data);
            song->type = { eExtension::_vgz, eReplay::VGM };
            for (uint16_t i = 0; i < dbPack.numArtists; ++i)
                song->artists.Add(m_db.artists[dbPack.artists[i].index].name(m_db.data));
        };
        if (entry.isSong)
        {
            addSong(*m_db.data.Items<VgmRipsPack>(context.stageDbIndex), *m_db.data.Items<VgmRipsSong>(entry.dbIndex));
        }
        else if (context.stage == kStagePacks)
        {
            auto getPack = [&]() { return m_db.data.Items<VgmRipsPack>(entry.dbIndex); };
            if (getPack()->songs == 0)
            {
                PackCollector packCollector(m_db, entry.dbIndex);
                packCollector.Fetch("https://vgmrips.net/packs/pack/%s", getPack()->url(m_db.data));
            }

            for (auto songOffset = getPack()->songs; songOffset; songOffset = m_db.data.Items<VgmRipsSong>(songOffset)->nextSong)
                addSong(*getPack(), *m_db.data.Items<VgmRipsSong>(songOffset));
        }
        else
        {
            // collect all packs
            auto& dbArtist = m_db.artists[entry.dbIndex];
            if (dbArtist.isComplete == false)
            {
                ArtistCollector collector(m_db);
                for (uint32_t i = 0; !collector.isDone; i++)
                {
                    collector.state = ArtistCollector::kStateInit;
                    if (collector.Fetch("https://vgmrips.net/packs/composer/%s?p=%u", dbArtist.url(m_db.data), i) != Status::kOk)
                        collector.isDone = true;
                }
                dbArtist.isComplete = true;
            }

            for (uint32_t packOffset = dbArtist.packs; packOffset;)
            {
                auto getPack = [&]() { return m_db.data.Items<VgmRipsPack>(packOffset); };
                if (getPack()->songs == 0)
                {
                    PackCollector packCollector(m_db, packOffset);
                    packCollector.Fetch("https://vgmrips.net/packs/pack/%s", getPack()->url(m_db.data));
                }

                for (auto songOffset = getPack()->songs; songOffset; songOffset = m_db.data.Items<VgmRipsSong>(songOffset)->nextSong)
                    addSong(*getPack(), *m_db.data.Items<VgmRipsSong>(songOffset));

                for (uint16_t i = 0; i < getPack()->numArtists; i++)
                {
                    if (getPack()->artists[i].index == entry.dbIndex)
                    {
                        packOffset = getPack()->artists[i].nextPack;
                        break;
                    }
                }
            }
        }
        return songs;
    }

    SourceVGMRips::SongSource* SourceVGMRips::FindSong(uint32_t pack, const std::string& titleUrl)
    {
        auto* song = m_songs.FindIf([&](auto& song)
        {
            if (song.url.offset == 0)
                return false;
            return pack == song.pack && titleUrl == song.url(m_data);
        });
        if (song == nullptr)
        {
            thread::ScopedSpinLock lock(m_mutex);
            song = m_songs.FindIf([&](auto& song)
            {
                return song.url.offset == 0;
            });
            if (song == nullptr)
            {
                song = m_songs.Push();
                song->id = m_availableSongIds.IsEmpty() ? m_songs.NumItems() - 1 : m_availableSongIds.Pop();
            }
            song->pack = pack;
            song->url.Set(m_data, titleUrl);
            m_areDataDirty = true;
        }
        return song;
    }

    SourceVGMRips::ArtistSource* SourceVGMRips::FindArtist(const std::string& url)
    {
        auto* artist = m_artists.FindIf([&](auto& artist)
        {
            if (artist.url.offset == 0)
                return false;
            return url == artist.url(m_data);
        });
        if (artist == nullptr)
        {
            thread::ScopedSpinLock lock(m_mutex);
            artist = m_artists.FindIf([&](auto& artist)
            {
                return artist.url.offset == 0;
            });
            if (artist == nullptr)
            {
                artist = m_artists.Push();
                artist->id = m_availableArtistIds.IsEmpty() ? m_artists.NumItems() - 1 : m_availableArtistIds.Pop();
            }
            artist->url.Set(m_data, url);
            m_areDataDirty = true;
        }
        return artist;
    }

    bool SourceVGMRips::DownloadArtists(BusySpinner& busySpinner)
    {
        if (m_db.artists.IsEmpty())
        {
            busySpinner.Info("downloading artists database");

            ArtistsCollector collector(m_db);
            collector.Fetch("https://vgmrips.net/packs/composers");
            if (!collector.error.empty())
                busySpinner.Error(collector.error);
        }
        return m_db.artists.IsEmpty();
    }
}
// namespace rePlayer
// Core
#include <Containers/HashMap.h>
#include <Core/Log.h>
#include <IO/File.h>
#include <IO/StreamMemory.h>
#include <Thread/Thread.h>

// rePlayer
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
                            artist->first.offset = artist->second.offset = dataOffset;
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
                                        artist->second.offset = db.data.NumItems();
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
        Array<uint32_t> packs; // url/name
        Array<char> data;

        std::string name;
        std::string url;

        const SourceVGMRips::DB& db;

        enum
        {
            kStateInit = 0,
            kStatePack,
            kStateChips,
            kStateSystems,
            kStateArtists,
            kStateEnd
        } state = kStateInit;
        bool isDone = false;

        ArtistCollector(const SourceVGMRips::DB& other);
        void OnReadNode(xmlNode* node) final;
        void ReadPack(xmlNode* node);
        void ReadChips(xmlNode* node);
        void ReadSystems(xmlNode* node);
        void ReadArtists(xmlNode* node);
    };

    SourceVGMRips::ArtistCollector::ArtistCollector(const SourceVGMRips::DB& other)
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
                        ReadSystems(node->children);

                        auto urlOffset = data.NumItems();
                        data.Add(url.c_str(), uint32_t(url.size() + 1));
                        auto nameOffset = data.NumItems();
                        data.Add(name.c_str(), uint32_t(name.size() + 1));
                        data.Resize(AlignUp(data.NumItems(), sizeof(uint32_t)));
                        packs.Add(data.NumItems());
                        auto* pack = data.Push<Pack*>(sizeof(Pack));
                        pack->url.offset = urlOffset;
                        pack->name.offset = nameOffset;
                        pack->numArtists = 0;

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
                                for (uint32_t i = 0, e = db.artists.NumItems(); i < e; i++)
                                {
                                    if (_stricmp(db.artists[i].first(db.data), reinterpret_cast<const char*>(artistUrl)) == 0)
                                    {
                                        data.Push<uint32_t&>(sizeof(uint32_t)) = i;
                                        data.Items<Pack>(packs.Last())->numArtists++;
                                        break;
                                    }
                                }
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
                        ReadSystems(node->children);

                        auto urlOffset = db.data.NumItems();
                        db.data.Add(url.c_str(), uint32_t(url.size() + 1));
                        auto nameOffset = db.data.NumItems();
                        db.data.Add(name.c_str(), uint32_t(name.size() + 1));
                        db.data.Resize(AlignUp(db.data.NumItems(), alignof(Pack)));
                        db.packs.Add(db.data.NumItems());
                        auto* pack = db.data.Push<Pack*>(sizeof(Pack));
                        pack->url.offset = urlOffset;
                        pack->name.offset = nameOffset;
                        pack->numArtists = 0;

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
                                for (uint32_t i = 0, e = db.artists.NumItems(); i < e; i++)
                                {
                                    if (_stricmp(db.artists[i].first(db.data), reinterpret_cast<const char*>(artistUrl)) == 0)
                                    {
                                        db.data.Push<uint32_t&>(sizeof(uint32_t)) = i;
                                        db.data.Items<Pack>(db.packs.Last())->numArtists++;
                                        break;
                                    }
                                }
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
        Array<std::pair<Chars, Chars>> songs; // url/name
        Array<char> data;
        uint32_t year = 0;

        enum
        {
            kStateInit = 0,
            kStateYear,
            kStateSongs,
            kStateEnd
        } state = kStateInit;

        void OnReadNode(xmlNode* node) final;
    };

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
                sscanf_s(reinterpret_cast<const char*>(node->content), "%u", &year);
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
                            if (data.IsEmpty())
                            {
                                data.Add(reinterpret_cast<const char*>(packUrl), uint32_t(songUrl - packUrl));
                                data.Last() = 0;
                            }
                            songs.Push()->first.Set(data, reinterpret_cast<const char*>(songUrl));
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
                            songs.Last().second.Set(data, reinterpret_cast<const char*>(child->content));
                            data.Pop();
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
                            data.Add(reinterpret_cast<const char*>(child->content), xmlStrlen(child->content) + 1);
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
    {}

    SourceVGMRips::~SourceVGMRips()
    {}

    void SourceVGMRips::FindArtists(ArtistsCollection& artists, const char* name, BusySpinner& busySpinner)
    {
        // download artists database
        if (m_db.artists.IsEmpty())
        {
            busySpinner.Info("downloading artists database");

            ArtistsCollector collector(m_db);
            collector.Fetch("https://vgmrips.net/packs/composers");
            if (!collector.error.empty())
                busySpinner.Error(collector.error);
        }

        // look for the artist
        auto lName = ToLower(name);
        for (auto& artist : m_db.artists)
        {
            if (strstr(ToLower(artist.second(m_db.data)).c_str(), lName.c_str()))
            {
                auto* newArtist = artists.matches.Push();
                newArtist->name = artist.second(m_db.data);
                newArtist->id = SourceID(kID, FindArtist(artist.first(m_db.data))->id);
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

        // download artists database
        if (m_db.artists.IsEmpty())
        {
            busySpinner.Info("downloading artists database");

            ArtistsCollector collector(m_db);
            collector.Fetch("https://vgmrips.net/packs/composers");
            if (!collector.error.empty())
                busySpinner.Error(collector.error);
        }

        // validation (has the artist been removed or renamed)
        {
            auto* dbArtist = m_db.artists.FindIf([&](auto& item)
            {
                return item.first.IsSame(m_db.data, m_artists[artistSourceIndex].url(m_data));
            });
            if (dbArtist == nullptr)
            {
                // has the artist disappeared from VGMRips?
                Log::Error("VGMRips: can't find artist \"%s\"\n", m_artists[artistSourceIndex].url(m_data));
                return;
            }
        }

        // collect all packs
        auto* message = busySpinner.Info("downloading artist packs database at %u", 0);
        ArtistCollector collector(m_db);
        for (uint32_t i = 0; !collector.isDone; i++)
        {
            busySpinner.UpdateMessageParam(message, i);

            collector.state = ArtistCollector::kStateInit;
            if (collector.Fetch("https://vgmrips.net/packs/composer/%s?p=%u", m_artists[artistSourceIndex].url(m_data), i) != Status::kOk)
                collector.isDone = true;
        }

        // collect all songs
        message = busySpinner.Info("downloading artist songs database from %s", "");
        for (auto packOffset : collector.packs)
        {
            auto* pack = collector.data.Items<Pack>(packOffset);

            busySpinner.UpdateMessageParam(message, pack->url(collector.data));

            PackCollector packCollector;
            if (packCollector.Fetch("https://vgmrips.net/packs/pack/%s", pack->url(collector.data)) != Status::kOk)
                break;

            // build pack
            auto sourcePackOffset = m_songs.FindIf<int64_t>([&](auto& song)
            {
                auto* packSource = m_data.Items<PackSource>(song.pack);
                return song.pack && packSource->url.IsSame(m_data, packCollector.data.Items());
            });
            if (sourcePackOffset < 0)
            {
                m_data.Resize(AlignUp(m_data.NumItems(), alignof(PackSource)));
                sourcePackOffset = m_data.NumItems();
                m_data.Push(sizeof(PackSource) + pack->numArtists * sizeof(uint32_t));
                m_data.Items<PackSource>(uint32_t(sourcePackOffset))->url.Set(m_data, packCollector.data.Items());
                m_data.Items<PackSource>(uint32_t(sourcePackOffset))->numArtists = pack->numArtists;
            }
            else
                sourcePackOffset = m_songs[sourcePackOffset].pack;

            // remap artists
            for (uint32_t i = 0; i < pack->numArtists; i++)
            {
                auto* newArtistSource = FindArtist(m_db.artists[pack->artists[i]].first(m_db.data));
                m_data.Items<PackSource>(uint32_t(sourcePackOffset))->artists[i] = newArtistSource->id;
                SourceID artistId(kID, newArtistSource->id);
                auto* artistIt = results.artists.FindIf([&](auto& entry)
                {
                    return entry->sources[0].id == artistId;
                });
                if (artistIt == nullptr)
                {
                    auto artist = new ArtistSheet();
                    artist->handles.Add(m_db.artists[pack->artists[i]].second(m_db.data));
                    artist->sources.Add(artistId);
                    pack->artists[i] = results.artists.NumItems();
                    results.artists.Add(artist);
                }
                else
                    pack->artists[i] = artistIt - results.artists;
            }

            // collect all songs from the pack
            for (auto& collectedSong : packCollector.songs)
            {
                auto* songSource = FindSong(uint32_t(sourcePackOffset), collectedSong.first(packCollector.data));
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

                song->name = pack->name(collector.data);
                song->name.String() += '/';
                song->name.String() += collectedSong.second(packCollector.data);
                song->type = { eExtension::_vgz, eReplay::VGM };
                song->releaseYear = uint16_t(packCollector.year);
                song->sourceIds.Add(songSourceId);
                for (uint32_t i = 0; i < pack->numArtists; i++)
                    song->artistIds.Add(ArtistID(pack->artists[i]));

                results.songs.Add(song);
                results.states.Add(state);
            }
        }
    }

    void SourceVGMRips::FindSongs(const char* name, SourceResults& collectedSongs, BusySpinner& busySpinner)
    {
        // download artists database
        if (m_db.artists.IsEmpty())
        {
            busySpinner.Info("downloading artists database");

            ArtistsCollector collector(m_db);
            collector.Fetch("https://vgmrips.net/packs/composers");
            if (!collector.error.empty())
                busySpinner.Error(collector.error);
        }

        // download packs database
        if (m_db.packs.IsEmpty())
        {
            auto* message = busySpinner.Info("downloading packs database at %u", 0);

            PacksCollector collector(m_db);
            for (uint32_t i = 0; !collector.isDone; i++)
            {
                busySpinner.UpdateMessageParam(message, i);

                collector.state = PacksCollector::kStateInit;
                if (collector.Fetch("https://vgmrips.net/packs/latest?p=%u", i) != Status::kOk)
                    collector.isDone = true;
            }
        }

        // collect all songs
        auto* message = busySpinner.Info("downloading songs database from %s", "");
        auto lName = ToLower(name);
        for (uint32_t packIdx = 0; packIdx < m_db.packs.NumItems(); packIdx++)
        {
            auto packOffset = m_db.packs[packIdx];
            auto* pack = m_db.data.Items<Pack>(packOffset);
            if (strstr(ToLower(pack->name(m_db.data)).c_str(), lName.c_str()))
            {
                char txt[1024];
                sprintf(txt, "%s / %u%%", pack->url(m_db.data), uint32_t(((packIdx + 1) * 100ull) / m_db.packs.NumItems()));
                busySpinner.UpdateMessageParam(message, txt);

                PackCollector packCollector;
                if (packCollector.Fetch("https://vgmrips.net/packs/pack/%s", pack->url(m_db.data)) != Status::kOk)
                    break;

                // throttle to avoid flooding website
                if ((packIdx & 31) == 31)
                    thread::Sleep(500);

                // build pack
                auto sourcePackOffset = m_songs.FindIf<int64_t>([&](auto& song)
                {
                    auto* packSource = m_data.Items<PackSource>(song.pack);
                    return song.pack && packSource->url.IsSame(m_data, packCollector.data.Items());
                });
                if (sourcePackOffset < 0)
                {
                    m_data.Resize(AlignUp(m_data.NumItems(), alignof(PackSource)));
                    sourcePackOffset = m_data.NumItems();
                    m_data.Push(sizeof(PackSource) + pack->numArtists * sizeof(uint32_t));
                    m_data.Items<PackSource>(uint32_t(sourcePackOffset))->url.Set(m_data, packCollector.data.Items());
                    m_data.Items<PackSource>(uint32_t(sourcePackOffset))->numArtists = pack->numArtists;
                }
                else
                    sourcePackOffset = m_songs[sourcePackOffset].pack;

                // remap artists
                for (uint32_t i = 0; i < pack->numArtists; i++)
                {
                    auto* newArtistSource = FindArtist(m_db.artists[pack->artists[i]].first(m_db.data));
                    m_data.Items<PackSource>(uint32_t(sourcePackOffset))->artists[i] = newArtistSource->id;
                    SourceID artistId(kID, newArtistSource->id);
                    auto* artistIt = collectedSongs.artists.FindIf([&](auto& entry)
                    {
                        return entry->sources[0].id == artistId;
                    });
                    if (artistIt == nullptr)
                    {
                        auto artist = new ArtistSheet();
                        artist->handles.Add(m_db.artists[pack->artists[i]].second(m_db.data));
                        artist->sources.Add(artistId);
                        pack->artists[i] = collectedSongs.artists.NumItems();
                        collectedSongs.artists.Add(artist);
                    }
                    else
                        pack->artists[i] = artistIt - collectedSongs.artists;
                }

                // collect all songs from the pack
                for (auto& collectedSong : packCollector.songs)
                {
                    auto* songSource = FindSong(uint32_t(sourcePackOffset), collectedSong.first(packCollector.data));
                    SourceID songSourceId(kID, songSource->id);
                    if (collectedSongs.IsSongAvailable(songSourceId))
                        continue;

                    auto* song = new SongSheet();

                    SourceResults::State state;
                    song->id = songSource->songId;
                    if (songSource->isDiscarded)
                        state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kDiscarded : SourceResults::kMerged);
                    else
                        song->id == SongID::Invalid ? state.SetSongStatus(SourceResults::kNew).SetChecked(true) : state.SetSongStatus(SourceResults::kOwned);

                    song->name = pack->name(m_db.data);
                    song->name.String() += '/';
                    song->name.String() += collectedSong.second(packCollector.data);
                    song->type = { eExtension::_vgz, eReplay::VGM };
                    song->releaseYear = uint16_t(packCollector.year);
                    song->sourceIds.Add(songSourceId);
                    for (uint32_t i = 0; i < pack->numArtists; i++)
                        song->artistIds.Add(ArtistID(pack->artists[i]));

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

        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
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
}
// namespace rePlayer
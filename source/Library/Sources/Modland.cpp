#include "Modland.h"

// Core
#include <Core/Log.h>
#include <Core/String.h>
#include <IO/File.h>
#include <IO/StreamMemory.h>

// rePlayer
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>

// zlib
#include <zlib.h>

// curl
#include <Curl/curl.h>

// libarchive
#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>

// stl
#include <algorithm>

namespace rePlayer
{
    const char* const SourceModland::ms_filename = MusicPath "Modland" MusicExt;

    // rePlayer blobbed format for multi-files modules
    SourceModland::ModlandReplayOverride SourceModland::ms_replayOverrides[] = {
        { // tfmx
            "TFMX",
            ModlandReplay::kTFMX,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto ext = strstr(url.data(), "/mdat."); ext[1] = 's'; ext[2] = 'm'; ext[3] = 'p'; ext[4] = 'l'; name[0] = 's'; name[1] = 'm'; name[2] = 'p'; name[3] = 'l'; },
            [](std::string& name) { name.erase(name.begin(), name.begin() + 5); return MediaType(eExtension::_mdat, eReplay::TFMX); },
            [](const char* name) { return strstr(name, "smpl.") != nullptr; }
        },
        { // uade
            "Richard Joseph",
            ModlandReplay::kRichardJoseph,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto ext = url.data() + url.size() - 3; ext[0] = 'i'; ext[1] = 'n'; ext[2] = 's'; ext = name.data() + name.size() - 3; ext[0] = 'i'; ext[1] = 'n'; ext[2] = 's'; },
            [](std::string& name) { auto extOffset = name.find_last_of('.'); name.resize(extOffset); return MediaType(eExtension::_rjp, eReplay::UADE); },
            [](const char* name) { return strstr(name, ".ins") == (name + strlen(name) - 4); }
        },
        { // uade
            "Audio Sculpture",
            ModlandReplay::kAudioSculpture,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { url += ".as"; name += ".as"; },
            [](std::string& name) { auto extOffset = name.find_last_of('.'); name.resize(extOffset); return MediaType(eExtension::_adsc, eReplay::UADE); },
            [](const char* name) { return strstr(name, ".adsc.as") == (name + strlen(name) - 8); }
        },
        { // uade
            "Dirk Bialluch",
            ModlandReplay::kDirkBialluch,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto ext = strstr(url.data(), "/tpu."); ext[1] = 's'; ext[2] = 'm'; ext[3] = 'p'; name[0] = 's'; name[1] = 'm'; name[2] = 'p'; },
            [](std::string& name) { name.erase(name.begin(), name.begin() + 4); return MediaType(eExtension::_tpu, eReplay::UADE); },
            [](const char* name) { return strstr(name, "smp.") != nullptr; }
        },
        { // uade
            "Dynamic Synthesizer",
            ModlandReplay::kDynamicSynthesizer,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto ext = strstr(url.data(), "/dns."); ext[1] = 's'; ext[2] = 'm'; ext[3] = 'p'; name[0] = 's'; name[1] = 'm'; name[2] = 'p'; },
            [](std::string& name) { name.erase(name.begin(), name.begin() + 4); return MediaType(eExtension::_dns, eReplay::UADE); },
            [](const char* name) { return strstr(name, "smp.") != nullptr; }
        },
        { // uade
            "Infogrames",
            ModlandReplay::kInfogrames,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name)
            {
                const char* const names[] = {"gobliiins", "mus2b", "mus2c", "bob4a", "bob4b", "bob4c", "bob4e"};
                for (auto* n : names)
                {
                    if (url.rfind(n) != url.npos)
                    {
                        auto ext = url.data() + url.size() - 5; ext[0] = '.';  ext[1] = 'i'; ext[2] = 'n'; ext[3] = 's'; ext[4] = 0;
                        ext = name.data() + name.size() - 5; ext[0] = '.';  ext[1] = 'i'; ext[2] = 'n'; ext[3] = 's'; ext[4] = 0;
                        return;
                    }
                }
                const char* const namesWithDots[] = { "north%20%26%20south.", "rh-bobo." };
                for (auto* n : namesWithDots)
                {
                    auto offset = url.rfind(n);
                    if (offset != url.npos)
                    {
                        auto ext = url.data() + offset + strlen(n); ext[0] = 'i'; ext[1] = 'n'; ext[2] = 's'; ext[3] = 0;
                        ext = name.data() + offset + strlen(n); ext[0] = 'i'; ext[1] = 'n'; ext[2] = 's'; ext[3] = 0;
                        return;
                    }
                }
                auto ext = url.data() + url.size() - 3; ext[0] = 'i'; ext[1] = 'n'; ext[2] = 's';
                ext = name.data() + name.size() - 3; ext[0] = 'i'; ext[1] = 'n'; ext[2] = 's';
            },
            [](std::string& name) { auto extOffset = name.find_last_of('.'); name.resize(extOffset); return MediaType(eExtension::_dum, eReplay::UADE); },
            [](const char* name) { return strstr(name, ".ins") == (name + strlen(name) - 4); }
        },
        { // uade
            "Jason Page",
            ModlandReplay::kJasonPage,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto ext = strstr(url.data(), "/jpn."); ext[1] = 's'; ext[2] = 'm'; ext[3] = 'p'; name[0] = 's'; name[1] = 'm'; name[2] = 'p'; },
            [](std::string& name) { name.erase(name.begin(), name.begin() + 4); return MediaType(eExtension::_jpn, eReplay::UADE); },
            [](const char* name) { return strstr(name, "smp.") != nullptr; }
        },
        { // uade
            "Kris Hatlelid",
            ModlandReplay::kKrisHatlelid,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto pos = url.find_last_of('/'); url.resize(pos + 1); url += "songplay"; name = "songplay"; },
            [](std::string& name) { auto extOffset = name.find_last_of('.'); name.resize(extOffset); return MediaType(eExtension::_kh, eReplay::UADE); },
            [](const char* name) { auto len = strlen(name); return len >= 8 && _stricmp(name + len - 8, "songplay") == 0; }
        },
        { // uade
            "Magnetic Fields Packer",
            ModlandReplay::kMagneticFieldsPacker,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto ext = strstr(url.data(), "/mfp."); ext[1] = 's'; ext[2] = 'm'; ext[3] = 'p'; name[0] = 's'; name[1] = 'm'; name[2] = 'p'; },
            [](std::string& name) { name.erase(name.begin(), name.begin() + 4); return MediaType(eExtension::_mfp, eReplay::UADE); },
            [](const char* name) { return strstr(name, "smp.") != nullptr; }
        },
        { // uade
            "Mark Cooksey Old",
            ModlandReplay::kMarkCookseyOld,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto ext = strstr(url.data(), "/mcr."); ext[3] = 's'; name[2] = 's'; },
            [](std::string& name) { auto ext = name[2] == 'o' ? eExtension::_mco : eExtension::_mcr; name.erase(name.begin(), name.begin() + 4); return MediaType(ext, eReplay::UADE); },
            [](const char* name) { return strstr(name, "mcs.") != nullptr; },
            [](const char* name) { return strstr(name, "mco.") == nullptr; }
        },
        { // uade
            "Pierre Adane Packer",
            ModlandReplay::kPierreAdanePacker,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto pos = url.find_last_of('/'); url.resize(pos + 1); url += "smp.set"; name = "smp.set"; },
            [](std::string& name) { auto extOffset = name.find_last_of('.'); name.resize(extOffset); return MediaType(eExtension::_pap, eReplay::UADE); },
            [](const char* name) { return strstr(name, "smp.set") != nullptr; }
        },
        { // uade
            "PokeyNoise",
            ModlandReplay::kPokeyNoise,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { url += ".info"; name += ".info"; },
            [](std::string& name) { name.erase(name.begin(), name.begin() + 3); return MediaType(eExtension::_pn, eReplay::UADE); },
            [](const char* name) { return strstr(name, ".info") == (name + strlen(name) - 5); }
        },
        { // uade
            "Quartet ST",
            ModlandReplay::kQuartetST,
            [](CURL* curl, const ModlandReplayOverride* const replay, const SourceModland& modland, std::string& url, std::string& name)
            {
                std::string lowerUrl = ToLower(url);
                auto replayPos = url.find("Quartet%20ST") + sizeof("Quartet%20ST");
                auto pos = lowerUrl.rfind("/qts.");
                if (pos != url.npos)
                {
                    url[pos + 1] = 's'; url[pos + 2] = 'm'; url[pos + 3] = 'p';
                    name[0] = 's'; name[1] = 'm'; name[2] = 'p';
                }
                else
                {
                    pos = lowerUrl.rfind(".4v");
                    if (pos != url.npos && lowerUrl[pos + 3] == 0)
                    {
                        url[pos + 1] = 's'; url[pos + 2] = 'e'; url += 't';
                        pos = lowerUrl.rfind('/');
                    }
                    name = "smp.set";
                }
                if (pos != url.npos)
                {
                    auto item = replay->item;
                    uint32_t smpSetItem = 0;
                    while (item)
                    {
                        auto* itemName = modland.m_db.items[item].name(modland.m_db.strings);
                        auto smpLink = CleanUrl(curl, itemName);
                        if (strncmp(smpLink.c_str(), url.c_str() + replayPos, pos - replayPos + 1) == 0)
                        {
                            if (_strnicmp(smpLink.c_str() + pos - replayPos + 1, url.c_str() + pos + 1, url.size() - pos - 1) == 0)
                            {
                                url.resize(pos + 1);
                                url += smpLink.c_str() + pos - replayPos + 1;
                                return;
                            }
                            if (!smpSetItem && _stricmp(smpLink.c_str() + pos - replayPos + 1, "smp.set") == 0)
                                smpSetItem = item;
                        }
                        item = modland.m_db.items[item].next;
                    }
                    if (smpSetItem)
                    {
                        url.resize(pos + 1);
                        auto smpLink = CleanUrl(curl, modland.m_db.items[smpSetItem].name(modland.m_db.strings));
                        url += smpLink.c_str() + pos - replayPos + 1;
                        return;
                    }
                }
                url.clear();
            },
            [](std::string& name)
            {
                auto pos = name.find("qts.");
                if (pos != name.npos)
                    name.erase(name.begin() + pos, name.begin() + pos + 4);
                else
                {
                    pos = name.find(".4v");
                    if (pos != name.npos && name[pos + 3] == 0)
                        name.erase(name.begin() + pos, name.begin() + pos + 3);;
                }
                return MediaType(eExtension::_qts, eReplay::Quartet);
            },
            [](const char* name) { return strstr(name, "smp.") != nullptr || strstr(name, ".set") != nullptr; },
            nullptr, true
        },
        { // uade
            "SoundPlayer",
            ModlandReplay::kSoundPlayer,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto ext = strstr(url.data(), "/sjs."); ext[1] = 's'; ext[2] = 'm'; ext[3] = 'p'; name[0] = 's'; name[1] = 'm'; name[2] = 'p'; },
            [](std::string& name) { name.erase(name.begin(), name.begin() + 4); return MediaType(eExtension::_sjs, eReplay::UADE); },
            [](const char* name) { return strstr(name, "smp.") != nullptr; }
        },
        { // uade
            "Synth Dream",
            ModlandReplay::kSynthDream,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name)
            {
                auto ext = strstr(url.data(), "/sdr.");
                if (strstr(ext, "nobuddiesland%20") && ext[sizeof("/sdr.nobuddiesland%20")] >= '1' && ext[sizeof("/sdr.nobuddiesland%20")] <= '8')
                {
                    ext[1] = 's'; ext[2] = 'm'; ext[3] = 'p';
                    name[0] = 's'; name[1] = 'm'; name[2] = 'p';
                }
                else
                {
                    memcpy(ext + 1, "smp.set", sizeof("smp.set"));
                    name = "smp.set";
                }
            },
            [](std::string& name) { name.erase(name.begin(), name.begin() + 4); return MediaType(eExtension::_sdr, eReplay::UADE); },
            [](const char* name) { return strstr(name, "smp.") != nullptr; }
        },
        { // uade
            "Synth Pack",
            ModlandReplay::kSynthPack,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto pos = url.find_last_of('/'); url.resize(pos + 1); url += "smp.set"; name = "smp.set"; },
            [](std::string& name) { auto extOffset = name.find_last_of('.'); name.resize(extOffset); return MediaType(eExtension::_osp, eReplay::UADE); },
            [](const char* name) { return strstr(name, "smp.set") != nullptr; }
        },
        { // uade
            "Thomas Hermann",
            ModlandReplay::kThomasHermann,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto ext = strstr(url.data(), "/thm."); ext[1] = 's'; ext[2] = 'm'; ext[3] = 'p'; name[0] = 's'; name[1] = 'm'; name[2] = 'p'; },
            [](std::string& name) { name.erase(name.begin(), name.begin() + 4); return MediaType(eExtension::_thm, eReplay::UADE); },
            [](const char* name) { return strstr(name, "smp.") != nullptr; }
        },
        { // uade
            "Paul Robotham",
            ModlandReplay::kPaulRobotham,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name)
            {
                const char* const names[] = { "ashes%20of%20empire-finale", "space%201889" };
                for (auto* n : names)
                {
                    if (url.rfind(n) != url.npos)
                    {
                        auto ext = url.data() + url.size() - 3; ext[0] = 's'; ext[1] = 's'; ext[2] = 'd';
                        ext = name.data() + name.size() - 3; ext[0] = 's'; ext[1] = 's'; ext[2] = 'd';
                        return;
                    }
                }
                auto pos = url.find_last_of('/'); url.resize(pos + 1); url += "mdtest.ssd";
                name = "mdtest.ssd";
            },
            [](std::string& name) { auto extOffset = name.find_last_of('.'); name.resize(extOffset); return MediaType(eExtension::_dat, eReplay::UADE); },
            [](const char* name) { return strstr(name, ".ssd") == (name + strlen(name) - 4); }
        },
        { // uade
            "TFMX ST",
            ModlandReplay::kTFMXST,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto ext = strstr(url.data(), "/mdat."); ext[1] = 's'; ext[2] = 'm'; ext[3] = 'p'; ext[4] = 'l'; name[0] = 's'; name[1] = 'm'; name[2] = 'p'; name[3] = 'l'; },
            [](std::string& name) { name.erase(name.begin(), name.begin() + 5); return MediaType(eExtension::_mdst, eReplay::UADE); },
            [](const char* name) { return strstr(name, "smpl.") != nullptr; }
        },
        { // uade
            "Startrekker AM",
            ModlandReplay::kStartrekkerAM,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { url += ".nt"; name += ".nt"; },
            [](std::string& name) { auto extOffset = name.find_last_of('.'); name.resize(extOffset); return MediaType(eExtension::_mod, eReplay::UADE); },
            [](const char* name) { return strstr(name, ".mod.nt") == (name + strlen(name) - 7); }
        },
        { // uade
            "Unique Development",
            ModlandReplay::kUniqueDevelopment,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto ext = strstr(url.data(), "/uds."); ext[1] = 's'; ext[2] = 'm'; ext[3] = 'p'; name[0] = 's'; name[1] = 'm'; name[2] = 'p'; },
            [](std::string& name) { name.erase(name.begin(), name.begin() + 4); return MediaType(eExtension::_uds, eReplay::UADE); },
            [](const char* name) { return strstr(name, "smp.") != nullptr; }
        },
        { // sidplay
            "Stereo Sidplayer",
            ModlandReplay::kStereoSidplayer,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto ext = url.data() + url.size() - 3; ext[0] = 's'; ext[1] = 't'; ext[2] = 'r'; ext = name.data() + name.size() - 3; ext[0] = 's'; ext[1] = 't'; ext[2] = 'r';  },
            [](std::string& name) { auto extOffset = name.find_last_of('.'); name.resize(extOffset); return MediaType(eExtension::_mus, eReplay::SidPlay); },
            [](const char* name) { return strstr(name, ".str") == (name + strlen(name) - 4); }
        },
        { // adlib
            "Ad Lib/Ken's AdLib Music",
            ModlandReplay::kKensAdLibMusic,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto pos = url.find_last_of('/'); url.resize(pos + 1); url += "insts.dat"; name = "insts.dat"; },
            [](std::string& name) { auto extOffset = name.find_last_of('.'); name.resize(extOffset); return MediaType(eExtension::_ksm, eReplay::AdLib); },
            [](const char* name) { return strstr(name, "insts.dat") != nullptr; }
        },
        { // adlib
            "Ad Lib/AdLib Tracker",
            ModlandReplay::kAdLibTracker,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto ext = url.data() + url.size() - 3; ext[0] = 'i'; ext[1] = 'n'; ext[2] = 's'; ext = name.data() + name.size() - 3; ext[0] = 'i'; ext[1] = 'n'; ext[2] = 's'; },
            [](std::string& name) { auto extOffset = name.find_last_of('.'); name.resize(extOffset); return MediaType(eExtension::_sng, eReplay::AdLib); },
            [](const char* name) { return strstr(name, ".ins") == (name + strlen(name) - 4); }
        },
        { // adlib
            "Ad Lib/Sierra",
            ModlandReplay::kAdLibSierra,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto pos = url.find_last_of('/'); url.resize(pos + 4); url += "patch.003"; if (name.size() > 3) name.resize(3); name += "patch.003"; },
            [](std::string& name) { auto extOffset = name.find_last_of('.'); name.resize(extOffset); return MediaType(eExtension::_sci, eReplay::AdLib); },
            [](const char* name) { return strstr(name, ".003") == (name + strlen(name) - 4); }
        },
        { // adlib
            "Ad Lib/Visual Composer",
            ModlandReplay::kAdLibVisualComposer,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto pos = url.find_last_of('/'); url.resize(pos + 1); url += "standard.bnk"; name = "standard.bnk"; },
            [](std::string& name) { auto extOffset = name.find_last_of('.'); name.resize(extOffset); return MediaType(eExtension::_rol, eReplay::AdLib); },
            [](const char* name) { return strstr(name, "standard.bnk") != nullptr; }
        },
        { // ken
            "Ken's Digital Music",
            ModlandReplay::kKensDigitalMusic,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { auto pos = url.find_last_of('/'); url.resize(pos + 1); url += "waves.kwv"; name = "waves.kwv"; },
            [](std::string& name) { auto extOffset = name.find_last_of('.'); name.resize(extOffset); return MediaType(eExtension::_kdm, eReplay::Ken); },
            [](const char* name) { return strstr(name, "waves.kwv") != nullptr; }
        },
        { // MegaTracker
            "SoundSmith",
            ModlandReplay::kSoundSmith,
            [](CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string& url, std::string& name) { url += ".W"; name += ".W"; },
            [](std::string& name) {  if (tolower(name[name.size() - 1]) == 'w' && name[name.size() - 2] == '.') name.resize(name.size() - 2); return MediaType(eExtension::_, eReplay::MegaTracker); },
            [](const char* name) { auto l = strlen(name); return tolower(name[l - 1]) == 'w' && name[l - 2] == '.'; }
        },
    };

    inline const char* SourceModland::Chars::operator()(const Array<char>& blob) const
    {
        return blob.Items() + offset;
    }

    inline void SourceModland::Chars::Set(Array<char>& blob, const char* otherString)
    {
        auto len = strlen(otherString);
        if (len)
        {
            offset = blob.NumItems();
            blob.Add(otherString, uint32_t(len + 1));
        }
    }

    inline void SourceModland::Chars::Set(Array<char>& blob, const std::string& otherString)
    {
        if (otherString.size())
        {
            offset = blob.NumItems();
            blob.Add(otherString.c_str(), uint32_t(otherString.size() + 1));
        }
    }

    template <typename T>
    inline void SourceModland::Chars::Copy(const Array<char>& blob, Array<T>& otherblob) const
    {
        auto src = blob.Items() + offset;
        otherblob.Copy(src, strlen(src) + 1);
    }

    template <bool isCaseSensitive>
    inline bool SourceModland::Chars::IsSame(const Array<char>& blob, const char* otherString) const
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

    SourceModland::SourceModland()
    {
        m_db.items.Push(); // add first item because it will be used as an invalid index

        m_artists.Push();
        m_replays.Push();
        m_songs.Push();
        m_strings.Add('\0'); // 0 == empty string == invalid offset
        m_data.Add(uint8_t(0), alignof(SourceSong));
    }

    SourceModland::~SourceModland()
    {}

    void SourceModland::FindArtists(ArtistsCollection& artists, const char* name)
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
                sprintf(buf, "%u %s", dbArtist.numSongs, dbArtist.numSongs <= 1 ? "song" : "songs");
                newArtist.description = buf;
            }
        }
    }

    void SourceModland::ImportArtist(SourceID importedArtistID, SourceResults& results)
    {
        assert(importedArtistID.sourceId == kID);
        results.importedArtists.Add(importedArtistID);

        {
            thread::ScopedSpinLock lock(m_mutex);
            if (DownloadDatabase())
                return;
        }

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

            assert(dbImportedArtistId != 0); // has the artist disappeared from modland?
            if (dbImportedArtistId == 0)
                Log::Error("Modland: can't find artist \"%s\"\n", name);
        }

        for (uint32_t dbSongId = m_db.artists[dbImportedArtistId].songs; dbSongId; dbSongId = m_db.songs[dbSongId].nextSong[m_db.songs[dbSongId].artists[0] == dbImportedArtistId ? 0 : 1])
        {
            const auto& dbSong = m_db.songs[dbSongId];
            std::string dbSongName(dbSong.name(m_db.strings));
            auto songSourceId = FindSong(dbSong);
            if (results.IsSongAvailable(SourceID(kID, songSourceId)))
                continue;

            auto* song = new SongSheet();
            auto* songSource = GetSongSource(songSourceId);

            SourceResults::State state;
            song->id = songSource->songId;
            if (songSource->isDiscarded)
                state.SetSongStatus(song->id == SongID::Invalid ? SourceResults::kDiscarded : SourceResults::kMerged);
            else
                song->id == SongID::Invalid ? state.SetSongStatus(SourceResults::kNew).SetChecked(true) : state.SetSongStatus(SourceResults::kOwned);

            uint32_t artistIds[2] = { 0, 0 };
            for (uint32_t i = 0; i < 2; i++)
            {
                if (songSource->artists[i])
                {
                    SourceID artistId(kID, songSource->artists[i]);
                    auto* artistIt = results.artists.FindIf([artistId](auto entry)
                    {
                        return entry->sources[0].id == artistId;
                    });
                    if (artistIt == nullptr)
                    {
                        artistIds[i] = results.artists.NumItems();
                        auto artist = new ArtistSheet();
                        artist->handles.Add(m_db.artists[dbSong.artists[i]].name(m_db.strings));
                        artist->sources.Add(artistId);
                        results.artists.Add(artist);
                    }
                    else
                        artistIds[i] = artistIt - results.artists;
                }
            }

            song->type = UpdateMediaType(dbSong, dbSongName);
            song->name = dbSongName;
            if (dbSong.artists[0])
            {
                song->artistIds.Add(static_cast<ArtistID>(artistIds[0]));
                if (dbSong.artists[1])
                    song->artistIds.Add(static_cast<ArtistID>(artistIds[1]));
            }
            song->sourceIds.Add(SourceID(kID, songSourceId));
            results.songs.Add(song);
            results.states.Add(state);
        }

        m_isDirty |= results.songs.IsNotEmpty();
    }

    void SourceModland::FindSongs(const char* name, SourceResults& collectedSongs)
    {
        {
            thread::ScopedSpinLock lock(m_mutex);
            if (DownloadDatabase())
                return;
        }

        std::string lName = ToLower(name);

        for (uint32_t i = 0, e = m_db.songs.NumItems(); i < e; i++)
        {
            const auto& dbSong = m_db.songs[i];
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

            uint32_t artistIds[2] = { 0, 0 };
            for (uint32_t artistIdx = 0; artistIdx < 2; artistIdx++)
            {
                if (songSource->artists[artistIdx])
                {
                    SourceID artistId(kID, songSource->artists[artistIdx]);
                    auto artistIt = collectedSongs.artists.FindIf([artistId](auto entry)
                    {
                        return entry->sources[0].id == artistId;
                    });
                    if (artistIt == nullptr)
                    {
                        artistIds[artistIdx] = collectedSongs.artists.NumItems();
                        auto artist = new ArtistSheet();
                        artist->handles.Add(m_db.artists[dbSong.artists[artistIdx]].name(m_db.strings));
                        artist->sources.Add(artistId);
                        collectedSongs.artists.Add(artist);
                    }
                    else
                        artistIds[artistIdx] = artistIt - collectedSongs.artists;
                }
            }

            song->type = UpdateMediaType(dbSong, dbSongName);
            song->name = dbSongName;
            if (dbSong.artists[0])
            {
                song->artistIds.Add(static_cast<ArtistID>(artistIds[0]));
                if (dbSong.artists[1])
                    song->artistIds.Add(static_cast<ArtistID>(artistIds[1]));
            }
            song->sourceIds.Add(SourceID(kID, songSourceId));
            collectedSongs.songs.Add(song);
            collectedSongs.states.Add(state);
        }
    }

    Source::Import SourceModland::ImportSong(SourceID sourceId, const std::string& path)
    {
        thread::ScopedSpinLock lock(m_mutex);
        assert(sourceId.sourceId == kID);
        auto* songSource = GetSongSource(sourceId.internalId);

        if (auto* replay = GetReplayOverride(songSource))
            return ImportMultiSong(sourceId, replay, path);
        if (strcmp(m_replays[songSource->replay].name(m_strings), "MDX") == 0)
            return ImportPkSong(sourceId, ModlandReplay::kMDX, path);
        if (strcmp(m_replays[songSource->replay].name(m_strings), "Capcom Q-Sound Format") == 0)
            return ImportPkSong(sourceId, ModlandReplay::kQSF, path);
        if (strcmp(m_replays[songSource->replay].name(m_strings), "Gameboy Sound Format") == 0)
            return ImportPkSong(sourceId, ModlandReplay::kGSF, path);
        if (strcmp(m_replays[songSource->replay].name(m_strings), "Nintendo DS Sound Format") == 0)
            return ImportPkSong(sourceId, ModlandReplay::k2SF, path);
        if (strcmp(m_replays[songSource->replay].name(m_strings), "Saturn Sound Format") == 0)
            return ImportPkSong(sourceId, ModlandReplay::kSSF, path);
        if (strcmp(m_replays[songSource->replay].name(m_strings), "Dreamcast Sound Format") == 0)
            return ImportPkSong(sourceId, ModlandReplay::kDSF, path);
        if (strcmp(m_replays[songSource->replay].name(m_strings), "Playstation Sound Format") == 0)
            return ImportPkSong(sourceId, ModlandReplay::kPSF, path);
        if (strcmp(m_replays[songSource->replay].name(m_strings), "Playstation 2 Sound Format") == 0)
            return ImportPkSong(sourceId, ModlandReplay::kPSF2, path);
        if (strcmp(m_replays[songSource->replay].name(m_strings), "Ultra64 Sound Format") == 0)
            return ImportPkSong(sourceId, ModlandReplay::kUSF, path);
        if (strcmp(m_replays[songSource->replay].name(m_strings), "Super Nintendo Sound Format") == 0)
            return ImportPkSong(sourceId, ModlandReplay::kSNSF, path);
        if (strstr(m_replays[songSource->replay].name(m_strings), "MoonBlaster") == m_replays[songSource->replay].name(m_strings))
            return ImportPkSong(sourceId, strstr(m_replays[songSource->replay].name(m_strings), "edit") ? ModlandReplay::kMBMEdit : ModlandReplay::kMBM, path);
        if (strcmp(m_replays[songSource->replay].name(m_strings), "FAC SoundTracker") == 0)
            return ImportPkSong(sourceId, ModlandReplay::kFACSoundTracker, path);
        if (strcmp(m_replays[songSource->replay].name(m_strings), "IFF-SMUS") == 0)
            return ImportPkSong(sourceId, ModlandReplay::kIFFSmus, path);
        if (strcmp(m_replays[songSource->replay].name(m_strings), "Euphony") == 0)
            return ImportPkSong(sourceId, ModlandReplay::kEuphony, path);

        CURL* curl = curl_easy_init();

        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);

        auto url = SetupUrl(curl, songSource);

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
            if ((buffer.Size() < 256 && strstr(buffer.Items<const char>(), "404 Not Found"))
                || strstr(buffer.Items<const char>(), "<!DOCTYPE html>"))
            {
                if (strcmp(m_replays[songSource->replay].name(m_strings), "Delitracker Custom") == 0)
                {
                    Log::Message("Discarded\n");
                    curl_easy_cleanup(curl);
                    return ImportPkSong(sourceId, ModlandReplay::kDelitrackerCustom, path);
                }

                isEntryMissing = true;
                if (songSource->artists[0])
                    m_areStringsDirty |= --m_artists[songSource->artists[0]].refcount == 0;
                if (songSource->artists[1])
                    m_areStringsDirty |= --m_artists[songSource->artists[1]].refcount == 0;
                new (songSource) SourceSong();
                m_areDataDirty = true;
                m_availableSongIds.Add(sourceId.internalId);

                Log::Error("Modland: file \"%s\" not found\n", url.c_str());
            }
            else
            {
                songSource->crc = crc32(0L, Z_NULL, 0);
                songSource->crc = crc32_z(songSource->crc, buffer.Items(), buffer.Size<size_t>());
                songSource->size = buffer.NumItems();

                stream = io::StreamMemory::Create(path, buffer.Items(), buffer.Size(), false);
                buffer.Detach();

                Log::Message("OK\n");
            }
            m_isDirty = true;
        }
        else
            Log::Error("Modland: %s\n", curl_easy_strerror(curlError));

        curl_easy_cleanup(curl);

        return { stream, isEntryMissing };
    }

    void SourceModland::OnArtistUpdate(ArtistSheet* artist)
    {
        (void)artist;
    }

    void SourceModland::OnSongUpdate(const Song* const song)
    {
        assert(song->GetSourceId(0).sourceId == kID);
        auto* songSource = GetSongSource(song->GetSourceId(0).internalId);
        if (!songSource->IsValid())
        {
            for (auto artistId : songSource->artists)
            {
                if (artistId)
                    m_artists[artistId].refcount++;
            }
        }
        songSource->songId = song->GetId();
        songSource->isDiscarded = false;
        m_isDirty = true;
    }

    void SourceModland::DiscardSong(SourceID sourceId, SongID newSongId)
    {
        thread::ScopedSpinLock lock(m_mutex);
        assert(sourceId.sourceId == kID);
        auto* songSource = GetSongSource(sourceId.internalId);
        songSource->songId = newSongId;
        songSource->isDiscarded = true;
        m_isDirty = true;
    }

    void SourceModland::InvalidateSong(SourceID sourceId, SongID newSongId)
    {
        thread::ScopedSpinLock lock(m_mutex);
        assert(sourceId.sourceId == kID && newSongId != SongID::Invalid);
        auto* songSource = GetSongSource(sourceId.internalId);
        songSource->songId = newSongId;
        songSource->isDiscarded = false;
        m_isDirty = true;
    }

    std::string SourceModland::GetArtistStub(SourceID artistId) const
    {
        return m_artists[artistId.internalId].name(m_strings);
    }

    bool SourceModland::IsValidationEnabled() const
    {
        // validation is only possible when the database has been downloaded
        return m_db.songs.IsNotEmpty();
    }

    Status SourceModland::Validate(SourceID sourceId, SongID songId)
    {
        thread::ScopedSpinLock lock(m_mutex);
        auto* songSource = GetSongSource(sourceId.internalId);
        if (songSource->songId == songId) // some bugs where introduced from time to time in this db, so we can have a mismatch
        {
            // now check if this song still exists in the remote database
            for (uint32_t i = 0, e = m_db.songs.NumItems(); i < e; i++)
            {
                const auto& dbSong = m_db.songs[i];
                if (dbSong.isExtensionOverriden != songSource->isExtensionOverriden)
                    continue;
                if (!dbSong.name.IsSame(m_db.strings, songSource->name))
                    continue;
                if (!m_db.replays[dbSong.replayId].name.IsSame(m_db.strings, m_replays[songSource->replay].name(m_strings)))
                    continue;
                if (!m_db.artists[dbSong.artists[0]].name.IsSame(m_db.strings, m_artists[songSource->artists[0]].name(m_strings)))
                    continue;
                if (!m_db.artists[dbSong.artists[1]].name.IsSame(m_db.strings, m_artists[songSource->artists[1]].name(m_strings)))
                    continue;
                return Status::kOk;
            }
        }

        std::string url = m_replays[songSource->replay].name(m_strings);
        if (songSource->artists[0] == 0)
            url += "/- unknown/";
        else
        {
            url += "/";
            url += m_artists[songSource->artists[0]].name(m_strings);
            if (songSource->artists[1] != 0)
            {
                url += "/coop-";
                url += m_artists[songSource->artists[1]].name(m_strings);
            }
            url += "/";
        }
        url += songSource->name;
        if (!songSource->isExtensionOverriden && m_replays[songSource->replay].ext.offset)
        {
            url += '.';
            url += m_replays[songSource->replay].ext(m_strings);
        }
        Log::Message("Modland: lost \"%s\"\n", url.c_str());

        if (songSource->artists[0])
        {
            assert(m_artists[songSource->artists[0]].refcount >= 0);
            m_artists[songSource->artists[0]].refcount--;
            if (songSource->artists[1])
            {
                assert(m_artists[songSource->artists[1]].refcount >= 0);
                m_artists[songSource->artists[1]].refcount--;
            }
        }
        new (songSource) SourceSong();
        m_isDirty = true;
        m_areStringsDirty = true;
        m_areDataDirty = true;

        return Status::kFail;
    }

    Status SourceModland::Validate(SourceID sourceId, ArtistID artistId)
    {
        (void)artistId;
        thread::ScopedSpinLock lock(m_mutex);
        if (sourceId.internalId >= m_artists.NumItems())
            return Status::kFail;
        return m_artists[sourceId.internalId].refcount > 0 ? Status::kOk : Status::kFail;
    }

    void SourceModland::Load()
    {
        auto file = io::File::OpenForRead(ms_filename);
        if (file.IsValid())
        {
            if (file.Read<uint32_t>() != kMusicFileStamp || file.Read<uint32_t>() > Core::GetVersion())
            {
                assert(0 && "file read error");
                return;
            }
            file.Read<uint32_t>(m_strings);
            file.Read<uint32_t>(m_replays);
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
            m_availableReplayIds.Clear();
            for (uint32_t i = 1, e = m_replays.NumItems(); i < e; i++)
            {
                if (m_replays[i].name.offset == 0)
                    m_availableReplayIds.Add(uint16_t(i));
            }
        }
    }

    void SourceModland::Save() const
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
                    Array<char> strings(size_t(0), m_strings.NumItems());
                    strings.Add('\0');
                    // scan for unused replays
                    Array<SourceReplay> replays(m_replays);
                    Array<bool> isReplayAvailable(m_replays.NumItems());
                    memset(isReplayAvailable.Items(), 0, isReplayAvailable.Size<size_t>());
                    for (uint32_t i = 1, e = m_songs.NumItems(); i < e; i++)
                    {
                        auto* song = GetSongSource(uint32_t(i));
                        if (song->songId == SongID::Invalid && !song->isDiscarded)
                            continue;
                        isReplayAvailable[song->replay] = true;
                    }
                    // clear unused replays
                    for (uint32_t i = 1, e = replays.NumItems(); i < e; i++)
                    {
                        if (!isReplayAvailable[i])
                            replays[i] = {};
                    }
                    // remove discarded replays at the end of the array
                    for (auto i = isReplayAvailable.NumItems<int64_t>() - 1; i > 0; i--)
                    {
                        if (isReplayAvailable[i])
                            break;
                        replays.Pop();
                    }
                    // re-pack replays
                    for (uint32_t i = 1, e = replays.NumItems(); i < e; i++)
                    {
                        if (replays[i].name.offset)
                        {
                            replays[i].name.Set(strings, replays[i].name(m_strings));
                            if (replays[i].ext.offset)
                                replays[i].ext.Set(strings, replays[i].ext(m_strings));
                        }
                    }
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
                    file.Write<uint32_t>(replays);
                    file.Write<uint32_t>(artists);
                }
                else
                {
                    file.Write<uint32_t>(m_strings);
                    file.Write<uint32_t>(m_replays);
                    file.Write<uint32_t>(m_artists);
                }
                if (m_areDataDirty)
                {
                    Array<uint8_t> data(0u, m_data.NumItems());
                    data.Add(m_data.Items(), alignof(SourceSong));
                    // remove discarded songs at the end of the array
                    Array<uint32_t> songs(m_songs);
                    for (auto i = m_songs.NumItems<int64_t>() - 1; i > 0; i--)
                    {
                        auto* song = GetSongSource(uint32_t(i));
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

    SourceModland::SourceSong* SourceModland::GetSongSource(uint32_t index) const
    {
        return m_data.Items<SourceSong>(m_songs[index]);
    }

    uint16_t SourceModland::FindArtist(const char* const name)
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

    uint32_t SourceModland::FindSong(const ModlandSong& dbSong)
    {
        // look in our database (this is ugly/slow)
        for (uint32_t i = 1, e = m_songs.NumItems(); i < e; i++)
        {
            if (m_songs[i] == 0)
                continue;
            auto* song = GetSongSource(i);
            if (dbSong.name.IsSame(m_db.strings, song->name))
            {
                if (!m_db.replays[dbSong.replayId].name.IsSame(m_db.strings, m_replays[song->replay].name(m_strings)))
                    continue;
                if (song->artists[0])
                {
                    if (!dbSong.artists[0])
                        continue;
                    if (!m_db.artists[dbSong.artists[0]].name.IsSame(m_db.strings, m_artists[song->artists[0]].name(m_strings)))
                        continue;
                    if (song->artists[1])
                    {
                        if (!dbSong.artists[1])
                            continue;
                        if (!m_db.artists[dbSong.artists[1]].name.IsSame(m_db.strings, m_artists[song->artists[1]].name(m_strings)))
                            continue;
                    }
                    else if (dbSong.artists[1])
                        continue;
                }
                else if (dbSong.artists[0])
                    continue;

                return i;
            }
        }
        // add a new one
        thread::ScopedSpinLock lock(m_mutex);
        uint32_t id = m_availableSongIds.IsEmpty() ? m_songs.Push<uint32_t>() : m_availableSongIds.Pop();
        m_songs[id] = m_data.NumItems();
        auto* songSource = new (m_data.Push(offsetof(SourceSong, name))) SourceSong();
        // look for the replay in our database
        for (uint32_t i = 1, e = m_replays.NumItems(); i < e; i++)
        {
            if (m_db.replays[dbSong.replayId].name.IsSame(m_db.strings, m_replays[i].name(m_strings)))
            {
                songSource->replay = uint16_t(i);
                break;
            }
        }
        if (songSource->replay == 0)
        {
            // add a new replay
            if (m_availableReplayIds.IsEmpty())
            {
                songSource->replay = m_replays.NumItems<uint16_t>();
                m_replays.Push();
            }
            else
                songSource->replay = m_availableReplayIds.Pop();
            auto replay = m_replays.Items(songSource->replay);
            replay->name.Set(m_strings, m_db.replays[dbSong.replayId].name(m_db.strings));
            if (m_db.replays[dbSong.replayId].type <= ModlandReplay::kDefault && m_db.replays[dbSong.replayId].ext.offset)
                replay->ext.Set(m_strings, m_db.replays[dbSong.replayId].ext(m_db.strings));
        }
        // get artists
        for (uint32_t i = 0; i < 2; i++)
        {
            if (dbSong.artists[i])
                songSource->artists[i] = FindArtist(m_db.artists[dbSong.artists[i]].name(m_db.strings));
        }
        // extension
        songSource->isExtensionOverriden = dbSong.isExtensionOverriden;
        // name
        dbSong.name.Copy(m_db.strings, m_data);
        m_data.Resize((m_data.NumItems() + alignof(SourceSong) - 1) & ~(alignof(SourceSong) - 1));

        m_areDataDirty = true;

        return id;
    }

    std::string SourceModland::SetupUrl(CURL* curl, SourceSong* songSource) const
    {
        std::string url("https://modland.com/pub/modules/");
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

        unescape(m_replays[songSource->replay].name(m_strings));
        if (songSource->artists[0] == 0)
            url += "/-%20unknown/";
        else
        {
            url += "/";
            unescape(m_artists[songSource->artists[0]].name(m_strings));
            if (songSource->artists[1] != 0)
            {
                url += "/coop-";
                unescape(m_artists[songSource->artists[1]].name(m_strings));
            }
            url += "/";
        }
        unescape(songSource->name);
        if (!songSource->isExtensionOverriden && m_replays[songSource->replay].ext.offset)
        {
            url += '.';
            unescape(m_replays[songSource->replay].ext(m_strings));
        }

        Log::Message("Modland: downloading \"%s\"...", url.c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

        return url;
    }

    std::string SourceModland::CleanUrl(CURL* curl, const char* url)
    {
        std::string cleanedUrl;
        const char* next;
        while (next = strchr(url, '/'))
        {
            if (url != next)
            {
                auto e = curl_easy_escape(curl, url, static_cast<int>(next - url));
                cleanedUrl += e;
                curl_free(e);
            }
            cleanedUrl += '/';
            url = next + 1;
        }
        auto e = curl_easy_escape(curl, url, int(strlen(url)));
        cleanedUrl += e;
        curl_free(e);
        return cleanedUrl;
    }

    Source::Import SourceModland::ImportMultiSong(SourceID sourceId, const ModlandReplayOverride* const replay, const std::string& path)
    {
        if (replay->isKeepingLink && DownloadDatabase())
            return {};

        auto* songSource = GetSongSource(sourceId.internalId);

        CURL* curl = curl_easy_init();

        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);

        auto url = SetupUrl(curl, songSource);

        struct Buffer : public Array<uint8_t>
        {
            static size_t Writer(const uint8_t* data, size_t size, size_t nmemb, Buffer* buffer)
            {
                buffer->Add(data, uint32_t(size * nmemb));
                return size * nmemb;
            }
        } curlBuffer;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Buffer::Writer);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curlBuffer);
        SmartPtr<io::Stream> stream;
        bool isEntryMissing = false;
        auto curlError = curl_easy_perform(curl);
        if (curlError == CURLE_OK)
        {
            struct ArchiveBuffer : public Array<uint8_t>
            {
                static int ArchiveOpen(struct archive*, void*)
                {
                    return ARCHIVE_OK;
                }

                static int ArchiveClose(struct archive*, void*)
                {
                    return ARCHIVE_OK;
                }

                static int ArchiveFree(struct archive*, void*)
                {
                    return ARCHIVE_OK;
                }

                static la_ssize_t ArchiveWrite(struct archive*, void* _client_data, const void* _buffer, size_t _length)
                {
                    reinterpret_cast<ArchiveBuffer*>(_client_data)->Add(reinterpret_cast<const uint8_t*>(_buffer), uint32_t(_length));
                    return _length;
                }
            } archiveBuffer;

            auto* archive = archive_write_new();
            archive_write_set_format_7zip(archive);
            auto r = archive_write_open2(archive, &archiveBuffer, archiveBuffer.ArchiveOpen, ArchiveBuffer::ArchiveWrite, archiveBuffer.ArchiveClose, archiveBuffer.ArchiveFree);
            assert(r == ARCHIVE_OK);
            auto entry = archive_entry_new();

            std::string name = songSource->name;
            auto npos = name.rfind('/');
            if (npos != name.npos)
                name.erase(0, npos + 1);
            if (!songSource->isExtensionOverriden && m_replays[songSource->replay].ext.offset)
            {
                name += '.';
                name += m_replays[songSource->replay].ext(m_strings);
            }

            archive_entry_set_pathname(entry, name.c_str());
            archive_entry_set_size(entry, curlBuffer.Size());
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_perm(entry, 0644);
            archive_write_header(archive, entry);
            archive_write_data(archive, curlBuffer.Items(), curlBuffer.Size<size_t>());
            archive_entry_clear(entry);
            curlBuffer.Clear();

            replay->nextUrlAndName(curl, replay, *this, url, name);
            if (!url.empty())
            {
                Log::Message("\"%s\"...", url.c_str());
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curlError = curl_easy_perform(curl);
                if (curlError == CURLE_OK)
                {
                    archive_entry_set_pathname(entry, name.c_str());
                    archive_entry_set_size(entry, curlBuffer.Size());
                    archive_entry_set_filetype(entry, AE_IFREG);
                    archive_entry_set_perm(entry, 0644);
                    archive_write_header(archive, entry);
                    archive_write_data(archive, curlBuffer.Items(), curlBuffer.Size<size_t>());
                    archive_entry_clear(entry);
                }
            }

            r = archive_write_free(archive);
            (void)r; assert(r == ARCHIVE_OK);
            archive_entry_free(entry);

            if (replay->type == ModlandReplay::kTFMXST)
                archiveBuffer.Copy("MDST-MOD", sizeof("MDST-MOD") - 1);

            songSource->crc = crc32(0L, Z_NULL, 0);
            songSource->crc = crc32_z(songSource->crc, archiveBuffer.Items(), archiveBuffer.Size<size_t>());
            songSource->size = archiveBuffer.Size<uint32_t>();
            m_isDirty = true;

            stream = io::StreamMemory::Create(path, archiveBuffer.Items(), archiveBuffer.Size(), false);
            archiveBuffer.Detach();

            Log::Message("OK\n");
        }
        else if (curlError == CURLE_HTTP_RETURNED_ERROR)
        {
            long responseCode = 0;
            if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode) == CURLE_OK && responseCode == 404)
            {
                isEntryMissing = true;
                if (songSource->artists[0])
                    m_areStringsDirty |= --m_artists[songSource->artists[0]].refcount == 0;
                if (songSource->artists[1])
                    m_areStringsDirty |= --m_artists[songSource->artists[1]].refcount == 0;
                new (songSource) SourceSong();
                m_areDataDirty = true;
                m_availableSongIds.Add(sourceId.internalId);
                m_isDirty = true;

                Log::Error("Modland: file \"%s\" not found\n", url.c_str());
            }
            else
                Log::Error("Modland: %s\n", curl_easy_strerror(curlError));
        }
        else
            Log::Error("Modland: %s\n", curl_easy_strerror(curlError));

        curl_easy_cleanup(curl);

        return { stream, isEntryMissing, true, true, replay->getTypeAndName(url).ext };
    }

    Source::Import SourceModland::ImportPkSong(SourceID sourceId, ModlandReplay::Type replayType, const std::string& path)
    {
        if (DownloadDatabase())
            return {};

        auto* songSource = GetSongSource(sourceId.internalId);
        CURL* curl = curl_easy_init();
        std::string baseUrl = SetupUrl(curl, songSource);
        baseUrl += "/";

        for (auto& dbSong : m_db.songs)
        {
            if (m_db.replays[dbSong.replayId].type != replayType)
                continue;
            if (!dbSong.name.IsSame(m_db.strings, songSource->name))
                continue;
            if (songSource->artists[0])
            {
                if (!dbSong.artists[0])
                    continue;
                if (!m_db.artists[dbSong.artists[0]].name.IsSame(m_db.strings, m_artists[songSource->artists[0]].name(m_strings)))
                    continue;
                if (songSource->artists[1])
                {
                    if (!dbSong.artists[1])
                        continue;
                    if (!m_db.artists[dbSong.artists[1]].name.IsSame(m_db.strings, m_artists[songSource->artists[1]].name(m_strings)))
                        continue;
                }
                else if (dbSong.artists[1])
                    continue;
            }
            else if (dbSong.artists[0])
                continue;

            Array<std::string> items;
            for (auto item = dbSong.item; item; item = m_db.items[item].next)
                items.Add(m_db.items[item].name(m_db.strings));
            std::sort(items.begin(), items.end(), [](const std::string& l, const std::string& r)
            {
                if (l[l.size() - 3] != r[r.size() - 3])
                    return l[l.size() - 3] < r[r.size() - 3];
                return strcmp(l.c_str(), r.c_str()) < 0;
            });

            struct CurlBuffer : public Array<uint8_t>
            {
                static size_t Writer(const uint8_t* data, size_t size, size_t nmemb, CurlBuffer* buffer)
                {
                    buffer->Add(data, uint32_t(size * nmemb));
                    return size * nmemb;
                }
            } curlBuffer;

            char errorBuffer[CURL_ERROR_SIZE];
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlBuffer.Writer);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curlBuffer);

            struct ArchiveBuffer : public Array<uint8_t>
            {
                static int ArchiveOpen(struct archive*, void*)
                {
                    return ARCHIVE_OK;
                }

                static int ArchiveClose(struct archive*, void*)
                {
                    return ARCHIVE_OK;
                }

                static int ArchiveFree(struct archive*, void*)
                {
                    return ARCHIVE_OK;
                }

                static la_ssize_t ArchiveWrite(struct archive*, void* _client_data, const void* _buffer, size_t _length)
                {
                    reinterpret_cast<ArchiveBuffer*>(_client_data)->Add(reinterpret_cast<const uint8_t*>(_buffer), uint32_t(_length));
                    return _length;
                }
            } archiveBuffer;

            bool hasFailed = false;
            SmartPtr<io::Stream> stream;
            bool isEntryMissing = false;

            auto* archive = archive_write_new();
            archive_write_set_format_7zip(archive);
            auto r = archive_write_open2(archive, &archiveBuffer, archiveBuffer.ArchiveOpen, ArchiveBuffer::ArchiveWrite, archiveBuffer.ArchiveClose, archiveBuffer.ArchiveFree);
            assert(r == ARCHIVE_OK);
            auto entry = archive_entry_new();
            Log::Message("Modland: downloading ");
            for (auto& item : items)
            {
                std::string url = baseUrl;
                auto e = curl_easy_escape(curl, item.c_str(), int32_t(item.size()));
                url += e;
                curl_free(e);
                Log::Message("\"%s\"...", url.c_str());
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                auto curlError = curl_easy_perform(curl);
                if (curlError == CURLE_OK && curlBuffer.IsNotEmpty())
                {
                    if (curlBuffer.Size() < 256 && strstr((const char*)curlBuffer.begin(), "404 Not Found"))
                    {
                        isEntryMissing = true;
                        hasFailed = true;
                        if (songSource->artists[0])
                            m_areStringsDirty |= --m_artists[songSource->artists[0]].refcount == 0;
                        if (songSource->artists[1])
                            m_areStringsDirty |= --m_artists[songSource->artists[1]].refcount == 0;
                        new (songSource) SourceSong();
                        m_areDataDirty = true;
                        m_availableSongIds.Add(sourceId.internalId);
                        m_isDirty = true;

                        Log::Error("Modland: file \"%s\" not found\n", url.c_str());

                        break;
                    }
                    else
                    {
                        archive_entry_set_pathname(entry, item.c_str());
                        archive_entry_set_size(entry, curlBuffer.Size());
                        archive_entry_set_filetype(entry, AE_IFREG);
                        archive_entry_set_perm(entry, 0644);
                        archive_write_header(archive, entry);
                        archive_write_data(archive, curlBuffer.Items(), curlBuffer.Size<size_t>());
                        archive_entry_clear(entry);
                        curlBuffer.Clear();
                    }
                }
                else
                {
                    Log::Error("Modland: %s\n", curl_easy_strerror(curlError));
                    hasFailed = true;
                    break;
                }
            }

            curl_easy_cleanup(curl);

            r = archive_write_free(archive);
            (void)r; assert(r == ARCHIVE_OK);
            archive_entry_free(entry);

            if (!hasFailed)
            {
                if (replayType == ModlandReplay::kDelitrackerCustom)
                    archiveBuffer.Copy("CUST-PKG", sizeof("CUST-PKG") - 1);
                else if (replayType == ModlandReplay::kIFFSmus)
                    archiveBuffer.Copy("SMUS-PKG", sizeof("SMUS-PKG") - 1);

                songSource->crc = crc32(0L, Z_NULL, 0);
                songSource->crc = crc32_z(songSource->crc, archiveBuffer.Items(), archiveBuffer.Size<size_t>());
                songSource->size = archiveBuffer.Size<uint32_t>();
                m_isDirty = true;

                stream = io::StreamMemory::Create(path, archiveBuffer.Items(), archiveBuffer.Size(), false);
                archiveBuffer.Detach();

                Log::Message("OK\n");
            }
            std::string dbSongName(dbSong.name(m_db.strings));
            return { stream, isEntryMissing, true, false, UpdateMediaType(dbSong, dbSongName).ext };
        }

        if (songSource->artists[0])
            m_areStringsDirty |= --m_artists[songSource->artists[0]].refcount == 0;
        if (songSource->artists[1])
            m_areStringsDirty |= --m_artists[songSource->artists[1]].refcount == 0;
        new (songSource) SourceSong();
        m_areDataDirty = true;
        m_availableSongIds.Add(sourceId.internalId);

        Log::Error("Modland: file \"%s\" not found\n", baseUrl.c_str());

        curl_easy_cleanup(curl);

        return { {}, true };
    }

    SourceModland::ModlandReplayOverride* const SourceModland::GetReplayOverride(ModlandReplay::Type type)
    {
        for (auto& replay : ms_replayOverrides)
        {
            if (replay.type == type)
                return &replay;
        }
        return nullptr;
    }

    const SourceModland::ModlandReplayOverride* const SourceModland::GetReplayOverride(const char* name)
    {
        for (auto& replay : ms_replayOverrides)
        {
            if (strcmp(replay.name, name) == 0)
                return &replay;
        }
        return nullptr;
    }

    const SourceModland::ModlandReplayOverride* const SourceModland::GetReplayOverride(SourceSong* songSource) const
    {
        if (auto replay = GetReplayOverride(m_replays[songSource->replay].name(m_strings)))
        {
            if (!replay->isMulti || replay->isMulti(songSource->name))
                return replay;
        }
        return nullptr;
    }

    MediaType SourceModland::UpdateMediaType(const ModlandSong& dbSong, std::string& dbSongName) const
    {
        MediaType type;
        if (auto* replay = GetReplayOverride(m_db.replays[dbSong.replayId].type))
            type = replay->getTypeAndName(dbSongName);
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kMDX)
            type = { eExtension::_mdx, eReplay::MDX };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kQSF)
            type = { eExtension::_miniqsf, eReplay::HighlyQuixotic };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kGSF)
            type = { eExtension::_minigsf, eReplay::HighlyAdvanced };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::k2SF)
            type = { eExtension::_mini2sf, eReplay::vio2sf };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kSSF)
            type = { eExtension::_minissf, eReplay::HighlyTheoretical };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kDSF)
            type = { eExtension::_minidsf, eReplay::HighlyTheoretical };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kPSF)
            type = { eExtension::_minipsf, eReplay::HighlyExperimental };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kPSF2)
            type = { eExtension::_minipsf2, eReplay::HighlyExperimental };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kUSF)
            type = { eExtension::_miniusf, eReplay::LazyUSF };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kSNSF)
            type = { eExtension::_minisnsf, eReplay::HighlyCompetitive };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kMBM || m_db.replays[dbSong.replayId].type == ModlandReplay::kMBMEdit)
            type = { eExtension::_mbm, eReplay::KSS };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kFACSoundTracker)
            type = { eExtension::_mus, eReplay::NEZplug };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kDelitrackerCustom && dbSong.item)
            type = { eExtension::_cust, eReplay::UADE };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kIFFSmus && dbSong.item)
            type = { eExtension::_smus, eReplay::UADE };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kSidMon1)
            type = { eExtension::_sid1, eReplay::UADE };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kMusicEditor)
            type = { eExtension::_med, eReplay::XMP };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kOktalyzer)
            type = { eExtension::_okt, eReplay::OpenMPT };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kOctaMED)
            type = { eExtension::_med, eReplay::OpenMPT };
        else if (m_db.replays[dbSong.replayId].type == ModlandReplay::kEuphony)
            type = { eExtension::_eup, eReplay::Euphony };
        else if (dbSong.isExtensionOverriden)
        {
            auto extOffset = dbSongName.find_last_of('.');
            if (extOffset != dbSongName.npos)
            {
                type = Core::GetReplays().Find(dbSongName.c_str() + extOffset + 1);
                if (type.ext != eExtension::Unknown)
                    dbSongName.resize(extOffset);
            }
        }
        else
        {
            type = Core::GetReplays().Find(m_db.replays[dbSong.replayId].ext(m_db.strings));
            if (type.ext == eExtension::Unknown)
            {
                dbSongName += '.';
                dbSongName += m_db.replays[dbSong.replayId].ext(m_db.strings);
            }
        }
        return type;
    }

    bool SourceModland::DownloadDatabase()
    {
        if (m_db.songs.IsEmpty())
        {
            Array<char> unpackedData;

            CURL* curl = curl_easy_init();

            char errorBuffer[CURL_ERROR_SIZE];
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
            curl_easy_setopt(curl, CURLOPT_URL, "https://modland.com/allmods.zip");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

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
            if (curl_easy_perform(curl) == CURLE_OK)
            {
                auto* zipArchive = archive_read_new();
                archive_read_support_compression_all(zipArchive);
                archive_read_support_format_zip(zipArchive);
                auto r = archive_read_open_memory(zipArchive, buffer.Items(), buffer.Size<size_t>());
                assert(r == ARCHIVE_OK);

                archive_entry* entry;
                //just one file
                if (archive_read_next_header(zipArchive, &entry) == ARCHIVE_OK)
                {
                    auto fileSize = archive_entry_size(entry);
                    unpackedData.Resize(uint32_t(fileSize));
                    auto readSize = archive_read_data(zipArchive, unpackedData.Items(), size_t(fileSize));
                    assert(readSize == fileSize); (void)readSize;
                }

                r = archive_read_free(zipArchive);
                assert(r == ARCHIVE_OK);
            }

            curl_easy_cleanup(curl);

            if (unpackedData.IsNotEmpty())
                DecodeDatabase(unpackedData.Items(), unpackedData.Items() + unpackedData.NumItems());

            // Check if discarded songs are still in the remote database
            if (m_db.songs.IsNotEmpty())
            {
                for (uint32_t i = 1, e = m_songs.NumItems(); i < e; i++)
                {
                    auto songOffset = m_songs[i];
                    if (songOffset == 0)
                        continue;
                    auto* songSource = m_data.Items<SourceSong>(songOffset);
                    if (songSource->isDiscarded && songSource->songId == SongID::Invalid)
                    {
                        if (Validate(SourceID(kID, i), SongID::Invalid) == Status::kFail)
                            m_songs[i] = 0;
                    }
                }
            }
        }
        return m_db.songs.IsEmpty();
    }

    #define BuildPathList(a) { a, sizeof(a) - 1 }

    void SourceModland::DecodeDatabase(char* bufBegin, const char* bufEnd)
    {
        //first item is ignore as index 0 is null
        m_db.replays.Resize(1);
        m_db.artists.Resize(1);
        m_db.songs.Resize(1);
        m_db.strings.Add('\0');

        struct
        {
            const char* const path;
            size_t size;
        } static ignoreList[] = {
            BuildPathList("Ad Lib/EdLib D01/"),                     // adlib multi-files - unplayable
            BuildPathList("Hippel ST COSO/Jochen Hippel/smp.set"),  // simply ignore this
            BuildPathList("HVSC"),                                  // just a mirror, conflict with actual modland structure
            BuildPathList("MusicMaker V8 Old/"),                    // uade issue?
            BuildPathList("PlaySID/"),                              // deprecated, everything is in the HVSC folder
            BuildPathList("Pollytracker/"),                         // c64 player, available as sid
            BuildPathList("RealSID/"),                              // deprecated, everything is in the HVSC folder
            BuildPathList("Renoise/"),
            BuildPathList("Renoise Old/"),
            BuildPathList("Stonetracker/"),                         // need to add to uade (multi-files & missing library support in uade)
            BuildPathList("TSS/"),                                  // T'Sound System?
            BuildPathList("Tunefish/"),
            BuildPathList("Zoundmonitor/Samples/"),                 // simply ignore this
            BuildPathList("Zoundmonitor/readme.txt")                // simply ignore this
        };

        auto buf = bufBegin;
        char* lineEnd = nullptr;
        for (auto* line = buf; line < bufEnd; line = lineEnd + 1)
        {
            // skip to the next tabulation (ignore first field)
            while (line < bufEnd && *line != '\t' && *line != '\n' && *line != '\r')
                line++;
            while (line < bufEnd && *line == '\t')
                line++;
            lineEnd = line;

            // find end of line
            while (lineEnd < bufEnd && *lineEnd != '\n' && *lineEnd != '\r')
                lineEnd++;
            lineEnd[0] = 0;

            bool isLineSkipped = false;
            for (auto ignore : ignoreList)
            {
                isLineSkipped = memcmp(line, ignore.path, ignore.size) == 0;
                if (isLineSkipped)
                    break;
            }

            if (!isLineSkipped)
            {
                std::string link(line);

                // get the replay
                auto replayIndex = FindDatabaseReplay(line);
                if (replayIndex != 0)
                {
                    line += strlen(m_db.replays[replayIndex].name(m_db.strings)) + 1;
                    uint16_t artists[2] = { 0, 0 };
                    if (memcmp(line, "- unknown/", sizeof("- unknown/") - 1) == 0)
                        line += sizeof("- unknown/") - 1;
                    else
                    {
                        auto newArtist = line;
                        while (*line != '/' && !(isLineSkipped = *line == 0))
                            line++;
                        if (isLineSkipped)
                            continue;
                        *line++ = 0;
                        artists[0] = FindDatabaseArtist(newArtist);
                        if (memcmp(line, "coop-", sizeof("coop-") - 1) == 0)
                        {
                            line += sizeof("coop-") - 1;
                            auto newArtist2 = line;
                            while (*line != '/' && !(isLineSkipped = *line == 0))
                                line++;
                            if (isLineSkipped)
                                continue;
                            *line++ = 0;
                            artists[1] = FindDatabaseArtist(newArtist2);
                        }
                    }
                    auto newSong = line;

                    const auto currentReplayType = m_db.replays[replayIndex].type;
                    // skip multi files samples
                    if (auto* replay = GetReplayOverride(currentReplayType))
                    {
                        if (replay->isIgnored(newSong))
                        {
                            if (replay->isKeepingLink)
                            {
                                auto item = m_db.items.NumItems();
                                m_db.items.Push();
                                m_db.items.Last().name.Set(m_db.strings, link.c_str() + strlen(replay->name) + 1);
                                m_db.items.Last().next = replay->item;
                                replay->item = item;
                            }
                            continue;
                        }
                    }

                    // check for .cust or .smus (multiple files to put in a package)
                    int32_t mergePackages = -1;
                    if (currentReplayType == ModlandReplay::kDelitrackerCustom || currentReplayType == ModlandReplay::kIFFSmus)
                    {
                        auto str = line;
                        while (*str != '/' && *str != 0)
                            str++;
                        mergePackages = *str == '/';
                    }

                    uint32_t item = 0;
                    char* ext = nullptr;
                    // packaged songs
                    if (currentReplayType == ModlandReplay::kMDX || currentReplayType == ModlandReplay::kQSF || currentReplayType == ModlandReplay::kGSF || currentReplayType == ModlandReplay::k2SF
                        || currentReplayType == ModlandReplay::kSSF || currentReplayType == ModlandReplay::kDSF || currentReplayType == ModlandReplay::kPSF || currentReplayType == ModlandReplay::kPSF2
                        || currentReplayType == ModlandReplay::kUSF || currentReplayType == ModlandReplay::kSNSF || currentReplayType == ModlandReplay::kMBM || currentReplayType == ModlandReplay::kMBMEdit
                        || currentReplayType == ModlandReplay::kFACSoundTracker || currentReplayType == ModlandReplay::kEuphony || mergePackages > 0)
                    {
                        auto oldLine = line;
                        while (*line != '/' && *line != 0)
                            line++;
                        if (*line == 0)
                            std::swap(line, oldLine);
                        else
                            *line++ = 0;

                        item = m_db.items.NumItems();
                        m_db.items.Push();
                        m_db.items.Last().name.Set(m_db.strings, line);
                        for (auto& dbSong : m_db.songs)
                        {
                            if (m_db.replays[dbSong.replayId].type == currentReplayType && dbSong.artists[0] == artists[0] && dbSong.artists[1] == artists[1] && dbSong.name.IsSame(m_db.strings, oldLine))
                            {
                                m_db.items.Last().next = dbSong.item;
                                dbSong.item = item;
                                item = 0;
                                break;
                            }
                        }
                        // skip if song already exists
                        if (item == 0)
                            continue;

                        m_db.items.Last().next = 0;
                        newSong = oldLine;
                    }
                    else if (currentReplayType <= ModlandReplay::kDefault || mergePackages == 0)
                    {
                        while (*line != 0)
                        {
                            if (*line == '.')
                                ext = ++line;
                            else
                                ++line;
                        }
                        if (ext == nullptr)
                        {
                            Log::Warning("Modland: missing extension \"%s\" for \"%s\"\n", m_db.replays[replayIndex].ext(m_db.strings), link.c_str());
                        }
                        else if (!m_db.replays[replayIndex].ext.IsSame<false>(m_db.strings, ext))
                        {
                            Log::Warning("Modland: extension mismatch \"%s\" for \"%s\"\n", m_db.replays[replayIndex].ext(m_db.strings), link.c_str());
                            ext = nullptr;
                        }
                        else
                            ext[-1] = 0;
                    }

                    auto songIndex = m_db.songs.NumItems();
                    m_db.songs.Push();
                    m_db.songs.Last().name.Set(m_db.strings, newSong);
                    m_db.songs.Last().replayId = replayIndex;
                    m_db.songs.Last().isExtensionOverriden = ext == nullptr;
                    for (uint32_t i = 0; i < 2; i++)
                    {
                        if (m_db.songs.Last().artists[i] = artists[i])
                        {
                            m_db.songs.Last().nextSong[i] = m_db.artists[artists[i]].songs;
                            m_db.artists[artists[i]].songs = songIndex;
                            m_db.artists[artists[i]].numSongs++;
                        }
                    }
                    m_db.songs.Last().item = item;
                }
            }
        }
    }

    uint16_t SourceModland::FindDatabaseReplay(const char* newReplay)
    {
        struct
        {
            const char* const path;
            size_t size;
        } static stripList[] = {
            BuildPathList("Ad Lib/"),
            BuildPathList("Spectrum/"),
            BuildPathList("Video Game Music/")
        };

        std::string theReplay(newReplay);
        auto offset = theReplay.find_first_of('/');
        if (offset == theReplay.npos)
            return 0;
        for (auto strip : stripList)
        {
            if (memcmp(newReplay, strip.path, strip.size) == 0)
            {
                offset = theReplay.find_first_of('/', offset + 1);
                break;
            }
        }
        theReplay.resize(offset);

        auto numReplays = m_db.replays.NumItems();
        for (auto i = numReplays - 1; i > 0; i--)
        {
            auto& replay = m_db.replays[i];

            if (theReplay == replay.name(m_db.strings))
                return uint16_t(i);
        }

        m_db.replays.Push();
        if (auto* replay = GetReplayOverride(theReplay.c_str()))
            m_db.replays.Last().type = replay->type;
        else if (theReplay == "MDX")
            m_db.replays.Last().type = ModlandReplay::kMDX;
        else if (theReplay == "Capcom Q-Sound Format")
            m_db.replays.Last().type = ModlandReplay::kQSF;
        else if (theReplay == "Gameboy Sound Format")
            m_db.replays.Last().type = ModlandReplay::kGSF;
        else if (theReplay == "Nintendo DS Sound Format")
            m_db.replays.Last().type = ModlandReplay::k2SF;
        else if (theReplay == "Saturn Sound Format")
            m_db.replays.Last().type = ModlandReplay::kSSF;
        else if (theReplay == "Dreamcast Sound Format")
            m_db.replays.Last().type = ModlandReplay::kDSF;
        else if (theReplay == "Playstation Sound Format")
            m_db.replays.Last().type = ModlandReplay::kPSF;
        else if (theReplay == "Playstation 2 Sound Format")
            m_db.replays.Last().type = ModlandReplay::kPSF2;
        else if (theReplay == "Ultra64 Sound Format")
            m_db.replays.Last().type = ModlandReplay::kUSF;
        else if (theReplay == "Super Nintendo Sound Format")
            m_db.replays.Last().type = ModlandReplay::kSNSF;
        else if (theReplay == "MoonBlaster")
            m_db.replays.Last().type = ModlandReplay::kMBM;
        else if (theReplay == "MoonBlaster (edit mode)")
            m_db.replays.Last().type = ModlandReplay::kMBMEdit;
        else if (theReplay == "FAC SoundTracker")
            m_db.replays.Last().type = ModlandReplay::kFACSoundTracker;
        else if (theReplay == "SidMon 1")
            m_db.replays.Last().type = ModlandReplay::kSidMon1;
        else if (theReplay == "Music Editor")
            m_db.replays.Last().type = ModlandReplay::kMusicEditor;
        else if (theReplay == "Oktalyzer")
            m_db.replays.Last().type = ModlandReplay::kOktalyzer;
        else if (theReplay.starts_with("OctaMED"))
            m_db.replays.Last().type = ModlandReplay::kOctaMED;
        else if (theReplay == "Delitracker Custom")
        {
            m_db.replays.Last().type = ModlandReplay::kDelitrackerCustom;
            m_db.replays.Last().ext.Set(m_db.strings, "cus");
        }
        else if (theReplay == "IFF-SMUS")
        {
            m_db.replays.Last().type = ModlandReplay::kIFFSmus;
            m_db.replays.Last().ext.Set(m_db.strings, "smus");
        }
        else if (theReplay == "Euphony")
            m_db.replays.Last().type = ModlandReplay::kEuphony;
        m_db.replays.Last().name.Set(m_db.strings, theReplay);
        if (m_db.replays.Last().type <= ModlandReplay::kDefault)
        {
            theReplay = newReplay;
            offset = theReplay.find_last_of('.');
            if (offset != theReplay.npos)
                m_db.replays.Last().ext.Set(m_db.strings, theReplay.c_str() + offset + 1);
        }
        return uint16_t(numReplays);
    }

    uint16_t SourceModland::FindDatabaseArtist(const char* newArtist)
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
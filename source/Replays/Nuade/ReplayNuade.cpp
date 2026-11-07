// some "minor" changes to make it compatible
// include\nuade\players\davidwhittaker.h
// include\nuade\players\medley.h
// include\nuade\nuade.h
// players\activisionpro\activisionpro.c
// players\deltamusic1\deltamusic1.c
// players\davidwhittaker\davidwhittaker.c
// players\deltamusic1\deltamusic1_nuade.c
// players\fred_np\fred_replayer.c
// players\majortom\majortom_nuade.c
// players\scottjohnston\scottjohnston_nuade.c
// players\sidmon2\sidmon2_nuade.c
// src\nuade_mixer.c
// src\nuade_mixer.h
// src\nuade_song.c
#include "ReplayNuade.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <IO/StreamFile.h>
#include <ReplayDll.h>
#include <RePlayer/Version.h>

#include <algorithm>
#include <filesystem>

extern "C" {
    extern thread_local const NuadePlayer* g_player;
}

namespace rePlayer
{
#   define NuadeVersion "0.1.0"

    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Nuade,
        .name = "Nuade",
        .about = "Nuade " NuadeVersion "\nCopyright (c) 2026 Daniel Collin"
            "Some players ported from NostalgicPlayer by Thomas Neumann",
        .settings = "Nuade " NuadeVersion,
        .init = ReplayNuade::Init,
        .release = ReplayNuade::Release,
        .load = ReplayNuade::Load,
        .displaySettings = ReplayNuade::DisplaySettings,
        .editMetadata = ReplayNuade::Settings::Edit
    };

    bool ReplayNuade::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayNuadeStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayNuadeSurround");
        window.RegisterSerializedData(ms_patterns, "ReplayNuadePatterns");

        std::string extensions;
        for (uint32_t i = 0; i < nuade_player_count(); ++i)
        {
            if (!extensions.empty())
                extensions += ';';
            auto* player = nuade_player_get(i);
            for (auto* ext = player->extensions; *ext; ++ext)
            {
                if (*ext != ',')
                    extensions += *ext;
                else
                    extensions += ';';
            }
        }

        auto ext = new char[extensions.size() + 1];
        memcpy(ext, extensions.c_str(), extensions.size() + 1);
        g_replayPlugin.extensions = ext;

        BuildPlayerNames();

        auto mainPath = std::filesystem::current_path() / "replays" REPLAYER_OS_STUB / "UADE\\extras";
        ms_uadePath = pcCast<char>(mainPath.u8string().c_str());

        return false;
    }

    void ReplayNuade::Release()
    {
        delete[] g_replayPlugin.extensions;
    }

    bool ReplayNuade::Loader::exists(Loader* self, const char* url)
    {
        if (url[0] == '.' && url[1] == '/')
            url += 2;
        bool ret = self->stream->Open(url).IsValid();
        if (!ret)
        {
            if (strcmp(g_player->id, "maximumeffect") == 0)
            {
                std::string lowUrl = ToLower(url);
                if (strstr(lowUrl.c_str(), "smp.set"))
                    ret = true;
            }
            else
            {
                // Look in UADE extras folder
                std::filesystem::path p(self->stream->GetName());
                p = p.remove_filename();
                p = std::filesystem::relative(url, p);
                p = std::filesystem::path(ms_uadePath) / p;
                ret = core::io::StreamFile::Create(pcCast<char>(p.u8string().c_str())).IsValid();
                if (!ret)
                {
                    p = self->stream->GetName();
                    p = p.remove_filename();
                    p = std::filesystem::relative(url, p);
                    for (auto& c : p.native())
                    {
                        if (c == L' ')
                            rCast<std::filesystem::path::value_type>(c) = L'_';
                    }
                    p = std::filesystem::path(ms_uadePath) / p;
                    ret = core::io::StreamFile::Create(pcCast<char>(p.u8string().c_str())).IsValid();
                }
            }
        }
        return ret;
    }

    NuadeIoReadResult ReplayNuade::Loader::read_url_to_memory(Loader* self, const char* url)
    {
        if (url[0] == '.' && url[1] == '/')
            url += 2;
        NuadeIoReadResult res = {};
        if (auto stream = self->stream->Open(url))
        {
            res.size = stream->GetSize();
            res.data = new uint8_t[res.size];
            stream->Read(res.data, res.size);
        }
        else
        {
            if (strcmp(g_player->id, "maximumeffect") == 0)
            {
                std::string lowUrl = ToLower(url);
                if (strstr(lowUrl.c_str(), "smp.set"))
                {
                    std::string newUrl = ms_uadePath + "/max.smp.set";
                    if (stream = core::io::StreamFile::Create(pcCast<char>(newUrl.c_str())))
                    {
                        res.size = stream->GetSize();
                        res.data = new uint8_t[res.size];
                        stream->Read(res.data, res.size);
                    }
                }
            }
            else
            {
                // Load in UADE extras folder
                std::filesystem::path p(self->stream->GetName());
                p = p.remove_filename();
                p = std::filesystem::relative(url, p);
                p = std::filesystem::path(ms_uadePath) / p;
                if (stream = core::io::StreamFile::Create(pcCast<char>(p.u8string().c_str())))
                {
                    res.size = stream->GetSize();
                    res.data = new uint8_t[res.size];
                    stream->Read(res.data, res.size);
                }
                else
                {
                    p = self->stream->GetName();
                    p = p.remove_filename();
                    p = std::filesystem::relative(url, p);
                    for (auto& c : p.native())
                    {
                        if (c == L' ')
                            rCast<std::filesystem::path::value_type>(c) = L'_';
                    }
                    p = std::filesystem::path(ms_uadePath) / p;
                    if (stream = core::io::StreamFile::Create(pcCast<char>(p.u8string().c_str())))
                    {
                        res.size = stream->GetSize();
                        res.data = new uint8_t[res.size];
                        stream->Read(res.data, res.size);
                    }
                }
            }
        }
        return res;
    }

    void ReplayNuade::Loader::free_url_to_memory(Loader* self, void* memory)
    {
        UnusedArg(self);
        delete[] pCast<uint8_t>(memory);
    }

    Replay* ReplayNuade::Load(io::Stream* stream, CommandBuffer metadata)
    {
        if (stream->GetSize() == 0)
            return nullptr;

        auto data = stream->Read();

        SmartPtr<io::Stream> tempStream;
        Loader loader = {
            .stream = stream
        };

        // Sonix package (multiple songs)
        std::string filename = stream->GetName();
        if (stream->GetComments() == "SMUS-PKG")
        {
            for (SmartPtr<io::Stream> s = stream; s; s = s->Next())
            {
                std::string entryName = s->GetName();
                auto name = ToLower(entryName);
                if (strstr(name.c_str(), ".smus") == (name.c_str() + name.size() - 5))
                {
                    entryName.resize(entryName.size() - 5);
                    loader.subsongNames.Add(entryName);
                }
            }
            if (loader.subsongNames.IsNotEmpty())
            {
                filename = loader.subsongNames[0] + ".smus";
                tempStream = stream->Open(filename);
                data = tempStream->Read();
            }
        }

        NuadeIo nuadeIO = {
            .private_data = pCast<NuadeIoPrivate>(&loader),
            .exists = gCast<decltype(NuadeIo::exists)>(loader.exists),
            .read_url_to_memory = gCast<decltype(NuadeIo::read_url_to_memory)>(loader.read_url_to_memory),
            .free_url_to_memory = gCast<decltype(NuadeIo::free_url_to_memory)>(loader.free_url_to_memory)
        };

        const NuadePlayer* nuadePlayer = nullptr;
        if (auto* settings = metadata.Find<Settings>(); settings && settings->overridePlayer)
        {
            for (uint32_t i = 1; i < ms_players.NumItems(); i++)
            {
                if (ms_players[i].second == settings->data[0])
                {
                    nuadePlayer = nuade_player_get(ms_players[i].first - 1);
                    break;
                }
            }
        }

        NuadeOpenParams nuadeParams = {
            .path = filename.c_str(),
            .data = data.Items(),
            .size  = data.NumItems(),
            .io = &nuadeIO,
            .player = nuadePlayer
        };

        if (auto res = nuade_open_ex(&nuadeParams); res.song)
        {
            std::string extension;
            if (loader.subsongNames.IsEmpty())
            {
                auto* player = nuade_player(res.song);
                if (auto* tool = nuade_meta(res.song, NUADE_META_TOOL))
                {
                    if (strcmp(player->id, "davelowe") == 0)
                    {
                        if (strcmp(tool, "game driver") == 0)
                            extension = "dl";
                        else if (strcmp(tool, "Deli module") == 0)
                            extension = "dl_deli";
                        else
                            extension = "dln";
                    }

                }
                else if (auto* format = nuade_meta(res.song, NUADE_META_FORMAT))
                {
                    if (strcmp(format, "Digital Mugician 2") == 0)
                        extension = "dmu2";
                    else if (strcmp(format, "Future Composer 1.0-1.3") == 0)
                        extension = "smod";
                    else if (strcmp(format, "MusicMaker V8") == 0)
                        extension = nuade_info(res.song)->display_voices == 4 ? "mm4" : "mm8";
                    else if (strcmp(format, "BP SoundMon 2.2") == 0)
                        extension = "bp3";
                }
                if (extension.empty()) for (auto* ext = player->extensions; *ext && *ext != ','; ++ext)
                    extension += *ext;
            }
            else
                extension = "smus";
            auto* replay = new ReplayNuade(res.song, nuade_open_ex(&nuadeParams).song, extension.c_str(), std::move(loader));
            replay->BuildDurations(metadata);
            return replay;
        }
        return nullptr;
    }

    bool ReplayNuade::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        const char* const panning[] = { "Default", "Amiga" };
        changed |= ImGui::Combo("Panning", &ms_panning, panning, NumItemsOf(panning));
        const char* const patterns[] = { "Disable", "Enable" };
        changed |= ImGui::Combo("Show Patterns", &ms_patterns, patterns, NumItemsOf(patterns));
        return changed;
    }

    void ReplayNuade::Settings::Edit(ReplayMetadataContext& context)
    {
        uint32_t numSubsongs = context.lastSubsongIndex + 1;
        const auto settingsSize = sizeof(Settings) + (numSubsongs + 1u) * sizeof(LoopInfo);
        auto* entry = new (_alloca(settingsSize)) Settings(numSubsongs);
        if (auto* oldEntry = context.metadata.Find<Settings>())
        {
            if (oldEntry->overridePlayer)
                entry->numEntries++;
            if (entry->NumSubsongs() == numSubsongs)
                memcpy(&entry->value, &oldEntry->value, oldEntry->numEntries * sizeof(uint32_t));
            else
                memcpy(&entry->value, &oldEntry->value, (oldEntry->overridePlayer + 1) * sizeof(uint32_t));
        }

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");
        ComboOverride("Panning", GETSET(entry, overridePanning), GETSET(entry, panning),
            ms_panning, "Panning: Default", "Panning: Amiga");

        bool isPlayerOverridden = entry->overridePlayer;
        int32_t playerIndex = 0;
        {
            if (isPlayerOverridden)
            {
                playerIndex = ms_players.FindIf<int32_t>([entry](auto& player)
                {
                    return player.second == entry->data[0];
                });
                if (playerIndex < 0)
                {
                    isPlayerOverridden = false;
                    playerIndex = 0;
                }
            }
            ImGui::PushID("Player");
            if (ImGui::Checkbox("##Checkbox", &isPlayerOverridden))
                playerIndex = 0;
            ImGui::SameLine();
            ImGui::BeginDisabled(!isPlayerOverridden);
            ImGui::SetNextItemWidth(-FLT_MIN);
            struct Label
            {
                static const char* Get(void* data, int idx)
                {
                    auto* This = reinterpret_cast<Label*>(data);
                    This->str = "Player: ";
                    This->str += idx == 0 ? "Default" : nuade_player_get(ms_players[idx - 1].first)->name;
                    return This->str.c_str();
                }
                std::string str;
            } label;
            ImGui::Combo("##Combo", &playerIndex, &Label::Get, &label, ms_players.NumItems());
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Applied only on reload");
            ImGui::EndDisabled();
            ImGui::PopID();
            if (uint32_t(isPlayerOverridden) != entry->overridePlayer)
            {
                memmove(entry->data + (isPlayerOverridden ? 1 : 0), entry->GetLoops(), numSubsongs * sizeof(LoopInfo));
                entry->numEntries += isPlayerOverridden ? 1 : -1;
                entry->overridePlayer = isPlayerOverridden;
            }
            if (isPlayerOverridden)
                entry->data[0] = ms_players[playerIndex].second;
        }

        bool isZero = Loops(context, entry->GetLoops(), numSubsongs, kDefaultSongDuration);
        if (isZero)
            entry->numEntries -= uint16_t(numSubsongs) * 2; // no need to save the loops

        context.metadata.Update(entry, isZero && entry->value == 0);
    }

    int32_t ReplayNuade::ms_stereoSeparation = 100;
    int32_t ReplayNuade::ms_surround = 1;
    int32_t ReplayNuade::ms_panning = 1;
    int32_t ReplayNuade::ms_patterns = 1;
    Array<std::pair<uint32_t, uint32_t>> ReplayNuade::ms_players;
    std::string ReplayNuade::ms_uadePath;

    ReplayNuade::~ReplayNuade()
    {
        nuade_close(m_songs[0]);
        nuade_close(m_songs[1]);
        delete[] m_panning;
    }

    ReplayNuade::ReplayNuade(NuadeSong* song1, NuadeSong* song2, const char* extension, Loader&& loader)
        : Replay(extension, eReplay::Nuade)
        , m_surround(kSampleRate)
        , m_songs{ song1, song2 }
        , m_panning(new float[nuade_info(m_songs[0])->voice_count])
        , m_loader(loader.subsongNames.IsNotEmpty() ? std::move(loader) : Loader())
    {
        nuade_pan(m_songs[0], m_panning, nullptr);
    }

    uint32_t ReplayNuade::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_hasLooped)
        {
            m_hasLooped = false;
            return 0;
        }

        auto currentDuration = m_currentDuration;
        auto currentPosition = m_currentPosition;
        if (currentDuration != 0 && (currentPosition + numSamples) >= currentDuration)
        {
            numSamples = currentPosition < currentDuration ? uint32_t(currentDuration - currentPosition) : 0;
            if (numSamples == 0)
            {
                m_currentPosition = 0;
                m_currentDuration = (uint64_t(m_loops[m_subsongIndex].length) * kSampleRate) / 1000;
                return 0;
            }
        }
        m_currentPosition = currentPosition + numSamples;


        auto res = nuade_render(m_songs[0], pCast<float>(output), nullptr, numSamples);
        m_hasLooped = res.flags && res.frames && currentDuration == 0;

        output->Convert(m_surround, res.frames, m_stereoSeparation);

        return res.frames;
    }

    uint32_t ReplayNuade::Seek(uint32_t timeInMs)
    {
        nuade_seek(m_songs[0], (timeInMs * uint64_t(kSampleRate)) / 1000ull);
        nuade_seek(m_songs[1], (timeInMs * uint64_t(kSampleRate)) / 1000ull);
        return uint32_t((nuade_position(m_songs[0]) * 1000ull) / uint64_t(kSampleRate));
    }

    void ReplayNuade::ResetPlayback()
    {
        auto subsongIndex = m_subsongIndex;
        if (m_loader.subsongNames.IsNotEmpty())
        {
            if (m_lastSubsongIndex != subsongIndex)
            {
                m_lastSubsongIndex = subsongIndex;
                nuade_close(m_songs[0]);
                nuade_close(m_songs[1]);

                NuadeIo nuadeIO = {
                    .private_data = pCast<NuadeIoPrivate>(&m_loader),
                    .exists = gCast<decltype(NuadeIo::exists)>(m_loader.exists),
                    .read_url_to_memory = gCast<decltype(NuadeIo::read_url_to_memory)>(m_loader.read_url_to_memory),
                    .free_url_to_memory = gCast<decltype(NuadeIo::free_url_to_memory)>(m_loader.free_url_to_memory)
                };

                SmartPtr<io::Stream> stream;
                for (stream = m_loader.stream; stream; stream = stream->Next())
                {
                    std::string entryName = stream->GetName();
                    auto name = ToLower(entryName);
                    if (strstr(name.c_str(), ".smus") == (name.c_str() + name.size() - 5))
                    {
                        if (subsongIndex == 0)
                            break;
                        --subsongIndex;
                    }
                }
                auto data = stream->Read();

                std::string filename = m_loader.subsongNames[subsongIndex] + ".smus";
                NuadeOpenParams nuadeParams = {
                    .path = filename.c_str(),
                    .data = data.Items(),
                    .size = data.NumItems(),
                    .io = &nuadeIO,
                    .player = nullptr
                };

                m_songs[0] = nuade_open_ex(&nuadeParams).song;
                m_songs[1] = nuade_open_ex(&nuadeParams).song;
            }
            subsongIndex = 0;
        }

        m_hasLooped = false;
        for (auto* song : m_songs)
            nuade_start(song, subsongIndex, kSampleRate);
        for (uint32_t i = 0; i < nuade_info(m_songs[0])->voice_count; ++i)
            nuade_voice_set_pan(nullptr, 0, -1.0f);
        m_surround.Reset();
        m_currentPosition = 0;
        m_currentDuration = (uint64_t(m_loops[subsongIndex].GetDuration()) * kSampleRate) / 1000;
        PreparePatterns();
    }

    void ReplayNuade::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
        if ((settings && settings->overridePanning) ? settings->panning : ms_panning)
        {
            auto panning = pCast<float>(_alloca(nuade_info(m_songs[0])->voice_count * sizeof(float)));
            for (uint32_t i = 0; i < nuade_info(m_songs[0])->voice_count; ++i)
                panning[i] = ((i + 1) & 2) ? 1.0f : -1.0f;
            nuade_pan(m_songs[0], nullptr, panning);
        }
        else
            nuade_pan(m_songs[0], nullptr, m_panning);

        if (settings && settings->NumSubsongs())
        {
            auto loops = settings->GetLoops();
            for (uint32_t i = 0, e = settings->NumSubsongs(); i < e; i++)
                m_loops[i] = loops[i].GetFixed();
        }
        else
        {
            for (uint32_t i = 0, e = GetNumSubsongs(); i < e; i++)
                m_loops[i] = {};
        }
        m_currentDuration = (uint64_t(m_loops[m_subsongIndex].GetDuration()) * kSampleRate) / 1000;
    }

    void ReplayNuade::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
        // clear properties
        m_properties.Clear();
        // push empty property info
        auto* property = m_properties.Push();
        property->label = "Info";
        property->numColumns = 2;
        // push property instruments
        property = m_properties.Push();
        property->label = "Instruments";
        if (auto e = nuade_instrument_count(m_songs[0]))
        {
            property->numColumns = 1;
            for (uint32_t i = 0; i < e; ++i)
                property->Add(nuade_instrument_name(m_songs[0], i, nullptr), Property::kIsEditable);
        }
        else
            property->data.Add(0);
        // push property message
        property = m_properties.Push();
        property->label = "Message";
        if (auto* meta = nuade_meta(m_songs[0], NUADE_META_MESSAGE))
            property->data.Add(pcCast<uint8_t>(meta), uint32_t(strlen(meta) + 1));
        else
            property->data.Add(0);
    }

    uint32_t ReplayNuade::GetDurationMs() const
    {
        NuadeDuration duration;
        nuade_duration(m_songs[0], m_subsongIndex, 4096, 4096ull * 30ull * 60ull, &duration);
        return duration.capped ? 0 : uint32_t((duration.frames * 1000ull) / 4096ull);
    }

    uint32_t ReplayNuade::GetNumSubsongs() const
    {
        if (m_loader.subsongNames.IsNotEmpty())
            return m_loader.subsongNames.NumItems();
        return nuade_info(m_songs[0])->subsong_count;
    }

    std::string ReplayNuade::GetSubsongTitle() const
    {
        if (m_loader.subsongNames.IsEmpty())
            return {};
        return m_loader.subsongNames[m_subsongIndex];
    }
    std::string ReplayNuade::GetExtraInfo() const
    {
        std::string metadata;

        auto* meta = nuade_meta(m_songs[0], NUADE_META_TITLE);
        metadata = "Title : ";
        if (meta)
            metadata += meta;

        metadata += "\nArtist: ";
        meta = nuade_meta(m_songs[0], NUADE_META_ARTIST);
        if (meta)
            metadata += meta;

        meta = nuade_meta(m_songs[0], NUADE_META_MESSAGE);
        if (meta && meta[0])
        {
            metadata += "\n\n";
            metadata += meta;
        }

        for (uint32_t i = 0, e = nuade_instrument_count(m_songs[0]); i < e; ++i)
        {
            if (i == 0)
                metadata += "\n";
            metadata += "\n";
            if (auto* name = nuade_instrument_name(m_songs[0], i, nullptr))
                metadata += name;
        }

        return metadata;
    }

    std::string ReplayNuade::GetInfo() const
    {
        std::string info;
        info = "4 channels";
        if (m_surround.IsEnabled())
            info += " + surround";
        info += "\n";
        if (auto* format = nuade_meta(m_songs[0], NUADE_META_FORMAT))
            info += format;
        else
            info += nuade_player(m_songs[0])->name;
        if (auto* tool = nuade_meta(m_songs[0], NUADE_META_TOOL))
        {
            info += " / ";
            info += tool;
        }
        info += "\nNuade " NuadeVersion;
        return info;
    }

    const Replay::Properties& ReplayNuade::BuildProperties()
    {
        // build realtime basic info
        auto& property = m_properties[0];
        property.numEntries = 0;
        property.data.Clear();

        property.Add("Title", Property::kIsNotEditable, nuade_meta(m_songs[1], NUADE_META_TITLE), Property::kIsEditable);
        property.Add("Artist", Property::kIsNotEditable, nuade_meta(m_songs[1], NUADE_META_ARTIST), Property::kIsEditable);
        property.Add("Format", Property::kIsNotEditable, nuade_meta(m_songs[1], NUADE_META_FORMAT), Property::kIsEditable);
        property.Add("Tool", Property::kIsNotEditable, nuade_meta(m_songs[1], NUADE_META_TOOL), Property::kIsEditable);
        char buf[64];
        sprintf(buf, "%u", nuade_info(m_songs[1])->display_voices);
        property.Add("Channels", Property::kIsNotEditable, buf, Property::kIsNotEditable);

        NuadeSnapshotResult snap = nuade_snapshot(m_songs[1], nuade_position(m_songs[1]), nullptr, 0);
        if (snap.valid)
        {
            sprintf(buf, "%u", snap.snapshot.position.speed);
            property.Add("Speed", Property::kIsNotEditable, buf, Property::kIsNotEditable);
            sprintf(buf, "%u", snap.snapshot.position.tempo);
            property.Add("Tempo", Property::kIsNotEditable, buf, Property::kIsNotEditable);
            sprintf(buf, "%u", snap.snapshot.position.order);
            property.Add("Order", Property::kIsNotEditable, buf, Property::kIsNotEditable);
            sprintf(buf, "%u", snap.snapshot.position.pattern);
            property.Add("Pattern", Property::kIsNotEditable, buf, Property::kIsNotEditable);
            sprintf(buf, "%u", snap.snapshot.position.row);
            property.Add("Row", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        }

        return m_properties;
    }

    void ReplayNuade::PreparePatterns()
    {
        NuadePosition at = {};
        auto trackerInfoRes = nuade_tracker_info(m_songs[1], &at);
        if (!trackerInfoRes.valid)
        {
            m_patterns = {};
            return;
        }

        m_patterns.trackerInfo = trackerInfoRes.info;
        auto trackerInfo = trackerInfoRes.info;

        auto findColumn = [&](const char* label)
        {
            for (uint32_t i = 0; i < trackerInfo.column_count; ++i)
            {
                if (_stricmp(trackerInfo.columns[i].name, label) == 0)
                    return int8_t(i);
            }
            return int8_t(-1);
        };
        if (findColumn("note") == 0)
            m_patterns.noteWidth = trackerInfo.columns[0].octave_shift < 0 ? 4 : 3;
        else
        {
            m_patterns = {};
            return;
        }
        uint32_t numColumns = 1;
        if (findColumn("ins") == 1 || findColumn("inst") == 1 || findColumn("smp") == 1 || findColumn("cmd") == 1)
        {
            m_patterns.instWidth = trackerInfo.columns[1].digits;
            numColumns++;
        }
        else
            m_patterns.instWidth = 0;

        m_patterns.numEffects = uint8_t(trackerInfo.column_count - numColumns);
        // merge fx with arg?
        if (m_patterns.fxColumn = findColumn("fx"), m_patterns.argColumn = findColumn("arg"); m_patterns.fxColumn + 1 == m_patterns.argColumn)
            m_patterns.numEffects--;

        if (m_patterns.numEffects)
        {
            m_patterns.fxChars = 0;
            for (uint32_t i = m_patterns.instWidth ? 2 : 1; i < trackerInfo.column_count; ++i)
                m_patterns.fxChars += trackerInfo.columns[i].digits;
        }

        // trigger the cache the cells
        NuadePatternCell tempCells[128];
        nuade_tracker_cells(m_songs[0], &at, 0, 0, 16, tempCells, 128);
    }

    Replay::Patterns ReplayNuade::UpdatePatterns(uint32_t numSamples, uint32_t numLines, uint32_t charWidth, uint32_t spaceWidth, Patterns::Flags flags)
    {
            while (numSamples)
            {
                float output[1024 * 2];
                auto res = nuade_render(m_songs[1], output, nullptr, Min(numSamples, 1024u));
                numSamples -= res.frames;
                if (res.flags & NuadeTick_Ended)
                    break;
            }

        if (m_patterns.trackerInfo.channels == 0 || (!ms_patterns && !(flags & Patterns::kEnablePatterns))) // kEnablePatterns has priority over internal flag
            return {};

        auto* info = nuade_info(m_songs[1]);
        int32_t numChannels = info->display_voices;

        NuadeSnapshotResult snap = nuade_snapshot(m_songs[1], nuade_position(m_songs[1]), nullptr, 0);
        if (!snap.valid)
        {
            memset(&snap.snapshot.position, 0, sizeof(snap.snapshot.position));
            snap.snapshot.position.mode = NuadeScroll_Rows;
        }

        Patterns patterns;

        patterns.lines.Reserve(65536);
        patterns.sizes.Resize(numLines);
        patterns.currentLine = snap.snapshot.position.row;

        if (flags & (Patterns::kEnableInstruments | Patterns::kEnableEffects))
            patterns.width = (numChannels * (m_patterns.noteWidth + 1) - 1) * charWidth;
        else
            patterns.width = (numChannels * m_patterns.noteWidth) * charWidth + (numChannels - 1) * spaceWidth;
        uint16_t size = uint16_t(numChannels * (m_patterns.noteWidth + 1) - 1);
        int numRowNumbers = 0;
        if (flags & Patterns::kEnableRowNumbers && snap.snapshot.position.rows)
        {
            for (auto r = snap.snapshot.position.rows - 1;;)
            {
                numRowNumbers++;
                r /= 10;
                if (!r)
                    break;
            }

            patterns.width += (numRowNumbers + 1) * charWidth;
            size += uint16_t(numRowNumbers + 1);
        }
        if (flags & Patterns::kEnableInstruments && m_patterns.instWidth)
        {
            patterns.width += numChannels * m_patterns.instWidth * charWidth + spaceWidth * numChannels;
            size += uint16_t(numChannels * (m_patterns.instWidth + 1));
        }
        if (flags & Patterns::kEnableEffects && m_patterns.numEffects)
        {
            patterns.width += numChannels * m_patterns.fxChars * charWidth + spaceWidth * m_patterns.numEffects * numChannels;
            size += uint16_t(numChannels * (m_patterns.fxChars + m_patterns.numEffects));
        }

        auto numRows = int32_t(snap.snapshot.position.rows == 0 ? m_patterns.trackerInfo.rows : snap.snapshot.position.rows);
        auto currentRow = patterns.currentLine - int32_t(numLines) / 2;

        Array<NuadePatternCell> patternCells(numLines * numChannels);
        for (int32_t channel = 0; channel < numChannels; ++channel)
        {
            auto numCells = nuade_tracker_cells(m_songs[1], &snap.snapshot.position, channel, currentRow, currentRow + numLines, &patternCells[channel * numLines], numLines);
            assert(numCells == numLines);
        }

        auto printNumber = [&](NuadeColumnKind kind, int width, int value)
        {
            auto linePos = patterns.lines.Add<int>('\0', width);
            if (kind == NuadeColumn_Dec)
            {
                do
                {
                    patterns.lines[linePos + --width] = '0' + (value % 10);
                    value /= 10;
                } while (width);
            }
            else
            {
                static constexpr char hex[] = "0123456789ABCDEF";
                do
                {
                    patterns.lines[linePos + --width] = hex[value & 0xf];
                    value /= 16;
                } while (width);
            }
        };

        for (uint32_t line = 0; line < numLines; currentRow++, line++)
        {
            if (currentRow >= 0 && currentRow < numRows)
            {
                char colorBit = -128; // 0x80
                if (flags & Patterns::kEnableRowNumbers && numRowNumbers)
                {
                    if (flags & Patterns::kEnableColors)
                        patterns.lines.Add(colorBit | Patterns::kColorDefault);
                    printNumber(NuadeColumn_Dec, numRowNumbers, currentRow);
                    patterns.lines.Add('\x7f');
                }

                for (int32_t channel = 0;;)
                {
                    // Note
                    if (patternCells[line + channel * numLines].value[0] != NUADE_CELL_EMPTY)
                    {
                        char notename[8];
                        nuade_note_name(notename, patternCells[line + channel * numLines].value[0], m_patterns.trackerInfo.columns[0].octave_shift);

                        if (flags & Patterns::kEnableColors)
                            patterns.lines.Add(colorBit | (patternCells[line + channel * numLines].value[0] ? Patterns::kColorText : Patterns::kColorEmpty));
                        auto linePos = patterns.lines.Add<int>('\0', m_patterns.noteWidth + 1);
                        sprintf(&patterns.lines[linePos], m_patterns.noteWidth + 1, "%*s", m_patterns.noteWidth, notename);
                        for (uint8_t i = 0; i < m_patterns.noteWidth; ++i)
                            if (patterns.lines[linePos + i] == ' ')
                                patterns.lines[linePos + i] = '\x7f';
                        patterns.lines.Pop();
                    }
                    else
                    {
                        if (flags & Patterns::kEnableColors)
                            patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                        patterns.lines.Add('.', m_patterns.noteWidth);
                    }

                    if (flags & Patterns::kEnableInstruments && m_patterns.instWidth)
                    {
                        patterns.lines.Add(' ');
                        if (patternCells[line + channel * numLines].value[1] != NUADE_CELL_EMPTY)
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorInstrument);

                            printNumber(m_patterns.trackerInfo.columns[1].kind, m_patterns.instWidth, patternCells[line + channel * numLines].value[1]);
                        }
                        else
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                            patterns.lines.Add('.', m_patterns.instWidth);
                        }
                    }
                    if (flags & Patterns::kEnableEffects && m_patterns.numEffects)
                    {
                        for (uint32_t i = m_patterns.instWidth ? 2 : 1; i < m_patterns.trackerInfo.column_count; ++i)
                        {
                            patterns.lines.Add(' ');
                            auto& column = m_patterns.trackerInfo.columns[i];
                            if (patternCells[line + channel * numLines].value[i] != NUADE_CELL_EMPTY)
                            {
                                if (flags & Patterns::kEnableColors)
                                    patterns.lines.Add(colorBit | Patterns::kColorText);

                                printNumber(column.kind, column.digits, patternCells[line + channel * numLines].value[i]);
                            }
                            else
                            {
                                if (flags & Patterns::kEnableColors)
                                    patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                                patterns.lines.Add('.', column.digits);
                            }
                            if (int(i) == m_patterns.fxColumn && int(i + 1) == m_patterns.argColumn)
                            {
                                ++i;
                                auto& argColumn = m_patterns.trackerInfo.columns[i];
                                if (patternCells[line + channel * numLines].value[i] != NUADE_CELL_EMPTY)
                                {
                                    if (flags & Patterns::kEnableColors)
                                        patterns.lines.Add(colorBit | Patterns::kColorText);

                                    printNumber(argColumn.kind, argColumn.digits, patternCells[line + channel * numLines].value[i]);
                                }
                                else
                                {
                                    if (flags & Patterns::kEnableColors)
                                        patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                                    patterns.lines.Add('.', argColumn.digits);
                                }
                            }
                        }
                    }
                    if (++channel == numChannels)
                        break;
                    if (flags & (Patterns::kEnableInstruments | Patterns::kEnableEffects))
                        patterns.lines.Add('\x7f');
                    else
                        patterns.lines.Add(' ');
                }

                patterns.sizes[line] = size;
            }
            else
                patterns.sizes[line] = 0;
            patterns.lines.Add('\0');
        }

        return patterns;
    }

    void ReplayNuade::BuildPlayerNames()
    {
        ms_players.Reserve(nuade_player_count() + 1);
        ms_players.Add({ 0, 0 });
        for (uint32_t i = 0; i < nuade_player_count(); i++)
        {
            // http://www.isthe.com/chongo/tech/comp/fnv/
            constexpr uint32_t fnvOffset = 2166136261ul;
            constexpr uint32_t fnvPrime = 16777619ul;
            uint32_t hash = fnvOffset;
            for (auto* str = nuade_player_get(i)->id; *str; ++str)
                hash = static_cast<uint32_t>(static_cast<uint64_t>(hash ^ *str) * fnvPrime);

            ms_players.Add({ i + 1, hash });
        }
        std::sort(ms_players.begin() + 1, ms_players.end(), [](auto& l, auto& r)
        {
            return _stricmp(nuade_player_get(l.first - 1)->name, nuade_player_get(r.first - 1)->name) < 0;
        });
    }

    void ReplayNuade::BuildDurations(CommandBuffer metadata)
    {
        auto numSongs = GetNumSubsongs();
        m_loops = new LoopInfo[numSongs];
        auto settings = metadata.Find<Settings>();
        if (settings && settings->NumSubsongs() == numSongs)
        {
            auto* loops = settings->GetLoops();
            for (uint32_t i = 0; i < numSongs; i++)
                m_loops[i] = loops[i].GetFixed();
            m_currentDuration = (uint64_t(loops[m_subsongIndex].GetDuration()) * kSampleRate) / 1000;
        }
        else
        {
            for (uint16_t i = 0; i < numSongs; i++)
                m_loops[i] = {};
            m_currentDuration = 0;
        }
    }
}
// namespace rePlayer
// Some minor changes are done in FluidSynth:
// - src/midi/fluid_midi.c
#include "ReplayFluidSynth.h"

#include <Core/Window.inl.h>
#include <Imgui.h>
#include <IO/StreamFile.h>
#include <IO/StreamMemory.h>
#include <ReplayDll.h>

#include <fluid_sys.h>
#include <fluid_synth.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::FluidSynth,
        .name = "FluidSynth",
        .extensions = "mid",
        .about = "FluidSynth 2.5.3\nCopyright (c) 2003-2026 Peter Hanappe and others",
        .settings = "FluidSynth 2.5.3",
        .init = ReplayFluidSynth::Init,
        .load = ReplayFluidSynth::Load,
        .displaySettings = ReplayFluidSynth::DisplaySettings,
        .editMetadata = ReplayFluidSynth::Settings::Edit
    };

    bool ReplayFluidSynth::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_gain, "ReplayFluidSynthGain");
        window.RegisterSerializedData(ms_polyphony, "ReplayFluidSynthPolyphony");
        window.RegisterSerializedData(ms_soundfont, "ReplayFluidSynthSoundfont");

        return false;
    }

    Replay* ReplayFluidSynth::Load(io::Stream* stream, CommandBuffer metadata)
    {
        uint32_t id = 0;
        stream->Read(&id, sizeof(id));
        if (id != FLUID_FOURCC('M', 'T', 'h', 'd'))
            return nullptr;
        stream->Rewind();
        auto data = stream->Read();

        auto* settings = new_fluid_settings();
        auto* synth = new_fluid_synth(settings);

        int sfid = -1;
        if (auto* entry = metadata.Find<Settings>())
            sfid = fluid_synth_sfload(synth, entry->soundfont, 1);
        if (sfid < 0 && (ms_soundfont.empty() || fluid_synth_sfload(synth, ms_soundfont.c_str(), 1) < 0))
        {
            struct
            {
                static void* Open(const char* filename)
                {
                    if (strcmp(filename, "::DefaultBank") == 0)
                    {
                        if (ms_defaultSoundfont.IsEmpty())
                        {
                            ms_defaultSoundfont = g_replayPlugin.download("https://musical-artifacts.com/artifacts/1481/Phoenix_MT-32.sf2");
                            if (ms_defaultSoundfont.IsEmpty())
                                return nullptr;
                        }
                        return io::StreamMemory::Create(filename, ms_defaultSoundfont.Items(), ms_defaultSoundfont.NumItems(), true).Detach();
                    }
                    return io::StreamFile::Create(filename).Detach();
                }
                static int Read(void* buf, fluid_long_long_t count, void* handle)
                {
                    return reinterpret_cast<io::Stream*>(handle)->Read(buf, count) == uint64_t(count) ? FLUID_OK : FLUID_FAILED;
                }
                static int Seek(void* handle, fluid_long_long_t offset, int origin)
                {
                    return reinterpret_cast<io::Stream*>(handle)->Seek(offset, io::Stream::SeekWhence(origin)) == Status::kOk ? FLUID_OK : FLUID_FAILED;
                }
                static fluid_long_long_t Tell(void* handle)
                {
                    return fluid_long_long_t(reinterpret_cast<io::Stream*>(handle)->GetPosition());
                }
                static int Close(void* handle)
                {
                    reinterpret_cast<io::Stream*>(handle)->Release();
                    return FLUID_OK;
                }

            } cb;

            fluid_sfloader_t* my_sfloader = new_fluid_defsfloader(settings);
            fluid_sfloader_set_callbacks(my_sfloader,
                cb.Open,
                cb.Read,
                cb.Seek,
                cb.Tell,
                cb.Close);
            fluid_synth_add_sfloader(synth, my_sfloader);

            if (fluid_synth_sfload(synth, "::DefaultBank", 1) < 0)
            {
                delete_fluid_synth(synth);
                delete_fluid_settings(settings);
                return nullptr;
            }
        }

        auto* player = new_fluid_player(synth);
        fluid_player_add_mem(player, data.Items(), data.NumItems());
        fluid_player_play(player);

        StereoSample s;
        fluid_synth_write_float(synth, 1, &s, 0, 2, &s, 1, 2);
        if (fluid_player_get_status(player) != FLUID_PLAYER_PLAYING)
        {
            // something wrong with the midi data?
            delete_fluid_player(player);
            delete_fluid_synth(synth);
            delete_fluid_settings(settings);
            return nullptr;
        }

        return new ReplayFluidSynth(settings, synth, player);
    }

    bool ReplayFluidSynth::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderFloat("Gain", &ms_gain, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::SliderInt("Polyphony", &ms_polyphony, 1, 65535, "%d voices", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::InputText("Soundfont", &ms_soundfont);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            ImGui::Tooltip("Set the current soundfont (sf2)");
        return changed;
    }

    void ReplayFluidSynth::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        // use sign as enabled value
        bool gainIsEnabled = entry->uGain & 0x80000000;
        entry->uGain &= 0x7fFFffFF;
        float gain = *(float*)&entry->uGain;
        SliderOverride("Gain", GetSet([&]() { return gainIsEnabled; }, [&](auto v) { gainIsEnabled = v; })
            , GetSet([&]() { return gain; }, [&](auto v) { gain = v; })
            , ms_gain, 0.0f, 10.0f, "Gain %.2f");

        if (gainIsEnabled)
            entry->uGain = 0x80000000 | *(uint32_t*)&gain;
        else
            entry->uGain = 0;

        SliderOverride("Polyphony", GetSet([entry](){ return entry->polyphony != 0; }, [&](auto v) { entry->polyphony = v == false ?  0 : ms_polyphony; })
            , GetSet([&]() { return entry->polyphony != 0 ? entry->polyphony : ms_polyphony; }, [&](auto v) {  entry->polyphony = v; })
            , ms_polyphony, 1, 65535, "Polyphony voices %d");

        struct  
        {
            static int InputTextCallback(ImGuiInputTextCallbackData* data)
            {
                auto* str = (std::string*)data->UserData;
                if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                {
                    str->resize(data->BufTextLen);
                    data->Buf = (char*)str->c_str();
                }
                return 0;
            }
        } cb;
        std::string str = entry->soundfont;
        ImGui::TextUnformatted("Soundfont");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##Soundfont", (char*)str.c_str(), str.capacity() + 1, ImGuiInputTextFlags_CallbackResize, cb.InputTextCallback, &str);
        if (!str.empty())
        {
            auto newNumEntries = (Max(0, str.size() - 1) + sizeof(uint32_t) - 1) / sizeof(uint32_t);
            auto* newEntry = new (_alloca(sizeof(Settings) + newNumEntries * sizeof(uint32_t))) Settings();
            newEntry->numEntries += uint16_t(newNumEntries);
            newEntry->gain = entry->gain;
            newEntry->polyphony = entry->polyphony;
            memcpy(newEntry->soundfont, str.c_str(), str.size() + 1);
            auto* c = newEntry->soundfont + str.size() + 1;
            auto* e = newEntry->soundfont + 2 + newNumEntries * sizeof(uint32_t);
            while (c != e)
                *c++ = 0;
            context.metadata.Update(newEntry, false);
        }
        else
            context.metadata.Update(entry, entry->gain == 0 && entry->polyphony == 0);
    }

    float ReplayFluidSynth::ms_gain = 0.5f;
    int ReplayFluidSynth::ms_polyphony = 256;
    std::string ReplayFluidSynth::ms_soundfont;
    Array<uint8_t> ReplayFluidSynth::ms_defaultSoundfont;

    ReplayFluidSynth::~ReplayFluidSynth()
    {
        fluid_player_stop(m_fs.player);
        delete_fluid_player(m_fs.player);
        delete_fluid_synth(m_fs.synth);
        delete_fluid_settings(m_fs.settings);
    }

    ReplayFluidSynth::ReplayFluidSynth(fluid_settings_t* settings, fluid_synth_t* synth, fluid_player_t* player)
        : Replay(eExtension::_mid, eReplay::FluidSynth)
        , m_fs{
            .settings = settings,
            .synth = synth,
            .player = player
        }
    {
        for (int i = 0; i < m_fs.player->ntracks; ++i)
        {
            if (m_fs.player->track[i]->name)
            {
                if (!m_info.empty())
                    m_info += "\n";
                m_info += m_fs.player->track[i]->name;
            }
            auto evt = m_fs.player->track[i]->first;
            while (evt)
            {
                if (evt->type == MIDI_LYRIC || evt->type == MIDI_TEXT)
                {
                    if (!m_info.empty())
                        m_info += "\n";
                    m_info += (char*)evt->paramptr;
                }
                evt = evt->next;
            }
        }
    }

    uint32_t ReplayFluidSynth::Render(StereoSample* output, uint32_t numSamples)
    {
        if (fluid_player_get_status(m_fs.player) != FLUID_PLAYER_PLAYING)
            return 0;

        memset(output, 0, sizeof(StereoSample) * numSamples);
        fluid_synth_write_float(m_fs.synth, numSamples, output, 0, 2, output, 1, 2);

        return numSamples;
    }

    void ReplayFluidSynth::ResetPlayback()
    {
        StereoSample s;
        // hack: internal reset of the FluidSynth update
        m_fs.synth->cur = m_fs.synth->curmax = 0;
        // force update
        fluid_synth_write_float(m_fs.synth, 1, &s, 0, 2, &s, 1, 2);

        // stop and update
        fluid_player_stop(m_fs.player);
        m_fs.synth->cur = m_fs.synth->curmax = 0;
        fluid_synth_write_float(m_fs.synth, 1, &s, 0, 2, &s, 1, 2);

        // reset and restart
        fluid_player_seek(m_fs.player, 0);
        fluid_player_play(m_fs.player);
        m_fs.synth->cur = m_fs.synth->curmax = 0;
    }

    void ReplayFluidSynth::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        fluid_settings_setnum(m_fs.settings, "synth.gain", (settings && settings->gain) ? -settings->gain : ms_gain);
        fluid_settings_setnum(m_fs.settings, "synth.polyphony", (settings && settings->polyphony) ? settings->polyphony : ms_polyphony);
    }

    void ReplayFluidSynth::SetSubsong(uint32_t subsongIndex)
    {
        UnusedArg(subsongIndex);
        ResetPlayback();
    }

    uint32_t ReplayFluidSynth::GetDurationMs() const
    {
        // ugly rendering
        StereoSample s[512];
        while (fluid_player_get_status(m_fs.player) == FLUID_PLAYER_PLAYING)
            fluid_synth_write_float(m_fs.synth, 512, s, 0, 2, s, 1, 2);
        return m_fs.player->cur_msec;
    }

    uint32_t ReplayFluidSynth::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayFluidSynth::GetExtraInfo() const
    {
        return m_info;
    }

    std::string ReplayFluidSynth::GetInfo() const
    {
        std::string info;
        char buf[16];
        sprintf(buf, "%d channel%c", m_fs.player->ntracks, m_fs.player->ntracks > 1 ? 's' : ' ');
        info = buf;
        info += "\nMIDI\nFluidSynth 2.5.3";
        return info;
    }
}
// namespace rePlayer
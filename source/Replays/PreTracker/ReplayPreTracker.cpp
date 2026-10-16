// some "minor" changes to make it compatible
// pretracker.c
// pretracker.h
// pretracker_internal.h
// pretracker_mixer.c
// pretracker_parse.c
// pretracker_sequencer.c
#include "ReplayPreTracker.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::PreTracker,
        .name = "PreTracker",
        .extensions = "prt;pt",
        .about = "PreTracker 1.5\nCopyright (c) Original PreTracker format and player by Pink/Abyss\n"
            "Copyright (c) 2022 Amiga / 68k replayer Raspberry Casket by Chris 'platon42' Hodges\n"
            "Copyright (c) 2026 C port by Daniel Collin",
        .settings = "PreTracker 1.5",
        .init = ReplayPreTracker::Init,
        .load = ReplayPreTracker::Load,
        .displaySettings = ReplayPreTracker::DisplaySettings,
        .editMetadata = ReplayPreTracker::Settings::Edit
    };

    bool ReplayPreTracker::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayPreTrackerStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayPreTrackerSurround");
        window.RegisterSerializedData(ms_filter, "ReplayPreTrackerFilter");
        window.RegisterSerializedData(ms_surround, "ReplayPreTrackerPatterns");

        return false;
    }

    Replay* ReplayPreTracker::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        if (stream->GetSize() == 0)
            return nullptr;

        auto data = stream->Read();
        if (auto* song = pre_song_create(data.Items(), data.NumItems()))
            return new ReplayPreTracker(song, pre_song_create(data.Items(), data.NumItems()), data[3]);
		return nullptr;
    }

    bool ReplayPreTracker::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        const char* const filter[] = { "Default", "Sinc" };
        changed |= ImGui::Combo("Filter", &ms_filter, filter, NumItemsOf(filter));
        const char* const patterns[] = { "Disable", "Enable" };
        changed |= ImGui::Combo("Show Patterns", &ms_patterns, patterns, NumItemsOf(patterns));
        return changed;
    }

    void ReplayPreTracker::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find<Settings>(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");
        ComboOverride("Filter", GETSET(entry, overrideFilter), GETSET(entry, filter),
            ms_filter, "Default Filter", "Sinc filter");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplayPreTracker::ms_stereoSeparation = 100;
    int32_t ReplayPreTracker::ms_surround = 1;
    int32_t ReplayPreTracker::ms_filter = 0;
    int32_t ReplayPreTracker::ms_patterns = 1;

    ReplayPreTracker::~ReplayPreTracker()
    {
        pre_song_destroy(m_songs[0]);
        pre_song_destroy(m_songs[1]);
    }

    ReplayPreTracker::ReplayPreTracker(PreSong* song1, PreSong* song2, const char version)
        : Replay(eExtension::_prt, eReplay::PreTracker)
        , m_surround(kSampleRate)
        , m_songs{ song1, song2 }
        , m_type("\nPRT 0x")
    {
        char buf[8];
        sprintf(buf, "%02x", version);
        m_type += buf;
        m_type += "\nPreTracker 1.5";
    }

    uint32_t ReplayPreTracker::Render(StereoSample* output, uint32_t numSamples)
    {
        auto framesWritten = uint32_t(pre_song_decode(m_songs[0], pCast<float>(output), (int)numSamples));

        output->Convert(m_surround, framesWritten, m_stereoSeparation);

        return framesWritten;
    }

    void ReplayPreTracker::ResetPlayback()
    {
        for (auto* song : m_songs)
		{
			pre_song_set_subsong(song, int(m_subsongIndex));
			pre_song_set_sample_rate(song, kSampleRate);
			pre_song_start(song);
		}
        m_surround.Reset();
    }

    void ReplayPreTracker::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);

        pre_song_set_interp_mode(m_songs[0], PreInterpMode((settings && settings->overrideFilter) ? settings->filter : ms_filter));
    }

    void ReplayPreTracker::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
        // clear properties
        m_properties.Clear();
        // push empty property info
        auto* property = m_properties.Push();
        property->label = "Info";
        property->numColumns = 2;
        // push property waves
        property = m_properties.Push();
        property->label = "Waves";
        property->numColumns = 1;
        auto* info = pre_song_get_metadata(m_songs[0]);
        for (int i = 0; i < info->num_waves; ++i)
            property->Add(info->wave_names[i], Property::kIsEditable);
        // push property instruments
        property = m_properties.Push();
        property->label = "Instruments";
        property->numColumns = 1;
        for (int i = 0; i < info->num_instruments; ++i)
            property->Add(info->instrument_names[i], Property::kIsEditable);
    }

    uint32_t ReplayPreTracker::GetDurationMs() const
    {
        return uint32_t((pre_song_get_length(m_songs[0]) * 1000ull) / kSampleRate); 
    }

    uint32_t ReplayPreTracker::GetNumSubsongs() const
    {
        return pre_song_get_metadata(m_songs[0])->num_subsongs;
    }

    std::string ReplayPreTracker::GetExtraInfo() const
    {
        std::string metadata;

        auto* info = pre_song_get_metadata(m_songs[0]);
        metadata = "Name  : ";
        metadata += info->song_name;
        metadata += "\nAuthor: ";
        metadata += info->author;
        for (int i = 0; i < info->num_instruments; ++i)
        {
            metadata += "\n";
            metadata += info->instrument_names[i];
        }

        return metadata;
    }

    std::string ReplayPreTracker::GetInfo() const
    {
        std::string info;

        info = "4 channels";
        if (m_surround.IsEnabled())
            info += " + surround";
        info += m_type;

        return info;
    }

    const Replay::Properties& ReplayPreTracker::BuildProperties()
    {
        // build realtime basic info
        auto& property = m_properties[0];
        property.numEntries = 0;
        property.data.Clear();

        auto* info = pre_song_get_metadata(m_songs[1]);

        property.Add("Title", Property::kIsNotEditable, info->song_name, Property::kIsEditable);
        property.Add("Author", Property::kIsNotEditable, info->author, Property::kIsEditable);
        property.Add("Channels", Property::kIsNotEditable, "4", Property::kIsNotEditable);

        auto* state = pre_song_get_playback_state(m_songs[1]);

        char buf[16];
        sprintf(buf, "%u", state->speed);
        property.Add("Speed", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        sprintf(buf, "%u", state->position);
        property.Add("Order", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        sprintf(buf, "%u", state->row);
        property.Add("Row", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        for (int i = 0; i < 4; ++i)
        {
            std::string patterns;
            sprintf(buf, "T:%2u", state->channels[i].track_num);
            patterns += buf;
            sprintf(buf, ", P:%3d", state->channels[i].pitch_shift);
            patterns += buf;
            sprintf(buf, ", I:%2u", state->channels[i].instrument);
            patterns += buf;
            sprintf(buf, ", V:%2u", state->channels[i].volume);
            patterns += buf;
            sprintf(buf, ", W:%2u, ", state->channels[i].wave);
            patterns += buf;
            const char* adsr[4] = { "attack", "decay", "sustain", "release" };
            patterns += adsr[state->channels[i].adsr_phase];
            sprintf(buf, "Channel %d", i);
            property.Add(buf, Property::kIsNotEditable, patterns.c_str(), Property::kIsNotEditable);
        }

        return m_properties;
    }

    Replay::Patterns ReplayPreTracker::UpdatePatterns(uint32_t numSamples, uint32_t numLines, uint32_t charWidth, uint32_t spaceWidth, Patterns::Flags flags)
    {
        pre_song_decode_muted(m_songs[1], numSamples);

        if (!ms_patterns && !(flags & Patterns::kEnablePatterns)) // kEnablePatterns has priority over internal flag
            return {};

        auto* info = pre_song_get_metadata(m_songs[1]);
        auto* state = pre_song_get_playback_state(m_songs[1]);
        int32_t numChannels = 4;

        Patterns patterns;
        patterns.lines.Reserve(65536);
        patterns.sizes.Resize(numLines);
        patterns.currentLine = state->row;
        if (flags & (Patterns::kEnableInstruments | Patterns::kEnableEffects))
            patterns.width = (numChannels * 4 - 1) * charWidth;
        else
            patterns.width = (numChannels * 3) * charWidth + (numChannels - 1) * spaceWidth;
        uint16_t size = uint16_t(numChannels * 4 - 1);
        if (flags & Patterns::kEnableRowNumbers)
        {
            patterns.width += 3 * charWidth;
            size += uint16_t(3);
        }
        if (flags & Patterns::kEnableInstruments)
        {
            patterns.width += numChannels * 2 * charWidth + spaceWidth * numChannels;
            size += uint16_t(numChannels * 3);
        }
        if (flags & Patterns::kEnableEffects)
        {
            patterns.width += numChannels * 3 * charWidth + spaceWidth * numChannels;
            size += uint16_t(numChannels * 4);
        }

        int32_t numRows = info->num_steps;
        auto currentRow = patterns.currentLine - int32_t(numLines) / 2;

        for (uint32_t line = 0; line < numLines; currentRow++, line++)
        {
            if (currentRow >= 0 && currentRow < numRows)
            {
                char colorBit = -128; // 0x80
                if (flags & Patterns::kEnableRowNumbers)
                {
                    if (flags & Patterns::kEnableColors)
                        patterns.lines.Add(colorBit | Patterns::kColorDefault);
                    patterns.lines.Add('0' + ((currentRow / 10) % 10));
                    patterns.lines.Add('0' + (currentRow % 10));
                    patterns.lines.Add('\x7f');
                }

                for (int32_t channel = 0;;)
                {
                    uint8_t track = 0;
                    int8_t pitch_shift = 0;
                    PreTrackCell source = { 0 };

                    bool has_cell = pre_song_get_position_entry(m_songs[1], state->position, (uint8_t)channel, &track, &pitch_shift)
                        && pre_song_get_track_cell(m_songs[1], track, (uint8_t)currentRow, &source);

                    char notename[4] = "...";
                    if (has_cell)
					{
						static const char* names[12] = { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };
						if (source.note == 0x3d)
						{
							notename[0] = notename[1] = notename[2] = '=';
						}
						else if(source.note != 0)
						{
							int value = source.note - 1;
							sprintf(notename, "%s%d", names[value % 12], value / 12 + 1);
						}
					}

                    if (flags & Patterns::kEnableColors)
                    {
                        patterns.lines.Add(colorBit | (source.note ? Patterns::kColorText : Patterns::kColorEmpty));
                    }
                    patterns.lines.Add(notename, 3);
                    if (flags & Patterns::kEnableInstruments)
                    {
                        patterns.lines.Add(' ');
                        if (has_cell && source.instrument)
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorInstrument);
                            patterns.lines.Add('0' + ((source.instrument / 10) % 10));
                            patterns.lines.Add('0' + (source.instrument % 10));
                        }
                        else
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                            patterns.lines.Add("..", 2);
                        }
                    }
                    if (flags & Patterns::kEnableEffects)
                    {
                        if (has_cell && (source.has_arpeggio || source.effect_cmd != 0 || source.effect_data != 0))
                        {
                            if (flags & Patterns::kEnableColors)
                            {
                                if (source.has_arpeggio || source.effect_cmd == 1 || source.effect_cmd == 2 || source.effect_cmd == 3 || source.effect_cmd == 4)
                                    patterns.lines.Add(colorBit | Patterns::kColorPitch);
                                else if (source.effect_cmd == 0xb || source.effect_cmd == 0xd || source.effect_cmd == 0xf)
                                    patterns.lines.Add(colorBit | Patterns::kColorGlobal);
                                else if (source.effect_cmd == 0xa || source.effect_cmd == 0xc)
                                    patterns.lines.Add(colorBit | Patterns::kColorVolume);
                                else
                                    patterns.lines.Add(colorBit | Patterns::kColorText);
                            }
                            char buf[5];
                            sprintf(buf, " %01X%02X", source.has_arpeggio ? 0xA : source.effect_cmd, source.effect_data);
                            patterns.lines.Add(buf, 4);
                        }
                        else
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                            patterns.lines.Add(" ...", 4);
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
}
// namespace rePlayer
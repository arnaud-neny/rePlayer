// some "minor" changes to make it compatible
// aon_player.c
// aon_player.h
#include "ReplayArtOfNoise.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::ArtOfNoise,
        .name = "Art Of Noise",
        .extensions = "aon",
        .about = "Copyright (c) 2026 Daniel Collin"
            "Ported from NostalgicPlayer by Thomas Neumann",
        .settings = "Art Of Noise",
        .init = ReplayArtOfNoise::Init,
        .load = ReplayArtOfNoise::Load,
        .displaySettings = ReplayArtOfNoise::DisplaySettings,
        .editMetadata = ReplayArtOfNoise::Settings::Edit
    };

    bool ReplayArtOfNoise::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayArtOfNoiseStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayArtOfNoiseSurround");
        window.RegisterSerializedData(ms_patterns, "ReplayArtOfNoisePatterns");

        return false;
    }

    Replay* ReplayArtOfNoise::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        if (stream->GetSize() == 0)
            return nullptr;

        auto data = stream->Read();
        if (auto* song = aon_song_create(data.Items(), data.NumItems()))
            return new ReplayArtOfNoise(song, aon_song_create(data.Items(), data.NumItems()));
		return nullptr;
    }

    bool ReplayArtOfNoise::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        const char* const patterns[] = { "Disable", "Enable" };
        changed |= ImGui::Combo("Show Patterns", &ms_patterns, patterns, NumItemsOf(patterns));
        return changed;
    }

    void ReplayArtOfNoise::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find<Settings>(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplayArtOfNoise::ms_stereoSeparation = 100;
    int32_t ReplayArtOfNoise::ms_surround = 1;
    int32_t ReplayArtOfNoise::ms_patterns = 1;

    ReplayArtOfNoise::~ReplayArtOfNoise()
    {
        aon_song_destroy(m_songs[0]);
        aon_song_destroy(m_songs[1]);
    }

    ReplayArtOfNoise::ReplayArtOfNoise(AonSong* song1, AonSong* song2)
        : Replay(eExtension::_aon, eReplay::ArtOfNoise)
        , m_surround(kSampleRate)
        , m_songs{ song1, song2 }
    {}

    uint32_t ReplayArtOfNoise::Render(StereoSample* output, uint32_t numSamples)
    {
        auto framesWritten = uint32_t(aon_song_decode(m_songs[0], pCast<float>(output), (int)numSamples));

        output->Convert(m_surround, framesWritten, m_stereoSeparation);

        return framesWritten;
    }

    void ReplayArtOfNoise::ResetPlayback()
    {
        for (auto* song : m_songs)
		{
			aon_song_set_sample_rate(song, kSampleRate);
			aon_song_start(song);
		}
        m_surround.Reset();
    }

    void ReplayArtOfNoise::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
    }

    void ReplayArtOfNoise::SetSubsong(uint32_t subsongIndex)
    {
        UnusedArg(subsongIndex);
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
        property->numColumns = 1;
        auto* info = aon_song_get_metadata(m_songs[0]);
        for (int i = 0; i < info->num_instruments; ++i)
            property->Add(aon_song_get_instrument_name(m_songs[0], uint8_t(i)), Property::kIsEditable);
    }

    uint32_t ReplayArtOfNoise::GetDurationMs() const
    {
        return uint32_t((aon_song_get_length(m_songs[0]) * 1000ull) / kSampleRate);
    }

    uint32_t ReplayArtOfNoise::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayArtOfNoise::GetExtraInfo() const
    {
        std::string metadata;

        auto* info = aon_song_get_metadata(m_songs[0]);
        metadata = "Name  : ";
        metadata += info->song_name;
        metadata += "\nAuthor: ";
        metadata += info->author;
        for (int i = 0; i < info->num_instruments; ++i)
        {
            metadata += "\n";
            metadata += aon_song_get_instrument_name(m_songs[0], uint8_t(i));
        }

        return metadata;
    }

    std::string ReplayArtOfNoise::GetInfo() const
    {
        std::string info;
        info = aon_song_get_metadata(m_songs[0])->num_channels == 4 ? "4 channels" : "8 channels";
        if (m_surround.IsEnabled())
            info += " + surround";
        info += aon_song_get_metadata(m_songs[0])->num_channels == 4 ? "\nAON4\nArt Of Noise" : "\nAON8\nArt Of Noise";
        return info;
    }

    const Replay::Properties& ReplayArtOfNoise::BuildProperties()
    {
        // build realtime basic info
        auto& property = m_properties[0];
        property.numEntries = 0;
        property.data.Clear();

        auto* info = aon_song_get_metadata(m_songs[1]);

        property.Add("Title", Property::kIsNotEditable, info->song_name, Property::kIsEditable);
        property.Add("Author", Property::kIsNotEditable, info->author, Property::kIsEditable);
        char buf[64];
        sprintf(buf, "%u", aon_song_get_metadata(m_songs[1])->num_channels);
        property.Add("Channels", Property::kIsNotEditable, buf, Property::kIsNotEditable);

        auto* state = aon_song_get_playback_state(m_songs[1]);

        sprintf(buf, "%u", state->speed);
        property.Add("Speed", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        sprintf(buf, "%u", state->tempo);
        property.Add("Tempo", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        sprintf(buf, "%u", state->position);
        property.Add("Order", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        sprintf(buf, "%u", state->pattern);
        property.Add("Pattern", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        sprintf(buf, "%u", state->row);
        property.Add("Row", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        for (int i = 0; i < info->num_channels; ++i)
        {
            std::string patterns;
            sprintf(buf, "I:%2u", state->channels[i].instrument);
            patterns += buf;
            sprintf(buf, ", V:%2u.%3u.%2u", state->channels[i].volume, state->channels[i].synth_volume, state->channels[i].track_volume);
            patterns += buf;
            sprintf(buf, ", P:%4u", state->channels[i].period);
            patterns += buf;
            sprintf(buf, ", E:%2u.%3u, ", state->channels[i].effect, state->channels[i].effect_arg);
            patterns += buf;
            const char* adsr[3] = { "done", "add", "sub" };
            patterns += adsr[state->channels[i].envelope_phase];
            sprintf(buf, "Channel %d", i);
            property.Add(buf, Property::kIsNotEditable, patterns.c_str(), Property::kIsNotEditable);
        }

        return m_properties;
    }

    Replay::Patterns ReplayArtOfNoise::UpdatePatterns(uint32_t numSamples, uint32_t numLines, uint32_t charWidth, uint32_t spaceWidth, Patterns::Flags flags)
    {
        aon_song_decode_muted(m_songs[1], numSamples);

        if (!ms_patterns && !(flags & Patterns::kEnablePatterns)) // kEnablePatterns has priority over internal flag
            return {};

        auto* info = aon_song_get_metadata(m_songs[1]);
        auto* state = aon_song_get_playback_state(m_songs[1]);
        int32_t numChannels = info->num_channels;

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
            patterns.width += numChannels * 7 * charWidth + spaceWidth * numChannels;
            size += uint16_t(numChannels * 8);
        }

        int32_t numRows = AON_ROWS_PER_PATTERN;
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
                    AonPatternCell source = { 0 };

                    bool has_cell = aon_song_get_pattern_cell(m_songs[1], state->pattern, (uint8_t)currentRow, (uint8_t)channel, &source);

                    char notename[4] = "...";
                    if (has_cell)
					{
						static const char* names[12] = { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };
						if(source.note != 0)
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
                        if (has_cell && source.arpeggio)
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorText);
                            char buf[4];
                            sprintf(buf, " %02X", source.arpeggio);
                            patterns.lines.Add(buf, 3);
                        }
                        else
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                            patterns.lines.Add(" ..", 3);
                        }
                        if (has_cell && (source.effect != 0 || source.effect_arg != 0))
                        {
                            if (flags & Patterns::kEnableColors)
                            {
                                if (source.effect == 1 || source.effect == 2 || source.effect == 3 || source.effect == 4 || source.effect == 19 || source.effect == 21 || source.effect == 22)
                                    patterns.lines.Add(colorBit | Patterns::kColorPitch);
                                else if (source.effect == 11 || source.effect == 13 || source.effect == 15)
                                    patterns.lines.Add(colorBit | Patterns::kColorGlobal);
                                else if (source.effect == 5 || source.effect == 6 || source.effect == 10 || source.effect == 12 || source.effect == 16 || source.effect == 20 || source.effect == 25 || source.effect == 27 || source.effect == 28 || source.effect == 29)
                                    patterns.lines.Add(colorBit | Patterns::kColorVolume);
                                else
                                    patterns.lines.Add(colorBit | Patterns::kColorText);
                            }
                            char buf[6];
                            sprintf(buf, " %02X%02X", source.effect, source.effect_arg);
                            patterns.lines.Add(buf, 5);
                        }
                        else
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                            patterns.lines.Add(" ....", 5);
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
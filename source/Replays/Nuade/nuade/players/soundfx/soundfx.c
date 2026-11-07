// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/soundfx.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nuade_base.h"
#include "nuade_paula.h"

// A module without a delay value ticks at the VBlank rate.
#define DEFAULT_TICK_HZ 50

// Tables

static const int16_t note_table[]
    = { 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076, 1076,
        1076, 1076, 1076, 1016, 960,  906,  856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  453,
        428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,  214,  202,  190,  180,  170,  160,
        151,  143,  135,  127,  120,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,
        113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  -1

      };

// Reader

typedef struct SfxReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} SfxReader;

static void reader_init(SfxReader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

static bool reader_eof(const SfxReader* r) {
    return r->pos > r->size;
}

static uint8_t reader_read_uint8(SfxReader* r) {
    if (r->pos >= r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    return r->data[r->pos++];
}

static uint16_t reader_read_b_uint16(SfxReader* r) {
    uint8_t hi = reader_read_uint8(r);
    uint8_t lo = reader_read_uint8(r);
    return (uint16_t)((hi << 8) | lo);
}

static uint32_t reader_read_b_uint32(SfxReader* r) {
    uint16_t hi = reader_read_b_uint16(r);
    uint16_t lo = reader_read_b_uint16(r);
    return ((uint32_t)hi << 16) | lo;
}

static void reader_skip(SfxReader* r, size_t bytes) {
    r->pos += bytes;
}

static size_t reader_read(SfxReader* r, void* dst, size_t count) {
    size_t avail = 0;
    if (r->pos < r->size)
        avail = r->size - r->pos;
    if (count > avail)
        count = avail;
    if (count > 0) {
        memcpy(dst, r->data + r->pos, count);
        r->pos += count;
    }
    if (avail == 0 && count == 0)
        r->pos = r->size + 1;
    return count;
}

// Visited position tracking

static void clear_visited(SfxModule* m) {
    memset(m->visited, 0, sizeof(m->visited));
}

static void mark_position_as_visited(SfxModule* m, int pos) {
    if (pos >= 0 && pos < (int)(SFX_MAX_VISITED * 32))
        m->visited[pos / 32] |= (1u << (pos % 32));
}

static bool has_position_been_visited(const SfxModule* m, int pos) {
    if (pos < 0 || pos >= (int)(SFX_MAX_VISITED * 32))
        return false;
    return (m->visited[pos / 32] & (1u << (pos % 32))) != 0;
}

// IChannel emulation helpers

static void ch_play_sample(SfxChannel* ch, int8_t* sample_data, uint32_t length) {
    ch->ch_sample_data = sample_data;
    ch->ch_sample_length = length;
    ch->ch_loop_start = 0;
    ch->ch_loop_length = 0;
    ch->voice_action = SfxVoiceAction_Play;
}

static void ch_set_loop(SfxChannel* ch, uint32_t start_offset, uint32_t length) {
    ch->ch_loop_start = start_offset;
    ch->ch_loop_length = length;
    ch->ch_sample_length = start_offset + length;
}

static void ch_set_amiga_volume(SfxChannel* ch, uint16_t vol) {
    if (vol > 64)
        vol = 64;
    ch->ch_volume = vol;
}

static void ch_set_amiga_period(SfxChannel* ch, uint16_t period) {
    ch->ch_period = period;
}

static void ch_mute(SfxChannel* ch) {
    ch->voice_action = SfxVoiceAction_Stop;
}

// Playback methods

static void sfx_step_finder(SfxChannel* channel, bool step_down) {
    int16_t step_value = (int16_t)(channel->pattern_data & 0x0000000f);
    int16_t end_index = (int16_t)((channel->pattern_data & 0x000000f0) >> 4);

    if (step_down)
        step_value = (int16_t)-step_value;
    else
        end_index = (int16_t)-end_index;

    channel->step_note = channel->current_note;
    channel->step_value = step_value;

    // Find the period in the period table
    int note = 20;
    for (;;) {
        if (note_table[note] == -1) {
            channel->step_end_note = channel->current_note;
            return;
        }
        if (note_table[note] == channel->current_note)
            break;
        note++;
    }

    channel->step_end_note = (uint16_t)note_table[note + end_index];
}

static void sfx_arpeggio(SfxModule* m, SfxChannel* channel) {
    int16_t index;

    switch (m->playing_info.timer) {
        case 1:
        case 5:
            index = (int16_t)((channel->pattern_data & 0x000000f0) >> 4);
            break;
        case 2:
        case 4:
            index = (int16_t)(channel->pattern_data & 0x0000000f);
            break;
        default:
            ch_set_amiga_period(channel, channel->current_note);
            return;
    }

    // Got the index, now find the period
    int note = 20;
    for (;;) {
        if (note_table[note] == -1)
            return;
        if (note_table[note] == channel->current_note)
            break;
        note++;
    }

    ch_set_amiga_period(channel, (uint16_t)note_table[note + index]);
}

static void sfx_make_effects(SfxModule* m, SfxChannel* channel) {
    if (channel->step_value != 0) {
        if (channel->step_value < 0) {
            channel->step_note = (uint16_t)(channel->step_note + channel->step_value);

            if (channel->step_note <= channel->step_end_note) {
                channel->step_value = 0;
                channel->step_note = channel->step_end_note;
            }
        } else {
            channel->step_note = (uint16_t)(channel->step_note + channel->step_value);

            if (channel->step_note >= channel->step_end_note) {
                channel->step_value = 0;
                channel->step_note = channel->step_end_note;
            }
        }

        channel->current_note = channel->step_note;
        ch_set_amiga_period(channel, channel->current_note);
    } else {
        if (channel->slide_speed != 0) {
            uint16_t value = (uint16_t)(channel->slide_param & 0x0f);

            if (value != 0) {
                channel->slide_control++;
                if (channel->slide_control == value) {
                    channel->slide_control = 0;
                    value = (uint16_t)((channel->slide_param << 4) << 3);

                    if (!channel->slide_direction) {
                        channel->slide_period += 8;
                        value += channel->slide_speed;

                        if (value == channel->slide_period)
                            channel->slide_direction = true;
                    } else {
                        channel->slide_period -= 8;
                        value -= channel->slide_speed;

                        if (value == channel->slide_period)
                            channel->slide_direction = false;
                    }

                    channel->current_note = channel->slide_period;
                    ch_set_amiga_period(channel, channel->slide_period);
                }
            }
        }

        // Effects
        switch ((channel->pattern_data & 0x00000f00) >> 8) {
            case 1: // Arpeggio
                sfx_arpeggio(m, channel);
                break;

            case 2: { // Pitchbend
                uint16_t new_period;

                int16_t bend_value = (int16_t)((channel->pattern_data & 0x000000f0) >> 4);
                if (bend_value != 0)
                    new_period = (uint16_t)(((channel->pattern_data & 0xefff0000) >> 16) + bend_value);
                else {
                    bend_value = (int16_t)(channel->pattern_data & 0x0000000f);
                    if (bend_value != 0)
                        new_period = (uint16_t)(((channel->pattern_data & 0xefff0000) >> 16) - bend_value);
                    else
                        break;
                }

                ch_set_amiga_period(channel, new_period);

                channel->pattern_data = (channel->pattern_data & 0x1000ffff) | ((uint32_t)new_period << 16);
                break;
            }

            case 3: // LedOn (filter off!)
                break;

            case 4: // LedOff (filter on!)
                break;

            case 7: // SetStepUp
                sfx_step_finder(channel, false);
                break;

            case 8: // SetStepDown
                sfx_step_finder(channel, true);
                break;

            case 9: // Auto slide
                // No official Sound-FX replayer has this effect ("Alcatrash" by Kerni uses it). NostalgicPlayer's
                // implementation, ported here, is based on Flod 4.1 by Christian Corti.
                channel->slide_speed = channel->slide_period = channel->current_note;
                channel->slide_param = (uint16_t)(channel->pattern_data & 0x000000ff);
                channel->slide_direction = false;
                channel->slide_control = 0;
                break;
        }
    }
}

static void sfx_play_note(SfxModule* m, SfxChannel* channel, uint32_t pattern_data) {
    channel->pattern_data = pattern_data;

    // If there is a PIC command, don't parse the pattern data
    if ((pattern_data & 0xffff0000) != 0xfffd0000) {
        // Get the sample number
        uint8_t sample_num = (uint8_t)((pattern_data & 0x0000f000) >> 12);
        if ((pattern_data & 0x10000000) != 0)
            sample_num += 16;

        if (sample_num != 0) {
            SfxSample* cur_sample = &m->samples[sample_num - 1];
            channel->sample_number = sample_num;

            channel->sample = cur_sample->sample_addr;
            channel->sample_len = cur_sample->length;
            channel->volume = cur_sample->volume;
            channel->loop_start = cur_sample->loop_start;
            channel->loop_length = cur_sample->loop_length;

            int16_t volume = (int16_t)channel->volume;

            switch ((pattern_data & 0x00000f00) >> 8) {
                case 5: // Change volume up
                    volume += (int16_t)(pattern_data & 0x000000ff);
                    if (volume > 64)
                        volume = 64;
                    break;
                case 6: // Change volume down
                    volume -= (int16_t)(pattern_data & 0x000000ff);
                    if (volume < 0)
                        volume = 0;
                    break;
            }

            ch_set_amiga_volume(channel, (uint16_t)volume);
        }
    }

    // PIC command?
    if ((pattern_data & 0xffff0000) == 0xfffd0000) {
        channel->pattern_data &= 0xffff0000;
        return;
    }

    if ((pattern_data & 0xffff0000) == 0)
        return;

    // Stop sliding and stepping
    channel->slide_speed = 0;
    channel->step_value = 0;

    // Get the period
    channel->current_note = (uint16_t)(((pattern_data & 0xffff0000) >> 16) & 0xefff);

    // STP command?
    if ((pattern_data & 0xffff0000) == 0xfffe0000) {
        ch_mute(channel);
        return;
    }

    // BRK command?
    if (m->has_break && (pattern_data & 0xffff0000) == 0xfffc0000) {
        m->playing_info.break_flag = true;
        channel->pattern_data &= 0xefffffff;
        return;
    }

    // ??? command?
    if ((pattern_data & 0xffff0000) == 0xfffb0000) {
        channel->pattern_data &= 0xefffffff;
        return;
    }

    // Play the note
    if (channel->sample != nullptr) {
        channel->started_period = channel->current_note;
        channel->started_sample = channel->sample_number;
        ch_play_sample(channel, channel->sample, channel->sample_len);
        ch_set_amiga_period(channel, channel->current_note);

        if (channel->loop_length > 2)
            ch_set_loop(channel, channel->loop_start, channel->loop_length);
    }
}

static void sfx_play_sound(SfxModule* m) {
    uint32_t* pattern_adr = m->patterns[m->orders[m->playing_info.track_pos]];
    m->playing_info.shown_track_pos = m->playing_info.track_pos;
    m->playing_info.shown_row = m->playing_info.pos_counter / 4;

    sfx_play_note(m, &m->channels[0], pattern_adr[m->playing_info.pos_counter]);
    sfx_play_note(m, &m->channels[1], pattern_adr[m->playing_info.pos_counter + 1]);
    sfx_play_note(m, &m->channels[2], pattern_adr[m->playing_info.pos_counter + 2]);
    sfx_play_note(m, &m->channels[3], pattern_adr[m->playing_info.pos_counter + 3]);

    if (m->playing_info.break_flag) {
        m->playing_info.break_flag = false;
        m->playing_info.pos_counter = 4 * 63;
    }

    m->playing_info.pos_counter += 4;
    if (m->playing_info.pos_counter == 4 * 64) {
        m->playing_info.pos_counter = 0;
        m->playing_info.track_pos++;

        if (m->playing_info.track_pos == m->song_length)
            m->playing_info.track_pos = 0;

        if (has_position_been_visited(m, (int)m->playing_info.track_pos))
            m->end_reached = true;

        mark_position_as_visited(m, (int)m->playing_info.track_pos);
    }
}

static void play_tick(SfxModule* m) {
    m->playing_info.timer++;
    if (m->playing_info.timer == 6) {
        m->playing_info.timer = 0;
        sfx_play_sound(m);
    } else {
        sfx_make_effects(m, &m->channels[0]);
        sfx_make_effects(m, &m->channels[1]);
        sfx_make_effects(m, &m->channels[2]);
        sfx_make_effects(m, &m->channels[3]);
    }

    if (m->end_reached) {
        m->loop_count++;
        m->end_reached = false;
        mark_position_as_visited(m, (int)m->playing_info.track_pos);
    }
}

// Hand the registers the tick wrote to the voice. Voice n is Paula channel n.
static void sync_voice(SfxModule* m, uint32_t index) {
    SfxChannel* c = &m->channels[index];

    if (c->voice_action == SfxVoiceAction_Play && c->ch_sample_data != nullptr && c->ch_sample_length != 0) {
        const NuadeSample sample = { .data = c->ch_sample_data,
                                     .format = NuadeSampleFormat_S8,
                                     .length = c->ch_sample_length,
                                     .loop_start = c->ch_loop_start,
                                     .loop_length = c->ch_loop_length };
        nuade_voice_play(m->voices, index, &sample, 0);
    } else if (c->voice_action != SfxVoiceAction_None) {
        nuade_voice_stop(m->voices, index);
    }
    c->voice_action = SfxVoiceAction_None;

    const bool silent = c->ch_period == 0;
    nuade_voice_set_pitch(m->voices, index, silent ? 0.0 : nuade_paula_period_hz(c->ch_period));
    nuade_voice_set_volume(m->voices, index, silent ? 0.0f : (float)c->ch_volume / 64.0f);
    // Amiga panning: channels 0,3 = left; channels 1,2 = right.
    nuade_voice_set_pan(m->voices, index, (index == 0 || index == 3) ? -1.0f : 1.0f);
}

// Initialize sound

static void initialize_sound(SfxModule* m, int start_position) {
    m->playing_info.timer = 0;
    m->playing_info.track_pos = (uint32_t)start_position;
    m->playing_info.pos_counter = 0;
    m->playing_info.break_flag = false;
    m->playing_info.shown_track_pos = (uint32_t)start_position;
    m->playing_info.shown_row = 0;

    m->end_reached = false;
    m->loop_count = 0;

    for (int i = 0; i < 4; i++) {
        SfxChannel* ch = &m->channels[i];
        ch->pattern_data = 0;
        ch->sample = nullptr;
        ch->sample_len = 0;
        ch->loop_start = 0;
        ch->loop_length = 0;
        ch->current_note = 0;
        ch->sample_number = 0;
        ch->started_period = 0;
        ch->started_sample = 0;
        ch->volume = 0;
        ch->step_value = 0;
        ch->step_note = 0;
        ch->step_end_note = 0;
        ch->slide_control = 0;
        ch->slide_direction = false;
        ch->slide_param = 0;
        ch->slide_period = 0;
        ch->slide_speed = 0;

        ch->ch_sample_data = nullptr;
        ch->ch_sample_length = 0;
        ch->ch_loop_start = 0;
        ch->ch_loop_length = 0;
        ch->ch_volume = 0;
        ch->ch_period = 0;
        ch->voice_action = SfxVoiceAction_None;
    }
}

// Loading

static const SfxLayout layout_v1 = { 15, 60, "SONG", 0, "SoundFX 1.x", false };
static const SfxLayout layout_v2 = { 31, 124, "SO31", 4, "SoundFX 2.0", true };

static const SfxLayout* sfx_find_layout(const uint8_t* data, size_t size) {
    if (data == nullptr)
        return nullptr;
    const SfxLayout* all[] = { &layout_v2, &layout_v1 };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        const SfxLayout* l = all[i];
        // Mark, delay, pad, sample records, song info and order table must all fit.
        size_t header = l->mark_offset + 20 + (size_t)l->sample_count * 30 + 2 + 128 + l->extra_pattern_skip;
        if (size >= header && memcmp(data + l->mark_offset, l->mark, 4) == 0)
            return l;
    }
    return nullptr;
}

static bool sfx_load(SfxModule* m, const uint8_t* data, size_t size) {
    SfxReader reader;
    reader_init(&reader, data, size);

    const SfxLayout* layout = m->layout;
    const uint32_t sample_count = layout->sample_count;

    // Read the sample size table
    uint32_t sample_sizes[SFX_INSTRUMENTS];
    for (uint32_t i = 0; i < sample_count; i++)
        sample_sizes[i] = reader_read_b_uint32(&reader);

    // Skip the mark
    reader_skip(&reader, 4);

    // Read the delay value
    m->delay = reader_read_b_uint16(&reader);

    // Skip the pads
    reader_skip(&reader, 14);

    // Read the sample information
    uint32_t alloc_sizes[SFX_INSTRUMENTS];
    for (uint32_t i = 0; i < sample_count; i++) {
        SfxSample* sample = &m->samples[i];

        reader_read(&reader, sample->name, 22);
        sample->name[22] = '\0';

        uint32_t length = (uint32_t)reader_read_b_uint16(&reader) * 2;
        sample->volume = reader_read_b_uint16(&reader);
        uint32_t loop_start = reader_read_b_uint16(&reader);
        uint32_t loop_length = (uint32_t)reader_read_b_uint16(&reader) * 2;

        // Sample loop fix
        if ((loop_start + loop_length) > sample_sizes[i])
            loop_length = loop_start < sample_sizes[i] ? sample_sizes[i] - loop_start : 0;

        if ((length != 0) && (loop_start == length))
            length += loop_length;

        // Adjust the sample length
        if (length > 2) {
            uint32_t min_len = loop_start + loop_length;
            if (length < min_len)
                length = min_len;
        }

        // September by Allister Brimble uses a sample with 0 length, but contains some data
        if ((length == 0) && (sample_sizes[i] != 0))
            length = sample_sizes[i];

        // Volume fix
        if (sample->volume > 64)
            sample->volume = 64;

        sample->length = length;
        sample->loop_start = loop_start;
        sample->loop_length = loop_length;

        // The mixer reads up to the played length and the loop end, so the buffer covers both.
        alloc_sizes[i] = sample_sizes[i];
        if (alloc_sizes[i] < length)
            alloc_sizes[i] = length;
        if (alloc_sizes[i] < loop_start + loop_length)
            alloc_sizes[i] = loop_start + loop_length;

        if (reader_eof(&reader))
            return false;
    }

    // Read the song length
    m->song_length = reader_read_uint8(&reader);
    reader_skip(&reader, 1);
    if (m->song_length == 0 || m->song_length > sizeof(m->orders))
        return false;

    // Read the orders
    size_t bytes_read = reader_read(&reader, m->orders, sizeof(m->orders));
    if (bytes_read < sizeof(m->orders))
        return false;

    reader_skip(&reader, layout->extra_pattern_skip);

    // Find highest pattern number
    m->max_pattern = 0;
    for (uint32_t i = 0; i < m->song_length; i++) {
        if (m->orders[i] > m->max_pattern)
            m->max_pattern = m->orders[i];
    }
    m->max_pattern++;

    // Each pattern must be present in the file before it is allocated
    if (reader.pos > size || (size - reader.pos) / (4 * 64 * 4) < m->max_pattern)
        return false;

    // Allocate pattern array
    m->patterns = (uint32_t**)calloc(m->max_pattern, sizeof(uint32_t*));
    if (!m->patterns)
        return false;

    // Allocate and load patterns
    for (int i = 0; i < m->max_pattern; i++) {
        m->patterns[i] = (uint32_t*)calloc(4 * 64, sizeof(uint32_t));
        if (!m->patterns[i])
            return false;

        for (int j = 0; j < 4 * 64; j++)
            m->patterns[i][j] = reader_read_b_uint32(&reader);

        if (reader_eof(&reader))
            return false;
    }

    // Read sample data
    for (uint32_t i = 0; i < sample_count; i++) {
        uint32_t length = sample_sizes[i];
        if (length != 0) {
            size_t remaining = reader.pos < size ? size - reader.pos : 0;

            // Check to see if we miss too much from the last sample
            if (length > remaining + 512)
                return false;

            m->samples[i].sample_addr = (int8_t*)calloc(alloc_sizes[i], 1);
            if (!m->samples[i].sample_addr)
                return false;

            reader_read(&reader, m->samples[i].sample_addr, length);
        } else if (alloc_sizes[i] != 0) {
            m->samples[i].sample_addr = (int8_t*)calloc(alloc_sizes[i], 1);
            if (!m->samples[i].sample_addr)
                return false;
        }
    }

    return true;
}

// Public API

// The mark follows the sample size words: 15 for 1.x at offset 60, 31 for 2.0 at offset 124.
bool sfx_probe(const uint8_t* data, size_t size) {
    return sfx_find_layout(data, size) != nullptr;
}

SfxModule* sfx_create(const uint8_t* data, size_t size) {
    const SfxLayout* layout = sfx_find_layout(data, size);
    if (layout == nullptr)
        return nullptr;

    SfxModule* m = (SfxModule*)calloc(1, sizeof(SfxModule));
    if (!m)
        return nullptr;
    m->layout = layout;
    m->has_break = layout->has_break;

    // Keep original data for export
    m->original_data = (uint8_t*)malloc(size);
    if (m->original_data) {
        memcpy(m->original_data, data, size);
        m->original_size = size;
    }

    if (!sfx_load(m, data, size)) {
        sfx_destroy(m);
        return nullptr;
    }

    return m;
}

void sfx_destroy(SfxModule* module) {
    if (!module)
        return;

    if (module->patterns) {
        for (int i = 0; i < module->max_pattern; i++)
            free(module->patterns[i]);
        free(module->patterns);
    }

    for (int i = 0; i < 31; i++)
        free(module->samples[i].sample_addr);

    if (module->original_data)
        free(module->original_data);
    free(module);
}

int sfx_subsong_count(const SfxModule* module) {
    if (!module)
        return 0;
    return 1;
}

bool sfx_start(SfxModule* module, int subsong, NuadeVoices* voices) {
    if (!module || subsong != 0 || voices == nullptr)
        return false;

    module->voices = voices;
    clear_visited(module);
    initialize_sound(module, 0);
    return true;
}

int sfx_channel_count(const SfxModule* module) {
    (void)module;
    return SFX_CHANNELS;
}

uint32_t sfx_tick_length(const SfxModule* module) {
    return module->delay > 0 ? module->delay : SFX_TICK_HZ / DEFAULT_TICK_HZ;
}

bool sfx_tick(SfxModule* module) {
    const uint32_t loops = module->loop_count;
    play_tick(module);
    for (uint32_t i = 0; i < SFX_CHANNELS; i++)
        sync_voice(module, i);
    return module->loop_count != loops;
}

uint32_t sfx_loop_count(const SfxModule* module) {
    return module ? module->loop_count : 0;
}

const char* sfx_format_name(const SfxModule* module) {
    return module ? module->layout->format_name : "SoundFX";
}

const char* sfx_instrument_name(const SfxModule* module, int inst) {
    if (!module || inst < 0 || inst >= SFX_INSTRUMENTS)
        return "";
    return module->samples[inst].name;
}

// Edit API

int sfx_get_instrument_count(const SfxModule* module) {
    return module ? 31 : 0;
}

int sfx_get_num_patterns(const SfxModule* module) {
    return module ? (int)(module->max_pattern + 1) : 0;
}

SfxPosition sfx_position(const SfxModule* module) {
    return (SfxPosition) { .position = module->playing_info.shown_track_pos,
                           .row = module->playing_info.shown_row,
                           .speed = SFX_TICKS_PER_ROW };
}

uint32_t sfx_position_count(const SfxModule* module) {
    return module->song_length;
}

SfxVoiceState sfx_voice_state(const SfxModule* module, int channel) {
    const SfxChannel* c = &module->channels[channel];
    return (SfxVoiceState) { .period = c->started_period, .sample = c->started_sample };
}

static uint32_t pattern_word(const SfxModule* module, uint32_t position, uint32_t channel, uint32_t row) {
    return module->patterns[module->orders[position]][row * 4 + channel];
}

SfxCell sfx_pattern_cell(const SfxModule* module, int position, int channel, int row) {
    const uint32_t word = pattern_word(module, (uint32_t)position, (uint32_t)channel, (uint32_t)row);
    const uint16_t high = (uint16_t)(word >> 16);
    SfxCell cell = { .period = (uint16_t)(high & 0xefff),
                     .sample = (uint8_t)(((word & 0x0000f000) >> 12) | ((word & 0x10000000) != 0 ? 16 : 0)),
                     .effect = (uint8_t)((word & 0x00000f00) >> 8),
                     .effect_arg = (uint8_t)(word & 0x000000ff) };
    const bool special = high == SFX_SPECIAL_STOP || high == SFX_SPECIAL_PIC || high == SFX_SPECIAL_UNKNOWN
                         || (high == SFX_SPECIAL_BREAK && module->has_break);
    cell.special = special ? high : 0;
    return cell;
}

// A break in any channel ends the pattern after its row.
uint32_t sfx_pattern_rows(const SfxModule* module, int position) {
    if (module->has_break) {
        for (uint32_t row = 0; row < SFX_PATTERN_ROWS; row++) {
            for (uint32_t channel = 0; channel < SFX_CHANNELS; channel++) {
                if ((pattern_word(module, (uint32_t)position, channel, row) >> 16) == SFX_SPECIAL_BREAK) {
                    return row + 1;
                }
            }
        }
    }
    return SFX_PATTERN_ROWS;
}

void sfx_get_cell(const SfxModule* module, int pattern, int row, int channel, uint16_t* period, uint8_t* sample,
                  uint8_t* effect, uint8_t* effect_arg) {
    if (!module || pattern < 0 || pattern >= (int)module->max_pattern || row < 0 || row >= 64 || channel < 0
        || channel >= 4 || !module->patterns || !module->patterns[pattern]) {
        if (period)
            *period = 0;
        if (sample)
            *sample = 0;
        if (effect)
            *effect = 0;
        if (effect_arg)
            *effect_arg = 0;
        return;
    }
    uint32_t data = module->patterns[pattern][row * 4 + channel];
    // Packed format: bits 31-16 = period (bit 28 = sample hi bit), bits 15-12 = sample lo nibble,
    //                bits 11-8 = effect, bits 7-0 = effect_arg
    uint16_t per = (uint16_t)(((data & 0xEFFF0000) >> 16) & 0xEFFF);
    uint8_t smp = (uint8_t)(((data & 0x0000F000) >> 12) | (((data & 0x10000000) != 0) ? 16 : 0));
    uint8_t eff = (uint8_t)((data & 0x00000F00) >> 8);
    uint8_t arg = (uint8_t)(data & 0x000000FF);

    if (period)
        *period = per;
    if (sample)
        *sample = smp;
    if (effect)
        *effect = eff;
    if (effect_arg)
        *effect_arg = arg;
}

void sfx_set_cell(SfxModule* module, int pattern, int row, int channel, uint16_t period, uint8_t sample, uint8_t effect,
                  uint8_t effect_arg) {
    if (!module || pattern < 0 || pattern >= (int)module->max_pattern || row < 0 || row >= 64 || channel < 0
        || channel >= 4 || !module->patterns || !module->patterns[pattern])
        return;
    uint32_t data = 0;
    data |= ((uint32_t)(period & 0xEFFF)) << 16;
    if (sample >= 16)
        data |= 0x10000000;
    data |= ((uint32_t)(sample & 0x0F)) << 12;
    data |= ((uint32_t)(effect & 0x0F)) << 8;
    data |= (uint32_t)effect_arg;
    module->patterns[pattern][row * 4 + channel] = data;
}

float sfx_get_instrument_param(const SfxModule* module, int inst, const char* param) {
    if (!module || inst < 0 || inst >= 31 || !param)
        return -1.0f;
    const SfxSample* s = &module->samples[inst];

    if (strcmp(param, "length") == 0)
        return (float)s->length;
    if (strcmp(param, "volume") == 0)
        return (float)s->volume;
    if (strcmp(param, "loopStart") == 0)
        return (float)s->loop_start;
    if (strcmp(param, "loopLength") == 0)
        return (float)s->loop_length;

    return -1.0f;
}

void sfx_set_instrument_param(SfxModule* module, int inst, const char* param, float value) {
    if (!module || inst < 0 || inst >= 31 || !param)
        return;
    SfxSample* s = &module->samples[inst];

    if (strcmp(param, "length") == 0) {
        s->length = (uint32_t)value;
        return;
    }
    if (strcmp(param, "volume") == 0) {
        s->volume = (uint16_t)value;
        return;
    }
    if (strcmp(param, "loopStart") == 0) {
        s->loop_start = (uint32_t)value;
        return;
    }
    if (strcmp(param, "loopLength") == 0) {
        s->loop_length = (uint32_t)value;
        return;
    }
}

size_t sfx_export(const SfxModule* module, uint8_t* out, size_t max_size) {
    if (!module || !module->original_data)
        return 0;
    size_t total = module->original_size;
    if (!out)
        return total;
    if (max_size < total)
        return 0;
    memcpy(out, module->original_data, total);
    return total;
}

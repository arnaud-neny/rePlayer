// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/oktalyzer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nuade_base.h"
#include "nuade_paula.h"

// Tables

static const int16_t okt_periods[36]
    = { 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320,
        302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113

      };

// Panning: 0=left, 1=right
// L L R R R R L L
static const uint8_t okt_pan_pos[8] = { 0, 0, 1, 1, 1, 1, 0, 0 };

static const int8_t okt_arp10[16] = { 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0

};

static const int8_t okt_arp12[16] = { 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3

};

// Reader helpers

typedef struct OktReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} OktReader;

static void reader_init(OktReader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

static bool reader_eof(const OktReader* r) {
    return r->pos > r->size;
}

static void reader_skip(OktReader* r, size_t n) {
    // A skip past the end only marks end of file (pos + n could wrap a 32-bit size_t)
    r->pos = (r->pos <= r->size && n <= r->size - r->pos) ? r->pos + n : r->size + 1;
}

static void reader_seek(OktReader* r, size_t pos) {
    r->pos = pos;
}

static void reader_seek_end(OktReader* r) {
    r->pos = r->size;
}

static uint8_t reader_read_uint8(OktReader* r) {
    if (r->pos >= r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    return r->data[r->pos++];
}

static int8_t reader_read_int8(OktReader* r) {
    return (int8_t)reader_read_uint8(r);
}

static uint16_t reader_read_b_uint16(OktReader* r) {
    uint8_t hi = reader_read_uint8(r);
    uint8_t lo = reader_read_uint8(r);
    return (uint16_t)((hi << 8) | lo);
}

static uint32_t reader_read_b_uint32(OktReader* r) {
    uint8_t b0 = reader_read_uint8(r);
    uint8_t b1 = reader_read_uint8(r);
    uint8_t b2 = reader_read_uint8(r);
    uint8_t b3 = reader_read_uint8(r);
    return ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | b3;
}

static void reader_read_bytes(OktReader* r, uint8_t* out, size_t count) {
    for (size_t i = 0; i < count; i++)
        out[i] = reader_read_uint8(r);
}

static void reader_read_signed(OktReader* r, int8_t* out, size_t count) {
    for (size_t i = 0; i < count; i++)
        out[i] = reader_read_int8(r);
}

static void reader_read_mark(OktReader* r, char* out, size_t count) {
    for (size_t i = 0; i < count; i++)
        out[i] = (char)reader_read_uint8(r);
    out[count] = '\0';
}

// Channel register operations

static void channel_play_sample(OktChannel* c, int8_t* addr, uint32_t offset, uint32_t length) {
    c->sample_data = addr + offset;
    c->sample_length = length;
    c->loop_start = 0;
    c->loop_length = 0;
    c->play_pending = true;
    c->release_pending = false;
}

static void channel_set_loop(OktChannel* c, uint32_t start, uint32_t length) {
    c->loop_start = start;
    c->loop_length = length;
}

// Continue into the release part of the sample when the current pass ends; no retrigger.
static void channel_release(OktChannel* c) {
    c->release_pending = true;
}

// Position visited tracking

static void clear_visited(OktModule* m) {
    memset(m->visited, 0, sizeof(m->visited));
}

static void mark_position_visited(OktModule* m, int16_t pos) {
    if (pos >= 0 && pos < 128)
        m->visited[pos] = 1;
}

static bool has_position_been_visited(OktModule* m, int16_t pos) {
    if (pos >= 0 && pos < 128)
        return m->visited[pos] != 0;
    return false;
}

// PlayNote helper

static void play_note(OktModule* m, uint32_t channel_num, OktChannelInfo* chan_data, int8_t note) {
    // Check for out of bounds
    if (note < 0)
        note = 0;

    if (note > 35)
        note = 35;

    // Play the note
    chan_data->curr_period = okt_periods[note];
    m->channels[channel_num].period = (uint32_t)chan_data->curr_period;
}

// PlayChannel: parse pattern data for one channel

static void play_channel(OktModule* m, uint32_t channel_num) {
    // Get the pattern and channel data
    OktPatternLine* patt_data = &m->curr_line[channel_num];
    OktChannelInfo* chan_data = &m->chan_info[channel_num];

    // If we shouldn't play any note, well just return
    if (patt_data->note == 0)
        return;

    // Get the note number
    uint8_t note = (uint8_t)(patt_data->note - 1);

    // Does the instrument have a sample attached?
    if (patt_data->sample_num >= m->samp_num)
        return;
    OktSample* samp = &m->samples[patt_data->sample_num];
    if ((samp->sample_data == nullptr) || (samp->length == 0))
        return;

    // Well, find out if we are playing in a mixed or normal channel
    if (m->channel_flags[m->chan_index[channel_num]]) {
        // Mixed
        //
        // If the sample is mode "4", it won't be played
        if (samp->mode == 1)
            return;

        // Just play the sample. Samples doesn't loop in mixed channels
        channel_play_sample(&m->channels[channel_num], samp->sample_data, 0, samp->length);

        chan_data->release_start = 0;
        chan_data->release_length = 0;
    } else {
        // Normal
        //
        // If the sample is mode "8", it won't be played
        if (samp->mode == 0)
            return;

        // Set the channel volume
        m->chan_vol[m->chan_index[channel_num]] = (int8_t)samp->volume;

        // Does the sample loop?
        if (samp->repeat_length == 0) {
            // No
            channel_play_sample(&m->channels[channel_num], samp->sample_data, 0, samp->length);

            chan_data->release_start = 0;
            chan_data->release_length = 0;
        } else {
            // Yes
            channel_play_sample(&m->channels[channel_num], samp->sample_data, 0,
                                (uint32_t)samp->repeat_start + samp->repeat_length);
            channel_set_loop(&m->channels[channel_num], samp->repeat_start, samp->repeat_length);

            chan_data->release_start = (uint32_t)samp->repeat_start + samp->repeat_length;
            chan_data->release_length = samp->length - chan_data->release_start;
        }
    }

    // Find the period
    if (note > 35)
        note = 35;
    chan_data->curr_note = note;
    chan_data->shown_note = (uint8_t)(note + 1);
    chan_data->shown_sample = patt_data->sample_num;
    chan_data->curr_period = okt_periods[note];

    m->channels[channel_num].period = (uint32_t)chan_data->curr_period;
}

// DoChannelEffect

static void do_channel_effect(OktModule* m, uint32_t channel_num) {
    // Get the pattern and channel data
    OktPatternLine* patt_data = &m->curr_line[channel_num];
    OktChannelInfo* chan_data = &m->chan_info[channel_num];

    switch (patt_data->effect) {
        // Effect '1': Portamento down
        case 1: {
            chan_data->curr_period -= patt_data->effect_arg;
            if (chan_data->curr_period < 113)
                chan_data->curr_period = 113;

            m->channels[channel_num].period = (uint32_t)chan_data->curr_period;
            break;
        }

        // Effect '2': Portamento up
        case 2: {
            chan_data->curr_period += patt_data->effect_arg;
            if (chan_data->curr_period > 856)
                chan_data->curr_period = 856;

            m->channels[channel_num].period = (uint32_t)chan_data->curr_period;
            break;
        }

        // Effect 'A': Arpeggio type 1
        case 10: {
            int8_t work_note = (int8_t)chan_data->curr_note;
            // The table covers speeds up to 16; a damaged SPEE start speed wraps
            int8_t arp_num = okt_arp10[m->speed_counter & 15];

            switch (arp_num) {
                // Note - upper 4 bits
                case 0:
                    work_note -= (int8_t)((patt_data->effect_arg & 0xf0) >> 4);
                    break;

                // Note
                case 1:
                    break;

                // Note + lower 4 bits
                case 2:
                    work_note += (int8_t)(patt_data->effect_arg & 0x0f);
                    break;
            }

            play_note(m, channel_num, chan_data, work_note);
            break;
        }

        // Effect 'B': Arpeggio type 2
        case 11: {
            int8_t work_note = (int8_t)chan_data->curr_note;

            switch (m->speed_counter & 0x3) {
                // Note
                case 0:
                case 2:
                    break;

                // Note + lower 4 bits
                case 1:
                    work_note += (int8_t)(patt_data->effect_arg & 0x0f);
                    break;

                // Note - upper 4 bits
                case 3:
                    work_note -= (int8_t)((patt_data->effect_arg & 0xf0) >> 4);
                    break;
            }

            play_note(m, channel_num, chan_data, work_note);
            break;
        }

        // Effect 'C': Arpeggio type 3
        case 12: {
            int8_t work_note = (int8_t)chan_data->curr_note;
            // The table covers speeds up to 16; a damaged SPEE start speed wraps
            int8_t arp_num = okt_arp12[m->speed_counter & 15];

            if (arp_num == 0)
                break;

            switch (arp_num) {
                // Note - upper 4 bits
                case 1:
                    work_note -= (int8_t)((patt_data->effect_arg & 0xf0) >> 4);
                    break;

                // Note + lower 4 bits
                case 2:
                    work_note += (int8_t)(patt_data->effect_arg & 0x0f);
                    break;

                // Note
                case 3:
                    break;
            }

            play_note(m, channel_num, chan_data, work_note);
            break;
        }

        // Effect 'H': Increase note once per line
        case 17: {
            if (m->speed_counter != 0)
                break;

            // fall through to case 30
            chan_data->curr_note += patt_data->effect_arg;
            play_note(m, channel_num, chan_data, (int8_t)chan_data->curr_note);
            break;
        }

        // Effect 'U': Increase note once per tick
        case 30: {
            chan_data->curr_note += patt_data->effect_arg;
            play_note(m, channel_num, chan_data, (int8_t)chan_data->curr_note);
            break;
        }

        // Effect 'L': Decrease note once per line
        case 21: {
            if (m->speed_counter != 0)
                break;

            // fall through to case 13
            chan_data->curr_note -= patt_data->effect_arg;
            play_note(m, channel_num, chan_data, (int8_t)chan_data->curr_note);
            break;
        }

        // Effect 'D': Decrease note once per tick
        case 13: {
            chan_data->curr_note -= patt_data->effect_arg;
            play_note(m, channel_num, chan_data, (int8_t)chan_data->curr_note);
            break;
        }

        // Effect 'F': Filter control
        case 15: {
            if (m->speed_counter == 0)
                m->filter_status = patt_data->effect_arg != 0;
            break;
        }

        // Effect 'P': Position jump
        case 25: {
            if (m->speed_counter == 0) {
                uint16_t new_pos
                    = (uint16_t)(((patt_data->effect_arg & 0xf0) >> 4) * 10 + (patt_data->effect_arg & 0x0f));

                if (new_pos < m->song_length)
                    m->new_song_pos = (int16_t)new_pos;
            }
            break;
        }

        // Effect 'R': Release sample
        case 27: {
            if ((chan_data->release_start != 0) && (chan_data->release_length != 0))
                channel_release(&m->channels[channel_num]);
            break;
        }

        // Effect 'S': Set speed
        case 28: {
            if ((m->speed_counter == 0) && ((patt_data->effect_arg & 0xf) != 0)) {
                m->current_speed = (uint16_t)(patt_data->effect_arg & 0xf);
            }
            break;
        }

        // Effect 'O': Volume control with retrig
        case 24: {
            m->chan_vol[m->chan_index[channel_num]] = m->chan_vol[m->chan_index[channel_num] + 4];

            // fall through to case 31
        }
        /* FALLTHROUGH */

        // Effect 'V': Volume control
        case 31: {
            int vol_index = m->chan_index[channel_num];
            uint8_t eff_arg = patt_data->effect_arg;

            if (eff_arg <= 64) {
                // Set the volume
                m->chan_vol[vol_index] = (int8_t)eff_arg;
                break;
            }

            eff_arg -= 64;
            if (eff_arg < 16) {
                // Decrease the volume for every tick
                m->chan_vol[vol_index] -= (int8_t)eff_arg;
                if (m->chan_vol[vol_index] < 0)
                    m->chan_vol[vol_index] = 0;
                break;
            }

            eff_arg -= 16;
            if (eff_arg < 16) {
                // Increase the volume for every tick
                m->chan_vol[vol_index] += (int8_t)eff_arg;
                if (m->chan_vol[vol_index] > 64)
                    m->chan_vol[vol_index] = 64;
                break;
            }

            eff_arg -= 16;
            if (eff_arg < 16) {
                // Decrease the volume for every line
                if (m->speed_counter == 0) {
                    m->chan_vol[vol_index] -= (int8_t)eff_arg;
                    if (m->chan_vol[vol_index] < 0)
                        m->chan_vol[vol_index] = 0;
                }
                break;
            }

            eff_arg -= 16;
            if (eff_arg < 16) {
                // Increase the volume for every line
                if (m->speed_counter == 0) {
                    m->chan_vol[vol_index] += (int8_t)eff_arg;
                    if (m->chan_vol[vol_index] > 64)
                        m->chan_vol[vol_index] = 64;
                }
                break;
            }
            break;
        }
    }
}

// SetVolumes

static void set_volumes(OktModule* m) {
    // Start to copy the volumes
    m->chan_vol[4] = m->chan_vol[0];
    m->chan_vol[5] = m->chan_vol[1];
    m->chan_vol[6] = m->chan_vol[2];
    m->chan_vol[7] = m->chan_vol[3];

    // Now set the volume
    for (int i = 0, j = 0; i < 4; i++, j++) {
        m->channels[j].volume = (uint16_t)m->chan_vol[i];

        if (m->channel_flags[i])
            m->channels[++j].volume = (uint16_t)m->chan_vol[i];
    }
}

// DoEffects

static void do_effects(OktModule* m) {
    for (uint32_t i = 0, j = 0; i < 4; i++, j++) {
        do_channel_effect(m, j);

        if (m->channel_flags[i])
            do_channel_effect(m, ++j);
    }
}

// FindNextPatternLine

static void find_next_pattern_line(OktModule* m) {
    // Find the right pattern
    OktPattern* patt = &m->patterns[m->pattern_table[m->song_pos]];

    // Go to next pattern line
    m->patt_pos++;

    if ((m->patt_pos >= patt->line_num) || (m->new_song_pos != -1)) {
        // Okay, we're done with the current pattern. Find the next one
        m->patt_pos = 0;

        if (m->new_song_pos != -1) {
            m->song_pos = m->new_song_pos;
            m->new_song_pos = -1;
        } else
            m->song_pos++;

        if (m->song_pos == m->song_length)
            m->song_pos = 0;

        if (has_position_been_visited(m, m->song_pos))
            m->end_reached = true;

        mark_position_visited(m, m->song_pos);

        // Find the right pattern
        patt = &m->patterns[m->pattern_table[m->song_pos]];
    }

    // Copy the current line data
    for (int i = 0; i < m->chan_num; i++)
        m->curr_line[i] = patt->lines[m->patt_pos * m->chan_num + i];
}

// PlayPatternLine

static void play_pattern_line(OktModule* m) {
    for (uint32_t i = 0, j = 0; i < 4; i++, j++) {
        play_channel(m, j);

        if (m->channel_flags[i])
            play_channel(m, ++j);
    }
}

// Play tick

static void play_tick(OktModule* m) {
    // Wait until we need to play another pattern line
    m->speed_counter++;
    if (m->speed_counter >= m->current_speed) {
        // Play next pattern line
        m->speed_counter = 0;

        find_next_pattern_line(m);
        play_pattern_line(m);
    }

    // Do each frame stuff
    do_effects(m);
    set_volumes(m);

    // Have we reached the end of the module
    if (m->end_reached) {
        m->loop_count++;
        m->end_reached = false;
        mark_position_visited(m, m->song_pos);
    }
}

// Initialize sound

static void initialize_sound(OktModule* m, int start_position) {
    memset(m->chan_info, 0, sizeof(m->chan_info));
    memset(m->curr_line, 0, sizeof(m->curr_line));

    for (int i = 0; i < 8; i++)
        m->chan_vol[i] = 64;

    m->song_pos = (int16_t)start_position;
    m->new_song_pos = -1;
    m->patt_pos = -1;
    m->current_speed = m->start_speed;
    m->speed_counter = 0;
    m->filter_status = false;

    // Build chan_index
    memset(m->chan_index, 0, sizeof(m->chan_index));

    // Set the channel panning + create the channel index
    for (int i = 0, pan_num = 0; i < m->chan_num; i++, pan_num++) {
        m->channels[i].panning = okt_pan_pos[pan_num];
        m->chan_index[i] = (uint8_t)(pan_num / 2);

        if (!m->channel_flags[pan_num / 2])
            pan_num++;
    }

    m->end_reached = false;
    m->loop_count = 0;

    clear_visited(m);
    mark_position_visited(m, start_position);

    // Reset the channels; panning is set above
    for (int i = 0; i < 8; i++) {
        OktChannel* c = &m->channels[i];
        c->sample_data = nullptr;
        c->sample_length = 0;
        c->loop_start = 0;
        c->loop_length = 0;
        c->volume = 0;
        c->period = 0;
        c->play_pending = false;
        c->release_pending = false;
    }
}

// Loading

// The song must only name patterns that were loaded.
static bool validate_module(const OktModule* m) {
    if (m->song_length == 0 || m->song_length > 128 || m->chan_num == 0 || !m->patterns)
        return false;

    for (uint32_t i = 0; i < m->song_length; i++) {
        uint8_t patt = m->pattern_table[i];
        if (patt >= m->patt_num || !m->patterns[patt].lines || m->patterns[patt].line_num == 0)
            return false;
    }

    return true;
}

static bool load_module(OktModule* m, const uint8_t* data, size_t size) {
    OktReader reader;
    reader_init(&reader, data, size);

    // Skip the mark
    reader_seek(&reader, 8);

    // Initialize variables
    m->samp_num = 0;
    m->patt_num = 0;
    m->song_length = 0;
    m->start_speed = 6;

    uint32_t read_patt = 0;
    uint32_t read_samp = 0;
    uint32_t real_used_samp_num = 0;

    // Okay, now read each chunk and parse them
    for (;;) {
        // Read the chunk name and length
        char chunk_name[5];
        reader_read_mark(&reader, chunk_name, 4);
        uint32_t chunk_size = reader_read_b_uint32(&reader);

        // Do we have any chunks left?
        if (reader_eof(&reader))
            break;

        // Find out what the chunk is and begin to parse it
        if (strcmp(chunk_name, "CMOD") == 0) {
            // Channel modes
            if (chunk_size != 8)
                return false;

            if (m->patterns)
                return false; // channel layout must precede the pattern data
            m->chan_num = 4;
            for (int i = 0; i < 4; i++) {
                if (reader_read_b_uint16(&reader) == 0)
                    m->channel_flags[i] = false;
                else {
                    m->channel_flags[i] = true;
                    m->chan_num++;
                }
            }
        } else if (strcmp(chunk_name, "SAMP") == 0) {
            // Sample information
            if (m->samples || chunk_size > reader.size - reader.pos)
                return false;
            m->samp_num = chunk_size / 32;
            m->samples = (OktSample*)calloc(m->samp_num, sizeof(OktSample));
            if (!m->samples)
                return false;

            for (uint32_t i = 0; i < m->samp_num; i++) {
                OktSample* samp = &m->samples[i];

                reader_read_bytes(&reader, (uint8_t*)samp->name, 20);
                samp->name[20] = '\0';

                // Other information
                samp->length = reader_read_b_uint32(&reader);
                samp->repeat_start = (uint16_t)(reader_read_b_uint16(&reader) * 2);
                samp->repeat_length = (uint16_t)(reader_read_b_uint16(&reader) * 2);

                // A one word repeat is a loop (a sample ending in one holds its last word)
                if (samp->repeat_length < 2 || (uint32_t)samp->repeat_start + samp->repeat_length > samp->length) {
                    samp->repeat_start = 0;
                    samp->repeat_length = 0;
                }

                reader_skip(&reader, 1);
                samp->volume = reader_read_uint8(&reader);
                samp->mode = reader_read_b_uint16(&reader);

                if (reader_eof(&reader))
                    return false;

                if (samp->length != 0)
                    real_used_samp_num++;
            }
        } else if (strcmp(chunk_name, "SPEE") == 0) {
            if (chunk_size != 2)
                return false;
            m->start_speed = reader_read_b_uint16(&reader);
        } else if (strcmp(chunk_name, "SLEN") == 0) {
            if (chunk_size != 2)
                return false;
            if (m->patterns)
                return false;
            m->patt_num = reader_read_b_uint16(&reader);
            m->patterns = (OktPattern*)calloc(m->patt_num, sizeof(OktPattern));
            if (!m->patterns)
                return false;
        } else if (strcmp(chunk_name, "PLEN") == 0) {
            if (chunk_size != 2)
                return false;
            m->song_length = reader_read_b_uint16(&reader);
        } else if (strcmp(chunk_name, "PATT") == 0) {
            if (chunk_size != 128)
                return false;
            reader_read_bytes(&reader, m->pattern_table, 128);
        } else if (strcmp(chunk_name, "PBOD") == 0) {
            if ((read_patt < m->patt_num) && (m->patterns != nullptr)) {
                // Parse PBOD
                OktPattern* pattern = &m->patterns[read_patt];

                pattern->line_num = (int16_t)reader_read_b_uint16(&reader);
                if (pattern->line_num < 0)
                    return false;
                pattern->lines = (OktPatternLine*)calloc(pattern->line_num * m->chan_num, sizeof(OktPatternLine));
                if (!pattern->lines)
                    return false;

                for (int i = 0; i < pattern->line_num; i++) {
                    for (int j = 0; j < m->chan_num; j++) {
                        OktPatternLine* line = &pattern->lines[i * m->chan_num + j];

                        line->note = reader_read_uint8(&reader);
                        line->sample_num = reader_read_uint8(&reader);
                        line->effect = reader_read_uint8(&reader);
                        line->effect_arg = reader_read_uint8(&reader);
                    }
                }

                if (reader_eof(&reader))
                    return false;

                read_patt++;
            } else {
                // Ignore the chunk
                reader_skip(&reader, chunk_size);
            }
        } else if (strcmp(chunk_name, "SBOD") == 0) {
            if ((read_samp < m->samp_num) && (m->samples != nullptr)) {
                // Find the next sample slot with data
                while (m->samples[read_samp].length == 0) {
                    read_samp++;
                    if (read_samp >= m->samp_num) {
                        reader_skip(&reader, chunk_size);
                        goto next_chunk;
                    }
                }

                OktSample* samp = &m->samples[read_samp];

                // Allocate memory to hold the sample data
                if (samp->sample_data)
                    return false;

                // A truncated file keeps the part of the sample that is present
                uint32_t remaining = (uint32_t)(reader.size - reader.pos);
                uint32_t read_len = chunk_size < remaining ? chunk_size : remaining;
                uint32_t alloc_len = chunk_size > samp->length ? chunk_size : samp->length;
                samp->sample_data = (int8_t*)calloc(alloc_len, sizeof(int8_t));
                if (!samp->sample_data)
                    return false;

                reader_read_signed(&reader, samp->sample_data, read_len);
                reader_skip(&reader, chunk_size - read_len);

                read_samp++;
            } else {
                reader_skip(&reader, chunk_size);
            }
        } else {
            // Unknown chunk
            if ((read_samp == 0) || (read_samp < real_used_samp_num))
                return false;
            else
                reader_seek_end(&reader);
        }

    next_chunk:;
    }

    return validate_module(m);
}

// Voices

// Hand the registers the tick wrote to the voice. Paula channel n is voice n.
static void sync_voice(OktModule* m, uint32_t index) {
    OktChannel* c = &m->channels[index];
    const OktChannelInfo* info = &m->chan_info[index];

    if (c->play_pending && c->sample_data != nullptr) {
        const NuadeSample sample = { .data = c->sample_data,
                                     .format = NuadeSampleFormat_S8,
                                     .length = c->sample_length,
                                     .loop_start = c->loop_start,
                                     .loop_length = c->loop_length };
        nuade_voice_play(m->voices, index, &sample, 0);
    }
    c->play_pending = false;

    if (c->release_pending && c->sample_data != nullptr) {
        const NuadeSample tail = { .data = c->sample_data + info->release_start,
                                   .format = NuadeSampleFormat_S8,
                                   .length = info->release_length,
                                   .loop_start = 0,
                                   .loop_length = info->release_length };
        nuade_voice_queue(m->voices, index, &tail);
    }
    c->release_pending = false;

    const bool silent = c->period == 0 || c->sample_data == nullptr;
    nuade_voice_set_pitch(m->voices, index, silent ? 0.0 : nuade_paula_period_hz(c->period));
    nuade_voice_set_volume(m->voices, index, silent ? 0.0f : (float)c->volume / 64.0f);
    nuade_voice_set_pan(m->voices, index, c->panning == 0 ? -1.0f : 1.0f);
}

bool okt_tick(OktModule* module) {
    const uint32_t loops = module->loop_count;
    play_tick(module);
    for (uint32_t i = 0; i < module->chan_num; i++)
        sync_voice(module, i);
    return module->loop_count != loops;
}

// Public API

bool okt_probe(const uint8_t* data, size_t size) {
    return data && size >= 1368 && memcmp(data, "OKTASONG", 8) == 0;
}

OktModule* okt_create(const uint8_t* data, size_t size) {
    if (!okt_probe(data, size))
        return nullptr;

    OktModule* m = (OktModule*)calloc(1, sizeof(OktModule));
    if (!m)
        return nullptr;

    // Keep original data for export
    m->original_data = (uint8_t*)malloc(size);
    if (m->original_data) {
        memcpy(m->original_data, data, size);
        m->original_size = size;
    }

    if (!load_module(m, data, size)) {
        okt_destroy(m);
        return nullptr;
    }

    initialize_sound(m, 0);

    return m;
}

void okt_destroy(OktModule* module) {
    if (!module)
        return;

    if (module->samples) {
        for (uint32_t i = 0; i < module->samp_num; i++)
            free(module->samples[i].sample_data);
        free(module->samples);
    }

    if (module->patterns) {
        for (uint16_t i = 0; i < module->patt_num; i++)
            free(module->patterns[i].lines);
        free(module->patterns);
    }

    if (module->original_data)
        free(module->original_data);
    free(module);
}

int okt_subsong_count(const OktModule* module) {
    if (!module)
        return 0;
    return 1; // Oktalyzer has no sub-songs
}

bool okt_start(OktModule* module, int subsong, NuadeVoices* voices) {
    if (!module || !voices || subsong != 0)
        return false;

    module->voices = voices;
    initialize_sound(module, 0);
    return true;
}

int okt_channel_count(const OktModule* module) {
    if (!module)
        return 0;
    return module->chan_num;
}

OktPosition okt_position(const OktModule* module) {
    // The tick that plays a line moves the position to it, so the position is the line that was just played.
    if (module->patt_pos < 0) {
        return (OktPosition) { 0 };
    }
    return (OktPosition) { .valid = true,
                           .song_pos = module->song_pos,
                           .pattern = module->pattern_table[module->song_pos],
                           .row = module->patt_pos,
                           .speed = module->current_speed,
                           .tick = module->speed_counter };
}

OktVoiceState okt_voice_state(const OktModule* module, int voice) {
    if (voice < 0 || voice >= module->chan_num) {
        return (OktVoiceState) { 0 };
    }
    return (OktVoiceState) { .note = module->chan_info[voice].shown_note,
                             .sample = module->chan_info[voice].shown_sample };
}

int okt_order_pattern(const OktModule* module, int order) {
    return order >= 0 && order < module->song_length ? module->pattern_table[order] : -1;
}

uint32_t okt_loop_count(const OktModule* module) {
    return module ? module->loop_count : 0;
}

const char* okt_instrument_name(const OktModule* module, int inst) {
    if (!module || inst < 0 || inst >= (int)module->samp_num)
        return "";
    return module->samples[inst].name;
}

// Edit API

int okt_get_instrument_count(const OktModule* module) {
    return module ? (int)module->samp_num : 0;
}

int okt_get_num_patterns(const OktModule* module) {
    return module ? (int)module->patt_num : 0;
}

int okt_get_pattern_rows(const OktModule* module, int pattern) {
    if (!module || pattern < 0 || pattern >= module->patt_num)
        return 0;
    return module->patterns[pattern].line_num;
}

int okt_get_num_positions(const OktModule* module) {
    return module ? (int)module->song_length : 0;
}

void okt_get_cell(const OktModule* module, int pattern, int row, int channel, uint8_t* note, uint8_t* sample_num,
                  uint8_t* effect, uint8_t* effect_arg) {
    if (!module || pattern < 0 || pattern >= module->patt_num || channel < 0 || channel >= module->chan_num) {
        if (note)
            *note = 0;
        if (sample_num)
            *sample_num = 0;
        if (effect)
            *effect = 0;
        if (effect_arg)
            *effect_arg = 0;
        return;
    }
    const OktPattern* pat = &module->patterns[pattern];
    if (row < 0 || row >= pat->line_num) {
        if (note)
            *note = 0;
        if (sample_num)
            *sample_num = 0;
        if (effect)
            *effect = 0;
        if (effect_arg)
            *effect_arg = 0;
        return;
    }
    const OktPatternLine* line = &pat->lines[row * module->chan_num + channel];
    if (note)
        *note = line->note;
    if (sample_num)
        *sample_num = line->sample_num;
    if (effect)
        *effect = line->effect;
    if (effect_arg)
        *effect_arg = line->effect_arg;
}

void okt_set_cell(OktModule* module, int pattern, int row, int channel, uint8_t note, uint8_t sample_num,
                  uint8_t effect, uint8_t effect_arg) {
    if (!module || pattern < 0 || pattern >= module->patt_num || channel < 0 || channel >= module->chan_num)
        return;
    OktPattern* pat = &module->patterns[pattern];
    if (row < 0 || row >= pat->line_num)
        return;
    OktPatternLine* line = &pat->lines[row * module->chan_num + channel];
    line->note = note;
    line->sample_num = sample_num;
    line->effect = effect;
    line->effect_arg = effect_arg;
}

float okt_get_instrument_param(const OktModule* module, int inst, const char* param) {
    if (!module || inst < 0 || inst >= (int)module->samp_num || !param)
        return -1.0f;
    const OktSample* s = &module->samples[inst];

    if (strcmp(param, "length") == 0)
        return (float)s->length;
    if (strcmp(param, "repeatStart") == 0)
        return (float)s->repeat_start;
    if (strcmp(param, "repeatLength") == 0)
        return (float)s->repeat_length;
    if (strcmp(param, "volume") == 0)
        return (float)s->volume;
    if (strcmp(param, "mode") == 0)
        return (float)s->mode;

    return -1.0f;
}

void okt_set_instrument_param(OktModule* module, int inst, const char* param, float value) {
    if (!module || inst < 0 || inst >= (int)module->samp_num || !param)
        return;
    OktSample* s = &module->samples[inst];
    uint16_t v = (uint16_t)value;
    uint32_t v32 = (uint32_t)value;

    if (strcmp(param, "length") == 0) {
        s->length = v32;
        return;
    }
    if (strcmp(param, "repeatStart") == 0) {
        s->repeat_start = v;
        return;
    }
    if (strcmp(param, "repeatLength") == 0) {
        s->repeat_length = v;
        return;
    }
    if (strcmp(param, "volume") == 0) {
        s->volume = v;
        return;
    }
    if (strcmp(param, "mode") == 0) {
        s->mode = v;
        return;
    }
}

size_t okt_export(const OktModule* module, uint8_t* out, size_t max_size) {
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

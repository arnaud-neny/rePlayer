// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/instereo1.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nuade_base.h"
#include "nuade_paula.h"

#define IS1_VOICES 4
#define ENVELOPE_GENERATOR_TABLE_LENGTH 128
#define ADSR_TABLE_LENGTH 256
#define SYNTH_SAMPLE_SIZE 256

// Tables

static const uint16_t is1_periods[109]
    = { 0,    13696, 12928, 12192, 11520, 10848, 10240, 9664, 9120, 8608, 8128, 7680, 7248, 6848, 6464, 6096,
        5760, 5424,  5120,  4832,  4560,  4304,  4064,  3840, 3624, 3424, 3232, 3048, 2880, 2712, 2560, 2416,
        2280, 2152,  2032,  1920,  1812,  1712,  1616,  1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960,
        906,  856,   808,   762,   720,   678,   640,   604,  570,  538,  508,  480,  453,  428,  404,  381,
        360,  339,   320,   302,   285,   269,   254,   240,  226,  214,  202,  190,  180,  170,  160,  151,
        143,  135,   127,   120,   113,   107,   101,   95,   90,   85,   80,   75,   71,   67,   63,   60,
        56,   53,    50,    47,    45,    42,    40,    37,   35,   33,   31,   30,   28

      };

// Reader helpers

typedef struct Is1Reader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} Is1Reader;

static void reader_init(Is1Reader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

static bool reader_eof(const Is1Reader* r) {
    return r->pos > r->size;
}

static void reader_seek(Is1Reader* r, size_t pos) {
    r->pos = pos;
}

static void reader_skip(Is1Reader* r, size_t n) {
    r->pos += n;
}

static uint8_t reader_read_uint8(Is1Reader* r) {
    if (r->pos >= r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    return r->data[r->pos++];
}

static int8_t reader_read_int8(Is1Reader* r) {
    return (int8_t)reader_read_uint8(r);
}

static uint16_t reader_read_b_uint16(Is1Reader* r) {
    uint8_t hi = reader_read_uint8(r);
    uint8_t lo = reader_read_uint8(r);
    return (uint16_t)((hi << 8) | lo);
}

static uint32_t reader_read_b_uint32(Is1Reader* r) {
    uint8_t b0 = reader_read_uint8(r);
    uint8_t b1 = reader_read_uint8(r);
    uint8_t b2 = reader_read_uint8(r);
    uint8_t b3 = reader_read_uint8(r);
    return ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | b3;
}

static void reader_read_bytes(Is1Reader* r, uint8_t* out, size_t count) {
    for (size_t i = 0; i < count; i++)
        out[i] = reader_read_uint8(r);
}

static void reader_read_signed(Is1Reader* r, int8_t* out, size_t count) {
    for (size_t i = 0; i < count; i++)
        out[i] = reader_read_int8(r);
}

// Voice register operations

static void channel_play_sample(Is1Channel* mc, int8_t* addr, uint32_t length) {
    mc->sample_data = addr;
    mc->sample_length = length;
    mc->loop_start = 0;
    mc->loop_length = 0;
    mc->active = true;
    mc->play_pending = true;
}

static void channel_set_loop(Is1Channel* mc, uint32_t start, uint32_t length) {
    mc->loop_start = start;
    mc->loop_length = length;
}

// Replace the sample data of a running voice without restarting it
static void channel_swap_sample(Is1Channel* mc, int8_t* addr, uint32_t length) {
    mc->sample_data = addr;
    mc->sample_length = length;
    mc->loop_start = 0;
    mc->loop_length = length;
    mc->swap_pending = true;
}

static void channel_set_amiga_volume(Is1Channel* mc, uint16_t vol) {
    mc->volume = vol;
}

static void channel_set_amiga_period(Is1Channel* mc, uint32_t period) {
    mc->period = period;
}

static void channel_mute(Is1Channel* mc) {
    mc->active = false;
}

// Position visited tracking

static void clear_visited(Is1Module* m) {
    memset(m->visited, 0, sizeof(m->visited));
}

static void mark_position_visited(Is1Module* m, uint16_t pos) {
    if (pos < 256)
        m->visited[pos] = 1;
}

static bool has_position_been_visited(Is1Module* m, uint16_t pos) {
    if (pos < 256)
        return m->visited[pos] != 0;
    return false;
}

// Build vibrato table (triangle)

static void build_vibrato_table(Is1Module* m) {
    int8_t vib_val = 0;
    int offset = 0;

    for (int i = 0; i < 64; i++) {
        m->vibrato_table[offset++] = vib_val;
        vib_val += 2;
    }

    vib_val++;

    for (int i = 0; i < 128; i++) {
        vib_val -= 2;
        m->vibrato_table[offset++] = vib_val;
    }

    vib_val--;

    for (int i = 0; i < 64; i++) {
        m->vibrato_table[offset++] = vib_val;
        vib_val += 2;
    }
}

// DoArpeggio

static uint16_t note_period(uint8_t note) {
    return note < sizeof(is1_periods) / sizeof(is1_periods[0]) ? is1_periods[note] : 0;
}

// Reads a table byte; positions outside the table read as zero.
static uint8_t table_byte(const uint8_t* table, size_t table_size, size_t index) {
    return index < table_size ? table[index] : 0;
}

// Instruments are numbered from 1; 0 and numbers past the table select nothing.
static Is1Instrument* instrument_at(const Is1Module* m, uint8_t number) {
    return number >= 1 && number <= m->num_instruments ? &m->instruments[number - 1] : nullptr;
}

typedef struct Is1PeriodInfo {
    uint16_t period;
    uint16_t previous_period;
} Is1PeriodInfo;

static Is1PeriodInfo do_arpeggio(Is1Module* m, Is1VoiceInfo* vi) {
    uint8_t note = vi->transposed_note;
    uint8_t prev_note = vi->previous_transposed_note;

    uint8_t arp_val = table_byte(m->arpeggio_tables, sizeof(m->arpeggio_tables),
                                 (size_t)vi->arpeggio * IS1_ARPEGGIO_TABLE_LENGTH + m->speed_counter);
    note += arp_val;
    prev_note += arp_val;

    uint16_t period = note_period(note);
    uint16_t previous_period = note_period(prev_note);

    Is1PeriodInfo info;
    info.period = period;
    info.previous_period = previous_period;
    return info;
}

// DoPortamento

static void do_portamento(Is1PeriodInfo* pi, Is1VoiceInfo* vi) {
    if (vi->portamento_enabled && (vi->portamento_speed_counter != 0) && (pi->period != pi->previous_period)) {
        vi->portamento_speed_counter--;

        // Swap period and previous_period
        uint16_t tmp = pi->period;
        pi->period = pi->previous_period;
        pi->previous_period = tmp;

        int new_period = (pi->period - pi->previous_period) * vi->portamento_speed_counter;

        if (vi->portamento_speed != 0)
            new_period /= vi->portamento_speed;

        new_period += pi->previous_period;
        pi->period = (uint16_t)new_period;
    }
}

// DoVibrato

static bool do_vibrato(Is1Module* m, Is1PeriodInfo* pi, Is1VoiceInfo* vi) {
    if (vi->vibrato_delay == 0) {
        if (vi->transposed_instrument == 0)
            return false;

        Is1Instrument* instr = instrument_at(m, vi->transposed_instrument);
        if (instr == nullptr)
            return false;

        int8_t vib_val = m->vibrato_table[vi->vibrato_position];
        uint8_t vib_level = instr->vibrato_level;

        if (vib_val < 0) {
            if (vib_level != 0)
                pi->period -= (uint16_t)((-vib_val * 4) / vib_level);
        } else {
            if (vib_level != 0)
                pi->period += (uint16_t)((vib_val * 4) / vib_level);
            else
                pi->period = 0;
        }

        vi->vibrato_position += instr->vibrato_speed;
    } else
        vi->vibrato_delay--;

    return true;
}

// DoAdsr

static void do_adsr(Is1Module* m, Is1VoiceInfo* vi, Is1Channel* mc) {
    Is1Instrument* instr = instrument_at(m, vi->transposed_instrument);
    if (instr == nullptr)
        return;

    if (vi->adsr_enabled) {
        if (vi->adsr_position >= instr->adsr_table_length)
            vi->adsr_enabled = false;

        uint16_t adsr_val = table_byte(m->adsr_tables, (size_t)m->num_adsr_tables * ADSR_TABLE_LENGTH,
                                       (size_t)instr->adsr_table_number * ADSR_TABLE_LENGTH + vi->adsr_position);
        adsr_val++;

        uint16_t volume = instr->volume;
        volume = (uint16_t)((volume * adsr_val) / 128);
        if (volume > 64)
            volume = 64;

        channel_set_amiga_volume(mc, volume);

        vi->adsr_position++;
    }
}

// DoEnvelopeGeneratorCounter

static void do_egc(Is1Module* m, Is1VoiceInfo* vi, Is1Channel* mc) {
    if (vi->egc_mode != IS1_EGC_OFF) {
        Is1Instrument* instr = instrument_at(m, vi->transposed_instrument);
        if (instr == nullptr || instr->waveform_number >= m->num_waveforms)
            return;

        int8_t* waveform = m->waveforms[instr->waveform_number];
        const uint32_t length = instr->waveform_length < SYNTH_SAMPLE_SIZE ? instr->waveform_length : SYNTH_SAMPLE_SIZE;
        int8_t* synth_buf;

        vi->use_buffer ^= 1;

        if (vi->use_buffer == 0)
            synth_buf = vi->synth_sample2;
        else
            synth_buf = vi->synth_sample1;

        channel_swap_sample(mc, synth_buf, length);

        memcpy(synth_buf, waveform, length);

        uint8_t egc_val
            = table_byte(m->eg_tables, (size_t)m->num_eg_tables * ENVELOPE_GENERATOR_TABLE_LENGTH,
                         (size_t)instr->egc_table_number * ENVELOPE_GENERATOR_TABLE_LENGTH + vi->egc_position);
        egc_val += instr->egc_offset;

        if (egc_val != 0) {
            for (int i = 0; i < egc_val; i++)
                synth_buf[i] = (int8_t)-synth_buf[i];
        }

        vi->egc_position++;

        if (vi->egc_position >= instr->egc_table_length) {
            if (vi->egc_mode == IS1_EGC_ONES)
                vi->egc_mode = IS1_EGC_OFF;
            else
                vi->egc_position = 0;
        }
    }
}

// DoEffects for a single voice

static void do_effects(Is1Module* m, Is1VoiceInfo* vi, Is1Channel* mc) {
    Is1PeriodInfo pi = do_arpeggio(m, vi);
    do_portamento(&pi, vi);

    if (do_vibrato(m, &pi, vi)) {
        pi.period = (uint16_t)(pi.period + vi->slide_increment);

        channel_set_amiga_period(mc, pi.period);

        vi->slide_increment = (int16_t)(vi->slide_increment - vi->slide_speed);

        do_adsr(m, vi, mc);
    }

    do_egc(m, vi, mc);
}

// DoEffectsForAllVoices

static void do_effects_for_all_voices(Is1Module* m) {
    for (int i = 0; i < 4; i++)
        do_effects(m, &m->voices[i], &m->channels[i]);
}

// PlayRow: initialize channel for a single row

static void play_row(Is1Module* m, Is1VoiceInfo* vi, Is1Channel* mc) {
    vi->current_volume = 0;
    vi->slide_speed = 0;

    if (vi->effect != IS1_EFFECT_NONE) {
        switch (vi->effect) {
            case IS1_EFFECT_SET_SLIDE_SPEED:
                vi->slide_speed = (int8_t)vi->effect_arg;
                break;

            case IS1_EFFECT_RESTART_ADSR:
                vi->adsr_position = vi->effect_arg;
                vi->adsr_enabled = true;
                break;

            case IS1_EFFECT_RESTART_EGC:
                vi->egc_position = vi->effect_arg;
                vi->egc_mode = IS1_EGC_ONES;
                break;

            case IS1_EFFECT_SET_SLIDE_INCREMENT:
                vi->slide_increment = (int8_t)vi->effect_arg;
                break;

            case IS1_EFFECT_SET_VIBRATO_DELAY:
                vi->vibrato_delay = vi->effect_arg;
                vi->vibrato_position = 0;
                break;

            case IS1_EFFECT_SET_VIBRATO_POSITION:
                vi->vibrato_position = vi->effect_arg;
                break;

            case IS1_EFFECT_SET_VOLUME:
                vi->current_volume = (uint8_t)(vi->effect_arg & 0x3f);
                break;

            case IS1_EFFECT_SET_TRACK_LEN:
                if (vi->effect_arg <= 64)
                    m->rows_per_track = vi->effect_arg;
                break;

            // These effects do nothing in the replayer source
            case IS1_EFFECT_EFF_C:
            case IS1_EFFECT_EFF_D:
                break;

            case IS1_EFFECT_SET_FILTER:
                // AmigaFilter = voiceInfo.EffectArg == 0;
                // Not implemented in our C renderer
                break;

            case IS1_EFFECT_SET_SPEED:
                if ((vi->effect_arg > 0) && (vi->effect_arg <= 16))
                    m->current_speed = vi->effect_arg;
                break;

            default:
                break;
        }
    }

    uint8_t note = vi->note;
    if (note != 0) {
        if (note == 0x7f) {
            channel_mute(mc);
            vi->current_volume = 0;
            return;
        }

        if (vi->effect != IS1_EFFECT_SKIP_NT)
            note = (uint8_t)(note + vi->note_transpose);

        vi->previous_transposed_note = vi->transposed_note;
        vi->transposed_note = note;

        channel_set_amiga_period(mc, note_period(note));

        uint8_t instr_num = vi->instrument;
        if (instr_num != 0) {
            if (vi->effect != IS1_EFFECT_SKIP_ST)
                instr_num = (uint8_t)(instr_num + vi->sound_transpose);

            Is1Instrument* instr = instrument_at(m, instr_num);
            if (instr == nullptr) {
                vi->transposed_instrument = 0;
                channel_mute(mc);
                vi->current_volume = 0;
                return;
            }
            vi->transposed_instrument = instr_num;

            vi->adsr_enabled = false;
            vi->adsr_position = 0;

            vi->vibrato_delay = 0;
            vi->vibrato_position = 0;

            vi->egc_mode = IS1_EGC_OFF;
            vi->egc_position = 0;

            vi->slide_increment = 0;

            vi->portamento_enabled = instr->portamento_enabled;

            vi->portamento_speed = instr->portamento_speed;
            vi->portamento_speed_counter = instr->portamento_speed;

            if (vi->effect == IS1_EFFECT_SKIP_PORTAMENTO) {
                vi->portamento_enabled = false;
                vi->portamento_speed = 0;
                vi->portamento_speed_counter = 0;
            }

            vi->vibrato_delay = instr->vibrato_delay;

            if (instr->adsr_enabled)
                vi->adsr_enabled = true;

            if (instr->synthesis_enabled) {
                if (instr->waveform_number >= m->num_waveforms) {
                    channel_mute(mc);
                    return;
                }
                int8_t* waveform = m->waveforms[instr->waveform_number];

                vi->use_buffer = 1;

                vi->egc_mode = instr->egc_mode;

                // The voice starts on its own synth buffer; with the envelope generator running, do_egc rewrites
                // that buffer every tick.
                uint16_t length = instr->waveform_length;
                if (length > SYNTH_SAMPLE_SIZE)
                    length = SYNTH_SAMPLE_SIZE;

                channel_play_sample(mc, vi->synth_sample1, length);
                channel_set_loop(mc, 0, length);

                memcpy(vi->synth_sample1, waveform, length);

                if (instr->egc_mode == IS1_EGC_OFF) {
                    for (int i = 0; i < instr->egc_offset && i < SYNTH_SAMPLE_SIZE; i++)
                        vi->synth_sample1[i] = (int8_t)-vi->synth_sample1[i];
                }

                vi->current_volume = instr->volume;
                channel_set_amiga_volume(mc, vi->current_volume);
            } else {
                uint8_t sample_num = (uint8_t)(instr->waveform_number & 0x3f);
                if (sample_num >= m->num_samples) {
                    channel_mute(mc);
                    return;
                }

                Is1Sample* sample = &m->samples[sample_num];

                uint32_t play_length = instr->waveform_length;
                uint32_t loop_start = 0;
                uint32_t loop_length = 0;

                if (instr->repeat_length == 0)
                    loop_length = instr->waveform_length;
                else if (instr->repeat_length != 2) {
                    play_length += instr->repeat_length;

                    loop_start = instr->waveform_length;
                    loop_length = instr->repeat_length;
                }

                uint32_t actual_length = play_length < sample->length ? play_length : sample->length;
                channel_play_sample(mc, sample->sample_addr, actual_length);

                if (loop_length != 0)
                    channel_set_loop(mc, loop_start, loop_length);

                channel_set_amiga_volume(mc, instr->volume);

                if (sample_num == 7)
                    vi->current_volume = (uint8_t)(vi->effect_arg & 0x3f);
                else
                    vi->current_volume = instr->volume;
            }
        }
    }
}

// GetNextRow

static void get_next_row(Is1Module* m) {
    m->speed_counter = 0;

    if (m->row_position >= m->rows_per_track) {
        m->row_position = 0;

        if (m->song_position > m->current_song_info->last_position)
            m->song_position = m->current_song_info->restart_position;

        if (m->song_position >= m->total_positions)
            m->song_position = 0;

        if (has_position_been_visited(m, m->song_position)) {
            m->loop_count++;
            clear_visited(m);
        }

        mark_position_visited(m, m->song_position);
        m->song_position++;

        Is1SinglePositionInfo* pos_row = m->positions[m->song_position - 1];

        for (int i = 0; i < 4; i++) {
            Is1SinglePositionInfo* pos_info = &pos_row[i];
            Is1VoiceInfo* vi = &m->voices[i];

            vi->start_track_row = pos_info->start_track_row;
            vi->sound_transpose = pos_info->sound_transpose;
            vi->note_transpose = pos_info->note_transpose;
        }
    }

    for (int i = 0; i < 4; i++) {
        Is1VoiceInfo* vi = &m->voices[i];

        int position = vi->start_track_row + m->row_position;
        Is1TrackLine* tl;
        Is1TrackLine empty_line;
        memset(&empty_line, 0, sizeof(Is1TrackLine));

        if (position < (int)m->total_track_rows + 64)
            tl = &m->track_lines[position];
        else
            tl = &empty_line;

        vi->note = tl->note;
        vi->instrument = tl->instrument;
        vi->arpeggio = tl->arpeggio;
        vi->effect = tl->effect;
        vi->effect_arg = tl->effect_arg;
    }

    for (int i = 0; i < 4; i++)
        play_row(m, &m->voices[i], &m->channels[i]);

    m->shown_position = (uint16_t)(m->song_position - 1);
    m->shown_row = m->row_position;
    m->shown_rows = m->rows_per_track;
    m->row_position++;
}

// Play tick

static void play_tick(Is1Module* m) {
    m->speed_counter++;

    if (m->speed_counter >= m->current_speed)
        get_next_row(m);

    do_effects_for_all_voices(m);
}

// Initialize sound

static void initialize_sound(Is1Module* m, int sub_song) {
    m->current_song_info = &m->sub_songs[sub_song];

    m->speed_counter = m->current_song_info->start_speed;
    m->current_speed = m->current_song_info->start_speed;

    m->song_position = m->current_song_info->first_position;
    m->row_position = m->current_song_info->rows_per_track;
    m->rows_per_track = m->current_song_info->rows_per_track;
    m->shown_position = m->current_song_info->first_position;
    m->shown_row = 0;
    m->shown_rows = m->rows_per_track;

    for (int i = 0; i < 4; i++) {
        Is1VoiceInfo* vi = &m->voices[i];
        memset(vi, 0, sizeof(Is1VoiceInfo));

        vi->egc_mode = IS1_EGC_OFF;
    }

    m->loop_count = 0;

    clear_visited(m);

    // Reset the voice registers
    memset(m->channels, 0, sizeof(m->channels));
}

// Loading

static bool load_module(Is1Module* m, const uint8_t* data, size_t size) {
    Is1Reader reader;
    reader_init(&reader, data, size);

    // Read meta data
    reader_seek(&reader, 8);

    uint16_t total_positions = reader_read_b_uint16(&reader);
    uint16_t total_track_rows = reader_read_b_uint16(&reader);

    reader_skip(&reader, 4);

    uint8_t num_samples = reader_read_uint8(&reader);
    uint8_t num_waveforms = reader_read_uint8(&reader);
    uint8_t num_instruments = reader_read_uint8(&reader);
    uint8_t num_sub_songs = reader_read_uint8(&reader);
    uint8_t num_eg_tables = reader_read_uint8(&reader);
    uint8_t num_adsr_tables = reader_read_uint8(&reader);

    if (reader_eof(&reader))
        return false;

    m->num_samples = num_samples;
    m->num_waveforms = num_waveforms;
    m->num_instruments = num_instruments;
    m->num_sub_songs = num_sub_songs;
    m->num_eg_tables = num_eg_tables;
    m->num_adsr_tables = num_adsr_tables;
    m->total_positions = total_positions;
    m->total_track_rows = total_track_rows;

    // Skip to past header text
    reader_skip(&reader, 14); // skip remaining header

    // Read module name (28 bytes)
    reader_read_bytes(&reader, (uint8_t*)m->title, 28);

    // Skip some text (140 bytes)
    reader_skip(&reader, 140);

    // Read sample information
    m->samples = (Is1Sample*)calloc(num_samples, sizeof(Is1Sample));
    if (!m->samples)
        return false;

    for (int i = 0; i < num_samples; i++) {
        reader_skip(&reader, 1);  // skip
        reader_skip(&reader, 23); // skip name

        if (reader_eof(&reader))
            return false;

        reader_skip(&reader, 4); // skip more
    }

    for (int i = 0; i < num_samples; i++)
        m->samples[i].length = reader_read_b_uint32(&reader);

    if (reader_eof(&reader))
        return false;

    if (total_positions == 0 || num_sub_songs == 0)
        return false;

    // Read envelope generator tables
    m->eg_tables = (uint8_t*)calloc(num_eg_tables * ENVELOPE_GENERATOR_TABLE_LENGTH, sizeof(uint8_t));
    if (!m->eg_tables && num_eg_tables > 0)
        return false;

    reader_read_bytes(&reader, m->eg_tables, num_eg_tables * ENVELOPE_GENERATOR_TABLE_LENGTH);

    if (reader_eof(&reader) && num_eg_tables > 0)
        return false;

    // Read ADSR tables
    m->adsr_tables = (uint8_t*)calloc(num_adsr_tables * ADSR_TABLE_LENGTH, sizeof(uint8_t));
    if (!m->adsr_tables && num_adsr_tables > 0)
        return false;

    reader_read_bytes(&reader, m->adsr_tables, num_adsr_tables * ADSR_TABLE_LENGTH);

    if (reader_eof(&reader) && num_adsr_tables > 0)
        return false;

    // Read instrument information
    m->instruments = (Is1Instrument*)calloc(num_instruments, sizeof(Is1Instrument));
    if (!m->instruments)
        return false;

    for (int i = 0; i < num_instruments; i++) {
        Is1Instrument* instr = &m->instruments[i];

        instr->waveform_number = reader_read_uint8(&reader);
        instr->synthesis_enabled = reader_read_uint8(&reader) != 0;
        instr->waveform_length = reader_read_b_uint16(&reader);
        instr->repeat_length = reader_read_b_uint16(&reader);
        instr->volume = reader_read_uint8(&reader);
        instr->portamento_speed = reader_read_int8(&reader);
        instr->adsr_enabled = reader_read_uint8(&reader) != 0;
        instr->adsr_table_number = reader_read_uint8(&reader);
        instr->adsr_table_length = reader_read_b_uint16(&reader);
        reader_skip(&reader, 2);
        instr->portamento_enabled = reader_read_uint8(&reader) != 0;
        reader_skip(&reader, 5);
        instr->vibrato_delay = reader_read_uint8(&reader);
        instr->vibrato_speed = reader_read_uint8(&reader);
        instr->vibrato_level = reader_read_uint8(&reader);
        instr->egc_offset = reader_read_uint8(&reader);
        instr->egc_mode = (Is1EgcMode)reader_read_uint8(&reader);
        instr->egc_table_number = reader_read_uint8(&reader);
        instr->egc_table_length = reader_read_b_uint16(&reader);

        if (reader_eof(&reader))
            return false;
    }

    // Read arpeggio tables (16 * 16 = 256 bytes)
    reader_read_bytes(&reader, m->arpeggio_tables, 16 * IS1_ARPEGGIO_TABLE_LENGTH);

    if (reader_eof(&reader))
        return false;

    // Read sub-song information
    m->sub_songs = (Is1SongInfo*)calloc(num_sub_songs, sizeof(Is1SongInfo));
    if (!m->sub_songs)
        return false;

    for (int i = 0; i < num_sub_songs; i++) {
        reader_skip(&reader, 4);

        Is1SongInfo* si = &m->sub_songs[i];
        si->start_speed = reader_read_uint8(&reader);
        si->rows_per_track = reader_read_uint8(&reader);
        si->first_position = reader_read_b_uint16(&reader);
        si->last_position = reader_read_b_uint16(&reader);
        si->restart_position = reader_read_b_uint16(&reader);

        if (reader_eof(&reader))
            return false;

        reader_skip(&reader, 2);
    }

    // Skip extra sub-song information (14 bytes)
    reader_skip(&reader, 14);

    // Read waveforms
    m->waveforms = (int8_t**)calloc(num_waveforms, sizeof(int8_t*));
    if (!m->waveforms)
        return false;

    for (int i = 0; i < num_waveforms; i++) {
        m->waveforms[i] = (int8_t*)calloc(256, sizeof(int8_t));
        if (!m->waveforms[i])
            return false;

        reader_read_signed(&reader, m->waveforms[i], 256);

        if (reader_eof(&reader))
            return false;
    }

    // Read position information
    m->positions = (Is1SinglePositionInfo**)calloc(total_positions, sizeof(Is1SinglePositionInfo*));
    if (!m->positions)
        return false;

    for (int i = 0; i < total_positions; i++) {
        m->positions[i] = (Is1SinglePositionInfo*)calloc(4, sizeof(Is1SinglePositionInfo));
        if (!m->positions[i])
            return false;

        for (int j = 0; j < 4; j++) {
            Is1SinglePositionInfo* sp = &m->positions[i][j];

            sp->start_track_row = reader_read_b_uint16(&reader);
            sp->sound_transpose = reader_read_int8(&reader);
            sp->note_transpose = reader_read_int8(&reader);
        }

        if (reader_eof(&reader))
            return false;
    }

    // Read track rows (+ 64 extra empty rows)
    uint32_t track_line_count = total_track_rows + 64;
    m->track_lines = (Is1TrackLine*)calloc(track_line_count, sizeof(Is1TrackLine));
    if (!m->track_lines)
        return false;

    for (uint32_t i = 0; i < track_line_count; i++) {
        Is1TrackLine* tl = &m->track_lines[i];

        uint8_t byt1 = reader_read_uint8(&reader);
        uint8_t byt2 = reader_read_uint8(&reader);
        uint8_t byt3 = reader_read_uint8(&reader);
        uint8_t byt4 = reader_read_uint8(&reader);

        tl->note = byt1;
        tl->instrument = byt2;
        tl->arpeggio = (uint8_t)((byt3 & 0xf0) >> 4);
        tl->effect = (Is1Effect)(byt3 & 0x0f);
        tl->effect_arg = byt4;

        if (reader_eof(&reader))
            return false;
    }

    // Read sample data
    for (int i = 0; i < num_samples; i++) {
        if (m->samples[i].length > size)
            return false;

        m->samples[i].sample_addr = (int8_t*)calloc(m->samples[i].length, sizeof(int8_t));
        if (!m->samples[i].sample_addr && m->samples[i].length > 0)
            return false;

        reader_read_signed(&reader, m->samples[i].sample_addr, m->samples[i].length);

        // Check if we read everything
        // (the C# version checks readBytes != length but we just check eof)
    }

    return true;
}

// Voice hand-off

// Hand the registers the tick wrote to the voice. The voice index is the Paula channel; 0 and 3 are left.
static void sync_voice(Is1Module* m, uint32_t index) {
    Is1Channel* c = &m->channels[index];

    if (!c->active || c->sample_data == nullptr) {
        nuade_voice_stop(m->nuade_voices, index);
    } else if (c->play_pending || c->swap_pending) {
        const NuadeSample sample = { .data = c->sample_data,
                                     .format = NuadeSampleFormat_S8,
                                     .length = c->sample_length,
                                     .loop_start = c->loop_start,
                                     .loop_length = c->loop_length };
        if (c->play_pending)
            nuade_voice_play(m->nuade_voices, index, &sample, 0);
        else if (sample.length != 0)
            nuade_voice_queue(m->nuade_voices, index, &sample);
    }
    c->play_pending = false;
    c->swap_pending = false;

    const bool silent = c->period == 0;
    nuade_voice_set_pitch(m->nuade_voices, index, silent ? 0.0 : nuade_paula_period_hz(c->period));
    nuade_voice_set_volume(m->nuade_voices, index, silent ? 0.0f : (float)c->volume / 64.0f);
    nuade_voice_set_pan(m->nuade_voices, index, (index == 0 || index == 3) ? -1.0f : 1.0f);
}

bool is1_tick(Is1Module* module) {
    // A one-shot that ran off its end with a sample queued behind it still counts as playing; the mixer switches on the
    // next frame.
    for (uint32_t i = 0; i < IS1_VOICES; i++)
        module->channels[i].active = module->channels[i].active && nuade_voice_is_playing(module->nuade_voices, i);

    const uint32_t loops = module->loop_count;
    play_tick(module);

    for (uint32_t i = 0; i < IS1_VOICES; i++)
        sync_voice(module, i);
    return module->loop_count != loops;
}

// Public API

bool is1_probe(const uint8_t* data, size_t size) {
    return data != nullptr && size >= 204 && memcmp(data, "ISM!V1.2", 8) == 0;
}

Is1Module* is1_create(const uint8_t* data, size_t size) {
    if (!is1_probe(data, size))
        return nullptr;

    Is1Module* m = (Is1Module*)calloc(1, sizeof(Is1Module));
    if (!m)
        return nullptr;

    if (!load_module(m, data, size)) {
        is1_destroy(m);
        return nullptr;
    }

    build_vibrato_table(m);

    return m;
}

void is1_destroy(Is1Module* module) {
    if (!module)
        return;

    if (module->sub_songs)
        free(module->sub_songs);

    if (module->positions) {
        for (int i = 0; i < module->total_positions; i++)
            free(module->positions[i]);
        free(module->positions);
    }

    if (module->track_lines)
        free(module->track_lines);

    if (module->samples) {
        for (int i = 0; i < module->num_samples; i++)
            free(module->samples[i].sample_addr);
        free(module->samples);
    }

    if (module->waveforms) {
        for (int i = 0; i < module->num_waveforms; i++)
            free(module->waveforms[i]);
        free(module->waveforms);
    }

    if (module->instruments)
        free(module->instruments);
    if (module->eg_tables)
        free(module->eg_tables);
    if (module->adsr_tables)
        free(module->adsr_tables);

    free(module);
}

int is1_subsong_count(const Is1Module* module) {
    if (!module)
        return 0;
    return module->num_sub_songs;
}

bool is1_start(Is1Module* module, int subsong, NuadeVoices* voices) {
    if (!module || !voices || subsong < 0 || subsong >= module->num_sub_songs)
        return false;

    module->nuade_voices = voices;

    initialize_sound(module, subsong);
    return true;
}

const char* is1_title(const Is1Module* module) {
    return module ? module->title : "";
}

// Display

Is1Position is1_position(const Is1Module* module) {
    return (Is1Position) { .position = module->shown_position,
                           .row = module->shown_row,
                           .rows = module->shown_rows,
                           .speed = module->current_speed };
}

int is1_position_count(const Is1Module* module) {
    return module->total_positions;
}

int is1_default_rows(const Is1Module* module) {
    return module->current_song_info != nullptr ? module->current_song_info->rows_per_track : 0;
}

Is1VoiceState is1_voice_state(const Is1Module* module, int voice) {
    const Is1VoiceInfo* vi = &module->voices[voice];
    return (Is1VoiceState) { .note = vi->transposed_note, .instrument = vi->transposed_instrument };
}

Is1Cell is1_pattern_cell(const Is1Module* module, int position, int channel, int row) {
    Is1Cell cell = { 0 };
    if (position < 0 || position >= module->total_positions) {
        return cell;
    }
    const uint32_t line = (uint32_t)module->positions[position][channel].start_track_row + (uint32_t)row;
    if (line >= module->total_track_rows + 64) {
        return cell;
    }
    const Is1TrackLine* tl = &module->track_lines[line];
    cell.note = tl->note;
    cell.instrument = tl->instrument;
    cell.arpeggio = tl->arpeggio;
    cell.effect = (uint8_t)tl->effect;
    cell.effect_arg = tl->effect_arg;
    return cell;
}

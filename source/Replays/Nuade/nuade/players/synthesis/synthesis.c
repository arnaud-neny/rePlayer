// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/synthesis.h>

#include "nuade_base.h"
#include "nuade_paula.h"

#define SYN_WAVE_SIZE 256
#include <stdlib.h>
#include <string.h>

// Structs

typedef struct SynPeriodInfo {
    uint16_t period;
    uint16_t previous_period;
} SynPeriodInfo;

#define SYN_EGC_TABLE_LENGTH 128
#define SYN_ADSR_TABLE_LENGTH 256
#define SYN_CHANNELS 4
#define SYN_MIN_SIZE 204
#define SYN_V42_OFFSET 0x1f0e

// Period table

static const uint16_t syn_periods[]
    = { 0,    13696, 12928, 12192, 11520, 10848, 10240, 9664, 9120, 8608, 8128, 7680, 7248, 6848, 6464, 6096,
        5760, 5424,  5120,  4832,  4560,  4304,  4064,  3840, 3624, 3424, 3232, 3048, 2880, 2712, 2560, 2416,
        2280, 2152,  2032,  1920,  1812,  1712,  1616,  1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960,
        906,  856,   808,   762,   720,   678,   640,   604,  570,  538,  508,  480,  453,  428,  404,  381,
        360,  339,   320,   302,   285,   269,   254,   240,  226,  214,  202,  190,  180,  170,  160,  151,
        143,  135,   127,   120,   113,   107,   101,   95,   90,   85,   80,   75,   71,   67,   63,   60,
        56,   53,    50,    47,    45,    42,    40,    37,   35,   33,   31,   30,   28

      };

// Reader helpers

typedef struct SynReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} SynReader;

static void reader_init(SynReader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

static bool reader_eof(const SynReader* r) {
    return r->pos > r->size;
}

static uint8_t read_u8(SynReader* r) {
    if (r->pos >= r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    return r->data[r->pos++];
}

static int8_t read_i8(SynReader* r) {
    return (int8_t)read_u8(r);
}

static uint16_t read_b_u16(SynReader* r) {
    uint8_t hi = read_u8(r);
    uint8_t lo = read_u8(r);
    return (uint16_t)((hi << 8) | lo);
}

static uint32_t read_b_u32(SynReader* r) {
    uint16_t hi = read_b_u16(r);
    uint16_t lo = read_b_u16(r);
    return ((uint32_t)hi << 16) | lo;
}

static void reader_skip(SynReader* r, size_t bytes) {
    r->pos += bytes;
}

static void reader_read_bytes(SynReader* r, uint8_t* dest, size_t count) {
    for (size_t i = 0; i < count; i++)
        dest[i] = read_u8(r);
}

static void reader_read_signed(SynReader* r, int8_t* dest, size_t count) {
    for (size_t i = 0; i < count; i++)
        dest[i] = read_i8(r);
}

// Visited positions tracking

static void clear_visited(SynModule* m) {
    memset(m->visited, 0, sizeof(m->visited));
}

static bool has_position_been_visited(SynModule* m, uint16_t pos) {
    if (pos >= SYN_MAX_VISITED)
        return false;
    return (m->visited[pos / 8] & (1 << (pos % 8))) != 0;
}

static void mark_position_as_visited(SynModule* m, uint16_t pos) {
    if (pos >= SYN_MAX_VISITED)
        return;
    m->visited[pos / 8] |= (1 << (pos % 8));
}

// Channel helpers (IChannel equivalent)

static void channel_mute(SynChannel* ch) {
    ch->active = false;
}

static void channel_play_sample(SynChannel* ch, int8_t* data, uint32_t start_offset, uint32_t length) {
    ch->sample_data = data;
    ch->sample_offset = start_offset;
    ch->sample_length = start_offset + length;
    ch->loop_start = 0;
    ch->loop_length = 0;
    ch->active = true;
    ch->play_pending = true;
}

static void channel_set_loop_offset(SynChannel* ch, uint32_t start, uint32_t length) {
    ch->loop_start = start;
    ch->loop_length = length;
    ch->sample_length = start + length;
}

static void channel_set_sample(SynChannel* ch, int8_t* data, uint32_t start, uint32_t length) {
    ch->sample_data = data;
    ch->sample_offset = start;
    ch->sample_length = start + length;
    // Does not retrigger: the new buffer takes over at the loop end.
    ch->swap_pending = true;
}

static void channel_set_amiga_period(SynChannel* ch, uint16_t period) {
    ch->period = period;
}

static void channel_set_amiga_volume(SynChannel* ch, uint16_t vol) {
    if (vol > 64)
        vol = 64;
    ch->volume = vol;
}

// Build vibrato table (triangle)

static void build_vibrato_table(SynModule* m) {
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

// Load module

// Lookups that tolerate indices from module data: a missing entry reads as silence.
static const int8_t s_empty_waveform[SYN_WAVE_ALLOC];
static const uint8_t s_empty_egc_table[SYN_EGC_TABLE_LENGTH];

static int8_t* waveform_at(const SynModule* m, unsigned index) {
    return index < (unsigned)m->num_waveforms ? m->waveforms[index] : (int8_t*)s_empty_waveform;
}

static const uint8_t* egc_table_at(const SynModule* m, unsigned index) {
    return index < (unsigned)m->num_egc_tables ? &m->egc_tables[index * SYN_EGC_TABLE_LENGTH] : s_empty_egc_table;
}

// One entry of an envelope table; a position past its end reads on into the next table, past the last one it is 0.
static uint8_t egc_value(const SynModule* m, unsigned index, unsigned position) {
    const size_t at = (size_t)index * SYN_EGC_TABLE_LENGTH + position;
    if (index >= (unsigned)m->num_egc_tables || at >= (size_t)m->num_egc_tables * SYN_EGC_TABLE_LENGTH)
        return 0;
    return m->egc_tables[at];
}

static uint16_t period_at(unsigned note) {
    return note < sizeof(syn_periods) / sizeof(syn_periods[0]) ? syn_periods[note] : 0;
}

static SynInstrument* instrument_at(SynModule* m, unsigned number) {
    return (number >= 1 && number <= (unsigned)m->num_instruments) ? &m->instruments[number - 1] : nullptr;
}

//////////////

static bool load_module(SynModule* m, SynReader* r) {
    // Read meta data
    uint16_t total_positions = read_b_u16(r);
    uint16_t total_track_rows = read_b_u16(r);

    reader_skip(r, 4); // skip 4 bytes

    uint8_t num_samples = read_u8(r);
    uint8_t num_waveforms = read_u8(r);
    uint8_t num_instruments = read_u8(r);
    uint8_t num_sub_songs = read_u8(r);
    uint8_t num_egc_tables = read_u8(r);
    uint8_t num_adsr_tables = read_u8(r);
    read_u8(r); // noise length - not used

    if (reader_eof(r))
        return false;

    // Read module name (skip 13 bytes, then 28 bytes name, then 140 bytes text)
    reader_skip(r, 13);
    reader_read_bytes(r, (uint8_t*)m->title, 28);
    m->title[28] = '\0';
    reader_skip(r, 140); // skip text

    // Read sample information
    m->num_samples = num_samples;
    m->samples = (SynSample*)calloc(num_samples, sizeof(SynSample));
    if (!m->samples)
        return false;

    for (int i = 0; i < num_samples; i++) {
        reader_skip(r, 1);  // skip 1 byte
        reader_skip(r, 27); // sample name
        if (reader_eof(r))
            return false;
    }

    for (int i = 0; i < num_samples; i++)
        m->samples[i].length = read_b_u32(r);

    if (reader_eof(r))
        return false;

    // Read envelope generator tables
    m->num_egc_tables = num_egc_tables;
    int egc_total = num_egc_tables * SYN_EGC_TABLE_LENGTH;
    m->egc_tables = (uint8_t*)malloc(egc_total);
    if (!m->egc_tables)
        return false;
    reader_read_bytes(r, m->egc_tables, egc_total);
    if (reader_eof(r))
        return false;

    // Read ADSR tables
    m->num_adsr_tables = num_adsr_tables;
    int adsr_total = num_adsr_tables * SYN_ADSR_TABLE_LENGTH;
    m->adsr_tables = (uint8_t*)malloc(adsr_total);
    if (!m->adsr_tables)
        return false;
    reader_read_bytes(r, m->adsr_tables, adsr_total);
    if (reader_eof(r))
        return false;

    // Read instrument information
    m->num_instruments = num_instruments;
    m->instruments = (SynInstrument*)calloc(num_instruments, sizeof(SynInstrument));
    if (!m->instruments)
        return false;

    for (int i = 0; i < num_instruments; i++) {
        SynInstrument* instr = &m->instruments[i];

        instr->waveform_number = read_u8(r);
        instr->synthesis_enabled = read_u8(r) != 0;
        instr->waveform_length = read_b_u16(r);
        instr->repeat_length = read_b_u16(r);
        instr->volume = read_u8(r);
        instr->portamento_speed = read_i8(r);
        instr->adsr_enabled = read_u8(r) != 0;
        instr->adsr_table_number = read_u8(r);
        instr->adsr_table_length = read_b_u16(r);
        reader_skip(r, 1);
        instr->arpeggio_start = read_u8(r);
        instr->arpeggio_length = read_u8(r);
        instr->arpeggio_repeat_length = read_u8(r);
        instr->effect = (SynSynthEffect)read_u8(r);
        instr->effect_arg1 = read_u8(r);
        instr->effect_arg2 = read_u8(r);
        instr->effect_arg3 = read_u8(r);
        instr->vibrato_delay = read_u8(r);
        instr->vibrato_speed = read_u8(r);
        instr->vibrato_level = read_u8(r);
        instr->egc_offset = read_u8(r);
        instr->egc_mode = (SynEgcMode)read_u8(r);
        instr->egc_table_number = read_u8(r);
        instr->egc_table_length = read_b_u16(r);

        // Synth instruments play from a 256 byte buffer.
        if (instr->synthesis_enabled && instr->waveform_length > SYN_WAVE_SIZE)
            instr->waveform_length = SYN_WAVE_SIZE;

        if (reader_eof(r))
            return false;
    }

    // Read arpeggio tables
    reader_read_bytes(r, m->arpeggio_tables, 16 * SYN_ARPEGGIO_TABLE_LENGTH);
    if (reader_eof(r))
        return false;

    // Read sub-song information
    m->num_sub_songs = num_sub_songs;
    m->sub_songs = (SynSongInfo*)calloc(num_sub_songs, sizeof(SynSongInfo));
    if (!m->sub_songs)
        return false;

    for (int i = 0; i < num_sub_songs; i++) {
        reader_skip(r, 4);

        SynSongInfo* song = &m->sub_songs[i];
        song->start_speed = read_u8(r);
        song->rows_per_track = read_u8(r);
        song->first_position = read_b_u16(r);
        song->last_position = read_b_u16(r);
        song->restart_position = read_b_u16(r);

        if (reader_eof(r))
            return false;

        reader_skip(r, 2);
    }

    // Skip extra sub-song information
    reader_skip(r, 14);

    // Read waveforms
    m->num_waveforms = num_waveforms;
    m->waveforms = (int8_t**)calloc(num_waveforms, sizeof(int8_t*));
    if (!m->waveforms)
        return false;

    for (int i = 0; i < num_waveforms; i++) {
        m->waveforms[i] = (int8_t*)calloc(SYN_WAVE_ALLOC, 1);
        if (!m->waveforms[i])
            return false;
        reader_read_signed(r, m->waveforms[i], SYN_WAVE_SIZE);
        if (reader_eof(r))
            return false;
    }

    // Read position information
    if (total_positions == 0)
        return false;
    m->num_positions = total_positions;
    m->positions = (SynSinglePositionInfo**)calloc(total_positions, sizeof(SynSinglePositionInfo*));
    if (!m->positions)
        return false;

    for (int i = 0; i < total_positions; i++) {
        m->positions[i] = (SynSinglePositionInfo*)calloc(4, sizeof(SynSinglePositionInfo));
        if (!m->positions[i])
            return false;

        for (int j = 0; j < 4; j++) {
            m->positions[i][j].start_track_row = read_b_u16(r);
            m->positions[i][j].sound_transpose = read_i8(r);
            m->positions[i][j].note_transpose = read_i8(r);
        }

        if (reader_eof(r))
            return false;
    }

    // Read track rows (add extra 64 empty rows)
    m->num_track_lines = total_track_rows + 64;
    m->track_lines = (SynTrackLine*)calloc(m->num_track_lines, sizeof(SynTrackLine));
    if (!m->track_lines)
        return false;

    for (int i = 0; i < m->num_track_lines; i++) {
        uint8_t byt1 = read_u8(r);
        uint8_t byt2 = read_u8(r);
        uint8_t byt3 = read_u8(r);
        uint8_t byt4 = read_u8(r);

        m->track_lines[i].note = byt1;
        m->track_lines[i].instrument = byt2;
        m->track_lines[i].arpeggio = (byt3 & 0xf0) >> 4;
        m->track_lines[i].effect = (SynEffect)(byt3 & 0x0f);
        m->track_lines[i].effect_arg = byt4;

        if (reader_eof(r))
            return false;
    }

    // Read sample data
    for (int i = 0; i < num_samples; i++) {
        uint32_t len = m->samples[i].length;
        if (len == 0)
            continue;

        if (len > r->size || r->pos > r->size - len)
            return false;

        m->samples[i].sample_addr = (int8_t*)malloc(len);
        if (!m->samples[i].sample_addr)
            return false;

        memcpy(m->samples[i].sample_addr, r->data + r->pos, len);
        r->pos += len;
    }

    return true;
}

// Initialize sound

static void initialize_sound(SynModule* m, int sub_song) {
    SynSongInfo* song = &m->sub_songs[sub_song];
    m->current_song_info = song;

    SynGlobalPlayingInfo* pi = &m->playing_info;
    pi->sync_mark = 0;
    pi->speed_counter = song->start_speed;
    pi->current_speed = song->start_speed;
    pi->song_position = song->first_position;
    pi->row_position = song->rows_per_track;
    pi->rows_per_track = song->rows_per_track;
    pi->transpose_enable_status = SYN_TRANSPOSE_ENABLED;
    pi->shown_position = song->first_position;
    pi->shown_row = 0;
    pi->shown_rows = song->rows_per_track;

    for (int i = 0; i < 4; i++) {
        SynVoiceInfo* v = &m->voices[i];
        memset(v, 0, sizeof(SynVoiceInfo));
        // Arrays are zero-filled by memset
    }

    // Reset channels
    for (int i = 0; i < 4; i++) {
        SynChannel* ch = &m->channels[i];
        memset(ch, 0, sizeof(SynChannel));
    }

    m->loop_count = 0;
}

// Forward declarations

static void play_tick(SynModule* m);

// PlayRow: Initialize channel for a single row

static void play_row(SynModule* m, SynVoiceInfo* v, SynChannel* ch) {
    v->current_volume = 0;
    v->slide_speed = 0;

    if (v->effect != SYN_EFFECT_NONE) {
        v->synth_effect_disabled = false;
        v->synth_effect = SYN_SYNTH_NONE;
        v->synth_effect_arg1 = 0;
        v->synth_effect_arg2 = 0;
        v->synth_effect_arg3 = 0;

        v->egc_disabled = false;
        v->new_volume = 0;

        switch (v->effect) {
            case SYN_EFFECT_SLIDE:
                v->slide_speed = (int8_t)v->effect_arg;
                break;

            case SYN_EFFECT_RESTART_ADSR:
                v->adsr_position = v->effect_arg;
                v->adsr_enabled = true;
                break;

            case SYN_EFFECT_RESTART_EGC:
                v->egc_position = v->effect_arg;
                v->egc_mode = SYN_EGC_ONES;
                break;

            case SYN_EFFECT_SET_TRACK_LEN:
                if (v->effect_arg <= 64)
                    m->playing_info.rows_per_track = v->effect_arg;
                break;

            case SYN_EFFECT_SKIP_STNT:
                m->playing_info.transpose_enable_status = (SynTransposeMode)v->effect_arg;
                break;

            case SYN_EFFECT_SYNC_MARK:
                m->playing_info.sync_mark = v->effect_arg;
                break;

            case SYN_EFFECT_SET_FILTER:
                // AmigaFilter = v->effect_arg == 0; (not relevant for WASM)
                break;

            case SYN_EFFECT_SET_SPEED:
                if ((v->effect_arg > 0) && (v->effect_arg <= 16))
                    m->playing_info.current_speed = v->effect_arg;
                break;

            case SYN_EFFECT_ENABLE_FX:
                v->synth_effect_disabled = v->effect_arg != 0;
                break;

            case SYN_EFFECT_CHANGE_FX:
                v->synth_effect = (SynSynthEffect)v->effect_arg;
                break;

            case SYN_EFFECT_CHANGE_ARG1:
                v->synth_effect_arg1 = v->effect_arg;
                break;

            case SYN_EFFECT_CHANGE_ARG2:
                v->synth_effect_arg2 = v->effect_arg;
                break;

            case SYN_EFFECT_CHANGE_ARG3:
                v->synth_effect_arg3 = v->effect_arg;
                break;

            case SYN_EFFECT_EGC_OFF:
                v->egc_disabled = v->effect_arg != 0;
                break;

            case SYN_EFFECT_SET_VOLUME:
                v->new_volume = v->effect_arg;
                break;

            default:
                break;
        }
    }

    uint8_t note = v->note;
    if (note != 0) {
        if (note == 0x7f) {
            channel_mute(ch);
            v->current_volume = 0;
            return;
        }

        if ((m->playing_info.transpose_enable_status != SYN_TRANSPOSE_NOTE_DISABLED)
            && (m->playing_info.transpose_enable_status != SYN_TRANSPOSE_ALL_DISABLED))
            note = (uint8_t)(note + v->note_transpose);

        v->previous_transposed_note = v->transposed_note;
        v->transposed_note = note;

        channel_set_amiga_period(ch, period_at(note));

        uint8_t instr_num = v->instrument;
        if (instr_num != 0) {
            if ((m->playing_info.transpose_enable_status != SYN_TRANSPOSE_SOUND_DISABLED)
                && (m->playing_info.transpose_enable_status != SYN_TRANSPOSE_ALL_DISABLED))
                instr_num = (uint8_t)(instr_num + v->sound_transpose);

            v->transposed_instrument = instr_num;

            SynInstrument* instr = instrument_at(m, instr_num);
            if (instr == nullptr) {
                channel_mute(ch);
                v->transposed_instrument = 0;
                return;
            }

            v->adsr_enabled = false;
            v->adsr_position = 0;

            v->vibrato_delay = 0;
            v->vibrato_position = 0;

            v->egc_mode = SYN_EGC_OFF;
            v->egc_position = 0;

            v->slide_increment = 0;
            v->arpeggio_position = 0;

            v->portamento_speed = instr->portamento_speed;
            v->portamento_speed_counter = instr->portamento_speed;

            if (v->effect == SYN_EFFECT_CHANGE_ARG1) {
                v->portamento_speed = 0;
                v->portamento_speed_counter = 0;
            }

            v->vibrato_delay = instr->vibrato_delay;

            if (instr->adsr_enabled)
                v->adsr_enabled = true;

            if (instr->synthesis_enabled) {
                int8_t* waveform = waveform_at(m, instr->waveform_number);

                if (instr->effect != SYN_SYNTH_NONE) {
                    v->slow_motion_counter = 0;
                    v->synth_position = 0;
                }

                v->use_buffer = 1;

                v->egc_mode = instr->egc_mode;

                // The channel starts on the synth buffer; with an envelope generator counter the buffer is
                // filled by the counter on every tick instead of here.
                uint16_t length = instr->waveform_length;
                if (length > SYN_WAVE_SIZE)
                    length = SYN_WAVE_SIZE;

                channel_play_sample(ch, v->synth_sample1, 0, length);
                channel_set_loop_offset(ch, 0, length);

                if (instr->egc_mode == SYN_EGC_OFF) {
                    v->slow_motion_counter = 0;

                    memcpy(v->synth_sample1, waveform, length);

                    if (instr->egc_offset != 0) {
                        for (int i = 0; i < instr->egc_offset; i++)
                            v->synth_sample1[i] = (int8_t)-v->synth_sample1[i];
                    }
                }

                v->current_volume = v->new_volume != 0 ? v->new_volume : instr->volume;
                channel_set_amiga_volume(ch, v->current_volume);
            } else {
                if (instr->waveform_length != 0) {
                    v->slow_motion_counter = 0;
                    v->synth_position = 0;

                    uint8_t sample_num = (uint8_t)(instr->waveform_number & 0x3f);
                    if (sample_num >= m->num_samples) {
                        channel_mute(ch);
                        return;
                    }

                    SynSample* sample = &m->samples[sample_num];

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

                    if (sample->sample_addr == nullptr) {
                        channel_mute(ch);
                        return;
                    }
                    if (play_length > sample->length)
                        play_length = sample->length;
                    if (loop_start + loop_length > play_length)
                        loop_length = loop_start < play_length ? play_length - loop_start : 0;

                    channel_play_sample(ch, sample->sample_addr, 0, play_length);

                    if (loop_length != 0)
                        channel_set_loop_offset(ch, loop_start, loop_length);

                    uint8_t volume = v->new_volume != 0 ? v->new_volume : instr->volume;
                    channel_set_amiga_volume(ch, volume);

                    if (sample_num == 7)
                        v->current_volume = (uint8_t)(v->effect_arg & 0x3f);
                    else
                        v->current_volume = volume;
                }
            }
        }
    }
}

// GetNextRow

static void get_next_row(SynModule* m) {
    SynGlobalPlayingInfo* pi = &m->playing_info;
    pi->speed_counter = 0;

    if (pi->row_position >= pi->rows_per_track) {
        pi->row_position = 0;

        if (pi->song_position > m->current_song_info->last_position)
            pi->song_position = m->current_song_info->restart_position;

        if (has_position_been_visited(m, pi->song_position)) {
            m->loop_count++;
            clear_visited(m);
        }

        mark_position_as_visited(m, pi->song_position);
        pi->song_position++;

        if (pi->song_position < 1 || pi->song_position > m->num_positions)
            pi->song_position = 1;
        SynSinglePositionInfo* pos_row = m->positions[pi->song_position - 1];

        for (int i = 0; i < 4; i++) {
            SynVoiceInfo* v = &m->voices[i];
            v->start_track_row = pos_row[i].start_track_row;
            v->sound_transpose = pos_row[i].sound_transpose;
            v->note_transpose = pos_row[i].note_transpose;
        }
    }

    for (int i = 0; i < 4; i++) {
        SynVoiceInfo* v = &m->voices[i];
        int position = v->start_track_row + pi->row_position;

        SynTrackLine empty = { 0 };
        SynTrackLine* tl = (position < m->num_track_lines) ? &m->track_lines[position] : &empty;

        v->note = tl->note;
        v->instrument = tl->instrument;
        v->arpeggio = tl->arpeggio;
        v->effect = tl->effect;
        v->effect_arg = tl->effect_arg;
    }

    for (int i = 0; i < 4; i++)
        play_row(m, &m->voices[i], &m->channels[i]);

    pi->shown_position = (uint16_t)(pi->song_position - 1);
    pi->shown_row = pi->row_position;
    pi->shown_rows = pi->rows_per_track;
    pi->row_position++;
}

// DoArpeggio

static bool do_arpeggio(SynModule* m, SynInstrument* instr, SynVoiceInfo* v, SynChannel* ch, SynPeriodInfo* pi_out) {
    uint16_t period, previous_period;

    uint8_t arp_num = v->arpeggio;
    if (arp_num == 0) {
        uint8_t note = v->transposed_note;
        if (note == 0) {
            channel_mute(ch);
            return false;
        }

        uint8_t prev_note = v->previous_transposed_note;

        // Do arpeggio
        uint8_t arp_len = (uint8_t)(instr->arpeggio_length + instr->arpeggio_repeat_length);
        if (arp_len != 0) {
            uint8_t arp_val
                = m->arpeggio_tables[(instr->arpeggio_start + v->arpeggio_position) % sizeof(m->arpeggio_tables)];

            if (v->arpeggio_position == arp_len)
                v->arpeggio_position = instr->arpeggio_length;
            else
                v->arpeggio_position++;

            note += arp_val;
            prev_note += arp_val;
        }

        period = period_at(note);
        previous_period = period_at(prev_note);
    } else {
        // Arpeggio in track
        uint8_t note = v->transposed_note;
        uint8_t prev_note = v->previous_transposed_note;

        uint8_t arp_val
            = m->arpeggio_tables[(arp_num * 16 + m->playing_info.speed_counter) % sizeof(m->arpeggio_tables)];
        note += arp_val;
        prev_note += arp_val;

        period = period_at(note);
        previous_period = period_at(prev_note);
    }

    pi_out->period = period;
    pi_out->previous_period = previous_period;
    return true;
}

// DoPortamento

static void do_portamento(SynPeriodInfo* pi, SynVoiceInfo* v) {
    if ((v->portamento_speed_counter != 0) && (pi->period != pi->previous_period)) {
        v->portamento_speed_counter--;

        // Swap
        uint16_t tmp = pi->period;
        pi->period = pi->previous_period;
        pi->previous_period = tmp;

        int new_period = (pi->period - pi->previous_period) * v->portamento_speed_counter;

        if (v->portamento_speed != 0)
            new_period /= v->portamento_speed;

        new_period += pi->previous_period;
        pi->period = (uint16_t)new_period;
    }
}

// DoVibrato

static void do_vibrato(SynPeriodInfo* pi, SynInstrument* instr, SynVoiceInfo* v, SynModule* m) {
    if (instr->vibrato_level != 0) {
        if (v->vibrato_delay == 0) {
            int8_t vib_val = m->vibrato_table[v->vibrato_position];
            uint8_t vib_level = instr->vibrato_level;

            if (vib_val < 0) {
                if (vib_level != 0)
                    pi->period -= (uint16_t)((-vib_val * 4) / vib_level);
                else
                    pi->period = 124;
            } else {
                if (vib_level != 0)
                    pi->period += (uint16_t)((vib_val * 4) / vib_level);
                else
                    pi->period = 124;
            }

            v->vibrato_position += instr->vibrato_speed;
        } else
            v->vibrato_delay--;
    }
}

// DoAdsr

// Forward declaration
static bool do_adsr_impl(SynModule* m, SynInstrument* instr, SynVoiceInfo* v, SynChannel* ch);

// DoEnvelopeGeneratorCounter

static void do_egc(SynModule* m, SynInstrument* instr, SynVoiceInfo* v, SynChannel* ch) {
    if ((v->egc_mode != SYN_EGC_OFF) && !v->egc_disabled) {
        int8_t* waveform = waveform_at(m, instr->waveform_number);
        int8_t* synth_buf;

        v->use_buffer ^= 1;

        if (v->use_buffer == 0)
            synth_buf = v->synth_sample2;
        else
            synth_buf = v->synth_sample1;

        // A sample instrument's length is not clamped at load; the synth buffers hold SYN_WAVE_ALLOC bytes
        const uint16_t length = instr->waveform_length < SYN_WAVE_ALLOC ? instr->waveform_length : SYN_WAVE_ALLOC;
        channel_set_sample(ch, synth_buf, 0, length);
        channel_set_loop_offset(ch, 0, length);

        for (int i = 0; i < length / 16; i++)
            memcpy(synth_buf + i * 16, waveform + i * 16, 16);

        uint8_t egc_val = egc_table_at(m, instr->egc_table_number)[v->egc_position % SYN_EGC_TABLE_LENGTH];
        egc_val += instr->egc_offset;

        if (egc_val != 0) {
            for (int i = 0; i < egc_val; i++)
                synth_buf[i] = (int8_t)-synth_buf[i];
        }

        v->egc_position++;

        if (v->egc_position >= instr->egc_table_length) {
            if (v->egc_mode == SYN_EGC_ONES)
                v->egc_mode = SYN_EGC_OFF;
            else
                v->egc_position = 0;
        }
    }
}

// Synth effects

static void do_synth_rotate1(SynVoiceInfo* v, uint8_t start_pos, uint8_t end_pos, uint8_t speed) {
    if (start_pos <= end_pos) {
        int count = end_pos - start_pos;
        for (int i = start_pos; i <= count; i++)
            v->synth_sample1[i] += (int8_t)speed;
    }
}

static void do_synth_rotate2(SynVoiceInfo* v, uint8_t start_pos, uint8_t end_pos, uint8_t slow_motion) {
    if (v->slow_motion_counter == 0) {
        v->slow_motion_counter = slow_motion;

        if (start_pos <= end_pos) {
            int count = end_pos - start_pos;
            int8_t first = v->synth_sample1[start_pos];

            for (int i = start_pos; i < count; i++)
                v->synth_sample1[i] = v->synth_sample1[i + 1];

            v->synth_sample1[count] = first;
        }
    } else
        v->slow_motion_counter--;
}

static void do_synth_alien(SynModule* m, SynVoiceInfo* v, uint8_t source_wave, uint8_t end_pos, uint8_t slow_motion) {
    if (v->slow_motion_counter == 0) {
        v->slow_motion_counter = slow_motion;

        int8_t* waveform = waveform_at(m, source_wave);

        for (int i = 0; i <= end_pos; i++)
            v->synth_sample1[i] += waveform[i];
    } else
        v->slow_motion_counter--;
}

static void do_synth_negator(SynVoiceInfo* v, uint8_t start_pos, uint8_t end_pos, uint8_t slow_motion) {
    if (v->slow_motion_counter == 0) {
        v->slow_motion_counter = slow_motion;

        if (start_pos <= end_pos) {
            int count = end_pos - start_pos;

            int offset = start_pos + v->synth_position;
            v->synth_sample1[offset] = (int8_t)-v->synth_sample1[offset];

            if (offset == count)
                v->synth_position = 0;
            else
                v->synth_position++;
        }
    } else
        v->slow_motion_counter--;
}

static void do_synth_polyneg(SynVoiceInfo* v, uint8_t start_pos, uint8_t end_pos, uint8_t slow_motion) {
    if (v->slow_motion_counter == 0) {
        v->slow_motion_counter = slow_motion;

        if (start_pos <= end_pos) {
            int count = end_pos - start_pos;

            if (v->synth_position == 0)
                v->synth_sample1[start_pos + count] = (int8_t)-v->synth_sample1[start_pos + count];
            else
                v->synth_sample1[start_pos + v->synth_position - 1]
                    = (int8_t)-v->synth_sample1[start_pos + v->synth_position - 1];

            v->synth_sample1[start_pos + v->synth_position] = (int8_t)-v->synth_sample1[start_pos + v->synth_position];

            if (v->synth_position == count)
                v->synth_position = 0;
            else
                v->synth_position++;
        }
    } else
        v->slow_motion_counter--;
}

static void do_synth_shaker1(SynModule* m, SynVoiceInfo* v, uint8_t source_wave, uint8_t mix_level,
                             uint8_t slow_motion) {
    if (v->slow_motion_counter == 0) {
        v->slow_motion_counter = slow_motion;

        int8_t mix_byte = waveform_at(m, source_wave)[v->synth_position];

        for (int i = 0; i <= mix_level; i++)
            v->synth_sample1[i] += mix_byte;

        if (v->synth_position == mix_level)
            v->synth_position = 0;
        else
            v->synth_position++;
    } else
        v->slow_motion_counter--;
}

static void do_synth_shaker2(SynModule* m, SynVoiceInfo* v, uint8_t source_wave, uint8_t mix_level,
                             uint8_t slow_motion) {
    if (v->slow_motion_counter == 0) {
        v->slow_motion_counter = slow_motion;

        int8_t mix_byte = waveform_at(m, source_wave)[v->synth_position];

        for (int i = 0; i <= mix_level; i++) {
            v->synth_sample1[i] += mix_byte;

            if (i == v->synth_position)
                v->synth_sample1[i] = (int8_t)-v->synth_sample1[i];
        }

        if (v->synth_position == mix_level)
            v->synth_position = 0;
        else
            v->synth_position++;
    } else
        v->slow_motion_counter--;
}

static void do_synth_amf_lfo(SynModule* m, SynVoiceInfo* v, uint8_t source_wave, uint8_t end_pos, uint8_t slow_motion) {
    if (v->slow_motion_counter == 0) {
        v->slow_motion_counter = slow_motion;

        int8_t* waveform = waveform_at(m, source_wave);

        v->slide_increment = (int16_t)-waveform[v->synth_position];

        if (v->synth_position == end_pos)
            v->synth_position = 0;
        else
            v->synth_position++;
    } else
        v->slow_motion_counter--;
}

static void do_synth_laser(SynVoiceInfo* v, uint8_t laser_speed, uint8_t laser_time, uint8_t slow_motion) {
    if (v->slow_motion_counter == 0) {
        v->slow_motion_counter = slow_motion;

        v->slide_increment += (int16_t)-(int8_t)laser_speed;

        if (v->synth_position == laser_time) {
            v->synth_position = 0;
            v->slide_increment = 0;
        } else
            v->synth_position++;
    } else
        v->slow_motion_counter--;
}

static void do_synth_octfx1(SynVoiceInfo* v, uint8_t mix_level, uint8_t slow_motion) {
    if (v->slow_motion_counter == 0) {
        v->slow_motion_counter = slow_motion;

        int count = mix_level / 2;
        int j = 0;

        for (int i = 0; j <= count; i += 2, j++)
            v->synth_sample1[j] = v->synth_sample1[i];

        for (int i = 0; i <= count; i++, j++)
            v->synth_sample1[j] = v->synth_sample1[i];
    } else {
        if (mix_level != 0)
            v->slow_motion_counter--;
    }
}

static void do_synth_octfx2(SynVoiceInfo* v, uint8_t mix_level_arg, uint8_t slow_motion) {
    if (v->slow_motion_counter == 0) {
        v->slow_motion_counter = slow_motion;

        uint8_t mix_level = mix_level_arg;

        for (int i = mix_level / 2; i >= 0; i--) {
            int8_t sample = v->synth_sample1[i];
            v->synth_sample1[--mix_level] = sample;
            v->synth_sample1[--mix_level] = sample;
        }
    } else
        v->slow_motion_counter--;
}

static void do_synth_alising(SynVoiceInfo* v, uint8_t mix_level, uint8_t alising_level, uint8_t slow_motion) {
    if (v->slow_motion_counter == 0) {
        v->slow_motion_counter = slow_motion;

        int offset = 0;

        for (int i = 0; i <= mix_level; i++) {
            int8_t sample1 = v->synth_sample1[offset];
            int8_t sample2 = v->synth_sample1[i + 1];

            if (sample2 > sample1)
                v->synth_sample1[offset++] = (int8_t)(sample1 + alising_level);
            else if (sample2 < sample1)
                v->synth_sample1[offset++] = (int8_t)(sample1 - alising_level);
        }
    } else
        v->slow_motion_counter--;
}

static void do_synth_egfx1(SynModule* m, SynVoiceInfo* v, uint8_t mix_level_arg, uint8_t eg_num, uint8_t slow_motion) {
    if (v->slow_motion_counter == 0) {
        v->slow_motion_counter = slow_motion;

        uint8_t mix_level = mix_level_arg;
        if (mix_level > SYN_EGC_TABLE_LENGTH - 1)
            mix_level = SYN_EGC_TABLE_LENGTH - 1;

        for (int i = 0; i <= mix_level; i++) {
            int8_t sample1 = v->synth_sample1[i];
            int8_t sample2 = v->synth_sample1[i + 1];

            if (sample2 > sample1)
                v->synth_sample1[i] = (int8_t)(sample1 + egc_value(m, eg_num, v->synth_position));
            else if (sample2 < sample1)
                v->synth_sample1[i] = (int8_t)(sample1 - egc_value(m, eg_num, v->synth_position));
        }

        if (v->synth_position == mix_level)
            v->synth_position = 0;
        else
            v->synth_position++;
    } else
        v->slow_motion_counter--;
}

static void do_synth_egfx2(SynModule* m, SynVoiceInfo* v, uint8_t mix_level, uint8_t eg_num, uint8_t slow_motion) {
    if (v->slow_motion_counter == 0) {
        v->slow_motion_counter = slow_motion;

        for (int i = 0, j = mix_level; i <= mix_level; i++, j--) {
            int8_t sample1 = v->synth_sample1[i];
            int8_t sample2 = v->synth_sample1[i + 1];

            if (sample2 > sample1)
                v->synth_sample1[i] = (int8_t)(sample1 + egc_value(m, eg_num, (unsigned)j));
            else if (sample2 < sample1)
                v->synth_sample1[i] = (int8_t)(sample1 - egc_value(m, eg_num, (unsigned)j));
        }
    } else
        v->slow_motion_counter--;
}

static void do_synth_changer(SynModule* m, SynVoiceInfo* v, uint8_t dest_wave, uint8_t mix_level, uint8_t slow_motion) {
    if (v->slow_motion_counter == 0) {
        v->slow_motion_counter = slow_motion;

        int8_t* waveform = waveform_at(m, dest_wave);

        for (int i = 0; i <= mix_level; i++) {
            uint8_t sample1 = (uint8_t)v->synth_sample1[i];
            uint8_t sample2 = (uint8_t)waveform[i];

            if (sample2 > sample1)
                v->synth_sample1[i] = (int8_t)(sample1 + 1);
            else if (sample2 < sample1)
                v->synth_sample1[i] = (int8_t)(sample1 - 1);
        }
    } else
        v->slow_motion_counter--;
}

static void do_synth_fmdrum(SynVoiceInfo* v, uint8_t mod_level, uint8_t mod_factor, uint8_t mod_depth) {
    v->slide_increment += (int16_t)(mod_level * mod_factor);

    if (v->synth_position == mod_depth) {
        v->synth_position = 0;
        v->slide_increment = 0;
    } else
        v->synth_position++;
}

// DoSynthEffects

static void do_synth_effects(SynModule* m, SynInstrument* instr, SynVoiceInfo* v) {
    if (instr->effect != SYN_SYNTH_NONE) {
        SynSynthEffect effect = v->synth_effect != SYN_SYNTH_NONE ? v->synth_effect : instr->effect;

        uint8_t arg1 = v->synth_effect_arg1 != 0 ? v->synth_effect_arg1 : instr->effect_arg1;
        uint8_t arg2 = v->synth_effect_arg2 != 0 ? v->synth_effect_arg2 : instr->effect_arg2;
        uint8_t arg3 = v->synth_effect_arg3 != 0 ? v->synth_effect_arg3 : instr->effect_arg3;

        switch (effect) {
            case SYN_SYNTH_ROTATE1:
                do_synth_rotate1(v, arg1, arg2, arg3);
                break;
            case SYN_SYNTH_ROTATE2:
                do_synth_rotate2(v, arg1, arg2, arg3);
                break;
            case SYN_SYNTH_ALIEN:
                do_synth_alien(m, v, arg1, arg2, arg3);
                break;
            case SYN_SYNTH_NEGATOR:
                do_synth_negator(v, arg1, arg2, arg3);
                break;
            case SYN_SYNTH_POLYNEG:
                do_synth_polyneg(v, arg1, arg2, arg3);
                break;
            case SYN_SYNTH_SHAKER1:
                do_synth_shaker1(m, v, arg1, arg2, arg3);
                break;
            case SYN_SYNTH_SHAKER2:
                do_synth_shaker2(m, v, arg1, arg2, arg3);
                break;
            case SYN_SYNTH_AMF_LFO:
                do_synth_amf_lfo(m, v, arg1, arg2, arg3);
                break;
            case SYN_SYNTH_LASER:
                do_synth_laser(v, arg1, arg2, arg3);
                break;
            case SYN_SYNTH_OCTFX1:
                do_synth_octfx1(v, arg1, arg3);
                break;
            case SYN_SYNTH_OCTFX2:
                do_synth_octfx2(v, arg1, arg3);
                break;
            case SYN_SYNTH_ALISING:
                do_synth_alising(v, arg1, arg2, arg3);
                break;
            case SYN_SYNTH_EGFX1:
                do_synth_egfx1(m, v, arg1, arg2, arg3);
                break;
            case SYN_SYNTH_EGFX2:
                do_synth_egfx2(m, v, arg1, arg2, arg3);
                break;
            case SYN_SYNTH_CHANGER:
                do_synth_changer(m, v, arg1, arg2, arg3);
                break;
            case SYN_SYNTH_FMDRUM:
                do_synth_fmdrum(v, arg1, arg2, arg3);
                break;
            default:
                break;
        }
    }
}

// DoEffects: Run effects for a single channel

static void do_effects(SynModule* m, SynVoiceInfo* v, SynChannel* ch) {
    if (v->transposed_instrument == 0) {
        channel_mute(ch);
        return;
    }

    SynInstrument* instr = instrument_at(m, v->transposed_instrument);
    if (instr == nullptr) {
        channel_mute(ch);
        return;
    }

    SynPeriodInfo pi;
    if (!do_arpeggio(m, instr, v, ch, &pi))
        return;

    if (instr->effect != SYN_SYNTH_NONE) {
        if (((instr->egc_mode == SYN_EGC_OFF) || v->egc_disabled) && !v->synth_effect_disabled)
            do_synth_effects(m, instr, v);
    }

    do_portamento(&pi, v);
    do_vibrato(&pi, instr, v, m);

    pi.period = (uint16_t)(pi.period + v->slide_increment);

    channel_set_amiga_period(ch, pi.period);

    v->slide_increment = (int16_t)(v->slide_increment - v->slide_speed);

    if (do_adsr_impl(m, instr, v, ch))
        do_egc(m, instr, v, ch);
}

static bool do_adsr_impl(SynModule* m, SynInstrument* instr, SynVoiceInfo* v, SynChannel* ch) {
    if (v->adsr_enabled) {
        if (v->adsr_position >= instr->adsr_table_length) {
            v->transposed_instrument = 0;
            channel_mute(ch);
            return false;
        }

        uint16_t adsr_val = 0;
        if (instr->adsr_table_number < m->num_adsr_tables)
            adsr_val = m->adsr_tables[instr->adsr_table_number * SYN_ADSR_TABLE_LENGTH
                                      + v->adsr_position % SYN_ADSR_TABLE_LENGTH];
        adsr_val++;

        uint16_t volume = v->new_volume != 0 ? v->new_volume : instr->volume;
        volume = (uint16_t)((volume * adsr_val) / 128);
        if (volume > 64)
            volume = 64;

        channel_set_amiga_volume(ch, volume);

        v->adsr_position++;
    }

    return true;
}

// DoEffectsAndSynths

static void do_effects_and_synths(SynModule* m) {
    for (int i = 0; i < 4; i++)
        do_effects(m, &m->voices[i], &m->channels[i]);
}

// Play tick

static void play_tick(SynModule* m) {
    m->playing_info.speed_counter++;

    if (m->playing_info.speed_counter >= m->playing_info.current_speed)
        get_next_row(m);

    do_effects_and_synths(m);
}

// Voice hand-off

// Hand the registers the tick wrote to the voice. Channel n is Paula channel n (0 and 3 left, 1 and 2 right).
static void sync_voice(SynModule* m, int index) {
    SynChannel* c = &m->channels[index];
    const uint32_t voice = (uint32_t)index;

    if (!c->active || c->sample_data == nullptr || c->sample_length == 0) {
        if (nuade_voice_is_playing(m->voices_out, voice))
            nuade_voice_stop(m->voices_out, voice);
    } else if (c->play_pending || c->swap_pending) {
        const NuadeSample sample = { .data = c->sample_data,
                                     .format = NuadeSampleFormat_S8,
                                     .length = c->sample_length,
                                     .loop_start = c->loop_start,
                                     .loop_length = c->loop_length };
        if (c->play_pending)
            nuade_voice_play(m->voices_out, voice, &sample, c->sample_offset);
        else
            nuade_voice_queue(m->voices_out, voice, &sample);
    }
    c->play_pending = false;
    c->swap_pending = false;

    nuade_voice_set_pitch(m->voices_out, voice, c->period != 0 ? nuade_paula_period_hz(c->period) : 0.0);
    nuade_voice_set_volume(m->voices_out, voice, (c->period != 0 ? (float)c->volume / 64.0f : 0.0f));
    nuade_voice_set_pan(m->voices_out, voice, (index == 0 || index == 3) ? -1.0f : 1.0f);
}

bool syn_tick(SynModule* module) {
    // A one-shot that ran off its end is over.
    for (int i = 0; i < SYN_CHANNELS; i++) {
        if (module->channels[i].active && !nuade_voice_is_playing(module->voices_out, (uint32_t)i))
            module->channels[i].active = false;
    }

    const uint32_t loops = module->loop_count;
    play_tick(module);

    for (int i = 0; i < SYN_CHANNELS; i++)
        sync_voice(module, i);
    return module->loop_count != loops;
}

// Public API

SynModule* syn_create(const uint8_t* data, size_t size) {
    if (!syn_probe(data, size))
        return nullptr;

    const int start_offset = memcmp(data, "Synth4.0", 8) == 0 ? 0 : SYN_V42_OFFSET;

    SynModule* m = (SynModule*)calloc(1, sizeof(SynModule));
    if (!m)
        return nullptr;

    // Keep original data for export
    m->original_data = (uint8_t*)malloc(size);
    if (m->original_data) {
        memcpy(m->original_data, data, size);
        m->original_size = size;
    }
    m->start_offset = start_offset;

    SynReader reader;
    reader_init(&reader, data + start_offset + 8, size - start_offset - 8);

    if (!load_module(m, &reader)) {
        syn_destroy(m);
        return nullptr;
    }

    build_vibrato_table(m);

    return m;
}

void syn_destroy(SynModule* module) {
    if (!module)
        return;

    if (module->sub_songs)
        free(module->sub_songs);

    if (module->positions) {
        for (int i = 0; i < module->num_positions; i++)
            free(module->positions[i]);
        free(module->positions);
    }

    if (module->track_lines)
        free(module->track_lines);
    if (module->instruments)
        free(module->instruments);

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

    if (module->egc_tables)
        free(module->egc_tables);
    if (module->adsr_tables)
        free(module->adsr_tables);

    if (module->original_data)
        free(module->original_data);
    free(module);
}

int syn_subsong_count(const SynModule* module) {
    if (!module)
        return 0;
    return module->num_sub_songs;
}

bool syn_start(SynModule* module, int subsong, NuadeVoices* voices) {
    if (!module || !voices || subsong < 0 || subsong >= module->num_sub_songs)
        return false;

    module->voices_out = voices;
    clear_visited(module);
    initialize_sound(module, subsong);
    return true;
}

SynPosition syn_position(const SynModule* module) {
    const SynGlobalPlayingInfo* pi = &module->playing_info;
    return (SynPosition) {
        .position = pi->shown_position, .row = pi->shown_row, .rows = pi->shown_rows, .speed = pi->current_speed
    };
}

int syn_default_rows(const SynModule* module) {
    return module->current_song_info != nullptr ? module->current_song_info->rows_per_track : 0;
}

SynVoiceState syn_voice_state(const SynModule* module, int voice) {
    const SynVoiceInfo* v = &module->voices[voice];
    return (SynVoiceState) { .note = v->transposed_note, .instrument = v->transposed_instrument };
}

int syn_channel_count(const SynModule* module) {
    (void)module;
    return 4;
}

const char* syn_title(const SynModule* module) {
    return module ? module->title : "";
}

const char* syn_format_name(const SynModule* module) {
    return module ? "Synthesis" : "";
}

bool syn_probe(const uint8_t* data, size_t size) {
    if (!data || size < SYN_MIN_SIZE)
        return false;
    if (memcmp(data, "Synth4.0", 8) == 0)
        return true;
    return size >= SYN_V42_OFFSET + SYN_MIN_SIZE && memcmp(data + SYN_V42_OFFSET, "Synth4.2", 8) == 0;
}

// Edit API

int syn_get_instrument_count(const SynModule* module) {
    return module ? module->num_instruments : 0;
}

int syn_get_num_track_lines(const SynModule* module) {
    return module ? module->num_track_lines : 0;
}

int syn_get_num_positions(const SynModule* module) {
    return module ? module->num_positions : 0;
}

void syn_get_cell(const SynModule* module, int idx, uint8_t* note, uint8_t* instrument, uint8_t* arpeggio,
                  uint8_t* effect, uint8_t* effect_arg) {
    if (!module || idx < 0 || idx >= module->num_track_lines) {
        if (note)
            *note = 0;
        if (instrument)
            *instrument = 0;
        if (arpeggio)
            *arpeggio = 0;
        if (effect)
            *effect = 0;
        if (effect_arg)
            *effect_arg = 0;
        return;
    }
    const SynTrackLine* tl = &module->track_lines[idx];
    if (note)
        *note = tl->note;
    if (instrument)
        *instrument = tl->instrument;
    if (arpeggio)
        *arpeggio = tl->arpeggio;
    if (effect)
        *effect = (uint8_t)tl->effect;
    if (effect_arg)
        *effect_arg = tl->effect_arg;
}

void syn_set_cell(SynModule* module, int idx, uint8_t note, uint8_t instrument, uint8_t arpeggio, uint8_t effect,
                  uint8_t effect_arg) {
    if (!module || idx < 0 || idx >= module->num_track_lines)
        return;
    SynTrackLine* tl = &module->track_lines[idx];
    tl->note = note;
    tl->instrument = instrument;
    tl->arpeggio = arpeggio;
    tl->effect = (SynEffect)effect;
    tl->effect_arg = effect_arg;
}

void syn_get_position(const SynModule* module, int pos, int channel, uint16_t* start_track_row, int8_t* sound_transpose,
                      int8_t* note_transpose) {
    if (!module || pos < 0 || pos >= module->num_positions || channel < 0 || channel >= 4) {
        if (start_track_row)
            *start_track_row = 0;
        if (sound_transpose)
            *sound_transpose = 0;
        if (note_transpose)
            *note_transpose = 0;
        return;
    }
    const SynSinglePositionInfo* pi = &module->positions[pos][channel];
    if (start_track_row)
        *start_track_row = pi->start_track_row;
    if (sound_transpose)
        *sound_transpose = pi->sound_transpose;
    if (note_transpose)
        *note_transpose = pi->note_transpose;
}

void syn_set_position(SynModule* module, int pos, int channel, uint16_t start_track_row, int8_t sound_transpose,
                      int8_t note_transpose) {
    if (!module || pos < 0 || pos >= module->num_positions || channel < 0 || channel >= 4)
        return;
    SynSinglePositionInfo* pi = &module->positions[pos][channel];
    pi->start_track_row = start_track_row;
    pi->sound_transpose = sound_transpose;
    pi->note_transpose = note_transpose;
}

float syn_get_instrument_param(const SynModule* module, int inst, const char* param) {
    if (!module || inst < 0 || inst >= module->num_instruments || !param)
        return -1.0f;
    const SynInstrument* in = &module->instruments[inst];

    if (strcmp(param, "waveformNumber") == 0)
        return (float)in->waveform_number;
    if (strcmp(param, "synthesisEnabled") == 0)
        return in->synthesis_enabled ? 1.0f : 0.0f;
    if (strcmp(param, "waveformLength") == 0)
        return (float)in->waveform_length;
    if (strcmp(param, "repeatLength") == 0)
        return (float)in->repeat_length;
    if (strcmp(param, "volume") == 0)
        return (float)in->volume;
    if (strcmp(param, "portamentoSpeed") == 0)
        return (float)in->portamento_speed;
    if (strcmp(param, "adsrEnabled") == 0)
        return in->adsr_enabled ? 1.0f : 0.0f;
    if (strcmp(param, "adsrTableNumber") == 0)
        return (float)in->adsr_table_number;
    if (strcmp(param, "adsrTableLength") == 0)
        return (float)in->adsr_table_length;
    if (strcmp(param, "arpeggioStart") == 0)
        return (float)in->arpeggio_start;
    if (strcmp(param, "arpeggioLength") == 0)
        return (float)in->arpeggio_length;
    if (strcmp(param, "arpeggioRepeatLength") == 0)
        return (float)in->arpeggio_repeat_length;
    if (strcmp(param, "effect") == 0)
        return (float)in->effect;
    if (strcmp(param, "effectArg1") == 0)
        return (float)in->effect_arg1;
    if (strcmp(param, "effectArg2") == 0)
        return (float)in->effect_arg2;
    if (strcmp(param, "effectArg3") == 0)
        return (float)in->effect_arg3;
    if (strcmp(param, "vibratoDelay") == 0)
        return (float)in->vibrato_delay;
    if (strcmp(param, "vibratoSpeed") == 0)
        return (float)in->vibrato_speed;
    if (strcmp(param, "vibratoLevel") == 0)
        return (float)in->vibrato_level;
    if (strcmp(param, "egcOffset") == 0)
        return (float)in->egc_offset;
    if (strcmp(param, "egcMode") == 0)
        return (float)in->egc_mode;
    if (strcmp(param, "egcTableNumber") == 0)
        return (float)in->egc_table_number;
    if (strcmp(param, "egcTableLength") == 0)
        return (float)in->egc_table_length;

    return -1.0f;
}

void syn_set_instrument_param(SynModule* module, int inst, const char* param, float value) {
    if (!module || inst < 0 || inst >= module->num_instruments || !param)
        return;
    SynInstrument* in = &module->instruments[inst];
    uint16_t v = (uint16_t)value;
    uint8_t v8 = (uint8_t)value;
    int8_t sv8 = (int8_t)value;

    if (strcmp(param, "waveformNumber") == 0) {
        in->waveform_number = v8;
        return;
    }
    if (strcmp(param, "synthesisEnabled") == 0) {
        in->synthesis_enabled = value != 0.0f;
        return;
    }
    if (strcmp(param, "waveformLength") == 0) {
        in->waveform_length = v;
        return;
    }
    if (strcmp(param, "repeatLength") == 0) {
        in->repeat_length = v;
        return;
    }
    if (strcmp(param, "volume") == 0) {
        in->volume = v8;
        return;
    }
    if (strcmp(param, "portamentoSpeed") == 0) {
        in->portamento_speed = sv8;
        return;
    }
    if (strcmp(param, "adsrEnabled") == 0) {
        in->adsr_enabled = value != 0.0f;
        return;
    }
    if (strcmp(param, "adsrTableNumber") == 0) {
        in->adsr_table_number = v8;
        return;
    }
    if (strcmp(param, "adsrTableLength") == 0) {
        in->adsr_table_length = v;
        return;
    }
    if (strcmp(param, "arpeggioStart") == 0) {
        in->arpeggio_start = v8;
        return;
    }
    if (strcmp(param, "arpeggioLength") == 0) {
        in->arpeggio_length = v8;
        return;
    }
    if (strcmp(param, "arpeggioRepeatLength") == 0) {
        in->arpeggio_repeat_length = v8;
        return;
    }
    if (strcmp(param, "effect") == 0) {
        in->effect = (SynSynthEffect)(int)value;
        return;
    }
    if (strcmp(param, "effectArg1") == 0) {
        in->effect_arg1 = v8;
        return;
    }
    if (strcmp(param, "effectArg2") == 0) {
        in->effect_arg2 = v8;
        return;
    }
    if (strcmp(param, "effectArg3") == 0) {
        in->effect_arg3 = v8;
        return;
    }
    if (strcmp(param, "vibratoDelay") == 0) {
        in->vibrato_delay = v8;
        return;
    }
    if (strcmp(param, "vibratoSpeed") == 0) {
        in->vibrato_speed = v8;
        return;
    }
    if (strcmp(param, "vibratoLevel") == 0) {
        in->vibrato_level = v8;
        return;
    }
    if (strcmp(param, "egcOffset") == 0) {
        in->egc_offset = v8;
        return;
    }
    if (strcmp(param, "egcMode") == 0) {
        in->egc_mode = (SynEgcMode)(int)value;
        return;
    }
    if (strcmp(param, "egcTableNumber") == 0) {
        in->egc_table_number = v8;
        return;
    }
    if (strcmp(param, "egcTableLength") == 0) {
        in->egc_table_length = v;
        return;
    }
}

size_t syn_export(const SynModule* module, uint8_t* out, size_t max_size) {
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

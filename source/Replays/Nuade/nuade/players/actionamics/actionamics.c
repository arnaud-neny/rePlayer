// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/actionamics.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nuade_base.h"
#include "nuade_paula.h"

// Tables

static const uint16_t act_periods[73] = {
    0,    5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3816, 3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152,
    2032, 1920, 1808, 1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960,  904,  856,  808,  762,  720,
    678,  640,  604,  570,  538,  508,  480,  453,  428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,
    226,  214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,  107,  101,  95

};

static const uint8_t act_sinus[32] = { 0,   24,  49,  74,  97,  120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
                                       255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120, 97,  74,  49,  24

};

// A note plus arpeggio offset can leave the table; the lookup clamps to its ends.
static int act_period_index(int note) {
    if (note < 0)
        return 0;
    return note > 72 ? 72 : note;
}

static uint16_t act_period(int note) {
    return act_periods[act_period_index(note)];
}

// Track bytes of a voice with nothing to play: every row is "no event, no delay".
static const uint8_t act_empty_track[] = { 0xff };

// Reader helpers (big-endian)

typedef struct ActReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} ActReader;

static void act_reader_init(ActReader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

static bool act_reader_eof(const ActReader* r) {
    return r->pos > r->size;
}

static void act_reader_seek(ActReader* r, size_t pos) {
    r->pos = pos;
}

static void act_reader_skip(ActReader* r, size_t bytes) {
    r->pos += bytes;
}

static uint8_t act_reader_read_u8(ActReader* r) {
    if (r->pos >= r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    return r->data[r->pos++];
}

static int8_t act_reader_read_i8(ActReader* r) {
    return (int8_t)act_reader_read_u8(r);
}

static uint16_t act_reader_read_b_u16(ActReader* r) {
    uint8_t hi = act_reader_read_u8(r);
    uint8_t lo = act_reader_read_u8(r);
    return (uint16_t)((hi << 8) | lo);
}

static int16_t act_reader_read_b_i16(ActReader* r) {
    return (int16_t)act_reader_read_b_u16(r);
}

static uint32_t act_reader_read_b_u32(ActReader* r) {
    uint8_t a = act_reader_read_u8(r);
    uint8_t b = act_reader_read_u8(r);
    uint8_t c = act_reader_read_u8(r);
    uint8_t d = act_reader_read_u8(r);
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | d;
}

static int32_t act_reader_read_b_i32(ActReader* r) {
    return (int32_t)act_reader_read_b_u32(r);
}

static size_t act_reader_read_signed(ActReader* r, int8_t* buf, size_t count) {
    size_t avail = 0;
    if (r->pos < r->size)
        avail = r->size - r->pos;
    if (count > avail)
        count = avail;
    if (count > 0) {
        memcpy(buf, r->data + r->pos, count);
        r->pos += count;
    }
    return count;
}

static size_t act_reader_read_into(ActReader* r, uint8_t* buf, size_t count) {
    return act_reader_read_signed(r, (int8_t*)buf, count);
}

static void act_reader_read_string(ActReader* r, char* buf, int max_len) {
    for (int i = 0; i < max_len; i++) {
        if (r->pos >= r->size) {
            buf[i] = '\0';
            return;
        }
        buf[i] = (char)r->data[r->pos++];
    }
    buf[max_len] = '\0';
}

// Checked lookups: module data decides these indices, so none is trusted

#define ACT_LIST_SIZE 16

static ActSample* act_sample(const ActModule* m, int number) {
    if (number < 0 || number >= m->num_samples)
        return nullptr;
    return &m->samples[number];
}

static ActInstrument* act_instrument(const ActModule* m, int number) {
    if (number < 0 || number >= m->num_instruments)
        return nullptr;
    return &m->instruments[number];
}

// One value of a 16-entry list; `fallback` when the list or the position does not exist.
static int8_t act_list_value(int8_t* const* lists, int list_count, int list_number, int position, int8_t fallback) {
    if (list_number < 0 || list_number >= list_count || position < 0 || position >= ACT_LIST_SIZE)
        return fallback;
    return lists[list_number][position];
}

// Forward declarations

static void act_initialize_sound(ActModule* m, int sub_song);
static void act_play_tick(ActModule* m);

// Channel operations

static void act_ch_play_sample(ActChannel* ch, int16_t samp_num, int8_t* data, uint32_t data_size,
                               uint32_t start_offset, uint32_t length) {
    ch->sample_data = data;
    ch->data_size = data_size;
    ch->sample_offset = start_offset;
    ch->sample_length = length;
    ch->play_pending = true;
    ch->loop_length = 0;
    ch->loop_start = 0;
    ch->sample_number = samp_num;
    ch->has_pending_sample = false;
    ch->has_pending_loop = false;
}

static void act_ch_set_loop(ActChannel* ch, uint32_t start, uint32_t length) {
    ch->loop_start = start;
    ch->loop_length = length;
}

static void act_ch_set_amiga_period(ActChannel* ch, uint32_t period) {
    ch->period = (uint16_t)period;
}

static void act_ch_set_amiga_volume(ActChannel* ch, uint16_t vol) {
    ch->volume = vol;
}

static void act_ch_set_sample_number(ActChannel* ch, int16_t num) {
    ch->sample_number = num;
}

static void act_ch_set_sample(ActChannel* ch, int8_t* data, uint32_t start, uint32_t length) {
    // SetSample with data: deferred until loop
    ch->has_pending_sample = true;
    ch->pending_sample_data = data;
    ch->pending_sample_offset = start;
    ch->pending_sample_length = length;
}

static void act_ch_set_loop_with_data(ActChannel* ch, uint32_t start, uint32_t length) {
    ch->has_pending_loop = true;
    ch->pending_loop_start = start;
    ch->pending_loop_length = length;
}

// Loading

static bool act_load_module_info(ActReader* r, long start_offset, uint32_t* total_length) {
    if (start_offset > (long)r->size) {
        *total_length = 0;
        return false;
    }

    act_reader_seek(r, start_offset);
    *total_length = act_reader_read_b_u32(r);

    if (act_reader_eof(r))
        return false;

    return true;
}

static bool act_load_position_lists(ActModule* m, ActReader* r, long start_offset, uint32_t track_num_len,
                                    uint32_t instr_trans_len, uint32_t note_trans_len) {
    if (start_offset > (long)r->size)
        return false;

    act_reader_seek(r, start_offset);

    if ((track_num_len != instr_trans_len) || (track_num_len != note_trans_len))
        return false;

    int count = (int)(track_num_len / 4);
    if (count == 0)
        return false;
    m->position_count = count;

    m->positions = (ActSinglePositionInfo**)calloc(4, sizeof(ActSinglePositionInfo*));
    if (!m->positions)
        return false;

    for (int i = 0; i < 4; i++) {
        m->positions[i] = (ActSinglePositionInfo*)calloc(count, sizeof(ActSinglePositionInfo));
        if (!m->positions[i])
            return false;
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < count; j++) {
            m->positions[i][j].track_number = act_reader_read_u8(r);
        }
        if (act_reader_eof(r))
            return false;
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < count; j++) {
            m->positions[i][j].note_transpose = act_reader_read_i8(r);
        }
        if (act_reader_eof(r))
            return false;
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < count; j++) {
            m->positions[i][j].instrument_transpose = act_reader_read_i8(r);
        }
        if (act_reader_eof(r))
            return false;
    }

    return true;
}

static ActInstrumentList act_load_instrument_list_info(ActReader* r) {
    ActInstrumentList list;
    list.list_number = act_reader_read_u8(r);
    list.number_of_values_in_list = act_reader_read_u8(r);
    list.start_counter_delta_value = act_reader_read_u8(r);
    list.counter_end_value = act_reader_read_u8(r);
    return list;
}

static bool act_load_instruments(ActModule* m, ActReader* r, long start_offset, uint32_t instr_length) {
    if (start_offset > (long)r->size)
        return false;

    act_reader_seek(r, start_offset);

    int count = (int)(instr_length / 32);
    m->num_instruments = count;
    m->instruments = (ActInstrument*)calloc(count, sizeof(ActInstrument));
    if (!m->instruments)
        return false;

    for (int i = 0; i < count; i++) {
        ActInstrument* instr = &m->instruments[i];

        instr->sample_number_list = act_load_instrument_list_info(r);
        instr->arpeggio_list = act_load_instrument_list_info(r);
        instr->frequency_list = act_load_instrument_list_info(r);

        if (act_reader_eof(r))
            return false;

        instr->portamento_increment = act_reader_read_i8(r);
        instr->portamento_delay = act_reader_read_u8(r);
        instr->note_transpose = act_reader_read_i8(r);

        if (act_reader_eof(r))
            return false;

        act_reader_skip(r, 1);

        instr->attack_end_volume = act_reader_read_u8(r);
        instr->attack_speed = act_reader_read_u8(r);
        instr->decay_end_volume = act_reader_read_u8(r);
        instr->decay_speed = act_reader_read_u8(r);
        instr->sustain_delay = act_reader_read_u8(r);
        instr->release_end_volume = act_reader_read_u8(r);
        instr->release_speed = act_reader_read_u8(r);

        if (act_reader_eof(r))
            return false;

        act_reader_skip(r, 9);
    }

    return true;
}

static int8_t** act_load_list(ActReader* r, long start_offset, uint32_t list_length, int* out_count) {
    if (start_offset > (long)r->size)
        return nullptr;

    act_reader_seek(r, start_offset);

    int count = (int)(list_length / ACT_LIST_SIZE);
    *out_count = count;

    int8_t** list = (int8_t**)calloc(count, sizeof(int8_t*));
    if (!list)
        return nullptr;

    for (int i = 0; i < count; i++) {
        list[i] = (int8_t*)malloc(ACT_LIST_SIZE);
        if (!list[i] || act_reader_read_signed(r, list[i], ACT_LIST_SIZE) != ACT_LIST_SIZE) {
            for (int k = 0; k <= i; k++)
                free(list[k]);
            free(list);
            return nullptr;
        }
    }

    return list;
}

static bool act_load_sub_songs(ActModule* m, ActReader* r, long start_offset, uint32_t sub_song_length) {
    if (start_offset > (long)r->size)
        return false;

    act_reader_seek(r, start_offset);

    int count = (int)(sub_song_length / 4);
    ActSongInfo* temp_list = (ActSongInfo*)calloc(count, sizeof(ActSongInfo));
    if (!temp_list)
        return false;

    for (int i = 0; i < count; i++) {
        temp_list[i].start_position = act_reader_read_u8(r);
        temp_list[i].end_position = act_reader_read_u8(r);
        temp_list[i].loop_position = act_reader_read_u8(r);
        temp_list[i].speed = act_reader_read_u8(r);

        if (act_reader_eof(r)) {
            free(temp_list);
            return false;
        }
    }

    // Filter out entries where start, end, loop are all 0
    int valid_count = 0;
    for (int i = 0; i < count; i++) {
        if (temp_list[i].start_position != 0 || temp_list[i].end_position != 0 || temp_list[i].loop_position != 0)
            valid_count++;
    }

    m->num_song_infos = valid_count;
    m->song_info_list = (ActSongInfo*)calloc(valid_count, sizeof(ActSongInfo));
    if (!m->song_info_list) {
        free(temp_list);
        return false;
    }

    int j = 0;
    for (int i = 0; i < count; i++) {
        if (temp_list[i].start_position != 0 || temp_list[i].end_position != 0 || temp_list[i].loop_position != 0)
            m->song_info_list[j++] = temp_list[i];
    }

    free(temp_list);
    return true;
}

static bool act_load_sample_info(ActModule* m, ActReader* r, long start_offset, uint32_t sample_length) {
    if (start_offset > (long)r->size)
        return false;

    act_reader_seek(r, start_offset);

    int count = (int)(sample_length / 64);
    m->num_samples = count;
    m->samples = (ActSample*)calloc(count, sizeof(ActSample));
    m->sample_extras = (ActSampleExtra*)calloc(count, sizeof(ActSampleExtra));
    if (!m->samples || !m->sample_extras)
        return false;

    for (int i = 0; i < count; i++) {
        ActSample* sample = &m->samples[i];
        ActSampleExtra* extra = &m->sample_extras[i];

        // Skip pointer to data
        act_reader_skip(r, 4);

        sample->length = act_reader_read_b_u16(r);
        sample->loop_start = act_reader_read_b_u16(r);
        sample->loop_length = act_reader_read_b_u16(r);

        sample->effect_start_position = act_reader_read_b_u16(r);
        sample->effect_length = act_reader_read_b_u16(r);

        // Bug in original player: hi-byte is also used as arpeggio list number
        sample->arpeggio_list_number = (uint8_t)(sample->effect_length >> 8);

        sample->effect_speed = act_reader_read_b_u16(r);
        sample->effect_mode = act_reader_read_b_u16(r);

        extra->effect_increment_value = act_reader_read_b_i16(r);
        extra->effect_position = act_reader_read_b_i32(r);
        extra->effect_speed_counter = act_reader_read_b_u16(r);
        extra->already_taken = act_reader_read_b_u16(r) != 0;

        if (act_reader_eof(r))
            return false;

        act_reader_skip(r, 4);

        act_reader_read_string(r, sample->name, 32);

        if (act_reader_eof(r))
            return false;
    }

    return true;
}

static bool act_load_tracks(ActModule* m, ActReader* r, long start_offset, uint32_t track_offset_length) {
    if (start_offset > (long)r->size)
        return false;

    act_reader_seek(r, start_offset);

    int count = (int)(track_offset_length / 2);
    if (count < 1)
        return false;
    m->num_tracks = count - 1;
    m->tracks = (uint8_t**)calloc(m->num_tracks, sizeof(uint8_t*));
    m->track_sizes = (int*)calloc(m->num_tracks, sizeof(int));
    if (!m->tracks || !m->track_sizes)
        return false;

    uint16_t* offsets = (uint16_t*)calloc(count, sizeof(uint16_t));
    if (!offsets)
        return false;

    for (int i = 0; i < count; i++)
        offsets[i] = act_reader_read_b_u16(r);

    if (act_reader_eof(r)) {
        free(offsets);
        return false;
    }

    long track_start_offset = r->pos;

    for (int i = 0; i < count - 1; i++) {
        act_reader_seek(r, track_start_offset + offsets[i]);

        int track_length = offsets[i + 1] - offsets[i];
        if (track_length < 0) {
            free(offsets);
            return false;
        }
        m->track_sizes[i] = track_length;
        m->tracks[i] = (uint8_t*)malloc(track_length != 0 ? track_length : 1);
        if (!m->tracks[i]) {
            free(offsets);
            return false;
        }

        if (act_reader_read_into(r, m->tracks[i], track_length) != (size_t)track_length) {
            free(offsets);
            return false;
        }
    }

    free(offsets);
    return true;
}

static bool act_load_sample_data(ActModule* m, ActReader* r, uint32_t total_length) {
    uint64_t total_sample_length = 0;
    for (int i = 0; i < m->num_samples; i++)
        total_sample_length += m->samples[i].length;
    total_sample_length *= 2;

    if (total_sample_length > total_length)
        return false;

    uint32_t sample_data_start = total_length - (uint32_t)total_sample_length;

    if (sample_data_start > r->size)
        return false;

    act_reader_seek(r, sample_data_start);

    for (int i = 0; i < m->num_samples; i++) {
        ActSample* sample = &m->samples[i];

        if (sample->length > 0) {
            int length = sample->length * 2;
            sample->sample_data = (int8_t*)malloc(length);
            if (!sample->sample_data)
                return false;
            if (act_reader_read_signed(r, sample->sample_data, length) != (size_t)length)
                return false;
        }
    }

    return true;
}

// Setup track

static void act_setup_track(ActModule* m, ActVoiceInfo* vi) {
    const ActSinglePositionInfo* pos_info = nullptr;
    if (m->current_position < m->position_count && vi->position_list[m->current_position].track_number < m->num_tracks)
        pos_info = &vi->position_list[m->current_position];

    if (pos_info != nullptr) {
        vi->track_data = m->tracks[pos_info->track_number];
        vi->track_size = m->track_sizes[pos_info->track_number];
        vi->note_transpose = pos_info->note_transpose;
        vi->instrument_transpose = pos_info->instrument_transpose;
    } else {
        vi->track_data = act_empty_track;
        vi->track_size = (int)sizeof(act_empty_track);
        vi->note_transpose = 0;
        vi->instrument_transpose = 0;
    }
    vi->track_position = 0;

    vi->delay_counter = 0;
}

// Read track data

// The next byte of the voice's track; past its end every byte reads as "no event, no delay".
static uint8_t act_next_track_byte(ActVoiceInfo* vi) {
    if (vi->track_position < 0 || vi->track_position >= vi->track_size) {
        vi->track_position++;
        return 0xff;
    }
    return vi->track_data[vi->track_position++];
}

static void act_read_track_data(ActVoiceInfo* vi) {
    vi->note = 0;
    vi->instrument_number = 0;
    vi->effect = ACT_EFFECT_NONE;
    vi->effect_argument = 0;

    if (vi->delay_counter == 0) {
        uint8_t data = act_next_track_byte(vi);
        if ((data & 0x80) != 0) {
            vi->delay_counter = (uint8_t)~data;
            return;
        }

        if (data >= 0x70) {
            vi->effect = (ActEffect)data;
            vi->effect_argument = act_next_track_byte(vi);
            return;
        }

        vi->note = data;

        data = act_next_track_byte(vi);
        if ((data & 0x80) != 0) {
            vi->delay_counter = (uint8_t)~data;
            return;
        }

        if (data >= 0x70) {
            vi->effect = (ActEffect)data;
            vi->effect_argument = act_next_track_byte(vi);
            return;
        }

        vi->instrument_number = data;

        data = act_next_track_byte(vi);
        if ((data & 0x80) != 0) {
            vi->delay_counter = (uint8_t)~data;
            return;
        }

        vi->effect = (ActEffect)data;
        vi->effect_argument = act_next_track_byte(vi);
    } else {
        vi->delay_counter--;
    }
}

// Read next row

static void act_read_next_row(ActVoiceInfo* vi) {
    act_read_track_data(vi);

    if (vi->note != 0) {
        vi->note = (uint16_t)(vi->note + vi->note_transpose);
        vi->trig_sample = true;
    }
}

// Setup note and sample

static void act_setup_note_and_sample(ActModule* m, ActVoiceInfo* vi) {
    vi->portamento_value = 0;

    if (vi->note != 0) {
        // A note needs an instrument, its first sample and the lists behind it; without them the note is dropped.
        if (vi->instrument_number != 0)
            vi->instrument = act_instrument(m, vi->instrument_number - 1 + vi->instrument_transpose);

        ActSample* sample = nullptr;
        if (vi->instrument != nullptr) {
            sample = act_sample(m, act_list_value(m->sample_number_list, m->num_sample_number_lists,
                                                  vi->instrument->sample_number_list.list_number, 0, -1));
        }
        if (sample == nullptr) {
            vi->trig_sample = false;
            return;
        }

        vi->final_volume = 0;
        vi->sample_number_list_speed_counter = 0;
        vi->sample_number_list_position = 0;
        vi->arpeggio_list_speed_counter = 0;
        vi->arpeggio_list_position = 0;
        vi->frequency_list_speed_counter = 0;
        vi->frequency_list_position = 0;
        vi->portamento_delay_counter = 0;
        vi->tone_portamento_increment_value = 0;
        vi->envelope_state = ACT_ENV_ATTACK;
        vi->sustain_counter = 0;

        vi->final_note = (uint16_t)(vi->note + vi->instrument->note_transpose);
        vi->shown_note = (int)(act_period_index(vi->final_note));
        vi->shown_instrument = (int)(vi->instrument - m->instruments) + 1;

        vi->sample_number = (uint16_t)(sample - m->samples);

        vi->sample_data = sample->sample_data;
        vi->sample_data_size = sample->length * 2U;
        vi->sample_offset = 0;
        vi->sample_length = sample->length;
        vi->sample_loop_start = sample->loop_start * 2U;
        vi->sample_loop_length = sample->loop_length;

        int note_idx = vi->final_note
                       + act_list_value(m->arpeggio_list, m->num_arpeggio_lists, sample->arpeggio_list_number, 0, 0);
        vi->note_period = act_period(note_idx);
        vi->final_period = vi->note_period;
    }
}

// Sample inversion effect

static void act_do_sample_inversion_effect(ActModule* m) {
    ActSampleExtra* samples_taken[4];

    for (int i = 0; i < 4; i++) {
        ActVoiceInfo* vi = &m->voices[i];

        ActSample* sample = act_sample(m, vi->sample_number);
        if (sample == nullptr) {
            samples_taken[i] = nullptr;
            continue;
        }
        ActSampleExtra* extra = &m->sample_extras[vi->sample_number];

        samples_taken[i] = extra;

        if (!extra->already_taken) {
            extra->already_taken = true;

            if (extra->effect_speed_counter == 0) {
                extra->effect_speed_counter = sample->counter_init_value;

                if (sample->effect_mode != 0) {
                    int end_position = sample->effect_length * 2 - 1;

                    // The 68k adds in 32 bits: a damaged effect position wraps instead of overflowing
                    int64_t position = sample->effect_start_position * 2 + (int64_t)extra->effect_position;
                    if (extra->modified_sample_data != nullptr && position >= 0 && position < sample->length * 2)
                        extra->modified_sample_data[position] = (int8_t)~extra->modified_sample_data[position];

                    extra->effect_position = (int32_t)((uint32_t)extra->effect_position
                                                       + (uint32_t)(int32_t)extra->effect_increment_value);

                    if (extra->effect_position < 0) {
                        if (sample->effect_mode == 2)
                            extra->effect_position = end_position;
                        else {
                            extra->effect_position = (int32_t)((uint32_t)extra->effect_position
                                                               - (uint32_t)(int32_t)extra->effect_increment_value);
                            extra->effect_increment_value = (int16_t)-extra->effect_increment_value;
                        }
                    } else {
                        if (extra->effect_position <= end_position) {
                            if (sample->effect_mode == 1)
                                extra->effect_position = 0;
                            else {
                                extra->effect_position = (int32_t)((uint32_t)extra->effect_position
                                                                   - (uint32_t)(int32_t)extra->effect_increment_value);
                                extra->effect_increment_value = (int16_t)-extra->effect_increment_value;
                            }
                        }
                    }
                }
            } else {
                extra->effect_speed_counter--;
                extra->effect_speed_counter &= 0x1f;
            }
        }
    }

    for (int i = 0; i < 4; i++) {
        if (samples_taken[i] != nullptr)
            samples_taken[i]->already_taken = false;
    }
}

// Envelope

static void act_do_envelope(ActVoiceInfo* vi) {
    ActInstrument* instr = vi->instrument;

    if (instr != nullptr) {
        switch (vi->envelope_state) {
            case ACT_ENV_ATTACK:
                vi->final_volume += instr->attack_speed;
                if (vi->final_volume >= instr->attack_end_volume) {
                    vi->final_volume = instr->attack_end_volume;
                    vi->envelope_state = ACT_ENV_DECAY;
                }
                break;

            case ACT_ENV_DECAY:
                if (instr->decay_speed != 0) {
                    vi->final_volume -= instr->decay_speed;
                    if (vi->final_volume <= instr->decay_end_volume) {
                        vi->final_volume = instr->decay_end_volume;
                        vi->envelope_state = ACT_ENV_SUSTAIN;
                    }
                } else {
                    vi->envelope_state = ACT_ENV_SUSTAIN;
                }
                break;

            case ACT_ENV_SUSTAIN:
                if (vi->sustain_counter != instr->sustain_delay)
                    vi->sustain_counter++;
                else
                    vi->envelope_state = ACT_ENV_RELEASE;
                break;

            case ACT_ENV_RELEASE:
                if (instr->release_speed != 0) {
                    vi->final_volume -= instr->release_speed;
                    if (vi->final_volume <= instr->release_end_volume) {
                        vi->final_volume = instr->release_end_volume;
                        vi->envelope_state = ACT_ENV_DONE;
                    }
                } else {
                    vi->envelope_state = ACT_ENV_DONE;
                }
                break;

            case ACT_ENV_DONE:
                break;
        }
    }
}

// Set volume

static void act_set_volume(ActModule* m, ActVoiceInfo* vi, ActChannel* ch) {
    (void)m;
    act_do_envelope(vi);

    vi->tremolo_effect_argument = 0;
    vi->tremolo_table_index = (int8_t)vi->final_volume;

    uint16_t volume = (uint16_t)((vi->final_volume * vi->global_voice_volume) >> 16);
    act_ch_set_amiga_volume(ch, volume);
}

// Sample number list

static void act_do_sample_number_list(ActModule* m, ActVoiceInfo* vi, ActChannel* ch) {
    ActInstrument* instr = vi->instrument;

    if (instr != nullptr) {
        ActInstrumentList* list = &instr->sample_number_list;

        if (list->number_of_values_in_list != 0) {
            if (vi->sample_number_list_speed_counter == list->counter_end_value) {
                vi->sample_number_list_speed_counter
                    = (uint8_t)(list->counter_end_value - list->start_counter_delta_value);

                if (vi->sample_number_list_position == list->number_of_values_in_list)
                    vi->sample_number_list_position = -1;

                vi->sample_number_list_position++;

                int8_t sample_number = act_list_value(m->sample_number_list, m->num_sample_number_lists,
                                                      list->list_number, vi->sample_number_list_position, -1);
                ActSample* sample = act_sample(m, sample_number);

                if (sample == nullptr)
                    vi->sample_number_list_position--;
                else {
                    vi->sample_number = (uint16_t)sample_number;

                    vi->sample_data = sample->sample_data;
                    vi->sample_data_size = sample->length * 2U;
                    vi->sample_offset = 0;
                    vi->sample_loop_start = 0;
                    vi->sample_loop_length = sample->length;

                    act_ch_set_sample_number(ch, sample_number);
                    act_ch_set_sample(ch, vi->sample_data, vi->sample_loop_start, vi->sample_loop_length * 2U);
                    act_ch_set_loop_with_data(ch, vi->sample_loop_start, vi->sample_loop_length * 2U);
                }
            } else {
                vi->sample_number_list_speed_counter++;
            }
        }
    }
}

// Arpeggio list

static void act_do_arpeggio_list(ActModule* m, ActVoiceInfo* vi) {
    ActInstrument* instr = vi->instrument;

    if (instr != nullptr) {
        ActInstrumentList* list = &instr->arpeggio_list;

        if (list->number_of_values_in_list != 0) {
            if (vi->arpeggio_list_speed_counter == list->counter_end_value) {
                vi->arpeggio_list_speed_counter = (uint8_t)(list->counter_end_value - list->start_counter_delta_value);

                if (vi->arpeggio_list_position == list->number_of_values_in_list)
                    vi->arpeggio_list_position = -1;

                vi->arpeggio_list_position++;

                int8_t arp_value = act_list_value(m->arpeggio_list, m->num_arpeggio_lists, list->list_number,
                                                  vi->arpeggio_list_position, 0);
                vi->final_period = act_period(arp_value + vi->final_note);
            } else {
                vi->arpeggio_list_speed_counter++;
            }
        }
    }
}

// Frequency list

static void act_do_frequency_list(ActModule* m, ActVoiceInfo* vi) {
    ActInstrument* instr = vi->instrument;

    if (instr != nullptr) {
        ActInstrumentList* list = &instr->frequency_list;

        if (list->number_of_values_in_list != 0) {
            if (vi->frequency_list_speed_counter == list->counter_end_value) {
                vi->frequency_list_speed_counter = (uint8_t)(list->counter_end_value - list->start_counter_delta_value);

                if (vi->frequency_list_position == list->number_of_values_in_list)
                    vi->frequency_list_position = -1;

                vi->frequency_list_position++;

                int8_t freq_value = act_list_value(m->frequency_list, m->num_frequency_lists, list->list_number,
                                                   vi->frequency_list_position, 0);
                vi->final_period = (uint16_t)(vi->final_period + freq_value);
            } else {
                vi->frequency_list_speed_counter++;
            }
        }
    }
}

// Portamento

static void act_do_portamento(ActVoiceInfo* vi) {
    ActInstrument* instr = vi->instrument;

    if (instr != nullptr) {
        if (instr->portamento_increment != 0) {
            if (vi->portamento_delay_counter == instr->portamento_delay)
                vi->portamento_value += instr->portamento_increment;
            else
                vi->portamento_delay_counter++;
        }
    }
}

// Tone portamento

static void act_do_tone_portamento(ActModule* m, ActVoiceInfo* vi) {
    (void)m;
    if ((vi->effect != ACT_EFFECT_NONE) && ((uint16_t)vi->effect < ACT_EFFECT_ARPEGGIO) && (vi->effect_argument != 0)
        && (vi->instrument != nullptr)) {
        ActInstrument* instr = vi->instrument;

        uint8_t end_note = (uint8_t)(((int8_t)vi->effect) + vi->note_transpose + instr->note_transpose);
        vi->tone_portamento_end_period = act_period(end_note);

        int speed = vi->effect_argument;

        int delta = vi->tone_portamento_end_period - vi->final_period;
        if (delta != 0) {
            if (delta < 0)
                speed = -speed;
            vi->tone_portamento_increment_value = (int16_t)speed;
        }
    }

    if (vi->tone_portamento_increment_value != 0) {
        if (vi->tone_portamento_increment_value < 0) {
            vi->final_period = (uint16_t)(vi->final_period + vi->tone_portamento_increment_value);

            if (vi->final_period <= vi->tone_portamento_end_period) {
                vi->tone_portamento_increment_value = 0;
                vi->final_period = vi->tone_portamento_end_period;
                vi->note_period = vi->final_period;
            }
        } else {
            vi->final_period = (uint16_t)(vi->final_period + vi->tone_portamento_increment_value);

            if (vi->final_period >= vi->tone_portamento_end_period) {
                vi->tone_portamento_increment_value = 0;
                vi->final_period = vi->tone_portamento_end_period;
                vi->note_period = vi->final_period;
            }
        }
    }
}

// Volume slide helper

static void act_do_volume_slide(ActVoiceInfo* vi) {
    uint16_t arg = vi->effect_argument;

    if ((arg & 0x0f) != 0) {
        int volume = vi->final_volume - (arg * 4);
        if (volume < 0)
            volume = 0;
        vi->final_volume = (int16_t)volume;
        vi->tremolo_effect_argument = 0;
        vi->tremolo_table_index = (int8_t)volume;
    } else {
        int volume = vi->final_volume + ((arg >> 4) * 4);
        if (volume > 255)
            volume = 255;
        vi->final_volume = (int16_t)volume;
        vi->tremolo_effect_argument = 0;
        vi->tremolo_table_index = (int8_t)volume;
    }
}

// Vibrato helper

static void act_do_vibrato(ActVoiceInfo* vi) {
    uint8_t val = act_sinus[(vi->vibrato_table_index >> 2) & 0x1f];
    int vib_val = (((vi->vibrato_effect_argument & 0x0f) * val) >> 7);

    if (vi->vibrato_table_index >= 0)
        vib_val = -vib_val;

    vi->final_period = (uint16_t)(vi->note_period - vib_val);
    vi->vibrato_table_index += (int8_t)(vi->vibrato_effect_argument >> 2);
}

// Track effects

static void act_do_effect_arpeggio(ActModule* m, ActVoiceInfo* vi) {
    uint16_t arg = vi->effect_argument;
    uint8_t arp_list[4] = { (uint8_t)(arg >> 4), 0, (uint8_t)(arg & 0x0f), 0 };
    uint8_t arp_val = arp_list[m->measure_counter];
    vi->final_period = act_period(vi->final_note + arp_val);
}

static void act_do_effect_slide_up(ActVoiceInfo* vi) {
    vi->portamento_value = (int16_t)-vi->effect_argument;
    vi->note_period = (uint16_t)(vi->note_period + vi->portamento_value);
}

static void act_do_effect_slide_down(ActVoiceInfo* vi) {
    vi->portamento_value = (int16_t)vi->effect_argument;
    vi->note_period = (uint16_t)(vi->note_period + vi->portamento_value);
}

static void act_do_effect_volume_slide_after_envelope(ActModule* m, ActVoiceInfo* vi) {
    if (vi->envelope_state == ACT_ENV_DONE) {
        if ((m->speed_counter == 0) && (vi->instrument_number != 0)) {
            if (vi->instrument == nullptr)
                return;
            vi->final_volume = vi->instrument->attack_speed;
        }
        act_do_volume_slide(vi);
    }
}

static void act_do_effect_vibrato(ActVoiceInfo* vi) {
    uint16_t arg = vi->effect_argument;
    if (arg != 0) {
        uint8_t new_arg = vi->vibrato_effect_argument;
        if ((arg & 0x0f) != 0)
            new_arg = (uint8_t)((new_arg & 0xf0) | (arg & 0x0f));
        if ((arg & 0xf0) != 0)
            new_arg = (uint8_t)((new_arg & 0x0f) | (arg & 0xf0));
        vi->vibrato_effect_argument = new_arg;
    }
    act_do_vibrato(vi);
}

static void act_do_effect_set_rows(ActModule* m, ActVoiceInfo* vi) {
    m->number_of_rows = (uint8_t)vi->effect_argument;
}

static void act_do_effect_set_sample_offset(ActVoiceInfo* vi) {
    if (vi->effect_argument != 0)
        vi->sample_offset_effect_argument = vi->effect_argument;

    uint16_t offset = (uint16_t)(vi->sample_offset_effect_argument << 7);

    if (offset < vi->sample_length) {
        vi->sample_length -= offset;
        vi->sample_offset += offset * 2U;
    } else {
        vi->sample_length = 1;
    }
}

static void act_do_effect_note_delay(ActVoiceInfo* vi) {
    if (vi->effect_argument != 0) {
        vi->note_delay_counter = vi->effect_argument;
        vi->effect = ACT_EFFECT_NONE;
        vi->effect_argument = 0;
    }
}

static void act_do_effect_mute(ActModule* m, ActVoiceInfo* vi, ActChannel* ch) {
    if ((vi->effect_argument != 0) && (m->speed_counter == 0)) {
        vi->final_volume = 0;
        vi->tremolo_effect_argument = 0;
        vi->tremolo_table_index = 0;
        act_ch_set_amiga_volume(ch, 0);
    }
}

static void act_do_effect_sample_restart(ActVoiceInfo* vi) {
    if (vi->effect_argument != 0) {
        vi->restart_delay_counter = vi->effect_argument;
        vi->restart_sample_data = vi->sample_data;
        vi->restart_sample_data_size = vi->sample_data_size;
        vi->restart_sample_offset = vi->sample_offset;
        vi->restart_sample_length = vi->sample_length;
        vi->effect = ACT_EFFECT_NONE;
        vi->effect_argument = 0;
    }
}

static void act_do_effect_tremolo(ActVoiceInfo* vi, ActChannel* ch) {
    uint16_t arg = vi->effect_argument;
    if (arg != 0) {
        uint8_t new_arg = vi->tremolo_effect_argument;
        if ((arg & 0x0f) != 0)
            new_arg = (uint8_t)((new_arg & 0xf0) | (arg & 0x0f));
        if ((arg & 0xf0) != 0)
            new_arg = (uint8_t)((new_arg & 0x0f) | (arg & 0xf0));
        vi->tremolo_effect_argument = new_arg;
    }

    uint8_t val = act_sinus[(vi->tremolo_table_index >> 2) & 0x1f];
    int vib_val = (((vi->tremolo_effect_argument & 0x0f) * val) >> 6);

    if (vi->tremolo_table_index >= 0)
        vib_val = -vib_val;

    int16_t volume = (int16_t)(vi->tremolo_volume - vib_val);
    if (volume < 0)
        volume = 0;
    if (volume > 64)
        volume = 64;
    volume *= 4;
    if (volume == 256)
        volume = 255;

    vi->final_volume = volume;
    act_ch_set_amiga_volume(ch, (uint16_t)((volume * vi->global_voice_volume) >> 16));

    vi->tremolo_table_index += (int8_t)((vi->tremolo_effect_argument >> 2) & 0x3c);
}

static void act_do_effect_break(ActModule* m) {
    m->current_row_position = (uint8_t)(m->number_of_rows - 1);
}

static void act_do_effect_set_volume(ActVoiceInfo* vi, ActChannel* ch) {
    int16_t volume = (int16_t)(vi->effect_argument * 4);
    if (volume > 255)
        volume = 255;
    vi->final_volume = volume;
    act_ch_set_amiga_volume(ch, (uint16_t)((volume * vi->global_voice_volume) >> 16));
}

static void act_do_effect_volume_slide(ActModule* m, ActVoiceInfo* vi) {
    if (m->speed_counter == 0) {
        if (vi->instrument == nullptr)
            return;
        vi->final_volume = vi->instrument->attack_speed;
    }
    act_do_volume_slide(vi);
}

static void act_do_effect_volume_slide_and_vibrato(ActModule* m, ActVoiceInfo* vi) {
    act_do_effect_volume_slide_after_envelope(m, vi);
    act_do_vibrato(vi);
}

static void act_do_effect_set_speed(ActModule* m, ActVoiceInfo* vi) {
    if (vi->effect_argument < 31)
        m->current_speed = (uint8_t)vi->effect_argument;
}

// Handle track effects

static void act_handle_track_effects(ActModule* m, ActVoiceInfo* vi, ActChannel* ch) {
    if (m->speed_counter != 0) {
        switch (vi->effect) {
            case ACT_EFFECT_ARPEGGIO:
                act_do_effect_arpeggio(m, vi);
                break;
            case ACT_EFFECT_SLIDE_UP:
                act_do_effect_slide_up(vi);
                break;
            case ACT_EFFECT_SLIDE_DOWN:
                act_do_effect_slide_down(vi);
                break;
            case ACT_EFFECT_VOLUME_SLIDE_AFTER_ENVELOPE:
                act_do_effect_volume_slide_after_envelope(m, vi);
                break;
            case ACT_EFFECT_VIBRATO:
                act_do_effect_vibrato(vi);
                break;
            default:
                break;
        }
    }

    switch (vi->effect) {
        case ACT_EFFECT_SET_ROWS:
            act_do_effect_set_rows(m, vi);
            break;
        case ACT_EFFECT_SET_SAMPLE_OFFSET:
            act_do_effect_set_sample_offset(vi);
            break;
        case ACT_EFFECT_NOTE_DELAY:
            act_do_effect_note_delay(vi);
            break;
        case ACT_EFFECT_MUTE:
            act_do_effect_mute(m, vi, ch);
            break;
        case ACT_EFFECT_SAMPLE_RESTART:
            act_do_effect_sample_restart(vi);
            break;
        case ACT_EFFECT_TREMOLO:
            act_do_effect_tremolo(vi, ch);
            break;
        case ACT_EFFECT_BREAK:
            act_do_effect_break(m);
            break;
        case ACT_EFFECT_SET_VOLUME:
            act_do_effect_set_volume(vi, ch);
            break;
        case ACT_EFFECT_VOLUME_SLIDE:
            act_do_effect_volume_slide(m, vi);
            break;
        case ACT_EFFECT_VOLUME_SLIDE_AND_VIBRATO:
            act_do_effect_volume_slide_and_vibrato(m, vi);
            break;
        case ACT_EFFECT_SET_SPEED:
            act_do_effect_set_speed(m, vi);
            break;
        default:
            break;
    }
}

// Do effects

static void act_do_effects(ActModule* m, ActVoiceInfo* vi, ActChannel* ch) {
    act_set_volume(m, vi, ch);
    act_do_sample_number_list(m, vi, ch);
    act_handle_track_effects(m, vi, ch);
    act_do_arpeggio_list(m, vi);
    act_do_frequency_list(m, vi);
    act_do_portamento(vi);
    act_do_tone_portamento(m, vi);

    int period = vi->final_period + vi->portamento_value;

    if (period < 95)
        period = 95;
    else if (period > 5760)
        period = 5760;

    vi->final_period = (uint16_t)period;

    act_ch_set_amiga_period(ch, vi->final_period);
}

// Initialize sound

static void act_initialize_sound(ActModule* m, int sub_song) {
    m->current_song_info = &m->song_info_list[sub_song];

    m->speed_counter = 0;
    m->current_speed = m->current_song_info->speed;
    m->measure_counter = 0;
    m->current_position = m->current_song_info->start_position;
    m->loop_position = m->current_song_info->loop_position;
    m->end_position = m->current_song_info->end_position;
    m->current_row_position = 0;
    m->number_of_rows = 64;
    m->shown_position = m->current_position;
    m->shown_row = 0;
    m->shown_rows = m->number_of_rows;

    for (int i = 0; i < 4; i++) {
        ActVoiceInfo* vi = &m->voices[i];
        memset(vi, 0, sizeof(ActVoiceInfo));

        vi->position_list = m->positions[i];
        vi->track_data = nullptr;
        vi->track_position = 0;
        vi->delay_counter = 0;

        vi->instrument_number = 0;
        vi->instrument = nullptr;
        vi->instrument_transpose = 0;

        vi->sample_number = 0;
        vi->sample_data = nullptr;
        vi->sample_data_size = 0;
        vi->sample_offset = 0;
        vi->sample_length = 0;
        vi->sample_loop_start = 0;
        vi->sample_loop_length = 0;

        vi->note = 0;
        vi->note_transpose = 0;
        vi->note_period = 0;

        vi->final_note = 0;
        vi->final_period = 0;
        vi->final_volume = 0;
        vi->global_voice_volume = 0x4041;

        vi->envelope_state = ACT_ENV_DONE;
        vi->sustain_counter = 0;

        vi->sample_number_list_speed_counter = 0;
        vi->sample_number_list_position = 0;
        vi->arpeggio_list_speed_counter = 0;
        vi->arpeggio_list_position = 0;
        vi->frequency_list_speed_counter = 0;
        vi->frequency_list_position = 0;

        vi->effect = ACT_EFFECT_NONE;
        vi->effect_argument = 0;

        vi->portamento_delay_counter = 0;
        vi->portamento_value = 0;

        vi->tone_portamento_end_period = 0;
        vi->tone_portamento_increment_value = 0;

        vi->vibrato_effect_argument = 0;
        vi->vibrato_table_index = 0;

        vi->tremolo_effect_argument = 0;
        vi->tremolo_table_index = 0;
        vi->tremolo_volume = 0;

        vi->sample_offset_effect_argument = 0;
        vi->note_delay_counter = 0;

        vi->restart_delay_counter = 0;
        vi->restart_sample_data = nullptr;
        vi->restart_sample_data_size = 0;
        vi->restart_sample_offset = 0;
        vi->restart_sample_length = 0;

        vi->trig_sample = false;

        act_setup_track(m, &m->voices[i]);
    }

    for (int i = m->num_samples - 1; i >= 0; i--) {
        ActSample* sample = &m->samples[i];
        if (sample->effect_mode != 0) {
            if (m->sample_extras[i].modified_sample_data)
                free(m->sample_extras[i].modified_sample_data);
            int len = sample->length * 2;
            m->sample_extras[i].modified_sample_data = (int8_t*)malloc(len);
            if (m->sample_extras[i].modified_sample_data && sample->sample_data)
                memcpy(m->sample_extras[i].modified_sample_data, sample->sample_data, len);
        }
    }

    m->loop_count = 0;

    for (int i = 0; i < 4; i++) {
        memset(&m->channels[i], 0, sizeof(ActChannel));
    }
}

// Play tick

static void act_play_tick(ActModule* m) {
    m->measure_counter++;
    if (m->measure_counter == 3)
        m->measure_counter = 0;

    m->speed_counter++;

    bool row_tick = false;
    if (m->speed_counter == m->current_speed) {
        m->speed_counter = 0;
        m->measure_counter = 0;
        row_tick = true;
        m->shown_position = m->current_position;
        m->shown_row = m->current_row_position;

        for (int i = 0; i < 4; i++) {
            ActVoiceInfo* vi = &m->voices[i];
            act_read_next_row(vi);
            act_setup_note_and_sample(m, vi);
        }

        act_do_sample_inversion_effect(m);

        m->current_row_position++;

        if (m->current_row_position == m->number_of_rows) {
            m->current_row_position = 0;

            uint8_t position = m->current_position;
            m->current_position++;

            if (position == m->end_position) {
                m->current_position = m->loop_position;
                m->loop_count++;
            }

            for (int i = 0; i < 4; i++)
                act_setup_track(m, &m->voices[i]);
        }
    }

    for (int i = 0; i < 4; i++) {
        ActVoiceInfo* vi = &m->voices[i];
        ActChannel* ch = &m->channels[i];

        if (vi->restart_delay_counter != 0) {
            vi->restart_delay_counter--;

            if (vi->restart_delay_counter == 0) {
                vi->sample_data = vi->restart_sample_data;
                vi->sample_data_size = vi->restart_sample_data_size;
                vi->sample_offset = vi->restart_sample_offset;
                vi->sample_length = vi->restart_sample_length;
                vi->trig_sample = true;
            }
        }

        act_do_effects(m, vi, ch);

        if (vi->note_delay_counter == 0) {
            if (vi->trig_sample) {
                vi->trig_sample = false;

                act_ch_play_sample(ch, (int16_t)vi->sample_number, vi->sample_data, vi->sample_data_size,
                                   vi->sample_offset, vi->sample_length * 2U);

                if (vi->sample_loop_length > 1)
                    act_ch_set_loop(ch, vi->sample_loop_start, vi->sample_loop_length * 2U);
            }

            act_ch_set_amiga_period(ch, vi->final_period);
        } else {
            vi->note_delay_counter--;
        }
    }

    if (m->speed_counter != 0)
        act_do_sample_inversion_effect(m);

    // A set rows effect of the row just read has run by now
    if (row_tick)
        m->shown_rows = m->number_of_rows;
}

// Voices

// Hand the registers the tick wrote to the voice. Paula channels 0 and 3 are left, 1 and 2 right.
static void act_sync_voice(ActModule* m, uint32_t index) {
    ActChannel* ch = &m->channels[index];

    if (ch->play_pending) {
        ch->play_pending = false;
        if (ch->sample_data != nullptr) {
            // The head plays to the sample end, then the loop takes over from its own start
            const uint32_t head = ch->sample_length < ch->data_size ? ch->sample_length : ch->data_size;
            const NuadeSample sample = { .data = ch->sample_data, .format = NuadeSampleFormat_S8, .length = head };
            nuade_voice_play(m->paula, index, &sample, ch->sample_offset);

            if (ch->loop_length > 0 && ch->loop_start < ch->data_size) {
                const uint32_t room = ch->data_size - ch->loop_start;
                const uint32_t length = ch->loop_length < room ? ch->loop_length : room;
                const NuadeSample loop = { .data = ch->sample_data + ch->loop_start,
                                           .format = NuadeSampleFormat_S8,
                                           .length = length,
                                           .loop_start = 0,
                                           .loop_length = length };
                nuade_voice_queue(m->paula, index, &loop);
            }
        } else {
            nuade_voice_stop(m->paula, index);
        }
    }

    const bool silent = ch->period == 0;
    nuade_voice_set_pitch(m->paula, index, silent ? 0.0 : nuade_paula_period_hz(ch->period));
    nuade_voice_set_volume(m->paula, index, silent ? 0.0f : (float)ch->volume / 64.0f);
    nuade_voice_set_pan(m->paula, index, (index == 0 || index == 3) ? -1.0f : 1.0f);
}

bool act_tick(ActModule* module) {
    if (!module->timer_started) {
        module->timer_started = true;
        return false;
    }
    const uint32_t loops = module->loop_count;
    act_play_tick(module);
    for (uint32_t i = 0; i < 4; i++) {
        act_sync_voice(module, i);
    }
    return module->loop_count != loops;
}

// Public API

ActModule* act_create(const uint8_t* data, size_t size) {
    if (!act_probe(data, size))
        return nullptr;

    ActReader reader;
    act_reader_init(&reader, data, size);

    uint16_t tempo = act_reader_read_b_u16(&reader);

    uint32_t lengths[15];
    for (int i = 0; i < 15; i++)
        lengths[i] = act_reader_read_b_u32(&reader);

    if (act_reader_eof(&reader) || tempo == 0)
        return nullptr;

    // Counts come from these lengths, so a length larger than the file cannot be real.
    for (int i = 0; i < 15; i++) {
        if (lengths[i] > size)
            return nullptr;
    }

    ActModule* m = (ActModule*)calloc(1, sizeof(ActModule));
    if (!m)
        return nullptr;

    m->tempo = tempo;

    long start_offset = reader.pos;
    start_offset += lengths[0];

    uint32_t total_length = 0;
    if (!act_load_module_info(&reader, start_offset, &total_length) || (total_length > size)) {
        act_destroy(m);
        return nullptr;
    }

    start_offset += lengths[1];

    if (!act_load_position_lists(m, &reader, start_offset, lengths[2], lengths[3], lengths[4])) {
        act_destroy(m);
        return nullptr;
    }

    start_offset += lengths[2] + lengths[3] + lengths[4];

    if (!act_load_instruments(m, &reader, start_offset, lengths[5])) {
        act_destroy(m);
        return nullptr;
    }

    start_offset += lengths[5];

    m->sample_number_list = act_load_list(&reader, start_offset, lengths[6], &m->num_sample_number_lists);
    if (!m->sample_number_list) {
        act_destroy(m);
        return nullptr;
    }

    start_offset += lengths[6];

    m->arpeggio_list = act_load_list(&reader, start_offset, lengths[7], &m->num_arpeggio_lists);
    if (!m->arpeggio_list) {
        act_destroy(m);
        return nullptr;
    }

    start_offset += lengths[7];

    m->frequency_list = act_load_list(&reader, start_offset, lengths[8], &m->num_frequency_lists);
    if (!m->frequency_list) {
        act_destroy(m);
        return nullptr;
    }

    start_offset += lengths[8] + lengths[9] + lengths[10];

    if (!act_load_sub_songs(m, &reader, start_offset, lengths[11])) {
        act_destroy(m);
        return nullptr;
    }

    start_offset += lengths[11] + lengths[12];

    if (!act_load_sample_info(m, &reader, start_offset, lengths[13])) {
        act_destroy(m);
        return nullptr;
    }

    start_offset += lengths[13];

    if (!act_load_tracks(m, &reader, start_offset, lengths[14])) {
        act_destroy(m);
        return nullptr;
    }

    if (!act_load_sample_data(m, &reader, total_length)) {
        act_destroy(m);
        return nullptr;
    }

    if (m->num_song_infos > 0)
        act_initialize_sound(m, 0);

    return m;
}

void act_destroy(ActModule* module) {
    if (!module)
        return;

    if (module->positions) {
        for (int i = 0; i < 4; i++)
            free(module->positions[i]);
        free(module->positions);
    }

    free(module->instruments);

    if (module->samples) {
        for (int i = 0; i < module->num_samples; i++)
            free(module->samples[i].sample_data);
        free(module->samples);
    }

    if (module->sample_extras) {
        for (int i = 0; i < module->num_samples; i++)
            free(module->sample_extras[i].modified_sample_data);
        free(module->sample_extras);
    }

    if (module->sample_number_list) {
        for (int i = 0; i < module->num_sample_number_lists; i++)
            free(module->sample_number_list[i]);
        free(module->sample_number_list);
    }

    if (module->arpeggio_list) {
        for (int i = 0; i < module->num_arpeggio_lists; i++)
            free(module->arpeggio_list[i]);
        free(module->arpeggio_list);
    }

    if (module->frequency_list) {
        for (int i = 0; i < module->num_frequency_lists; i++)
            free(module->frequency_list[i]);
        free(module->frequency_list);
    }

    if (module->tracks) {
        for (int i = 0; i < module->num_tracks; i++)
            free(module->tracks[i]);
        free(module->tracks);
    }
    free(module->track_sizes);

    free(module->song_info_list);
    free(module);
}

int act_subsong_count(const ActModule* module) {
    if (!module)
        return 0;
    return module->num_song_infos;
}

uint32_t act_timebase_hz(const ActModule* module) {
    return 2U * module->tempo;
}

bool act_start(ActModule* module, int subsong, NuadeVoices* voices) {
    if (!module || !voices || subsong < 0 || subsong >= module->num_song_infos)
        return false;

    module->paula = voices;
    module->timer_started = false;
    act_initialize_sound(module, subsong);
    return true;
}

bool act_probe(const uint8_t* data, size_t size) {
    return data && size >= 90 && memcmp(data + 62, "ACTIONAMICS SOUND TOOL", 22) == 0;
}

// Sample info

int act_get_sample_count(const ActModule* module) {
    return module ? module->num_samples : 0;
}

const char* act_get_sample_name(const ActModule* module, int samp) {
    if (!module || samp < 0 || samp >= module->num_samples)
        return "";
    return module->samples[samp].name;
}

// Display

ActPosition act_position(const ActModule* module) {
    return (ActPosition) { .position = module->shown_position,
                           .row = module->shown_row,
                           .rows = module->shown_rows,
                           .speed = module->current_speed,
                           .tick = module->speed_counter };
}

ActVoiceState act_voice_state(const ActModule* module, int voice) {
    const ActVoiceInfo* vi = &module->voices[voice];
    return (ActVoiceState) { .note = vi->shown_note, .instrument = vi->shown_instrument };
}

int act_position_count(const ActModule* module) {
    return module->position_count;
}

// Reads the track the way act_read_track_data does, row by row, from the start of the track.
ActCell act_pattern_cell(const ActModule* module, int position, int channel, int row) {
    ActCell cell = { 0 };
    if (position < 0 || position >= module->position_count || channel < 0 || channel >= 4 || row < 0)
        return cell;
    const ActSinglePositionInfo* info = &module->positions[channel][position];
    if (info->track_number >= module->num_tracks)
        return cell;
    const uint8_t* data = module->tracks[info->track_number];
    const int size = module->track_sizes[info->track_number];
    int pos = 0;
    int delay = 0;
    for (int r = 0; r <= row; r++) {
        cell = (ActCell) { 0 };
        if (delay != 0) {
            delay--;
            continue;
        }
#define ACT_NEXT() (pos < size ? data[pos++] : (pos++, (uint8_t)0xff))
        uint8_t b = ACT_NEXT();
        if ((b & 0x80) != 0) {
            delay = (uint8_t)~b;
            continue;
        }
        if (b < 0x70) {
            cell.note = b;
            b = ACT_NEXT();
            if ((b & 0x80) != 0) {
                delay = (uint8_t)~b;
                continue;
            }
            if (b < 0x70) {
                cell.instrument = b;
                b = ACT_NEXT();
                if ((b & 0x80) != 0) {
                    delay = (uint8_t)~b;
                    continue;
                }
            }
        }
        cell.effect = b;
        cell.effect_arg = ACT_NEXT();
#undef ACT_NEXT
    }
    return cell;
}

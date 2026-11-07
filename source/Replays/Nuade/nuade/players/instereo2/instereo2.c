// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/instereo2.h>

#include "nuade_base.h"
#include "nuade_paula.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IS2_CHANNELS 4

// Tables

static const uint16_t is2_periods[109]
    = { 0,    13696, 12928, 12192, 11520, 10848, 10240, 9664, 9120, 8608, 8128, 7680, 7248, 6848, 6464, 6096,
        5760, 5424,  5120,  4832,  4560,  4304,  4064,  3840, 3624, 3424, 3232, 3048, 2880, 2712, 2560, 2416,
        2280, 2152,  2032,  1920,  1812,  1712,  1616,  1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960,
        906,  856,   808,   762,   720,   678,   640,   604,  570,  538,  508,  480,  453,  428,  404,  381,
        360,  339,   320,   302,   285,   269,   254,   240,  226,  214,  202,  190,  180,  170,  160,  151,
        143,  135,   127,   120,   113,   107,   101,   95,   90,   85,   80,   75,   71,   67,   63,   60,
        56,   53,    50,    47,    45,    42,    40,    37,   35,   33,   31,   30,   28

      };

#define IS2_PERIODS_LEN 109

static const int8_t is2_vibrato[256] = {
    0,    3,    6,    9,    12,   16,   19,   22,   25,   28,   31,   34,   37,   40,   43,   46,   49,   52,   54,
    57,   60,   63,   66,   68,   71,   73,   76,   78,   81,   83,   86,   88,   90,   92,   94,   96,   98,   100,
    102,  104,  106,  108,  109,  111,  112,  114,  115,  116,  118,  119,  120,  121,  122,  123,  123,  124,  125,
    125,  126,  126,  126,  127,  127,  127,  127,  127,  127,  127,  126,  126,  125,  125,  124,  124,  123,  122,
    121,  120,  119,  118,  117,  116,  114,  113,  112,  110,  108,  107,  105,  103,  101,  99,   97,   95,   93,
    91,   89,   87,   84,   82,   80,   77,   75,   72,   69,   67,   64,   61,   59,   56,   53,   50,   47,   44,
    41,   39,   36,   32,   29,   26,   23,   20,   17,   14,   11,   8,    5,    2,    -1,   -4,   -7,   -10,  -14,
    -17,  -20,  -23,  -26,  -29,  -32,  -35,  -38,  -41,  -44,  -47,  -50,  -53,  -55,  -58,  -61,  -64,  -66,  -69,
    -72,  -74,  -77,  -79,  -82,  -84,  -86,  -88,  -91,  -93,  -95,  -97,  -99,  -101, -103, -104, -106, -108, -109,
    -111, -112, -114, -115, -116, -118, -119, -120, -121, -122, -122, -123, -124, -124, -125, -125, -126, -126, -126,
    -126, -126, -126, -126, -126, -126, -126, -125, -125, -124, -124, -123, -122, -121, -120, -119, -118, -117, -116,
    -115, -113, -112, -110, -109, -107, -105, -104, -102, -100, -98,  -96,  -94,  -92,  -90,  -87,  -85,  -83,  -80,
    -78,  -75,  -73,  -70,  -68,  -65,  -62,  -60,  -57,  -54,  -51,  -48,  -45,  -42,  -39,  -37,  -34,  -30,  -27,
    -24,  -21,  -18,  -15,  -12,  -9,   -6,   -3,   0

};

// Structs

// Encoded size of each record kind, used to reject counts that cannot fit in the file.
enum {
    IS2_SUB_SONG_BYTES = 10,
    IS2_POSITION_BYTES = 16,
    IS2_TRACK_LINE_BYTES = 4,
    IS2_SAMPLE_INFO_BYTES = 44,
    IS2_INSTRUMENT_BYTES = 1010,
};

// Big-endian reader
typedef struct Is2Reader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} Is2Reader;

// Reader helpers

static void is2_reader_init(Is2Reader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

static bool is2_reader_eof(const Is2Reader* r) {
    return r->pos > r->size;
}

static uint8_t is2_reader_u8(Is2Reader* r) {
    if (r->pos >= r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    return r->data[r->pos++];
}

static int8_t is2_reader_i8(Is2Reader* r) {
    return (int8_t)is2_reader_u8(r);
}

static uint16_t is2_reader_b_u16(Is2Reader* r) {
    uint8_t hi = is2_reader_u8(r);
    uint8_t lo = is2_reader_u8(r);
    return (uint16_t)((hi << 8) | lo);
}

static uint32_t is2_reader_b_u32(Is2Reader* r) {
    uint16_t hi = is2_reader_b_u16(r);
    uint16_t lo = is2_reader_b_u16(r);
    return ((uint32_t)hi << 16) | lo;
}

static void is2_reader_skip(Is2Reader* r, size_t bytes) {
    r->pos += bytes;
}

static void is2_reader_read(Is2Reader* r, void* dst, size_t len) {
    for (size_t i = 0; i < len; i++)
        ((uint8_t*)dst)[i] = is2_reader_u8(r);
}

static void is2_reader_read_signed(Is2Reader* r, int8_t* dst, size_t len) {
    for (size_t i = 0; i < len; i++)
        dst[i] = is2_reader_i8(r);
}

static bool is2_reader_read_mark(Is2Reader* r, char* buf, int len) {
    for (int i = 0; i < len; i++)
        buf[i] = (char)is2_reader_u8(r);
    buf[len] = '\0';
    return !is2_reader_eof(r);
}

// True when `count` records of `record_size` bytes still fit in the unread data.
static bool is2_reader_fits(const Is2Reader* r, uint32_t count, size_t record_size) {
    const size_t remaining = r->pos <= r->size ? r->size - r->pos : 0;
    return count <= remaining / record_size;
}

// Table lookups for indices that come from the module data; positions outside the table read as zero.
static uint8_t is2_table_u8(const uint8_t* table, size_t size, size_t index) {
    return index < size ? table[index] : 0;
}

static int8_t is2_table_i8(const int8_t* table, size_t size, size_t index) {
    return index < size ? table[index] : 0;
}

// Samples and instruments are numbered from 1; 0 and numbers past the table select nothing.
static Is2Sample* is2_sample_at(const Is2Module* m, uint8_t number) {
    return number >= 1 && number <= m->num_samples ? &m->samples[number - 1] : nullptr;
}

static Is2Instrument* is2_instrument_at(const Is2Module* m, uint8_t number) {
    return number >= 1 && number <= m->num_instruments ? &m->instruments[number - 1] : nullptr;
}

// Visit tracking

static void is2_clear_visited(Is2Module* m) {
    memset(m->visited, 0, sizeof(m->visited));
}

static void is2_mark_visited(Is2Module* m, int pos) {
    if (pos >= 0 && pos < 1024)
        m->visited[pos] = 1;
}

static bool is2_has_visited(const Is2Module* m, int pos) {
    if (pos >= 0 && pos < 1024)
        return m->visited[pos] != 0;
    return false;
}

// Channel operations

static void is2_ch_mute(Is2Channel* ch) {
    ch->active = false;
}

static void is2_ch_play_sample(Is2Channel* ch, int8_t* data, uint32_t data_size, uint32_t start_offset,
                               uint32_t length) {
    ch->sample_data = data;
    ch->data_size = data_size;
    ch->sample_offset = start_offset;
    ch->sample_length = start_offset + length;
    ch->loop_start = 0;
    ch->loop_length = 0;
    ch->active = true;
    ch->use_volume_256 = false;
    ch->play_pending = true;
    ch->latch_pending = false;
}

static void is2_ch_set_loop(Is2Channel* ch, uint32_t start_offset, uint32_t length) {
    ch->loop_start = start_offset;
    ch->loop_length = length;
}

// The registers as a sample, cut to the readable data.
static NuadeSample is2_ch_sample(const Is2Channel* ch) {
    const uint32_t start = ch->sample_offset < ch->data_size ? ch->sample_offset : ch->data_size;
    const uint32_t end = ch->sample_length < ch->data_size ? ch->sample_length : ch->data_size;
    NuadeSample sample
        = { .data = ch->sample_data + start, .format = NuadeSampleFormat_S8, .length = end > start ? end - start : 0 };
    if (ch->loop_length != 0 && ch->loop_start >= start) {
        sample.loop_start = ch->loop_start - start;
        sample.loop_length = ch->loop_length;
    }
    return sample;
}

// Point the channel at another window of the data; Paula takes it up when the current one ends.
static void is2_ch_latch_window(Is2Channel* ch, int8_t* data, uint32_t data_size, uint32_t start_offset,
                                uint32_t length) {
    if (ch->play_pending && !ch->latch_pending) {
        ch->first_window = is2_ch_sample(ch);
    }
    ch->sample_data = data;
    ch->data_size = data_size;
    ch->sample_offset = start_offset;
    ch->sample_length = start_offset + length;
    is2_ch_set_loop(ch, start_offset, length);
    ch->latch_pending = true;
}

static void is2_ch_set_amiga_period(Is2Channel* ch, uint16_t period) {
    ch->period = period;
}

static void is2_ch_set_amiga_volume(Is2Channel* ch, uint16_t vol) {
    if (vol > 64)
        vol = 64;
    ch->volume = vol;
    ch->use_volume_256 = false;
}

static void is2_ch_set_volume(Is2Channel* ch, uint16_t vol) {
    // Volume 0-256 range
    if (vol > 256)
        vol = 256;
    ch->volume = vol;
    ch->use_volume_256 = true;
}

// Loading

static bool is2_read_sub_songs(Is2Module* m, Is2Reader* r) {
    char mark[5];
    is2_reader_read_mark(r, mark, 4);
    if (strcmp(mark, "STBL") != 0)
        return false;

    uint32_t num = is2_reader_b_u32(r);
    if (num == 0 || !is2_reader_fits(r, num, IS2_SUB_SONG_BYTES))
        return false;
    m->num_sub_songs = (int)num;
    m->sub_songs = (Is2SongInfo*)calloc(num, sizeof(Is2SongInfo));
    if (!m->sub_songs)
        return false;

    for (uint32_t i = 0; i < num; i++) {
        Is2SongInfo* si = &m->sub_songs[i];

        si->start_speed = is2_reader_u8(r);
        si->rows_per_track = is2_reader_u8(r);
        si->first_position = is2_reader_b_u16(r);
        si->last_position = is2_reader_b_u16(r);
        si->restart_position = is2_reader_b_u16(r);
        si->tempo = is2_reader_b_u16(r);

        if (is2_reader_eof(r))
            return false;
    }

    return true;
}

static bool is2_read_positions(Is2Module* m, Is2Reader* r) {
    char mark[5];
    is2_reader_read_mark(r, mark, 4);
    if (strcmp(mark, "OVTB") != 0)
        return false;

    uint32_t num = is2_reader_b_u32(r);
    if (num == 0 || !is2_reader_fits(r, num, IS2_POSITION_BYTES))
        return false;
    m->num_positions = (int)num;
    m->positions = (Is2SinglePositionInfo**)calloc(num, sizeof(Is2SinglePositionInfo*));
    if (!m->positions)
        return false;

    for (uint32_t i = 0; i < num; i++) {
        m->positions[i] = (Is2SinglePositionInfo*)calloc(4, sizeof(Is2SinglePositionInfo));
        if (!m->positions[i])
            return false;

        for (int j = 0; j < 4; j++) {
            Is2SinglePositionInfo* sp = &m->positions[i][j];

            sp->start_track_row = is2_reader_b_u16(r);
            sp->sound_transpose = is2_reader_i8(r);
            sp->note_transpose = is2_reader_i8(r);
        }

        if (is2_reader_eof(r))
            return false;
    }

    return true;
}

static bool is2_read_track_rows(Is2Module* m, Is2Reader* r) {
    char mark[5];
    is2_reader_read_mark(r, mark, 4);
    if (strcmp(mark, "NTBL") != 0)
        return false;

    uint32_t num = is2_reader_b_u32(r);
    if (!is2_reader_fits(r, num, IS2_TRACK_LINE_BYTES))
        return false;
    m->num_track_lines = (int)num;
    m->track_lines = (Is2TrackLine*)calloc(num, sizeof(Is2TrackLine));
    if (num > 0 && !m->track_lines)
        return false;

    for (uint32_t i = 0; i < num; i++) {
        Is2TrackLine* tl = &m->track_lines[i];

        uint8_t byt1 = is2_reader_u8(r);
        uint8_t byt2 = is2_reader_u8(r);
        uint8_t byt3 = is2_reader_u8(r);
        uint8_t byt4 = is2_reader_u8(r);

        tl->note = byt1;
        tl->instrument = byt2;
        tl->disable_sound_transpose = (byt3 & 0x80) != 0;
        tl->disable_note_transpose = (byt3 & 0x40) != 0;
        tl->arpeggio = (uint8_t)((byt3 & 0x30) >> 4);
        tl->effect = (Is2Effect)(byt3 & 0x0f);
        tl->effect_arg = byt4;

        if (is2_reader_eof(r))
            return false;
    }

    return true;
}

static bool is2_read_sample_info(Is2Module* m, Is2Reader* r) {
    char mark[5];
    is2_reader_read_mark(r, mark, 4);
    if (strcmp(mark, "SAMP") != 0)
        return false;

    uint32_t num = is2_reader_b_u32(r);
    if (!is2_reader_fits(r, num, IS2_SAMPLE_INFO_BYTES))
        return false;
    m->num_samples = (int)num;
    m->samples = (Is2Sample*)calloc(num, sizeof(Is2Sample));
    if (num > 0 && !m->samples)
        return false;

    for (uint32_t i = 0; i < num; i++) {
        Is2Sample* samp = &m->samples[i];

        samp->one_shot_length = is2_reader_b_u16(r);
        samp->repeat_length = is2_reader_b_u16(r);
        samp->sample_number = is2_reader_i8(r);
        samp->volume = is2_reader_u8(r);
        samp->vibrato_delay = is2_reader_u8(r);
        samp->vibrato_speed = is2_reader_u8(r);
        samp->vibrato_level = is2_reader_u8(r);
        samp->portamento_speed = is2_reader_u8(r);

        if (is2_reader_eof(r))
            return false;

        is2_reader_skip(r, 6); // skip pad
    }

    // Skip sample names (20 bytes each)
    for (uint32_t i = 0; i < num; i++) {
        is2_reader_skip(r, 20);
        if (is2_reader_eof(r))
            return false;
    }

    // Skip copy of sample lengths and loop lengths stored in words
    is2_reader_skip(r, num * 4 * 2);

    return true;
}

static bool is2_read_sample_data(Is2Module* m, Is2Reader* r) {
    int num = m->num_samples;

    if (num > 0) {
        m->sample_data_size = (uint32_t*)calloc(num, sizeof(uint32_t));
        m->sample_data = (int8_t**)calloc(num, sizeof(int8_t*));
        if (!m->sample_data_size || !m->sample_data)
            return false;
        m->num_sample_data = num;

        for (int i = 0; i < num; i++)
            m->sample_data_size[i] = is2_reader_b_u32(r);

        if (is2_reader_eof(r))
            return false;

        // Sample data are stored in reverse order
        for (int i = num - 1; i >= 0; i--) {
            uint32_t len = m->sample_data_size[i];
            if (len > UINT32_MAX - 2U || !is2_reader_fits(r, len, 1))
                return false;

            if (len > 0) {
                // Two bytes of overrun room: the player fetches one word past a sample.
                m->sample_data[i] = (int8_t*)calloc(len + 2U, 1);
                if (!m->sample_data[i])
                    return false;
                is2_reader_read_signed(r, m->sample_data[i], len);
            }
        }

        // The samples sit in memory back to back, highest number first, so the word past a sample is the first word
        // of the next lower sample that has data.
        int next = -1; // the highest-numbered sample so far with a word of data
        for (int i = 0; i < num; i++) {
            if (m->sample_data[i] && next >= 0)
                memcpy(m->sample_data[i] + m->sample_data_size[i], m->sample_data[next], 2);
            if (m->sample_data_size[i] >= 2)
                next = i;
        }
    }

    return true;
}

static bool is2_read_instruments(Is2Module* m, Is2Reader* r) {
    char mark[5];
    is2_reader_read_mark(r, mark, 4);
    if (strcmp(mark, "SYNT") != 0)
        return false;

    uint32_t num = is2_reader_b_u32(r);
    if (!is2_reader_fits(r, num, IS2_INSTRUMENT_BYTES))
        return false;
    m->num_instruments = (int)num;
    m->instruments = (Is2Instrument*)calloc(num, sizeof(Is2Instrument));
    if (num > 0 && !m->instruments)
        return false;

    for (uint32_t i = 0; i < num; i++) {
        Is2Instrument* instr = &m->instruments[i];

        is2_reader_read_mark(r, mark, 4);
        if (strcmp(mark, "IS20") != 0)
            return false;

        uint8_t name[20];
        is2_reader_read(r, name, 20);
        instr->portamento_divisor = name[5];

        instr->waveform_length = is2_reader_b_u16(r);
        instr->volume = is2_reader_u8(r);
        instr->vibrato_delay = is2_reader_u8(r);
        instr->vibrato_speed = is2_reader_u8(r);
        instr->vibrato_level = is2_reader_u8(r);
        instr->portamento_speed = is2_reader_u8(r);
        instr->adsr_length = is2_reader_u8(r);
        instr->adsr_repeat = is2_reader_u8(r);

        if (is2_reader_eof(r))
            return false;

        is2_reader_skip(r, 4); // skip padding

        instr->sustain_point = is2_reader_u8(r);
        instr->sustain_speed = is2_reader_u8(r);
        instr->amf_length = is2_reader_u8(r);
        instr->amf_repeat = is2_reader_u8(r);

        uint8_t eg_mode = is2_reader_u8(r);
        uint8_t eg_enabled = is2_reader_u8(r);

        instr->eg_mode = eg_enabled == 0 ? IS2_EG_DISABLED : eg_mode == 0 ? IS2_EG_CALC : IS2_EG_FREE;

        instr->start_len = is2_reader_u8(r);
        instr->stop_rep = is2_reader_u8(r);
        instr->speed_up = is2_reader_u8(r);
        instr->speed_down = is2_reader_u8(r);

        if (is2_reader_eof(r))
            return false;

        is2_reader_skip(r, 19); // skip padding

        is2_reader_read(r, instr->adsr_table, 128);
        is2_reader_read_signed(r, instr->lfo_table, 128);

        for (int j = 0; j < 3; j++) {
            instr->arpeggios[j].length = is2_reader_u8(r);
            instr->arpeggios[j].repeat = is2_reader_u8(r);
            is2_reader_read_signed(r, instr->arpeggios[j].values, 14);
        }

        is2_reader_read(r, instr->eg_table, 128);
        is2_reader_read_signed(r, instr->waveform1, 256);
        is2_reader_read_signed(r, instr->waveform2, 256);
    }

    return true;
}

// Initialize

static void is2_initialize_sound(Is2Module* m, int sub_song) {
    m->current_song_info = &m->sub_songs[sub_song];

    m->playing_info.speed_counter = m->current_song_info->start_speed;
    m->playing_info.current_speed = m->current_song_info->start_speed;
    m->playing_info.song_position = (int16_t)(m->current_song_info->first_position - 1);
    m->playing_info.row_position = m->current_song_info->rows_per_track;
    m->playing_info.rows_per_track = m->current_song_info->rows_per_track;
    m->playing_info.shown_position = m->current_song_info->first_position;
    m->playing_info.shown_row = 0;
    m->playing_info.shown_rows = m->current_song_info->rows_per_track;

    for (int i = 0; i < 4; i++) {
        Is2VoiceInfo* v = &m->voices[i];
        memset(v, 0, sizeof(Is2VoiceInfo));
        v->playing_mode = IS2_MODE_SAMPLE;

        Is2Channel* ch = &m->channels[i];
        memset(ch, 0, sizeof(Is2Channel));
    }

    m->tempo = m->current_song_info->tempo != 0 ? m->current_song_info->tempo : 50;

    m->loop_count = 0;
}

// Playback - SetEffects

static void is2_set_effects(Is2Module* m, Is2VoiceInfo* v) {
    switch (v->effect) {
        case IS2_EFFECT_SET_SLIDE_SPEED:
            v->slide_speed = (int8_t)v->effect_arg;
            break;

        case IS2_EFFECT_RESTART_ADSR:
            v->adsr_position = v->effect_arg;
            break;

        case IS2_EFFECT_SET_PORTAMENTO:
            v->portamento_speed_counter = v->effect_arg;
            v->portamento_speed = v->effect_arg;
            break;

        case IS2_EFFECT_SET_VOLUME_INCREMENT: {
            int new_vol = v->current_volume + (int8_t)v->effect_arg;
            if (new_vol < 0)
                new_vol = 0;
            else {
                if (v->playing_mode == IS2_MODE_SAMPLE) {
                    if (new_vol > 64)
                        new_vol = 64;
                } else {
                    if (new_vol > 255)
                        new_vol = 255;
                }
            }
            v->current_volume = (uint8_t)new_vol;
            break;
        }

        case IS2_EFFECT_POSITION_JUMP:
            m->playing_info.song_position = v->effect_arg;
            m->playing_info.row_position = m->playing_info.rows_per_track;
            break;

        case IS2_EFFECT_TRACK_BREAK:
            m->playing_info.row_position = m->playing_info.rows_per_track;
            break;

        case IS2_EFFECT_SET_VOLUME: {
            uint8_t nv = v->effect_arg;
            if ((v->playing_mode == IS2_MODE_SAMPLE) && (nv > 64))
                nv = 64;
            v->current_volume = nv;
            break;
        }

        case IS2_EFFECT_SET_TRACK_LEN:
            if (v->effect_arg <= 64)
                m->playing_info.rows_per_track = v->effect_arg;
            break;

        case IS2_EFFECT_SET_FILTER:
            // No filter in our implementation
            break;

        case IS2_EFFECT_SET_SPEED:
            if ((v->effect_arg > 0) && (v->effect_arg <= 16))
                m->playing_info.current_speed = v->effect_arg;
            break;

        case IS2_EFFECT_SET_VIBRATO:
            v->vibrato_delay = 0;
            v->vibrato_speed = (uint8_t)(((v->effect_arg >> 4) & 0x0f) * 2);
            v->vibrato_level = (uint8_t)((-((v->effect_arg & 0x0f) << 4)) + 160);
            break;

        default:
            break;
    }
}

// Playback - Transpose, Reset, Force Quiet

static void is2_set_transpose(Is2VoiceInfo* v, uint8_t* note, uint8_t* instr_num) {
    if (!v->disable_note_transpose || v->disable_sound_transpose)
        *note = (uint8_t)(*note + v->note_transpose);

    if (!v->disable_sound_transpose || v->disable_note_transpose)
        *instr_num = (uint8_t)(*instr_num + v->sound_transpose);
}

static void is2_reset_voice(Is2VoiceInfo* v) {
    v->transposed_instrument = 0;
    v->arpeggio_position = 0;
    v->slide_speed = 0;
    v->slide_value = 0;
    v->vibrato_position = 0;
    v->adsr_position = 0;
    v->sustain_counter = 0;
    v->eg_duration = 0;
    v->eg_position = 0;
    v->lfo_position = 0;

    if (v->effect != IS2_EFFECT_SET_PORTAMENTO) {
        v->portamento_speed_counter = 0;
        v->portamento_speed = 0;
    }

    if (v->effect != IS2_EFFECT_SET_VIBRATO) {
        v->vibrato_delay = 0;
        v->vibrato_speed = 0;
        v->vibrato_level = 0;
    }
}

static void is2_force_quiet(Is2VoiceInfo* v, Is2Channel* ch) {
    is2_ch_mute(ch);
    v->current_volume = 0;
    v->transposed_instrument = 0;
    v->playing_mode = IS2_MODE_SAMPLE;
}

// Playback - RestoreVoice

static void is2_restore_voice(Is2Module* m, Is2VoiceInfo* v, uint8_t instr_num) {
    instr_num &= 0x3f;
    v->transposed_instrument = instr_num;

    if (v->playing_mode == IS2_MODE_SAMPLE) {
        Is2Sample* samp = is2_sample_at(m, instr_num);
        if (samp == nullptr)
            return;

        if (v->effect != IS2_EFFECT_SET_VIBRATO) {
            v->vibrato_delay = samp->vibrato_delay;
            v->vibrato_speed = samp->vibrato_speed;
            v->vibrato_level = samp->vibrato_level;
        }

        if ((v->effect != IS2_EFFECT_SKIP_PORTAMENTO) && (v->effect != IS2_EFFECT_SET_PORTAMENTO)) {
            v->portamento_speed_counter = samp->portamento_speed;
            v->portamento_speed = samp->portamento_speed;
        }

        if ((v->effect != IS2_EFFECT_SET_VOLUME) && (v->effect != IS2_EFFECT_SET_VOLUME_INCREMENT))
            v->current_volume = samp->volume;
    } else {
        Is2Instrument* instr = is2_instrument_at(m, instr_num);
        if (instr == nullptr)
            return;

        if (v->effect != IS2_EFFECT_SET_VIBRATO) {
            v->vibrato_delay = instr->vibrato_delay;
            v->vibrato_speed = instr->vibrato_speed;
            v->vibrato_level = instr->vibrato_level;
        }

        if ((v->effect != IS2_EFFECT_SKIP_PORTAMENTO) && (v->effect != IS2_EFFECT_SET_PORTAMENTO)) {
            v->portamento_speed_counter = instr->portamento_speed;
            v->portamento_speed = instr->portamento_divisor;
        }

        if ((v->effect != IS2_EFFECT_SET_VOLUME) && (v->effect != IS2_EFFECT_SET_VOLUME_INCREMENT))
            v->current_volume = instr->volume;
    }
}

// Playback - SetSampleInstrument

static void is2_set_sample_instrument(Is2Module* m, Is2VoiceInfo* v, Is2Channel* ch, uint8_t instr_num) {
    v->playing_mode = IS2_MODE_SAMPLE;

    instr_num &= 0x3f;
    v->transposed_instrument = instr_num;

    Is2Sample* samp = is2_sample_at(m, instr_num);
    if (samp == nullptr) {
        is2_force_quiet(v, ch);
        return;
    }

    if (v->effect != IS2_EFFECT_SET_VIBRATO) {
        v->vibrato_delay = samp->vibrato_delay;
        v->vibrato_speed = samp->vibrato_speed;
        v->vibrato_level = samp->vibrato_level;
    }

    if ((v->effect != IS2_EFFECT_SKIP_PORTAMENTO) && (v->effect != IS2_EFFECT_SET_PORTAMENTO)) {
        v->portamento_speed_counter = samp->portamento_speed;
        v->portamento_speed = samp->portamento_speed;
    }

    if ((samp->sample_number < 0) || (samp->sample_number >= m->num_sample_data)
        || (m->sample_data[samp->sample_number] == nullptr)) {
        is2_force_quiet(v, ch);
        return;
    }

    int8_t* data = m->sample_data[samp->sample_number];

    uint32_t play_length = samp->one_shot_length;
    uint32_t loop_start = 0;
    uint32_t loop_length = 0;

    if (samp->repeat_length == 0)
        loop_length = samp->one_shot_length;
    else {
        // The asm plays one_shot + repeat words. A repeat of 1 is the "no loop" marker, so its word is played once; it
        // is the first word of the next sample in memory, and the last sample has none (the voice stays quiet).
        play_length += samp->repeat_length;
        if (samp->repeat_length == 1) {
            if (samp->sample_number == 0) {
                is2_force_quiet(v, ch);
                return;
            }
        } else {
            loop_start = samp->one_shot_length;
            loop_length = samp->repeat_length;
        }
    }

    is2_ch_play_sample(ch, data, m->sample_data_size[samp->sample_number] + 2U, 0, play_length * 2U);

    if (loop_length != 0)
        is2_ch_set_loop(ch, loop_start * 2U, loop_length * 2U);

    if ((v->effect != IS2_EFFECT_SET_VOLUME) && (v->effect != IS2_EFFECT_SET_VOLUME_INCREMENT)) {
        v->current_volume = samp->volume;
        is2_ch_set_amiga_volume(ch, samp->volume);
    }
}

// Playback - SetSynthInstrument

static void is2_set_synth_instrument(Is2Module* m, Is2VoiceInfo* v, Is2Channel* ch, uint8_t instr_num) {
    v->playing_mode = IS2_MODE_SYNTH;

    instr_num &= 0x3f;
    v->transposed_instrument = instr_num;

    Is2Instrument* instr = is2_instrument_at(m, instr_num);
    if (instr == nullptr) {
        is2_force_quiet(v, ch);
        return;
    }

    if (v->effect != IS2_EFFECT_SET_VIBRATO) {
        v->vibrato_delay = instr->vibrato_delay;
        v->vibrato_speed = instr->vibrato_speed;
        v->vibrato_level = instr->vibrato_level;
    }

    if ((v->effect != IS2_EFFECT_SKIP_PORTAMENTO) && (v->effect != IS2_EFFECT_SET_PORTAMENTO)) {
        v->portamento_speed_counter = instr->portamento_speed;
        v->portamento_speed = instr->portamento_divisor;
    }

    uint8_t eg_val;

    switch (instr->eg_mode) {
        case IS2_EG_DISABLED:
        default:
            v->eg_duration = 0;
            eg_val = 0;
            break;

        case IS2_EG_FREE:
            v->eg_position = 0;
            if ((uint8_t)(instr->start_len + instr->stop_rep) == 0) {
                v->eg_duration = 0;
                eg_val = 0;
            } else {
                eg_val = instr->eg_table[0];
            }
            break;

        case IS2_EG_CALC:
            v->eg_position = (uint16_t)(instr->start_len << 8);
            v->eg_duration = 1;
            eg_val = instr->start_len;
            break;
    }

    int8_t* waveform = (eg_val & 1) != 0 ? instr->waveform2 : instr->waveform1;
    uint32_t length = instr->waveform_length;
    uint32_t start_offset = (uint32_t)eg_val & 0xfe;

    is2_ch_play_sample(ch, waveform, sizeof(instr->waveform1), start_offset, length);
    is2_ch_set_loop(ch, start_offset, length);

    if ((v->effect != IS2_EFFECT_SET_VOLUME) && (v->effect != IS2_EFFECT_SET_VOLUME_INCREMENT))
        v->current_volume = instr->volume;
}

// Playback - PlayVoice

static void is2_play_voice(Is2Module* m, Is2VoiceInfo* v, Is2Channel* ch) {
    is2_set_effects(m, v);

    uint8_t note = v->note;
    uint8_t instr_num = v->instrument;

    if (note == 0) {
        if (instr_num != 0)
            is2_restore_voice(m, v, instr_num);
    } else {
        if (note != 0x80) {
            if (note == 0x7f)
                is2_force_quiet(v, ch);
            else {
                is2_set_transpose(v, &note, &instr_num);

                v->previous_transposed_note = v->transposed_note;
                v->transposed_note = note;

                // A note without an instrument only changes the pitch of the voice.
                if (instr_num != 0 && instr_num <= 127) {
                    is2_reset_voice(v);

                    if (instr_num >= 64)
                        is2_set_sample_instrument(m, v, ch, instr_num);
                    else
                        is2_set_synth_instrument(m, v, ch, instr_num);
                }
            }
        }
    }
}

// Playback - GetNextRow / GetNextPosition / GetNewNotes / CheckForNewInstruments / UpdateVoices

static void is2_get_next_position(Is2Module* m) {
    m->playing_info.song_position++;

    if (m->playing_info.song_position > (int16_t)m->current_song_info->last_position) {
        // A restart at the very last position of the file plays from the first position of subsong 0.
        const bool restart_at_end = m->current_song_info->restart_position == (uint16_t)(m->num_positions - 1);
        m->playing_info.song_position
            = (int16_t)(restart_at_end ? m->sub_songs[0].first_position : m->current_song_info->restart_position);
    }

    if (m->playing_info.song_position < 0 || m->playing_info.song_position >= m->num_positions)
        m->playing_info.song_position = 0;

    if (is2_has_visited(m, m->playing_info.song_position)) {
        m->loop_count++;
        is2_clear_visited(m);
    }

    is2_mark_visited(m, m->playing_info.song_position);

    Is2SinglePositionInfo* pos_row = m->positions[m->playing_info.song_position];

    for (int i = 0; i < 4; i++) {
        Is2VoiceInfo* v = &m->voices[i];
        v->start_track_row = pos_row[i].start_track_row;
        v->sound_transpose = pos_row[i].sound_transpose;
        v->note_transpose = pos_row[i].note_transpose;
    }
}

static void is2_get_new_notes(Is2Module* m) {
    for (int i = 0; i < 4; i++) {
        Is2VoiceInfo* v = &m->voices[i];

        int position = v->start_track_row + m->playing_info.row_position;
        Is2TrackLine empty_track = { 0 };
        Is2TrackLine* tl = position < m->num_track_lines ? &m->track_lines[position] : &empty_track;

        v->note = tl->note;
        v->instrument = tl->instrument;
        v->disable_sound_transpose = tl->disable_sound_transpose;
        v->disable_note_transpose = tl->disable_note_transpose;
        v->arpeggio = tl->arpeggio;
        v->effect = tl->effect;
        v->effect_arg = tl->effect_arg;
    }
}

// Stop the voices that get a new note with an instrument. The original clears one byte per voice at a fixed distance
// from the first voice that matches the voice record size only for voice 0: for the others the byte it clears belongs
// to the voice before (the low byte of voice 1's sustain counter for voice 2, voice 2's envelope generator direction
// for voice 3, and padding for voice 1).
static void is2_check_for_new_instruments(Is2Module* m) {
    const Is2VoiceInfo* v = m->voices;
    bool restarted[4];
    for (int i = 0; i < 4; i++)
        restarted[i] = (v[i].note != 0) && (v[i].instrument != 0);

    if (restarted[0])
        m->voices[0].transposed_instrument = 0;
    if (restarted[2])
        m->voices[1].sustain_counter &= 0xff00;
    if (restarted[3])
        m->voices[2].eg_duration = 0;
}

static void is2_update_voices(Is2Module* m) {
    for (int i = 0; i < 4; i++)
        is2_play_voice(m, &m->voices[i], &m->channels[i]);
}

static void is2_get_next_row(Is2Module* m) {
    m->playing_info.row_position++;

    if (m->playing_info.row_position >= m->playing_info.rows_per_track) {
        m->playing_info.row_position = 0;
        is2_get_next_position(m);
    }

    is2_get_new_notes(m);
    is2_check_for_new_instruments(m);

    m->playing_info.shown_position = (uint16_t)m->playing_info.song_position;
    m->playing_info.shown_row = m->playing_info.row_position;
    is2_update_voices(m);
    m->playing_info.shown_rows = m->playing_info.rows_per_track;
}

// Playback - UpdateEffects (per-tick)

// Slide from the previous note's period to the current one over `portamento_speed` ticks, in either direction.
static void is2_do_portamento(uint16_t* period, const uint16_t* prev_period, Is2VoiceInfo* v) {
    if (v->portamento_speed == 0 || v->portamento_speed_counter == 0 || *period == *prev_period)
        return;

    v->portamento_speed_counter--;

    int32_t offset = (int16_t)(uint16_t)(*prev_period - *period) * (int16_t)v->portamento_speed_counter;
    const int32_t scaled = offset / (int16_t)v->portamento_speed;
    if (scaled >= INT16_MIN && scaled <= INT16_MAX) // divs.w leaves the dividend alone when the quotient overflows
        offset = scaled;
    *period = (uint16_t)(offset + *period);
}

static void is2_do_vibrato(uint16_t* period, Is2VoiceInfo* v) {
    if (v->vibrato_delay != 255) {
        if (v->vibrato_delay == 0) {
            int8_t vib_val = is2_vibrato[v->vibrato_position];
            uint8_t vib_level = v->vibrato_level;

            if (vib_val < 0) {
                if (vib_level != 0)
                    *period -= (uint16_t)((-vib_val * 4) / vib_level);
            } else {
                if (vib_level != 0)
                    *period += (uint16_t)((vib_val * 4) / vib_level);
            }

            v->vibrato_position = (uint16_t)((v->vibrato_position + v->vibrato_speed) & 0xff);
        } else {
            v->vibrato_delay--;
        }
    }

    *period = (uint16_t)(*period + v->slide_value);
}

static void is2_do_lfo(uint16_t* period, Is2VoiceInfo* v, Is2Instrument* instr) {
    if ((instr->amf_length + instr->amf_repeat) != 0) {
        int8_t lfo_val = is2_table_i8(instr->lfo_table, sizeof(instr->lfo_table), v->lfo_position);
        *period = (uint16_t)(*period - lfo_val);

        if (v->lfo_position == (instr->amf_length + instr->amf_repeat))
            v->lfo_position = instr->amf_length;
        else
            v->lfo_position++;
    }
}

static void is2_do_adsr(Is2VoiceInfo* v, Is2Channel* ch, Is2Instrument* instr) {
    if ((instr->adsr_length + instr->adsr_repeat) == 0) {
        is2_ch_set_volume(ch, v->current_volume);
    } else {
        uint8_t adsr_val = is2_table_u8(instr->adsr_table, sizeof(instr->adsr_table), v->adsr_position);
        uint16_t vol = (uint16_t)((v->current_volume * adsr_val) / 256);
        is2_ch_set_volume(ch, vol);

        if (v->adsr_position >= (instr->adsr_length + instr->adsr_repeat))
            v->adsr_position = instr->adsr_length;
        else {
            if ((v->note != 0x80) || (instr->sustain_speed == 1) || (v->adsr_position < instr->sustain_point))
                v->adsr_position++;
            else {
                if (instr->sustain_speed != 0) {
                    if (v->sustain_counter == 0) {
                        v->sustain_counter = instr->sustain_speed;
                        v->adsr_position++;
                    } else {
                        v->sustain_counter--;
                    }
                }
            }
        }
    }
}

static void is2_do_sample_arpeggio(Is2VoiceInfo* v, uint16_t* period, uint16_t* prev_period) {
    uint8_t note = v->transposed_note;
    uint8_t prev_note = v->previous_transposed_note;

    if (v->effect == IS2_EFFECT_ARPEGGIO) {
        uint8_t arp_val = v->arpeggio_effect_nibble ? (v->effect_arg & 0x0f) : (v->effect_arg >> 4);
        v->arpeggio_effect_nibble = !v->arpeggio_effect_nibble;

        note += arp_val;
        prev_note += arp_val;
    }

    *period = is2_periods[note < IS2_PERIODS_LEN ? note : 0];
    *prev_period = is2_periods[prev_note < IS2_PERIODS_LEN ? prev_note : 0];
}

static void is2_do_synth_arpeggio(Is2VoiceInfo* v, Is2Instrument* instr, uint16_t* period, uint16_t* prev_period) {
    uint8_t note = v->transposed_note;
    uint8_t prev_note = v->previous_transposed_note;

    if (v->arpeggio != 0) {
        Is2Arpeggio* arp = &instr->arpeggios[v->arpeggio - 1];

        int8_t arp_val = is2_table_i8(arp->values, sizeof(arp->values), v->arpeggio_position);
        note = (uint8_t)(note + arp_val);
        prev_note = (uint8_t)(prev_note + arp_val);

        if (v->arpeggio_position == (arp->length + arp->repeat))
            v->arpeggio_position = arp->length;
        else
            v->arpeggio_position++;
    }

    *period = note < IS2_PERIODS_LEN ? is2_periods[note] : 0;
    *prev_period = prev_note < IS2_PERIODS_LEN ? is2_periods[prev_note] : 0;
}

// Forward declaration needed for is2_do_envelope_generator which uses m
// We already inlined it above, but it references 'm' which needs to be passed.
// Let me restructure: the is2_do_envelope_generator needs the module pointer.

// One line per voice and tick: the state the tick leaves on the voice.
static void is2_dump_voice(const Is2Module* m, const Is2VoiceInfo* v, const Is2Channel* ch) {
    if (m->dump_state == nullptr)
        return;
    fprintf(m->dump_state,
            "%u ch%d mode=%d note=%u instr=%u eff=%x/%02x tnote=%u ptnote=%u tinstr=%u arp=%u arppos=%u pspd=%u "
            "pcnt=%u vib=%u/%u/%u/%u "
            "slide=%d lfo=%u adsr=%u sus=%u egpos=%u egdur=%d vol=%u per=%u\n",
            m->tick_number, (int)(ch - m->channels), v->playing_mode, v->note, v->instrument, v->effect, v->effect_arg,
            v->transposed_note, v->previous_transposed_note, v->transposed_instrument, v->arpeggio,
            v->arpeggio_position, v->portamento_speed, v->portamento_speed_counter, v->vibrato_delay, v->vibrato_speed,
            v->vibrato_level, v->vibrato_position, v->slide_value, v->lfo_position, v->adsr_position,
            v->sustain_counter, v->eg_position, v->eg_duration, ch->volume, ch->period);
}

static void is2_update_voice_effect(Is2Module* m, Is2VoiceInfo* v, Is2Channel* ch) {
    if (v->transposed_instrument == 0) {
        is2_ch_mute(ch);
        return;
    }

    if (v->playing_mode == IS2_MODE_SAMPLE) {
        uint16_t period, prev_period;
        is2_do_sample_arpeggio(v, &period, &prev_period);
        is2_do_portamento(&period, &prev_period, v);
        is2_do_vibrato(&period, v);

        is2_ch_set_amiga_period(ch, period);
        is2_ch_set_amiga_volume(ch, v->current_volume);
    } else {
        Is2Instrument* instr = is2_instrument_at(m, v->transposed_instrument);
        if (instr == nullptr) {
            is2_ch_mute(ch);
            return;
        }

        uint16_t period, prev_period;
        is2_do_synth_arpeggio(v, instr, &period, &prev_period);
        is2_do_portamento(&period, &prev_period, v);
        is2_do_vibrato(&period, v);
        is2_do_lfo(&period, v, instr);

        is2_ch_set_amiga_period(ch, period);

        is2_do_adsr(v, ch, instr);

        // Envelope generator
        if (instr->eg_mode != IS2_EG_DISABLED) {
            uint8_t eg_val;

            if (instr->eg_mode == IS2_EG_FREE) {
                int len = (uint8_t)(instr->start_len + instr->stop_rep);
                if (len != 0) {
                    if (v->eg_position > (uint16_t)len)
                        v->eg_position = instr->start_len;
                    else
                        v->eg_position++;

                    eg_val = is2_table_u8(instr->eg_table, sizeof(instr->eg_table), v->eg_position);

                    int8_t* waveform = (eg_val & 1) != 0 ? instr->waveform2 : instr->waveform1;
                    uint32_t length = instr->waveform_length;
                    uint32_t start_offset = (uint32_t)eg_val & 0xfe;

                    is2_ch_latch_window(ch, waveform, sizeof(instr->waveform1), start_offset, length);
                }
            } else {
                // Calc mode
                if (v->eg_duration != 0) {
                    uint16_t position = v->eg_position;

                    if (v->eg_duration > 0) {
                        position = (uint16_t)(position + instr->speed_up * 32);

                        if ((int8_t)(position >> 8) >= (int8_t)instr->stop_rep) {
                            v->eg_position = (uint16_t)(instr->stop_rep << 8);
                            v->eg_duration = -1;
                        } else {
                            v->eg_position = position;
                        }
                    } else {
                        position = (uint16_t)(position - instr->speed_down * 32);

                        if ((int8_t)(position >> 8) <= (int8_t)instr->start_len) {
                            v->eg_position = (uint16_t)(instr->start_len << 8);
                            v->eg_duration = 1;
                        } else {
                            v->eg_position = position;
                        }
                    }

                    eg_val = (uint8_t)(v->eg_position >> 8);

                    int8_t* waveform = (eg_val & 1) != 0 ? instr->waveform2 : instr->waveform1;
                    uint32_t length = instr->waveform_length;
                    uint32_t start_offset = (uint32_t)eg_val & 0xfe;

                    is2_ch_latch_window(ch, waveform, sizeof(instr->waveform1), start_offset, length);
                }
            }
        }
    }

    v->slide_value -= v->slide_speed;
    is2_dump_voice(m, v, ch);
}

static void is2_update_effects(Is2Module* m) {
    for (int i = 0; i < 4; i++)
        is2_update_voice_effect(m, &m->voices[i], &m->channels[i]);
}

// Play tick

static void is2_play_tick(Is2Module* m) {
    m->tick_number++;
    m->playing_info.speed_counter++;

    if (m->playing_info.speed_counter >= m->playing_info.current_speed) {
        m->playing_info.speed_counter = 0;
        is2_get_next_row(m);
    }

    is2_update_effects(m);
}

// Voices

static float is2_get_channel_volume(const Is2Channel* c) {
    if (c->use_volume_256)
        return (float)(c->volume >> 2) / 64.0f; // the asm writes the 0..256 volume shifted down to Paula's 0..64
    else
        return (float)c->volume / 64.0f;
}

// Hand the registers the tick wrote to the voice.
static void is2_sync_voice(Is2Module* m, uint32_t index) {
    Is2Channel* c = &m->channels[index];

    if (!c->active || c->sample_data == nullptr) {
        if (nuade_voice_is_playing(m->paula, index))
            nuade_voice_stop(m->paula, index);
    } else {
        const NuadeSample latest = is2_ch_sample(c);
        if (c->play_pending)
            nuade_voice_play(m->paula, index, c->latch_pending ? &c->first_window : &latest, 0);
        if (c->latch_pending && latest.length != 0)
            nuade_voice_queue(m->paula, index, &latest);
    }
    c->play_pending = false;
    c->latch_pending = false;

    nuade_voice_set_pitch(m->paula, index, c->period == 0 ? 0.0 : nuade_paula_period_hz(c->period));
    nuade_voice_set_volume(m->paula, index, c->period == 0 ? 0.0f : is2_get_channel_volume(c));
    // Paula channels 0 and 3 are left, 1 and 2 right.
    nuade_voice_set_pan(m->paula, index, (index == 0 || index == 3) ? -1.0f : 1.0f);
}

bool is2_tick(Is2Module* module) {
    // A one-shot that ran off its end with a sample queued behind it still counts as playing; the mixer switches on the
    // next frame.
    for (uint32_t i = 0; i < IS2_CHANNELS; i++)
        module->channels[i].active = module->channels[i].active && nuade_voice_is_playing(module->paula, i);

    const uint32_t loops = module->loop_count;
    is2_play_tick(module);

    for (uint32_t i = 0; i < IS2_CHANNELS; i++)
        is2_sync_voice(module, i);
    return module->loop_count != loops;
}

// The tick timer is loaded with 716000 divided by the tempo. The original picks a different base when the
// graphics flags report PAL; the UADE environment runs with this one.
#define IS2_CIA_TIMER_BASE 716000U

uint32_t is2_tick_length(const Is2Module* module) {
    return (IS2_CIA_TIMER_BASE + module->tempo / 2U) / module->tempo;
}

// Public API

bool is2_probe(const uint8_t* data, size_t size) {
    return data != nullptr && size >= 16 && memcmp(data, "IS20DF10", 8) == 0;
}

Is2Module* is2_create(const uint8_t* data, size_t size) {
    if (!is2_probe(data, size))
        return nullptr;

    Is2Module* m = (Is2Module*)calloc(1, sizeof(Is2Module));
    if (!m)
        return nullptr;

    Is2Reader reader;
    is2_reader_init(&reader, data + 8, size - 8);

    if (!is2_read_sub_songs(m, &reader)) {
        is2_destroy(m);
        return nullptr;
    }
    if (!is2_read_positions(m, &reader)) {
        is2_destroy(m);
        return nullptr;
    }
    if (!is2_read_track_rows(m, &reader)) {
        is2_destroy(m);
        return nullptr;
    }
    if (!is2_read_sample_info(m, &reader)) {
        is2_destroy(m);
        return nullptr;
    }
    if (!is2_read_sample_data(m, &reader)) {
        is2_destroy(m);
        return nullptr;
    }
    if (!is2_read_instruments(m, &reader)) {
        is2_destroy(m);
        return nullptr;
    }

    if (m->num_sub_songs <= 0) {
        is2_destroy(m);
        return nullptr;
    }

    return m;
}

void is2_destroy(Is2Module* module) {
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
    if (module->samples)
        free(module->samples);

    if (module->sample_data) {
        for (int i = 0; i < module->num_sample_data; i++)
            free(module->sample_data[i]);
        free(module->sample_data);
    }
    free(module->sample_data_size);

    if (module->instruments)
        free(module->instruments);

    if (module->dump_state != nullptr && module->dump_state != stderr)
        fclose(module->dump_state);

    free(module);
}

bool is2_set_dump_file(Is2Module* module, const char* path) {
    if (module->dump_state != nullptr && module->dump_state != stderr)
        fclose(module->dump_state);
    module->dump_state = nullptr;
    if (path[0] == '\0')
        return true;
    module->dump_state = strcmp(path, "-") == 0 ? stderr : fopen(path, "w");
    return module->dump_state != nullptr;
}

int is2_subsong_count(const Is2Module* module) {
    if (!module)
        return 0;
    return module->num_sub_songs;
}

bool is2_start(Is2Module* module, int subsong, NuadeVoices* voices) {
    if (!module || !voices || subsong < 0 || subsong >= module->num_sub_songs)
        return false;

    module->paula = voices;
    is2_clear_visited(module);
    is2_initialize_sound(module, subsong);
    return true;
}

uint32_t is2_loop_count(const Is2Module* module) {
    return module ? module->loop_count : 0;
}

// Display

Is2Position is2_position(const Is2Module* module) {
    const Is2GlobalPlayingInfo* pi = &module->playing_info;
    return (Is2Position) {
        .position = pi->shown_position, .row = pi->shown_row, .rows = pi->shown_rows, .speed = pi->current_speed
    };
}

int is2_position_count(const Is2Module* module) {
    return module->num_positions;
}

int is2_default_rows(const Is2Module* module) {
    return module->current_song_info != nullptr ? module->current_song_info->rows_per_track : 0;
}

Is2VoiceState is2_voice_state(const Is2Module* module, int voice) {
    const Is2VoiceInfo* v = &module->voices[voice];
    return (Is2VoiceState) { .note = v->transposed_note, .instrument = v->transposed_instrument };
}

Is2Cell is2_pattern_cell(const Is2Module* module, int position, int channel, int row) {
    Is2Cell cell = { 0 };
    if (position < 0 || position >= module->num_positions) {
        return cell;
    }
    const uint32_t line = (uint32_t)module->positions[position][channel].start_track_row + (uint32_t)row;
    if (line >= (uint32_t)module->num_track_lines) {
        return cell;
    }
    const Is2TrackLine* tl = &module->track_lines[line];
    cell.note = tl->note;
    cell.instrument = tl->instrument;
    cell.disable_sound_transpose = tl->disable_sound_transpose;
    cell.disable_note_transpose = tl->disable_note_transpose;
    cell.arpeggio = tl->arpeggio;
    cell.effect = (uint8_t)tl->effect;
    cell.effect_arg = tl->effect_arg;
    return cell;
}

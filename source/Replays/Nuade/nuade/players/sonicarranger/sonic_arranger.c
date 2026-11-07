// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/sonicarranger.h>

#include "nuade_base.h"
#include "nuade_paula.h"

#include <stdlib.h>
#include <string.h>

// Structs

typedef struct SaPeriodInfo {
    uint16_t period;
    uint16_t previous_period;
} SaPeriodInfo;

// Tables

static const uint16_t s_periods[109]
    = { 0,    13696, 12928, 12192, 11520, 10848, 10240, 9664, 9120, 8608, 8128, 7680, 7248, 6848, 6464, 6096,
        5760, 5424,  5120,  4832,  4560,  4304,  4064,  3840, 3624, 3424, 3232, 3048, 2880, 2712, 2560, 2416,
        2280, 2152,  2032,  1920,  1812,  1712,  1616,  1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960,
        906,  856,   808,   762,   720,   678,   640,   604,  570,  538,  508,  480,  453,  428,  404,  381,
        360,  339,   320,   302,   285,   269,   254,   240,  226,  214,  202,  190,  180,  170,  160,  151,
        143,  135,   127,   120,   113,   107,   101,   95,   90,   85,   80,   75,   71,   67,   63,   60,
        56,   53,    50,    47,    45,    42,    40,    37,   35,   33,   31,   30,   28 };

static const int8_t s_vibrato[256]
    = { 0,    3,    6,    9,    12,   16,   19,   22,   25,   28,   31,   34,   37,   40,   43,   46,   49,   52,
        54,   57,   60,   63,   66,   68,   71,   73,   76,   78,   81,   83,   86,   88,   90,   92,   94,   96,
        98,   100,  102,  104,  106,  108,  109,  111,  112,  114,  115,  116,  118,  119,  120,  121,  122,  123,
        123,  124,  125,  125,  126,  126,  126,  127,  127,  127,  127,  127,  127,  127,  126,  126,  125,  125,
        124,  124,  123,  122,  121,  120,  119,  118,  117,  116,  114,  113,  112,  110,  108,  107,  105,  103,
        101,  99,   97,   95,   93,   91,   89,   87,   84,   82,   80,   77,   75,   72,   69,   67,   64,   61,
        59,   56,   53,   50,   47,   44,   41,   39,   36,   32,   29,   26,   23,   20,   17,   14,   11,   8,
        5,    2,    -1,   -4,   -7,   -10,  -14,  -17,  -20,  -23,  -26,  -29,  -32,  -35,  -38,  -41,  -44,  -47,
        -50,  -53,  -55,  -58,  -61,  -64,  -66,  -69,  -72,  -74,  -77,  -79,  -82,  -84,  -86,  -88,  -91,  -93,
        -95,  -97,  -99,  -101, -103, -104, -106, -108, -109, -111, -112, -114, -115, -116, -118, -119, -120, -121,
        -122, -122, -123, -124, -124, -125, -125, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125,
        -125, -124, -124, -123, -122, -121, -120, -119, -118, -117, -116, -115, -113, -112, -110, -109, -107, -105,
        -104, -102, -100, -98,  -96,  -94,  -92,  -90,  -87,  -85,  -83,  -80,  -78,  -75,  -73,  -70,  -68,  -65,
        -62,  -60,  -57,  -54,  -51,  -48,  -45,  -42,  -39,  -37,  -34,  -30,  -27,  -24,  -21,  -18,  -15,  -12,
        -9,   -6,   -3,   0 };

// Big-endian read helpers

static inline uint16_t read_b_uint16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static inline int16_t read_b_int16(const uint8_t* p) {
    return (int16_t)((p[0] << 8) | p[1]);
}

static inline uint32_t read_b_uint32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static inline int32_t read_b_int32(const uint8_t* p) {
    return (int32_t)read_b_uint32(p);
}

// Random number generator

static int8_t sa_random(SaModule* m) {
    // Simple LCG to match C# RandomGenerator.GetRandomNumber(-128, 127)
    m->random_state = m->random_state * 1103515245 + 12345;
    return (int8_t)((m->random_state >> 16) & 0xff);
}

// Stream reader

typedef struct SaReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} SaReader;

static bool reader_init(SaReader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
    return true;
}

static bool reader_eof(const SaReader* r) {
    return r->pos >= r->size;
}

static bool reader_can_read(const SaReader* r, size_t n) {
    return (r->pos + n) <= r->size;
}

static void reader_skip(SaReader* r, size_t n) {
    r->pos += n;
}

static uint8_t reader_read_uint8(SaReader* r) {
    if (r->pos >= r->size)
        return 0;
    return r->data[r->pos++];
}

static int8_t reader_read_int8(SaReader* r) {
    return (int8_t)reader_read_uint8(r);
}

static uint16_t reader_read_b_uint16(SaReader* r) {
    if (!reader_can_read(r, 2))
        return 0;
    uint16_t val = read_b_uint16(r->data + r->pos);
    r->pos += 2;
    return val;
}

static uint32_t reader_read_b_uint32(SaReader* r) {
    if (!reader_can_read(r, 4))
        return 0;
    uint32_t val = read_b_uint32(r->data + r->pos);
    r->pos += 4;
    return val;
}

// Reads a table entry count and rejects counts that could not fit in the rest of the file.
static bool reader_read_count(SaReader* r, size_t record_bytes, int32_t* out_count) {
    uint32_t n = reader_read_b_uint32(r);
    size_t remaining = r->pos <= r->size ? r->size - r->pos : 0;
    if (n > remaining / record_bytes)
        return false;
    *out_count = (int32_t)n;
    return true;
}

static bool reader_read_mark(SaReader* r, const char* expected) {
    size_t len = strlen(expected);
    if (!reader_can_read(r, len))
        return false;
    bool match = (memcmp(r->data + r->pos, expected, len) == 0);
    r->pos += len;
    return match;
}

// Waveforms, ADSR tables and AMF tables are fixed-size blocks; a missing table reads as silence.
static const uint8_t s_empty_table[SA_WAVEFORM_SIZE];

static const uint8_t* table_at(uint8_t* const* tables, int count, uint16_t index) {
    return index < count ? tables[index] : s_empty_table;
}

static int8_t* waveform_at(const SaModule* m, uint16_t index) {
    return index < m->num_waveforms ? m->waveform_data[index] : (int8_t*)s_empty_table;
}

// Loading functions

static bool read_sub_songs(SaModule* m, SaReader* r, int32_t count) {
    if (count < 0 && !reader_read_count(r, 12, &count))
        return false;
    uint32_t num = (uint32_t)count;

    // First pass: count valid songs (skip 0xffff and duplicates)
    // Allocate temporary array for all potential songs
    SaSongInfo* temp = (SaSongInfo*)calloc(num, sizeof(SaSongInfo));
    if (!temp && num > 0)
        return false;

    int valid_count = 0;

    for (uint32_t i = 0; i < num; i++) {
        SaSongInfo si;
        si.start_speed = reader_read_b_uint16(r);
        si.rows_per_track = reader_read_b_uint16(r);
        si.first_position = reader_read_b_uint16(r);
        si.last_position = reader_read_b_uint16(r);
        si.restart_position = reader_read_b_uint16(r);
        si.tempo = reader_read_b_uint16(r);

        if (reader_eof(r)) {
            free(temp);
            return false;
        }

        if ((si.last_position == 0xffff) || (si.restart_position == 0xffff))
            continue;

        // Check for duplicates
        bool duplicate = false;
        for (int j = 0; j < valid_count; j++) {
            if ((si.first_position == temp[j].first_position) && (si.last_position == temp[j].last_position)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;

        temp[valid_count++] = si;
    }

    m->sub_songs = (SaSongInfo*)calloc(valid_count > 0 ? valid_count : 1, sizeof(SaSongInfo));
    if (!m->sub_songs) {
        free(temp);
        return false;
    }
    if (valid_count > 0)
        memcpy(m->sub_songs, temp, valid_count * sizeof(SaSongInfo));
    m->num_sub_songs = valid_count;

    free(temp);
    return true;
}

static bool read_position_information(SaModule* m, SaReader* r, int32_t count) {
    if (count < 0 && !reader_read_count(r, 16, &count))
        return false;

    m->num_positions = count;
    m->positions = (SaSinglePositionInfo**)calloc(count, sizeof(SaSinglePositionInfo*));
    if (!m->positions && count > 0)
        return false;

    for (int i = 0; i < count; i++) {
        m->positions[i] = (SaSinglePositionInfo*)calloc(4, sizeof(SaSinglePositionInfo));
        if (!m->positions[i])
            return false;

        for (int j = 0; j < 4; j++) {
            m->positions[i][j].start_track_row = reader_read_b_uint16(r);
            m->positions[i][j].sound_transpose = reader_read_int8(r);
            m->positions[i][j].note_transpose = reader_read_int8(r);
        }

        if (reader_eof(r))
            return false;
    }

    return true;
}

static bool read_track_rows(SaModule* m, SaReader* r, int32_t count) {
    if (count < 0 && !reader_read_count(r, 4, &count))
        return false;

    m->num_track_lines = count;
    m->track_lines = (SaTrackLine*)calloc(count, sizeof(SaTrackLine));
    if (!m->track_lines && count > 0)
        return false;

    for (int i = 0; i < count; i++) {
        uint8_t byt1 = reader_read_uint8(r);
        uint8_t byt2 = reader_read_uint8(r);
        uint8_t byt3 = reader_read_uint8(r);
        uint8_t byt4 = reader_read_uint8(r);

        m->track_lines[i].note = byt1;
        m->track_lines[i].instrument = byt2;
        m->track_lines[i].disable_sound_transpose = (byt3 & 0x80) != 0;
        m->track_lines[i].disable_note_transpose = (byt3 & 0x40) != 0;
        m->track_lines[i].arpeggio = (uint8_t)((byt3 & 0x30) >> 4);
        m->track_lines[i].effect = (SaEffect)(byt3 & 0x0f);
        m->track_lines[i].effect_arg = byt4;

        if (reader_eof(r))
            return false;
    }

    return true;
}

static bool read_instrument_information(SaModule* m, SaReader* r, int32_t count) {
    if (count < 0 && !reader_read_count(r, 152, &count))
        return false;

    m->num_instruments = count;
    m->instruments = (SaInstrument*)calloc(count, sizeof(SaInstrument));
    if (!m->instruments && count > 0)
        return false;

    for (int i = 0; i < count; i++) {
        SaInstrument* instr = &m->instruments[i];

        instr->type = (SaInstrumentType)reader_read_b_uint16(r);
        instr->waveform_number = reader_read_b_uint16(r);
        instr->waveform_length = reader_read_b_uint16(r);
        instr->repeat_length = reader_read_b_uint16(r);

        if (reader_eof(r))
            return false;

        // Skip 8 bytes
        reader_skip(r, 8);

        instr->volume = (uint16_t)(reader_read_b_uint16(r) & 0xff);
        instr->fine_tuning = (int8_t)reader_read_b_uint16(r);

        instr->portamento_speed = reader_read_b_uint16(r);

        instr->vibrato_delay = reader_read_b_uint16(r);
        instr->vibrato_speed = reader_read_b_uint16(r);
        instr->vibrato_level = reader_read_b_uint16(r);

        instr->amf_number = reader_read_b_uint16(r);
        instr->amf_delay = reader_read_b_uint16(r);
        instr->amf_length = reader_read_b_uint16(r);
        instr->amf_repeat = reader_read_b_uint16(r);

        instr->adsr_number = reader_read_b_uint16(r);
        instr->adsr_delay = reader_read_b_uint16(r);
        instr->adsr_length = reader_read_b_uint16(r);
        instr->adsr_repeat = reader_read_b_uint16(r);
        instr->sustain_point = reader_read_b_uint16(r);
        instr->sustain_delay = reader_read_b_uint16(r);

        if (reader_eof(r))
            return false;

        // Skip 16 bytes
        reader_skip(r, 16);

        instr->effect_arg1 = reader_read_b_uint16(r);
        instr->effect = (SaSynthesisEffect)reader_read_b_uint16(r);
        instr->effect_arg2 = reader_read_b_uint16(r);
        instr->effect_arg3 = reader_read_b_uint16(r);
        instr->effect_delay = reader_read_b_uint16(r);

        for (int j = 0; j < 3; j++) {
            instr->arpeggios[j].length = reader_read_uint8(r);
            instr->arpeggios[j].repeat = reader_read_uint8(r);

            if (!reader_can_read(r, 14))
                return false;

            for (int k = 0; k < 14; k++)
                instr->arpeggios[j].values[k] = reader_read_int8(r);
        }

        // Read 30-byte name
        if (!reader_can_read(r, 30))
            return false;

        memcpy(instr->name, r->data + r->pos, 30);
        instr->name[30] = '\0';
        reader_skip(r, 30);

        // Synth parameters address the 128 byte waveform buffer.
        if (instr->type != SA_INSTRUMENT_SAMPLE && instr->waveform_length > SA_WAVEFORM_SIZE / 2)
            instr->waveform_length = SA_WAVEFORM_SIZE / 2;
        if (instr->effect_arg2 >= SA_WAVEFORM_SIZE)
            instr->effect_arg2 = SA_WAVEFORM_SIZE - 1;
        if (instr->effect_arg3 >= SA_WAVEFORM_SIZE)
            instr->effect_arg3 = SA_WAVEFORM_SIZE - 1;
    }

    return true;
}

static bool read_sample_information(SaModule* m, SaReader* r, int* out_num_samples) {
    (void)m;
    // Sample lengths and loop lengths stored in words, plus the sample names, are skipped.
    const size_t info_bytes = 4 * 2 + 30;
    int32_t num = 0;
    if (!reader_read_count(r, info_bytes, &num))
        return false;
    *out_num_samples = num;
    reader_skip(r, (size_t)num * info_bytes);

    return true;
}

static bool read_sample_data(SaModule* m, SaReader* r, int num_samples) {
    m->num_samples = num_samples;
    m->sample_data = (int8_t**)calloc(num_samples > 0 ? num_samples : 1, sizeof(int8_t*));
    m->sample_lengths = (uint32_t*)calloc(num_samples > 0 ? num_samples : 1, sizeof(uint32_t));
    if (!m->sample_data || !m->sample_lengths)
        return false;

    if (num_samples > 0) {
        // Read sample lengths (uint32 each)
        uint32_t* lengths = (uint32_t*)calloc(num_samples, sizeof(uint32_t));
        if (!lengths)
            return false;

        for (int i = 0; i < num_samples; i++)
            lengths[i] = reader_read_b_uint32(r);

        if (reader_eof(r)) {
            free(lengths);
            return false;
        }

        for (int i = 0; i < num_samples; i++) {
            uint32_t len = lengths[i];
            m->sample_lengths[i] = len;

            if (len > 0) {
                if (!reader_can_read(r, len)) {
                    free(lengths);
                    return false;
                }

                m->sample_data[i] = (int8_t*)malloc(len);
                if (!m->sample_data[i]) {
                    free(lengths);
                    return false;
                }
                memcpy(m->sample_data[i], r->data + r->pos, len);
                reader_skip(r, len);
            }
        }

        free(lengths);
    }

    return true;
}

static bool read_waveform_data(SaModule* m, SaReader* r, int32_t count) {
    if (count < 0 && !reader_read_count(r, SA_WAVEFORM_SIZE, &count))
        return false;

    m->num_waveforms = count;
    m->waveform_data = (int8_t**)calloc(count > 0 ? count : 1, sizeof(int8_t*));
    if (!m->waveform_data)
        return false;

    for (int i = 0; i < count; i++) {
        if (!reader_can_read(r, SA_WAVEFORM_SIZE))
            return false;

        m->waveform_data[i] = (int8_t*)malloc(SA_WAVEFORM_SIZE);
        if (!m->waveform_data[i])
            return false;
        memcpy(m->waveform_data[i], r->data + r->pos, SA_WAVEFORM_SIZE);
        reader_skip(r, SA_WAVEFORM_SIZE);
    }

    return true;
}

static bool read_adsr_tables(SaModule* m, SaReader* r, int32_t count) {
    if (count < 0 && !reader_read_count(r, SA_WAVEFORM_SIZE, &count))
        return false;

    m->num_adsr_tables = count;
    m->adsr_tables = (uint8_t**)calloc(count > 0 ? count : 1, sizeof(uint8_t*));
    if (!m->adsr_tables)
        return false;

    for (int i = 0; i < count; i++) {
        if (!reader_can_read(r, SA_WAVEFORM_SIZE))
            return false;

        m->adsr_tables[i] = (uint8_t*)malloc(SA_WAVEFORM_SIZE);
        if (!m->adsr_tables[i])
            return false;
        memcpy(m->adsr_tables[i], r->data + r->pos, SA_WAVEFORM_SIZE);
        reader_skip(r, SA_WAVEFORM_SIZE);
    }

    return true;
}

static bool read_amf_tables(SaModule* m, SaReader* r, int32_t count) {
    if (count < 0 && !reader_read_count(r, SA_WAVEFORM_SIZE, &count))
        return false;

    m->num_amf_tables = count;
    m->amf_tables = (uint8_t**)calloc(count > 0 ? count : 1, sizeof(uint8_t*));
    if (!m->amf_tables)
        return false;

    for (int i = 0; i < count; i++) {
        if (!reader_can_read(r, SA_WAVEFORM_SIZE))
            return false;

        m->amf_tables[i] = (uint8_t*)malloc(SA_WAVEFORM_SIZE);
        if (!m->amf_tables[i])
            return false;
        memcpy(m->amf_tables[i], r->data + r->pos, SA_WAVEFORM_SIZE);
        reader_skip(r, SA_WAVEFORM_SIZE);
    }

    return true;
}

static bool load_normal_module(SaModule* m, SaReader* r) {
    // STBL + sub songs
    if (!reader_read_mark(r, "STBL") || !read_sub_songs(m, r, -1))
        return false;

    // OVTB + position information
    if (!reader_read_mark(r, "OVTB") || !read_position_information(m, r, -1))
        return false;

    // NTBL + track rows
    if (!reader_read_mark(r, "NTBL") || !read_track_rows(m, r, -1))
        return false;

    // INST + instruments
    if (!reader_read_mark(r, "INST") || !read_instrument_information(m, r, -1))
        return false;

    // SD8B + sample info
    int num_samples = 0;
    if (!reader_read_mark(r, "SD8B") || !read_sample_information(m, r, &num_samples))
        return false;

    // Sample data (no mark, follows directly)
    if (!read_sample_data(m, r, num_samples))
        return false;

    // SYWT + waveforms
    if (!reader_read_mark(r, "SYWT") || !read_waveform_data(m, r, -1))
        return false;

    // SYAR + ADSR tables
    if (!reader_read_mark(r, "SYAR") || !read_adsr_tables(m, r, -1))
        return false;

    // SYAF + AMF tables
    if (!reader_read_mark(r, "SYAF") || !read_amf_tables(m, r, -1))
        return false;

    return true;
}

// Compiled form: a 68k executable (jump table + replayer) that carries the module tables as a block of offsets.

enum {
    SA_COMPILED_JUMP = 0x4efa, // jmp d16(pc)
    SA_COMPILED_LEA = 0x41fa,  // lea d16(pc),a0
    SA_COMPILED_HEADER_SIZE = 0x28,
    SA_COMPILED_TABLES = 8,
};

static const char s_compiled_trailer[] = "deadbeef";

// The data block is found through two pc-relative hops: the leading jump reaches a lea whose displacement points at
// the block. A file without the code is just the block.
static bool find_compiled_block(const uint8_t* data, size_t size, size_t* out_base) {
    if (size < SA_COMPILED_HEADER_SIZE)
        return false;
    if (read_b_uint16(data) != SA_COMPILED_JUMP) {
        *out_base = 0;
        return true;
    }

    const int16_t jump = (int16_t)read_b_uint16(data + 2);
    if (jump <= 0 || (jump & 1))
        return false;

    const size_t lea_at = 6 + (size_t)jump;
    if (lea_at + 4 > size || read_b_uint16(data + lea_at) != SA_COMPILED_LEA)
        return false;

    const int16_t disp = (int16_t)read_b_uint16(data + lea_at + 2);
    if (disp <= 0 || (disp & 1))
        return false;

    const size_t base = lea_at + 2 + (size_t)disp;
    if (base + SA_COMPILED_HEADER_SIZE > size)
        return false;
    *out_base = base;
    return true;
}

// The block starts with the table offsets: sub songs, OVTB, NTBL, INST, SYWT, SYAR, SYAF and the sample table. Each
// table runs up to the next offset, so counts follow from the distances.
static bool read_compiled_offsets(const uint8_t* block, size_t block_size, uint32_t offsets[SA_COMPILED_TABLES]) {
    if (read_b_uint32(block) != SA_COMPILED_HEADER_SIZE)
        return false;

    uint32_t previous = 0;
    for (int i = 0; i < SA_COMPILED_TABLES; i++) {
        offsets[i] = read_b_uint32(block + 4 * i);
        if (offsets[i] == 0 || (offsets[i] & 1) || offsets[i] < previous || offsets[i] > block_size)
            return false;
        previous = offsets[i];
    }
    return true;
}

static bool load_compiled_module(SaModule* m, const uint8_t* block, size_t block_size) {
    uint32_t off[SA_COMPILED_TABLES];
    if (!read_compiled_offsets(block, block_size, off))
        return false;

    SaReader r;
    reader_init(&r, block, block_size);

    r.pos = off[0];
    if (!read_sub_songs(m, &r, (int32_t)((off[1] - off[0]) / 12)))
        return false;
    r.pos = off[1];
    if (!read_position_information(m, &r, (int32_t)((off[2] - off[1]) / 16)))
        return false;
    r.pos = off[2];
    if (!read_track_rows(m, &r, (int32_t)((off[3] - off[2]) / 4)))
        return false;
    r.pos = off[3];
    if (!read_instrument_information(m, &r, (int32_t)((off[4] - off[3]) / 152)))
        return false;
    r.pos = off[4];
    if (!read_waveform_data(m, &r, (int32_t)((off[5] - off[4]) / SA_WAVEFORM_SIZE)))
        return false;
    r.pos = off[5];
    if (!read_adsr_tables(m, &r, (int32_t)((off[6] - off[5]) / SA_WAVEFORM_SIZE)))
        return false;
    r.pos = off[6];
    if (!read_amf_tables(m, &r, (int32_t)((off[7] - off[6]) / SA_WAVEFORM_SIZE)))
        return false;

    r.pos = off[7];
    int32_t num_samples = 0;
    if (!reader_read_count(&r, 4, &num_samples) || !read_sample_data(m, &r, num_samples))
        return false;

    // The sample data is followed by a fixed trailer.
    return reader_read_mark(&r, s_compiled_trailer);
}

// InitializeSound

static void initialize_sound(SaModule* m, int sub_song) {
    m->current_song_info = &m->sub_songs[sub_song];

    m->playing_info.master_volume = 64;
    m->playing_info.speed_counter = m->current_song_info->start_speed;
    m->playing_info.current_speed = m->current_song_info->start_speed;
    m->playing_info.song_position = (int16_t)(m->current_song_info->first_position - 1);
    m->playing_info.row_position = m->current_song_info->rows_per_track;
    m->playing_info.rows_per_track = m->current_song_info->rows_per_track;
    m->playing_info.shown_position = -1;
    m->playing_info.shown_row = 0;

    for (int i = 0; i < SA_VOICES; i++) {
        SaVoiceInfo* v = &m->voices[i];
        memset(v, 0, sizeof(SaVoiceInfo));
        v->start_track_row = 0;
        v->sound_transpose = 0;
        v->note_transpose = 0;
        v->note = 0;
        v->instrument = 0;
        v->disable_sound_transpose = false;
        v->disable_note_transpose = false;
        v->arpeggio = 0;
        v->effect = SA_EFFECT_ARPEGGIO;
        v->effect_arg = 0;
        v->transposed_note = 0;
        v->previous_transposed_note = 0;
        v->instrument_type = SA_INSTRUMENT_SAMPLE;
        v->instrument_info = nullptr;
        v->transposed_instrument = 0;
        v->current_volume = 0;
        v->volume_slide_speed = 0;
        v->vibrato_position = 0;
        v->vibrato_delay = 0;
        v->vibrato_speed = 0;
        v->vibrato_level = 0;
        v->portamento_speed = 0;
        v->portamento_period = 0;
        v->arpeggio_position = 0;
        v->slide_speed = 0;
        v->slide_value = 0;
        v->adsr_position = 0;
        v->adsr_delay_counter = 0;
        v->sustain_delay_counter = 0;
        v->amf_position = 0;
        v->amf_delay_counter = 0;
        v->synth_effect_position = 0;
        v->synth_effect_wave_position = 0;
        v->effect_delay_counter = 0;
        v->flag = 0x01;
        memset(v->waveform_buffer, 0, SA_WAVEFORM_SIZE);

        // Reset channel
        memset(&m->channels[i], 0, sizeof(SaChannel));
    }

    m->loop_count = 0;
    memset(m->visited, 0, sizeof(m->visited));
}

// Channel operations (IChannel mapping)

static void channel_play_sample(SaChannel* ch, const int8_t* data, uint32_t length) {
    ch->sample_data = data;
    ch->sample_length = length;
    ch->loop_start = 0;
    ch->loop_length = 0;
    ch->active = true;
    ch->play_pending = true;
}

static void channel_set_loop(SaChannel* ch, uint32_t start, uint32_t length) {
    ch->loop_start = start;
    ch->loop_length = length;
}

static void channel_set_amiga_volume(SaChannel* ch, uint16_t vol) {
    if (vol > 64)
        vol = 64;
    ch->volume = vol;
}

static void channel_set_amiga_period(SaChannel* ch, uint16_t period) {
    ch->period = period;
}

static void channel_mute(SaChannel* ch) {
    ch->active = false;
}

// Forward declarations

static void get_next_row(SaModule* m);
static void get_next_position(SaModule* m);
static void get_new_notes(SaModule* m);
static void play_voice(SaModule* m, SaVoiceInfo* voice, SaChannel* channel);
static void force_quiet(SaVoiceInfo* voice, SaChannel* channel);
static SaInstrument* set_instrument(SaModule* m, SaVoiceInfo* voice, uint16_t instr_num);
static void set_synth_instrument(SaVoiceInfo* voice, SaInstrument* instr);
static void play_sample_instrument(SaModule* m, SaVoiceInfo* voice, SaChannel* channel, SaInstrument* instr);
static void play_synth_instrument(SaModule* m, SaVoiceInfo* voice, SaChannel* channel, SaInstrument* instr);
static void set_effects(SaModule* m, SaVoiceInfo* voice);
static void update_effects(SaModule* m);
static void update_voice_effect(SaModule* m, SaVoiceInfo* voice, SaChannel* channel);
static SaPeriodInfo do_arpeggio(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr);
static void do_portamento(SaPeriodInfo* pi, SaVoiceInfo* voice);
static void do_vibrato(SaPeriodInfo* pi, SaVoiceInfo* voice, SaInstrument* instr);
static void do_amf(SaModule* m, SaPeriodInfo* pi, SaVoiceInfo* voice, SaInstrument* instr);
static void do_slide(SaModule* m, SaPeriodInfo* pi, SaVoiceInfo* voice);
static void do_adsr(SaModule* m, SaVoiceInfo* voice, SaChannel* channel, SaInstrument* instr);
static void do_volume_slide(SaVoiceInfo* voice);
static void do_synth_effects(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr);
static void increment_synth_effect_position(SaVoiceInfo* voice, SaInstrument* instr);

// Synth effect handlers
static void do_synth_wave_negator(SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_free_negator(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_rotate_vertical(SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_rotate_horizontal(SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_alien_voice(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_poly_negator(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_shack_wave_1(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_shack_wave_2(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_metamorph(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_laser(SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_wave_alias(SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_noise_generator_1(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_low_pass_filter_1(SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_low_pass_filter_2(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_oszilator(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_noise_generator_2(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr);
static void do_synth_fm_drum(SaVoiceInfo* voice, SaInstrument* instr);
static void shack_wave_helper(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr);
static void metamorph_and_oszilator_helper(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr,
                                           int8_t* source_waveform);

// Play tick

static void play_tick(SaModule* m) {
    m->playing_info.speed_counter++;

    if (m->playing_info.speed_counter >= m->playing_info.current_speed) {
        m->playing_info.speed_counter = 0;
        get_next_row(m);
    }

    update_effects(m);
}

// GetNextRow

static void get_next_row(SaModule* m) {
    m->playing_info.row_position++;

    if (m->playing_info.row_position >= m->playing_info.rows_per_track) {
        m->playing_info.row_position = 0;
        get_next_position(m);
    }

    m->playing_info.shown_position = m->playing_info.song_position;
    m->playing_info.shown_row = m->playing_info.row_position;

    get_new_notes(m);
}

// GetNextPosition: position visited tracking

static bool has_position_been_visited(const SaModule* m, int pos) {
    if (pos < 0 || pos >= SA_MAX_POSITIONS)
        return false;
    return (m->visited[pos / 8] & (1 << (pos % 8))) != 0;
}

static void mark_position_as_visited(SaModule* m, int pos) {
    if (pos < 0 || pos >= SA_MAX_POSITIONS)
        return;
    m->visited[pos / 8] |= (1 << (pos % 8));
}

static void get_next_position(SaModule* m) {
    m->playing_info.song_position++;

    if ((m->playing_info.song_position > (int16_t)m->current_song_info->last_position)
        || (m->playing_info.song_position >= m->num_positions)) {
        m->playing_info.song_position = (int16_t)m->current_song_info->restart_position;
    }

    if (m->playing_info.song_position < 0 || m->playing_info.song_position >= m->num_positions)
        m->playing_info.song_position = 0;

    if (has_position_been_visited(m, m->playing_info.song_position))
        m->loop_count++;

    mark_position_as_visited(m, m->playing_info.song_position);

    SaSinglePositionInfo* pos_row = m->positions[m->playing_info.song_position];

    for (int i = 0; i < 4; i++) {
        SaSinglePositionInfo* pos_info = &pos_row[i];
        SaVoiceInfo* voice = &m->voices[i];

        voice->start_track_row = pos_info->start_track_row;
        voice->sound_transpose = pos_info->sound_transpose;
        voice->note_transpose = pos_info->note_transpose;
    }
}

// GetNewNotes

static void get_new_notes(SaModule* m) {
    for (int i = 0; i < 4; i++) {
        SaVoiceInfo* voice = &m->voices[i];
        SaChannel* channel = &m->channels[i];

        int position = voice->start_track_row + m->playing_info.row_position;

        SaTrackLine empty_line;
        memset(&empty_line, 0, sizeof(SaTrackLine));

        SaTrackLine* track_line;
        if (position < m->num_track_lines)
            track_line = &m->track_lines[position];
        else
            track_line = &empty_line;

        voice->note = track_line->note;
        voice->instrument = track_line->instrument;
        voice->disable_sound_transpose = track_line->disable_sound_transpose;
        voice->disable_note_transpose = track_line->disable_note_transpose;
        voice->arpeggio = track_line->arpeggio;
        voice->effect = track_line->effect;
        voice->effect_arg = track_line->effect_arg;

        play_voice(m, voice, channel);
    }
}

// PlayVoice

static void play_voice(SaModule* m, SaVoiceInfo* voice, SaChannel* channel) {
    uint16_t note = voice->note;
    int16_t instr_num = voice->instrument;

    if (note == 0) {
        if (instr_num != 0)
            set_instrument(m, voice, (uint16_t)instr_num);
    } else {
        if (note != 0x80) {
            if (note == 0x7f)
                force_quiet(voice, channel);
            else {
                if (!voice->disable_note_transpose)
                    note = (uint16_t)(note + voice->note_transpose);

                if ((instr_num != 0) && !voice->disable_sound_transpose)
                    instr_num = (int16_t)(instr_num + voice->sound_transpose);

                voice->previous_transposed_note = voice->transposed_note;
                voice->transposed_note = note;

                if (voice->previous_transposed_note == 0)
                    voice->previous_transposed_note = note;

                SaInstrument* instr;

                if (instr_num < 0) {
                    voice->flag |= 0x01;
                    force_quiet(voice, channel);
                    return;
                }

                if (instr_num == 0) {
                    instr = voice->instrument_info;

                    if (instr == nullptr) {
                        force_quiet(voice, channel);
                        return;
                    }

                    set_synth_instrument(voice, instr);
                } else
                    instr = set_instrument(m, voice, (uint16_t)instr_num);

                if (instr == nullptr) {
                    force_quiet(voice, channel);
                    return;
                }

                voice->instrument_type = instr->type;
                voice->shown_note = (uint8_t)note;
                voice->shown_instrument = (uint8_t)voice->transposed_instrument;

                if (voice->instrument_type == SA_INSTRUMENT_SAMPLE)
                    play_sample_instrument(m, voice, channel, instr);
                else
                    play_synth_instrument(m, voice, channel, instr);
            }
        }
    }

    set_effects(m, voice);
}

// ForceQuiet

static void force_quiet(SaVoiceInfo* voice, SaChannel* channel) {
    channel_mute(channel);

    voice->current_volume = 0;
    voice->transposed_instrument = 0;
    voice->instrument_info = nullptr;
    voice->instrument_type = SA_INSTRUMENT_SAMPLE;
}

// SetInstrument

static SaInstrument* set_instrument(SaModule* m, SaVoiceInfo* voice, uint16_t instr_num) {
    uint16_t number = (uint16_t)(instr_num & 0xff);
    if (number == 0 || number > m->num_instruments)
        return nullptr;

    voice->transposed_instrument = number;
    SaInstrument* instr = &m->instruments[number - 1];
    voice->instrument_info = instr;

    voice->current_volume = instr->volume;

    voice->portamento_speed = instr->portamento_speed;
    voice->portamento_period = 0;

    voice->vibrato_position = 0;
    voice->vibrato_delay = instr->vibrato_delay;
    voice->vibrato_speed = instr->vibrato_speed;
    voice->vibrato_level = instr->vibrato_level;

    set_synth_instrument(voice, instr);

    return instr;
}

// SetSynthInstrument

static void set_synth_instrument(SaVoiceInfo* voice, SaInstrument* instr) {
    voice->slide_value = instr->fine_tuning;

    voice->adsr_delay_counter = instr->adsr_delay;
    voice->adsr_position = 0;

    voice->amf_delay_counter = instr->amf_delay;
    voice->amf_position = 0;

    voice->synth_effect_position = instr->effect_arg2;
    voice->synth_effect_wave_position = 0;
    voice->effect_delay_counter = instr->effect_delay;

    voice->arpeggio_position = 0;

    voice->flag = 0x00;
}

// PlaySampleInstrument

static void play_sample_instrument(SaModule* m, SaVoiceInfo* voice, SaChannel* channel, SaInstrument* instr) {
    if (instr->waveform_number >= (uint16_t)m->num_samples) {
        voice->flag |= 0x01;
        force_quiet(voice, channel);
        return;
    }

    int8_t* data = m->sample_data[instr->waveform_number];
    if (data == nullptr) {
        voice->flag |= 0x01;
        force_quiet(voice, channel);
    } else {
        uint32_t play_length = instr->waveform_length;
        uint32_t loop_start = 0;
        uint32_t loop_length = 0;

        if (instr->repeat_length == 0)
            loop_length = instr->waveform_length;
        else if (instr->repeat_length != 1) {
            play_length += instr->repeat_length;

            loop_start = instr->waveform_length * 2U;
            loop_length = instr->repeat_length * 2U;
        }

        uint32_t data_len = m->sample_lengths[instr->waveform_number];
        play_length = play_length * 2;
        if (play_length > data_len)
            play_length = data_len;

        if ((loop_start + loop_length) > play_length)
            loop_length = play_length - loop_start;

        if (play_length > 0) {
            channel_play_sample(channel, data, play_length);

            if (loop_length != 0)
                channel_set_loop(channel, loop_start, loop_length);
        } else {
            voice->flag |= 0x01;
            force_quiet(voice, channel);
        }
    }
}

// PlaySynthInstrument

static void play_synth_instrument(SaModule* m, SaVoiceInfo* voice, SaChannel* channel, SaInstrument* instr) {
    if (instr->waveform_number >= (uint16_t)m->num_waveforms)
        return;

    int8_t* data = m->waveform_data[instr->waveform_number];

    uint32_t length = instr->waveform_length * 2U;
    if (length > SA_WAVEFORM_SIZE)
        length = SA_WAVEFORM_SIZE;

    channel_play_sample(channel, voice->waveform_buffer, length);
    channel_set_loop(channel, 0, length);

    memcpy(voice->waveform_buffer, data, length);
}

// SetEffects

static void set_effects(SaModule* m, SaVoiceInfo* voice) {
    voice->slide_speed = 0;
    voice->volume_slide_speed = 0;

    switch (voice->effect) {
        case SA_EFFECT_SET_SLIDE_SPEED:
            voice->slide_speed = (int8_t)voice->effect_arg;
            break;

        case SA_EFFECT_RESTART_ADSR:
            voice->adsr_position = voice->effect_arg;
            break;

        case SA_EFFECT_SET_VIBRATO:
            voice->vibrato_delay = 0;
            voice->vibrato_speed = (uint16_t)(((voice->effect_arg & 0xf0) >> 3));
            voice->vibrato_level = (uint16_t)((-((voice->effect_arg & 0x0f) << 4)) + 160);
            break;

        case SA_EFFECT_SET_MASTER_VOLUME:
            m->playing_info.master_volume = (uint16_t)(voice->effect_arg == 64 ? 64 : (voice->effect_arg & 0x3f));
            break;

        case SA_EFFECT_SET_PORTAMENTO:
            voice->portamento_speed = voice->effect_arg;
            break;

        case SA_EFFECT_SKIP_PORTAMENTO:
            voice->portamento_speed = 0;
            break;

        case SA_EFFECT_SET_TRACK_LEN:
            if (voice->effect_arg <= 64)
                m->playing_info.rows_per_track = voice->effect_arg;
            break;

        case SA_EFFECT_VOLUME_SLIDE:
            voice->volume_slide_speed = (int8_t)(voice->effect_arg);
            break;

        case SA_EFFECT_POSITION_JUMP:
            m->playing_info.song_position = (int16_t)(voice->effect_arg - 1);
            m->playing_info.row_position = m->playing_info.rows_per_track;
            break;

        case SA_EFFECT_SET_VOLUME: {
            uint8_t new_vol = voice->effect_arg;
            if (new_vol > 64)
                new_vol = 64;
            voice->current_volume = new_vol;
            break;
        }

        case SA_EFFECT_TRACK_BREAK:
            m->playing_info.row_position = m->playing_info.rows_per_track;
            break;

        case SA_EFFECT_SET_FILTER:
            // AmigaFilter = voiceInfo.EffectArg == 0;
            // No-op in our C implementation (no hardware filter emulation)
            break;

        case SA_EFFECT_SET_SPEED:
            if ((voice->effect_arg > 0) && (voice->effect_arg <= 16))
                m->playing_info.current_speed = voice->effect_arg;
            break;

        default:
            break;
    }
}

// UpdateEffects

static void update_effects(SaModule* m) {
    for (int i = 0; i < 4; i++) {
        SaVoiceInfo* voice = &m->voices[i];
        SaChannel* channel = &m->channels[i];
        update_voice_effect(m, voice, channel);
    }
}

// UpdateVoiceEffect

static void update_voice_effect(SaModule* m, SaVoiceInfo* voice, SaChannel* channel) {
    if (((voice->flag & 0x01) != 0) || (voice->instrument_info == nullptr)) {
        channel_mute(channel);
        return;
    }

    SaInstrument* instr = voice->instrument_info;

    SaPeriodInfo pi = do_arpeggio(m, voice, instr);
    do_portamento(&pi, voice);
    do_vibrato(&pi, voice, instr);
    do_amf(m, &pi, voice, instr);
    do_slide(m, &pi, voice);

    channel_set_amiga_period(channel, pi.period);

    if (instr->type == SA_INSTRUMENT_SYNTH)
        do_synth_effects(m, voice, instr);

    do_adsr(m, voice, channel, instr);
    do_volume_slide(voice);
}

// DoArpeggio

static SaPeriodInfo do_arpeggio(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr) {
    uint16_t note = voice->transposed_note;
    uint16_t prev_note = voice->previous_transposed_note;

    if (voice->arpeggio != 0) {
        SaArpeggio* arp = &instr->arpeggios[voice->arpeggio - 1];

        int8_t arp_val = arp->values[voice->arpeggio_position < 14 ? voice->arpeggio_position : 13];
        note = (uint16_t)(note + arp_val);
        prev_note = (uint16_t)(prev_note + arp_val);

        voice->arpeggio_position++;

        int max_length = arp->length + arp->repeat;
        if (max_length > 13)
            max_length = 13;

        if (voice->arpeggio_position > (uint16_t)max_length)
            voice->arpeggio_position = arp->length;
    } else {
        if ((voice->effect == SA_EFFECT_ARPEGGIO) && (voice->effect_arg != 0)) {
            uint16_t arp_val;

            switch (m->playing_info.speed_counter % 3) {
                default:
                case 0:
                    arp_val = 0;
                    break;
                case 1:
                    arp_val = (uint16_t)(voice->effect_arg >> 4);
                    break;
                case 2:
                    arp_val = (uint16_t)(voice->effect_arg & 0x0f);
                    break;
            }

            note += arp_val;
            prev_note += arp_val;
        }
    }

    if (note >= 109)
        note = 0;

    if (prev_note >= 109)
        prev_note = 0;

    SaPeriodInfo pi;
    pi.period = s_periods[note];
    pi.previous_period = s_periods[prev_note];

    return pi;
}

// DoPortamento

static void do_portamento(SaPeriodInfo* pi, SaVoiceInfo* voice) {
    if (voice->portamento_speed != 0) {
        if (voice->portamento_period == 0)
            voice->portamento_period = pi->previous_period;

        int diff = pi->period - voice->portamento_period;
        if (diff < 0)
            diff = -diff;

        diff -= voice->portamento_speed;
        if (diff < 0)
            voice->portamento_speed = 0;
        else {
            uint16_t new_period = voice->portamento_period;

            if (new_period >= pi->period)
                new_period -= voice->portamento_speed;
            else
                new_period += voice->portamento_speed;

            voice->portamento_period = new_period;
            pi->period = new_period;
        }
    }
}

// DoVibrato

static void do_vibrato(SaPeriodInfo* pi, SaVoiceInfo* voice, SaInstrument* instr) {
    if (voice->vibrato_delay != 255) {
        if (voice->vibrato_delay == 0) {
            int8_t vib_val = s_vibrato[voice->vibrato_position];
            uint16_t vib_level = instr->vibrato_level;

            if (vib_val != 0 && vib_level != 0)
                pi->period += (uint16_t)((vib_val * 4) / (int16_t)vib_level);

            voice->vibrato_position = (uint16_t)((voice->vibrato_position + instr->vibrato_speed) & 0xff);
        } else
            voice->vibrato_delay--;
    }
}

// DoAmf

static void do_amf(SaModule* m, SaPeriodInfo* pi, SaVoiceInfo* voice, SaInstrument* instr) {
    if ((instr->amf_length + instr->amf_repeat) != 0) {
        const uint8_t* amf_table = table_at(m->amf_tables, m->num_amf_tables, instr->amf_number);
        int8_t amf_val = (int8_t)amf_table[voice->amf_position & (SA_WAVEFORM_SIZE - 1)];

        pi->period = (uint16_t)(pi->period - amf_val);

        voice->amf_delay_counter--;
        if (voice->amf_delay_counter == 0) {
            voice->amf_delay_counter = instr->amf_delay;

            voice->amf_position++;

            if (voice->amf_position >= (instr->amf_length + instr->amf_repeat)) {
                voice->amf_position = instr->amf_length;

                if (instr->amf_repeat == 0)
                    voice->amf_position--;
            }
        }
    }
}

// DoSlide

static void do_slide(SaModule* m, SaPeriodInfo* pi, SaVoiceInfo* voice) {
    pi->period = (uint16_t)(pi->period - voice->slide_value);
    if (pi->period < 113)
        pi->period = 113;

    if (m->playing_info.speed_counter != 0)
        voice->slide_value += voice->slide_speed;
}

// DoAdsr

static void do_adsr(SaModule* m, SaVoiceInfo* voice, SaChannel* channel, SaInstrument* instr) {
    if ((instr->adsr_length + instr->adsr_repeat) == 0)
        channel_set_amiga_volume(channel, (uint16_t)((voice->current_volume * m->playing_info.master_volume) / 64));
    else {
        const uint8_t* adsr_table = table_at(m->adsr_tables, m->num_adsr_tables, instr->adsr_number);
        uint8_t adsr_val = adsr_table[voice->adsr_position & (SA_WAVEFORM_SIZE - 1)];

        uint16_t vol = (uint16_t)((m->playing_info.master_volume * voice->current_volume * adsr_val) / 4096);
        if (vol > 64)
            vol = 64;

        channel_set_amiga_volume(channel, vol);

        if ((voice->note == 0x80) && (voice->adsr_position >= instr->sustain_point)) {
            if (instr->sustain_delay == 0)
                return;

            if (voice->sustain_delay_counter != 0) {
                voice->sustain_delay_counter--;
                return;
            }

            voice->sustain_delay_counter = instr->sustain_delay;
        }

        voice->adsr_delay_counter--;

        if (voice->adsr_delay_counter == 0) {
            voice->adsr_delay_counter = instr->adsr_delay;

            voice->adsr_position++;

            if (voice->adsr_position >= (instr->adsr_length + instr->adsr_repeat)) {
                voice->adsr_position = instr->adsr_length;

                if (instr->adsr_repeat == 0)
                    voice->adsr_position--;

                if ((instr->adsr_repeat == 0) && (vol == 0))
                    voice->flag |= 0x01;
            }
        }
    }
}

// DoVolumeSlide

static void do_volume_slide(SaVoiceInfo* voice) {
    int16_t vol = (int16_t)(voice->current_volume + voice->volume_slide_speed);

    if (vol < 0)
        vol = 0;
    else if (vol > 64)
        vol = 64;

    voice->current_volume = (uint16_t)vol;
}

// DoSynthEffects

static void do_synth_effects(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr) {
    voice->effect_delay_counter--;

    if (voice->effect_delay_counter == 0) {
        voice->effect_delay_counter = instr->effect_delay;

        switch (instr->effect) {
            case SA_SYNTH_WAVE_NEGATOR:
                do_synth_wave_negator(voice, instr);
                break;
            case SA_SYNTH_FREE_NEGATOR:
                do_synth_free_negator(m, voice, instr);
                break;
            case SA_SYNTH_ROTATE_VERTICAL:
                do_synth_rotate_vertical(voice, instr);
                break;
            case SA_SYNTH_ROTATE_HORIZONTAL:
                do_synth_rotate_horizontal(voice, instr);
                break;
            case SA_SYNTH_ALIEN_VOICE:
                do_synth_alien_voice(m, voice, instr);
                break;
            case SA_SYNTH_POLY_NEGATOR:
                do_synth_poly_negator(m, voice, instr);
                break;
            case SA_SYNTH_SHACK_WAVE_1:
                do_synth_shack_wave_1(m, voice, instr);
                break;
            case SA_SYNTH_SHACK_WAVE_2:
                do_synth_shack_wave_2(m, voice, instr);
                break;
            case SA_SYNTH_METAMORPH:
                do_synth_metamorph(m, voice, instr);
                break;
            case SA_SYNTH_LASER:
                do_synth_laser(voice, instr);
                break;
            case SA_SYNTH_WAVE_ALIAS:
                do_synth_wave_alias(voice, instr);
                break;
            case SA_SYNTH_NOISE_GENERATOR_1:
                do_synth_noise_generator_1(m, voice, instr);
                break;
            case SA_SYNTH_LOW_PASS_FILTER_1:
                do_synth_low_pass_filter_1(voice, instr);
                break;
            case SA_SYNTH_LOW_PASS_FILTER_2:
                do_synth_low_pass_filter_2(m, voice, instr);
                break;
            case SA_SYNTH_OSZILATOR:
                do_synth_oszilator(m, voice, instr);
                break;
            case SA_SYNTH_NOISE_GENERATOR_2:
                do_synth_noise_generator_2(m, voice, instr);
                break;
            case SA_SYNTH_FM_DRUM:
                do_synth_fm_drum(voice, instr);
                break;
            default:
                break;
        }
    }
}

// IncrementSynthEffectPosition

static void increment_synth_effect_position(SaVoiceInfo* voice, SaInstrument* instr) {
    uint16_t start_pos = instr->effect_arg2;
    uint16_t stop_pos = instr->effect_arg3;

    voice->synth_effect_position++;

    if (voice->synth_effect_position >= stop_pos)
        voice->synth_effect_position = start_pos;
}

// Synth Effect: Wave Negator

static void do_synth_wave_negator(SaVoiceInfo* voice, SaInstrument* instr) {
    voice->waveform_buffer[voice->synth_effect_position]
        = (int8_t)-voice->waveform_buffer[voice->synth_effect_position];

    increment_synth_effect_position(voice, instr);
}

// Synth Effect: Free Negator

static void do_synth_free_negator(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr) {
    if ((voice->flag & 0x04) == 0) {
        int8_t* dest_waveform = voice->waveform_buffer;

        uint16_t waveform_number = instr->effect_arg1;
        uint16_t wave_length = instr->effect_arg2;
        uint16_t wave_repeat = instr->effect_arg3;

        // The cursor runs up to effect_arg2 + effect_arg3, which can pass the 128 byte waveform: read silence there.
        int8_t* waveform = waveform_at(m, waveform_number);
        const uint16_t wave_position = voice->synth_effect_wave_position;
        int16_t wave_val = wave_position < SA_WAVEFORM_SIZE ? (int16_t)(waveform[wave_position] & 0x7f) : 0;

        int8_t* source_waveform = waveform_at(m, instr->waveform_number);
        int16_t count = (int16_t)(instr->waveform_length * 2);

        int buffer_offset = count;

        while ((count > 0) && (count >= wave_val)) {
            buffer_offset--;
            dest_waveform[buffer_offset] = source_waveform[buffer_offset];
            count--;
        }

        int16_t left = (int16_t)(wave_val - count);
        count += left;
        buffer_offset += left;

        while (count > 0) {
            buffer_offset--;
            dest_waveform[buffer_offset] = (int8_t)-source_waveform[buffer_offset];
            count--;
        }

        voice->synth_effect_wave_position++;

        if (voice->synth_effect_wave_position > (wave_length + wave_repeat)) {
            voice->synth_effect_wave_position = wave_length;

            if ((wave_repeat == 0) && (wave_val == 0))
                voice->flag |= 0x04;
        }
    }
}

// Synth Effect: Rotate Vertical

static void do_synth_rotate_vertical(SaVoiceInfo* voice, SaInstrument* instr) {
    int8_t* dest_waveform = voice->waveform_buffer;

    int8_t delta_value = (int8_t)instr->effect_arg1;
    uint16_t start_pos = instr->effect_arg2;
    uint16_t stop_pos = instr->effect_arg3;

    int16_t count = (int16_t)(stop_pos - start_pos);
    int buffer_offset = start_pos;

    while (count >= 0) {
        dest_waveform[buffer_offset++] += delta_value;
        count--;
    }

    increment_synth_effect_position(voice, instr);
}

// Synth Effect: Rotate Horizontal

static void do_synth_rotate_horizontal(SaVoiceInfo* voice, SaInstrument* instr) {
    int8_t* dest_waveform = voice->waveform_buffer;

    uint16_t start_pos = instr->effect_arg2;
    uint16_t stop_pos = instr->effect_arg3;

    // The replayer rotates [start_pos, stop_pos + 1] (at least two bytes); the end stops at the buffer's last byte.
    int16_t count = (int16_t)(stop_pos - start_pos);
    int last = start_pos + (count < 0 ? 0 : count) + 1;
    if (last > SA_WAVEFORM_SIZE - 1)
        last = SA_WAVEFORM_SIZE - 1;

    int8_t first_byte = dest_waveform[start_pos];

    for (int buffer_offset = start_pos; buffer_offset < last; buffer_offset++)
        dest_waveform[buffer_offset] = dest_waveform[buffer_offset + 1];

    dest_waveform[last] = first_byte;

    increment_synth_effect_position(voice, instr);
}

// Synth Effect: Alien Voice

static void do_synth_alien_voice(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr) {
    int8_t* dest_waveform = voice->waveform_buffer;

    uint16_t waveform_number = instr->effect_arg1;
    uint16_t start_pos = instr->effect_arg2;
    uint16_t stop_pos = instr->effect_arg3;

    int8_t* source_waveform = waveform_at(m, waveform_number);
    int buffer_offset = start_pos;

    int16_t count = (int16_t)(stop_pos - start_pos);

    while (count >= 0) {
        dest_waveform[buffer_offset] += source_waveform[buffer_offset];
        buffer_offset++;
        count--;
    }
}

// Synth Effect: Poly Negator

static void do_synth_poly_negator(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr) {
    int8_t* dest_waveform = voice->waveform_buffer;
    int8_t* source_waveform = waveform_at(m, instr->waveform_number);

    uint16_t position = voice->synth_effect_position;
    uint16_t start_pos = instr->effect_arg2;
    uint16_t stop_pos = instr->effect_arg3;

    dest_waveform[position] = source_waveform[position];

    // The replayer steps to start_pos - 1 and negates the byte after it (a 16-bit index, so start_pos 0 negates 0).
    const uint16_t next = position >= stop_pos ? start_pos : (uint16_t)(position + 1);

    dest_waveform[next] = (int8_t)-dest_waveform[next];

    increment_synth_effect_position(voice, instr);
}

// Synth Effect: Shack Wave Helper

static void shack_wave_helper(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr) {
    int8_t* dest_waveform = voice->waveform_buffer;

    uint16_t waveform_number = instr->effect_arg1;
    uint16_t start_pos = instr->effect_arg2;
    uint16_t stop_pos = instr->effect_arg3;

    // start_pos + position can pass the 128 byte waveform: no change there.
    int8_t* source_waveform = waveform_at(m, waveform_number);
    const int source_index = start_pos + voice->synth_effect_position;
    int8_t delta_value = source_index < SA_WAVEFORM_SIZE ? source_waveform[source_index] : 0;

    int buffer_offset = start_pos;
    int16_t count = (int16_t)(stop_pos - start_pos);

    while (count >= 0) {
        dest_waveform[buffer_offset++] += delta_value;
        count--;
    }
}

// Synth Effect: Shack Wave 1

static void do_synth_shack_wave_1(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr) {
    shack_wave_helper(m, voice, instr);
    increment_synth_effect_position(voice, instr);
}

// Synth Effect: Shack Wave 2

static void do_synth_shack_wave_2(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr) {
    shack_wave_helper(m, voice, instr);

    int8_t* dest_waveform = voice->waveform_buffer;

    uint16_t start_pos = instr->effect_arg2;
    uint16_t stop_pos = instr->effect_arg3;

    dest_waveform[start_pos + voice->synth_effect_wave_position]
        = (int8_t)-dest_waveform[start_pos + voice->synth_effect_wave_position];

    voice->synth_effect_wave_position++;

    if (voice->synth_effect_wave_position > (stop_pos - start_pos))
        voice->synth_effect_wave_position = 0;

    increment_synth_effect_position(voice, instr);
}

// Synth Effect: Metamorph and Oszilator Helper

static void metamorph_and_oszilator_helper(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr,
                                           int8_t* source_waveform) {
    (void)m;
    int8_t* dest_waveform = voice->waveform_buffer;

    uint16_t start_pos = instr->effect_arg2;
    uint16_t stop_pos = instr->effect_arg3;

    int buffer_offset = start_pos;
    int16_t count = (int16_t)(stop_pos - start_pos);
    bool set_flag = false;

    while (count >= 0) {
        int8_t val = dest_waveform[buffer_offset];
        if (val != source_waveform[buffer_offset]) {
            set_flag = true;

            if (val < source_waveform[buffer_offset])
                val++;
            else
                val--;

            dest_waveform[buffer_offset] = val;
        }

        buffer_offset++;
        count--;
    }

    if (!set_flag)
        voice->flag |= 0x02;
}

// Synth Effect: Metamorph

static void do_synth_metamorph(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr) {
    if ((voice->flag & 0x01) == 0) {
        uint16_t waveform_number = instr->effect_arg1;

        int8_t* waveform = waveform_at(m, waveform_number);
        metamorph_and_oszilator_helper(m, voice, instr, waveform);
    }
}

// Synth Effect: Laser

static void do_synth_laser(SaVoiceInfo* voice, SaInstrument* instr) {
    int8_t detune = (int8_t)instr->effect_arg2;
    uint16_t repeats = instr->effect_arg3;

    if (voice->synth_effect_wave_position < repeats) {
        voice->slide_value += detune;
        voice->synth_effect_wave_position++;
    }

    increment_synth_effect_position(voice, instr);
}

// Synth Effect: Wave Alias

static void do_synth_wave_alias(SaVoiceInfo* voice, SaInstrument* instr) {
    int8_t* dest_waveform = voice->waveform_buffer;

    int8_t delta_value = (int8_t)instr->effect_arg1;
    uint16_t start_pos = instr->effect_arg2;
    uint16_t stop_pos = instr->effect_arg3;

    int16_t count = (int16_t)(stop_pos - start_pos);
    int buffer_offset = start_pos;

    while (count >= 0) {
        int8_t val = dest_waveform[buffer_offset];

        if (((buffer_offset + 1) >= 128) || (val > dest_waveform[buffer_offset + 1]))
            val -= delta_value;
        else
            val += delta_value;

        dest_waveform[buffer_offset++] = val;
        count--;
    }

    increment_synth_effect_position(voice, instr);
}

// Synth Effect: Noise Generator 1

static void do_synth_noise_generator_1(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr) {
    int8_t* dest_waveform = voice->waveform_buffer;
    dest_waveform[voice->synth_effect_position] ^= sa_random(m);

    increment_synth_effect_position(voice, instr);
}

// Synth Effect: Low Pass Filter 1

static void do_synth_low_pass_filter_1(SaVoiceInfo* voice, SaInstrument* instr) {
    int8_t* dest_waveform = voice->waveform_buffer;

    uint16_t delta_value = instr->effect_arg1;
    uint16_t start_pos = instr->effect_arg2;
    uint16_t stop_pos = instr->effect_arg3;

    for (int i = start_pos; i <= (int)stop_pos; i++) {
        bool flag = false;

        int8_t val1 = dest_waveform[i];
        int8_t val2 = (i == (int)stop_pos) ? dest_waveform[start_pos] : dest_waveform[i + 1];

        if (val1 <= val2)
            flag = true;

        val1 -= val2;
        if (val1 < 0)
            val1 = (int8_t)-val1;

        if (val1 > (int8_t)delta_value) {
            if (flag)
                dest_waveform[i] += 2;
            else
                dest_waveform[i] -= 2;
        }
    }

    increment_synth_effect_position(voice, instr);
}

// Synth Effect: Low Pass Filter 2

static void do_synth_low_pass_filter_2(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr) {
    uint16_t waveform_number = instr->effect_arg1;
    uint16_t start_pos = instr->effect_arg2;
    uint16_t stop_pos = instr->effect_arg3;

    int8_t* waveform = waveform_at(m, waveform_number);
    int8_t* dest_waveform = voice->waveform_buffer;
    int buffer_index = stop_pos;

    for (int i = start_pos; i <= (int)stop_pos; i++) {
        bool flag = false;

        int8_t val1 = dest_waveform[i];
        int8_t val2 = (i == (int)stop_pos) ? dest_waveform[start_pos] : dest_waveform[i + 1];

        if (val1 <= val2)
            flag = true;

        val1 -= val2;
        if (val1 < 0)
            val1 = (int8_t)-val1;

        uint8_t delta_val = (uint8_t)(waveform[buffer_index++] & 0x7f);
        buffer_index &= 0x7f;

        if (val1 > (int8_t)delta_val) {
            if (flag)
                dest_waveform[i] += 2;
            else
                dest_waveform[i] -= 2;
        }
    }

    increment_synth_effect_position(voice, instr);
}

// Synth Effect: Oszilator

static void do_synth_oszilator(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr) {
    if ((voice->flag & 0x02) != 0) {
        voice->flag ^= 0x08;
        voice->flag &= (uint8_t)~0x02;
    }

    int8_t* source_waveform;

    if ((voice->flag & 0x08) != 0)
        source_waveform = waveform_at(m, instr->waveform_number);
    else
        source_waveform = waveform_at(m, instr->effect_arg1);

    metamorph_and_oszilator_helper(m, voice, instr, source_waveform);
}

// Synth Effect: Noise Generator 2

static void do_synth_noise_generator_2(SaModule* m, SaVoiceInfo* voice, SaInstrument* instr) {
    int8_t* dest_waveform = voice->waveform_buffer;

    uint16_t start_pos = instr->effect_arg2;
    uint16_t stop_pos = instr->effect_arg3;

    int16_t count = (int16_t)(stop_pos - start_pos);
    int buffer_offset = start_pos;

    do {
        int8_t val = dest_waveform[buffer_offset + count];
        val ^= 0x05;
        val = (int8_t)(((uint8_t)val << 2) | ((uint8_t)val >> (8 - 2)));
        val += sa_random(m);

        dest_waveform[buffer_offset + count] = val;
        count--;
    } while (count >= 0);
}

// Synth Effect: FM Drum

static void do_synth_fm_drum(SaVoiceInfo* voice, SaInstrument* instr) {
    uint8_t level = (uint8_t)instr->effect_arg1;
    uint16_t factor = instr->effect_arg2;
    uint16_t repeats = instr->effect_arg3;

    if (voice->synth_effect_wave_position >= repeats) {
        voice->slide_value = instr->fine_tuning;
        voice->synth_effect_wave_position = 0;
    }

    uint16_t decrement = (uint16_t)((factor << 8) | level);
    voice->slide_value = (int16_t)(voice->slide_value - decrement);

    voice->synth_effect_wave_position++;

    increment_synth_effect_position(voice, instr);
}

// Voices

// Paula channels 0 and 3 are left, 1 and 2 right.
static float channel_pan(int channel) {
    return (channel == 0 || channel == 3) ? -1.0f : 1.0f;
}

// A sample is played from its start up to `length`, then from loop_start for loop_length. The voice loops on its own
// when the loop ends where the sample does; any other loop is a one-shot head with the loop queued behind it.
static void play_channel_sample(NuadeVoices* voices, uint32_t index, const SaChannel* c) {
    NuadeSample sample = { .data = c->sample_data, .format = NuadeSampleFormat_S8, .length = c->sample_length };
    const bool loops = c->loop_length != 0 && c->loop_start < c->sample_length;
    if (loops && c->loop_start + c->loop_length == c->sample_length) {
        sample.loop_start = c->loop_start;
        sample.loop_length = c->loop_length;
        nuade_voice_play(voices, index, &sample, 0);
    } else if (loops) {
        const NuadeSample loop = { .data = c->sample_data + c->loop_start,
                                   .format = NuadeSampleFormat_S8,
                                   .length = c->loop_length,
                                   .loop_length = c->loop_length };
        nuade_voice_play(voices, index, &sample, 0);
        nuade_voice_queue(voices, index, &loop);
    } else {
        nuade_voice_play(voices, index, &sample, 0);
    }
}

// Hand the registers the tick wrote to the voice.
static void sync_voice(SaModule* m, int index) {
    SaChannel* c = &m->channels[index];
    const bool audible = c->active && c->sample_data != nullptr && c->sample_length != 0;

    if (!audible) {
        if (nuade_voice_is_playing(m->mixer, (uint32_t)index))
            nuade_voice_stop(m->mixer, (uint32_t)index);
    } else if (c->play_pending) {
        play_channel_sample(m->mixer, (uint32_t)index, c);
    }
    c->play_pending = false;

    const bool silent = !audible || c->period == 0;
    nuade_voice_set_pitch(m->mixer, (uint32_t)index, silent ? 0.0 : nuade_paula_period_hz(c->period));
    nuade_voice_set_volume(m->mixer, (uint32_t)index, silent ? 0.0f : (float)c->volume / 64.0f);
    nuade_voice_set_pan(m->mixer, (uint32_t)index, channel_pan(index));
}

bool sa_tick(SaModule* module) {
    const uint32_t loops = module->loop_count;
    play_tick(module);
    for (int i = 0; i < SA_VOICES; i++) {
        sync_voice(module, i);
    }
    return module->loop_count != loops;
}

uint32_t sa_tick_length(const SaModule* module) {
    const uint16_t tempo = module->current_song_info->tempo;
    return SA_TIMEBASE_HZ / (tempo > 0 ? tempo : 50);
}

// Public API

static bool is_soar_module(const uint8_t* data, size_t size) {
    return size >= 16 && memcmp(data, "SOARV1.0", 8) == 0;
}

static bool is_compiled_module(const uint8_t* data, size_t size) {
    size_t base = 0;
    uint32_t offsets[SA_COMPILED_TABLES];
    return find_compiled_block(data, size, &base) && read_compiled_offsets(data + base, size - base, offsets);
}

// Reads the plain "SOARV1.0" container and the compiled executable form. The compressed "@OARV1.0" variant needs
// lh.library decompression and is not supported.
bool sa_probe(const uint8_t* data, size_t size) {
    return data != nullptr && (is_soar_module(data, size) || is_compiled_module(data, size));
}

SaModule* sa_create(const uint8_t* data, size_t size) {
    if (!sa_probe(data, size))
        return nullptr;

    SaModule* m = (SaModule*)calloc(1, sizeof(SaModule));
    if (!m)
        return nullptr;

    m->random_state = 12345;

    // Keep original data for export
    m->original_data = (uint8_t*)malloc(size);
    if (m->original_data) {
        memcpy(m->original_data, data, size);
        m->original_size = size;
    }

    bool loaded = false;
    if (is_soar_module(data, size)) {
        SaReader reader;
        reader_init(&reader, data + 8, size - 8);
        loaded = load_normal_module(m, &reader);
    } else {
        size_t base = 0;
        loaded = find_compiled_block(data, size, &base) && load_compiled_module(m, data + base, size - base);
    }

    // Playback reads a position row from the first tick.
    if (!loaded || m->num_positions == 0) {
        sa_destroy(m);
        return nullptr;
    }

    return m;
}

void sa_destroy(SaModule* module) {
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

    if (module->sample_data) {
        for (int i = 0; i < module->num_samples; i++)
            free(module->sample_data[i]);
        free(module->sample_data);
    }
    if (module->sample_lengths)
        free(module->sample_lengths);

    if (module->waveform_data) {
        for (int i = 0; i < module->num_waveforms; i++)
            free(module->waveform_data[i]);
        free(module->waveform_data);
    }

    if (module->adsr_tables) {
        for (int i = 0; i < module->num_adsr_tables; i++)
            free(module->adsr_tables[i]);
        free(module->adsr_tables);
    }

    if (module->amf_tables) {
        for (int i = 0; i < module->num_amf_tables; i++)
            free(module->amf_tables[i]);
        free(module->amf_tables);
    }

    if (module->original_data)
        free(module->original_data);

    free(module);
}

int sa_subsong_count(const SaModule* module) {
    if (!module)
        return 0;
    return module->num_sub_songs;
}

bool sa_start(SaModule* module, int subsong, NuadeVoices* voices) {
    if (!module || subsong < 0 || subsong >= module->num_sub_songs || voices == nullptr)
        return false;

    module->mixer = voices;
    initialize_sound(module, subsong);
    return true;
}

// Edit API

SaPosition sa_position(const SaModule* module) {
    const SaGlobalPlayingInfo* info = &module->playing_info;
    if (info->shown_position < 0) {
        return (SaPosition) { 0 };
    }
    // A track length set on the row can end the pattern before the row; the row is still shown.
    const int rows = info->rows_per_track > info->shown_row ? info->rows_per_track : info->shown_row + 1;
    return (SaPosition) { .valid = true,
                          .position = info->shown_position,
                          .row = info->shown_row,
                          .rows = rows,
                          .speed = info->current_speed,
                          .tick = info->speed_counter };
}

SaVoiceState sa_voice_state(const SaModule* module, int voice) {
    if (voice < 0 || voice >= SA_VOICES) {
        return (SaVoiceState) { 0 };
    }
    return (SaVoiceState) { .note = module->voices[voice].shown_note,
                            .instrument = module->voices[voice].shown_instrument };
}

int sa_track_rows(const SaModule* module) {
    return module->current_song_info != nullptr ? module->current_song_info->rows_per_track : 0;
}

SaCell sa_pattern_cell(const SaModule* module, int position, int channel, int row) {
    if (position < 0 || position >= module->num_positions || channel < 0 || channel >= SA_VOICES || row < 0) {
        return (SaCell) { 0 };
    }
    const int index = module->positions[position][channel].start_track_row + row;
    if (index >= module->num_track_lines) {
        return (SaCell) { 0 };
    }
    const SaTrackLine* line = &module->track_lines[index];
    return (SaCell) { .note = line->note,
                      .instrument = line->instrument,
                      .arpeggio = line->arpeggio,
                      .effect = (uint8_t)line->effect,
                      .effect_arg = line->effect_arg };
}

int sa_get_num_positions(const SaModule* module) {
    return module ? module->num_positions : 0;
}

int sa_get_num_track_lines(const SaModule* module) {
    return module ? module->num_track_lines : 0;
}

int sa_get_rows_per_track(const SaModule* module, int subsong) {
    if (!module || subsong < 0 || subsong >= module->num_sub_songs)
        return 0;
    return module->sub_songs[subsong].rows_per_track;
}

void sa_get_cell(const SaModule* module, int idx, uint8_t* note, uint8_t* instrument, uint8_t* arpeggio,
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
    const SaTrackLine* tl = &module->track_lines[idx];
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

void sa_set_cell(SaModule* module, int idx, uint8_t note, uint8_t instrument, uint8_t arpeggio, uint8_t effect,
                 uint8_t effect_arg) {
    if (!module || idx < 0 || idx >= module->num_track_lines)
        return;
    SaTrackLine* tl = &module->track_lines[idx];
    tl->note = note;
    tl->instrument = instrument;
    tl->arpeggio = arpeggio;
    tl->effect = (SaEffect)effect;
    tl->effect_arg = effect_arg;
}

void sa_get_position(const SaModule* module, int pos, int channel, uint16_t* start_track_row, int8_t* sound_transpose,
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
    const SaSinglePositionInfo* pi = &module->positions[pos][channel];
    if (start_track_row)
        *start_track_row = pi->start_track_row;
    if (sound_transpose)
        *sound_transpose = pi->sound_transpose;
    if (note_transpose)
        *note_transpose = pi->note_transpose;
}

void sa_set_position(SaModule* module, int pos, int channel, uint16_t start_track_row, int8_t sound_transpose,
                     int8_t note_transpose) {
    if (!module || pos < 0 || pos >= module->num_positions || channel < 0 || channel >= 4)
        return;
    SaSinglePositionInfo* pi = &module->positions[pos][channel];
    pi->start_track_row = start_track_row;
    pi->sound_transpose = sound_transpose;
    pi->note_transpose = note_transpose;
}

int sa_get_instrument_count(const SaModule* module) {
    return module ? module->num_instruments : 0;
}

const char* sa_get_instrument_name(const SaModule* module, int inst) {
    if (!module || inst < 0 || inst >= module->num_instruments)
        return "";
    return module->instruments[inst].name;
}

float sa_get_instrument_param(const SaModule* module, int inst, const char* param) {
    if (!module || inst < 0 || inst >= module->num_instruments || !param)
        return -1.0f;
    const SaInstrument* in = &module->instruments[inst];

    if (strcmp(param, "type") == 0)
        return (float)in->type;
    if (strcmp(param, "waveformNumber") == 0)
        return (float)in->waveform_number;
    if (strcmp(param, "waveformLength") == 0)
        return (float)in->waveform_length;
    if (strcmp(param, "repeatLength") == 0)
        return (float)in->repeat_length;
    if (strcmp(param, "volume") == 0)
        return (float)in->volume;
    if (strcmp(param, "fineTuning") == 0)
        return (float)in->fine_tuning;
    if (strcmp(param, "portamentoSpeed") == 0)
        return (float)in->portamento_speed;
    if (strcmp(param, "vibratoDelay") == 0)
        return (float)in->vibrato_delay;
    if (strcmp(param, "vibratoSpeed") == 0)
        return (float)in->vibrato_speed;
    if (strcmp(param, "vibratoLevel") == 0)
        return (float)in->vibrato_level;
    if (strcmp(param, "amfNumber") == 0)
        return (float)in->amf_number;
    if (strcmp(param, "amfDelay") == 0)
        return (float)in->amf_delay;
    if (strcmp(param, "amfLength") == 0)
        return (float)in->amf_length;
    if (strcmp(param, "amfRepeat") == 0)
        return (float)in->amf_repeat;
    if (strcmp(param, "adsrNumber") == 0)
        return (float)in->adsr_number;
    if (strcmp(param, "adsrDelay") == 0)
        return (float)in->adsr_delay;
    if (strcmp(param, "adsrLength") == 0)
        return (float)in->adsr_length;
    if (strcmp(param, "adsrRepeat") == 0)
        return (float)in->adsr_repeat;
    if (strcmp(param, "sustainPoint") == 0)
        return (float)in->sustain_point;
    if (strcmp(param, "sustainDelay") == 0)
        return (float)in->sustain_delay;
    if (strcmp(param, "effect") == 0)
        return (float)in->effect;
    if (strcmp(param, "effectArg1") == 0)
        return (float)in->effect_arg1;
    if (strcmp(param, "effectArg2") == 0)
        return (float)in->effect_arg2;
    if (strcmp(param, "effectArg3") == 0)
        return (float)in->effect_arg3;
    if (strcmp(param, "effectDelay") == 0)
        return (float)in->effect_delay;

    return -1.0f;
}

void sa_set_instrument_param(SaModule* module, int inst, const char* param, float value) {
    if (!module || inst < 0 || inst >= module->num_instruments || !param)
        return;
    SaInstrument* in = &module->instruments[inst];
    uint16_t v = (uint16_t)value;
    int16_t sv = (int16_t)value;

    if (strcmp(param, "type") == 0) {
        in->type = (SaInstrumentType)(int)value;
        return;
    }
    if (strcmp(param, "waveformNumber") == 0) {
        in->waveform_number = v;
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
        in->volume = v;
        return;
    }
    if (strcmp(param, "fineTuning") == 0) {
        in->fine_tuning = sv;
        return;
    }
    if (strcmp(param, "portamentoSpeed") == 0) {
        in->portamento_speed = v;
        return;
    }
    if (strcmp(param, "vibratoDelay") == 0) {
        in->vibrato_delay = v;
        return;
    }
    if (strcmp(param, "vibratoSpeed") == 0) {
        in->vibrato_speed = v;
        return;
    }
    if (strcmp(param, "vibratoLevel") == 0) {
        in->vibrato_level = v;
        return;
    }
    if (strcmp(param, "amfNumber") == 0) {
        in->amf_number = v;
        return;
    }
    if (strcmp(param, "amfDelay") == 0) {
        in->amf_delay = v;
        return;
    }
    if (strcmp(param, "amfLength") == 0) {
        in->amf_length = v;
        return;
    }
    if (strcmp(param, "amfRepeat") == 0) {
        in->amf_repeat = v;
        return;
    }
    if (strcmp(param, "adsrNumber") == 0) {
        in->adsr_number = v;
        return;
    }
    if (strcmp(param, "adsrDelay") == 0) {
        in->adsr_delay = v;
        return;
    }
    if (strcmp(param, "adsrLength") == 0) {
        in->adsr_length = v;
        return;
    }
    if (strcmp(param, "adsrRepeat") == 0) {
        in->adsr_repeat = v;
        return;
    }
    if (strcmp(param, "sustainPoint") == 0) {
        in->sustain_point = v;
        return;
    }
    if (strcmp(param, "sustainDelay") == 0) {
        in->sustain_delay = v;
        return;
    }
    if (strcmp(param, "effect") == 0) {
        in->effect = (SaSynthesisEffect)(int)value;
        return;
    }
    if (strcmp(param, "effectArg1") == 0) {
        in->effect_arg1 = v;
        return;
    }
    if (strcmp(param, "effectArg2") == 0) {
        in->effect_arg2 = v;
        return;
    }
    if (strcmp(param, "effectArg3") == 0) {
        in->effect_arg3 = v;
        return;
    }
    if (strcmp(param, "effectDelay") == 0) {
        in->effect_delay = v;
        return;
    }
}

// Export: serialize back to SOARV1.0 binary

size_t sa_export(const SaModule* module, uint8_t* out, size_t max_size) {
    if (!module || !module->original_data)
        return 0;

    // Return original file with edits patched in.
    // Pattern cells and instrument params are modified in internal structs
    // by set_cell/set_instrument_param; we need to write those changes back
    // to the original byte positions. For now, return the original unmodified.
    // Export: return original file data. Edits applied via set_cell/set_instrument_param
    // modify internal structs which are used by the WASM replayer during playback.
    // The exported binary is the original file. A full re-serializer would need to
    // track byte offsets for every field to patch them back.
    size_t total = module->original_size;

    if (!out)
        return total;
    if (max_size < total)
        return 0;

    memcpy(out, module->original_data, total);
    return total;
}

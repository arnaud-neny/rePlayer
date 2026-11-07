// SPDX-License-Identifier: MIT
// Jesper Olsen player, replayer C: the data modules of the Lollypop and Georg Glaxo games, translated from the
// replayer in the eagleplayer.
//
// A module is a table of song lists. A list has one 10 byte entry per voice: the voice record in the module (the
// replayer keeps its state in the file), the instrument table, the start of the voice's track, the Paula channel, the
// tempo, a priority and a volume. Starting a song initialises the records its list names. The eagleplayer starts
// list 0 (which silences the channels) and then the list of the subsong.
//
// A voice steps when its tempo accumulator carries: the track (a list of pattern numbers with field commands in
// front), the pattern (rows of note and duration with field commands in front), then an instrument start. Every tick
// the voice then runs three small programs of (field, value) pairs, mixes its volume from four levels through a
// multiplication table, and writes sample pointer, volume and period. Samples restart through DMA off at the
// trigger tick and DMA on at the end of the next one.

#include "jo_internal.h"
#include <string.h>

typedef JoCVoice Voice;

#define RECORD_SIZE 0x44
#define FORM_HEADER_SIZE 0x68
#define WAVE_IS_SVX 0x80000000u
#define PROGRAM_HOLD 0x7e // a program entry that sets the program's reload value or waits for the row to end
#define PROGRAM_WAIT 0x80 // its argument: wait while the row is tied
#define ROW_TIED 0x80

// The replayer's period table (95 notes). The next table of the replayer follows it, the level rows
// 0, 0x40, 0x80 ..., which a note above 94 reads.
static const uint16_t k_periods[128] = {
    0x6ae0, 0x64e0, 0x5f40, 0x59e0, 0x54d0, 0x5010, 0x4b90, 0x4750, 0x4350, 0x3f90, 0x3c00, 0x38a0, 0x3570,
    0x3270, 0x2fa0, 0x2cf0, 0x2a68, 0x2808, 0x25c8, 0x23a8, 0x21a8, 0x1fc8, 0x1e00, 0x1c50, 0x1ab8, 0x1938,
    0x17d0, 0x1678, 0x1534, 0x1404, 0x12e4, 0x11d4, 0x10d4, 0x0fe4, 0x0f00, 0x0e28, 0x0d5c, 0x0c9c, 0x0be8,
    0x0b3c, 0x0a9a, 0x0a02, 0x0972, 0x08ea, 0x086a, 0x07f2, 0x0780, 0x0714, 0x06ae, 0x064e, 0x05f4, 0x059e,
    0x054d, 0x0501, 0x04b9, 0x0475, 0x0435, 0x03f9, 0x03c0, 0x038a, 0x0357, 0x0327, 0x02fa, 0x02cf, 0x02a7,
    0x0281, 0x025d, 0x023b, 0x021b, 0x01fc, 0x01e0, 0x01c5, 0x01ac, 0x0194, 0x017d, 0x0168, 0x0153, 0x0140,
    0x012e, 0x011d, 0x010d, 0x00fe, 0x00f0, 0x00e2, 0x00d6, 0x00ca, 0x00be, 0x00b4, 0x00aa, 0x00a0, 0x0097,
    0x008f, 0x0087, 0x007f, 0x0078, 0x0000, 0x0040, 0x0080, 0x00c0, 0x0100, 0x0140, 0x0180, 0x01c0, 0x0200,
    0x0240, 0x0280, 0x02c0, 0x0300, 0x0340, 0x0380, 0x03c0, 0x0400, 0x0440, 0x0480, 0x04c0, 0x0500, 0x0540,
    0x0580, 0x05c0, 0x0600, 0x0640, 0x0680, 0x06c0, 0x0700, 0x0740, 0x0780, 0x07c0, 0x0800,
};

// The voice record

enum {
    Record_Instrument = 0x00,
    Record_Speed = 0x03,
    Record_RowDuration = 0x05,
    Record_Repeat = 0x07,
    Record_BaseNote = 0x09,
    Record_Transpose = 0x0b,
    Record_Slide = 0x0d,
    Record_PitchA = 0x0f,
    Record_PitchB = 0x11,
    Record_VolumeC = 0x13,
    Record_VolumeB = 0x15,
    Record_VolumeA = 0x17,
    Record_NoPitchA = 0x19,
    Record_Tie = 0x1c,
    Record_Wave = 0x1f,
    Record_SlideSum = 0x20,
    Record_Programs = 0x22, // three of 4 bytes: position, counter, reload
    Record_InstrumentTable = 0x2e,
    Record_Note = 0x32,
    Record_Duration = 0x33,
    Record_TrackPosition = 0x34,
    Record_PatternPosition = 0x36,
    Record_Hardware = 0x3c, // the address of the Paula channel's registers
    Record_Priority = 0x40,
    Record_Tempo = 0x41,
    Record_NewNote = 0x42,
};

#define AUDIO_REGISTERS 0xdff0a0u
#define AUDIO_REGISTER_STRIDE 0x10u
#define FIXED_PITCH 0x80

static void unpack_record(Voice* v, const uint8_t* record) {
    const uint8_t base_note = record[Record_BaseNote];
    v->instrument = nuade_read_be16(record + Record_Instrument);
    v->speed = record[Record_Speed];
    v->row_duration = record[Record_RowDuration];
    v->repeat = (int8_t)record[Record_Repeat];
    v->fixed_pitch = (base_note & FIXED_PITCH) != 0;
    v->base_note = base_note & ~FIXED_PITCH;
    v->transpose = record[Record_Transpose];
    v->slide = (int8_t)record[Record_Slide];
    v->pitch_a = (int8_t)record[Record_PitchA];
    v->pitch_b = (int8_t)record[Record_PitchB];
    v->volume_c = record[Record_VolumeC];
    v->volume_b = record[Record_VolumeB];
    v->volume_a = record[Record_VolumeA];
    v->no_pitch_a = (record[Record_NoPitchA] & 1) != 0;
    v->tie = record[Record_Tie] != 0;
    v->wave = record[Record_Wave];
    v->slide_sum = nuade_read_be16(record + Record_SlideSum);
    for (size_t i = 0; i < nuade_sizeof_array(v->programs); i++) {
        const uint8_t* program = record + Record_Programs + 4 * i;
        v->programs[i] = (JoCProgram) { .position = nuade_read_be16(program),
                                        .counter = (int8_t)program[2],
                                        .reload = program[3] };
    }
    v->instrument_table = nuade_read_be32(record + Record_InstrumentTable);
    v->note = record[Record_Note];
    v->duration = record[Record_Duration];
    v->track_position = nuade_read_be16(record + Record_TrackPosition);
    v->pattern_position = nuade_read_be16(record + Record_PatternPosition);
    const uint32_t hardware = nuade_read_be32(record + Record_Hardware);
    const uint32_t channel = (hardware - AUDIO_REGISTERS) / AUDIO_REGISTER_STRIDE;
    if (hardware >= AUDIO_REGISTERS && channel < JO_VOICES) {
        v->channel = (uint8_t)channel;
    }
    v->priority = record[Record_Priority];
    v->tempo = record[Record_Tempo];
    v->new_note = record[Record_NewNote] != 0;
}

static void pack_record(const Voice* v, uint8_t* record) {
    memset(record, 0, RECORD_SIZE);
    jor_put16(record, Record_Instrument, v->instrument);
    record[Record_Speed] = v->speed;
    record[Record_RowDuration] = v->row_duration;
    record[Record_Repeat] = (uint8_t)v->repeat;
    record[Record_BaseNote] = (uint8_t)(v->base_note | (v->fixed_pitch ? FIXED_PITCH : 0));
    record[Record_Transpose] = v->transpose;
    record[Record_Slide] = (uint8_t)v->slide;
    record[Record_PitchA] = (uint8_t)v->pitch_a;
    record[Record_PitchB] = (uint8_t)v->pitch_b;
    record[Record_VolumeC] = v->volume_c;
    record[Record_VolumeB] = v->volume_b;
    record[Record_VolumeA] = v->volume_a;
    record[Record_NoPitchA] = v->no_pitch_a ? 1 : 0;
    record[Record_Tie] = v->tie ? 0xff : 0;
    record[Record_Wave] = v->wave;
    jor_put16(record, Record_SlideSum, v->slide_sum);
    for (size_t i = 0; i < nuade_sizeof_array(v->programs); i++) {
        uint8_t* program = record + Record_Programs + 4 * i;
        jor_put16(program, 0, v->programs[i].position);
        program[2] = (uint8_t)v->programs[i].counter;
        program[3] = v->programs[i].reload;
    }
    jor_put32(record, Record_InstrumentTable, v->instrument_table);
    record[Record_Note] = v->note;
    record[Record_Duration] = v->duration;
    jor_put16(record, Record_TrackPosition, v->track_position);
    jor_put16(record, Record_PatternPosition, v->pattern_position);
    jor_put32(record, Record_Hardware, AUDIO_REGISTERS + AUDIO_REGISTER_STRIDE * v->channel);
    record[Record_Priority] = v->priority;
    record[Record_Tempo] = v->tempo;
    record[Record_NewNote] = v->new_note ? 0xff : 0;
}

// A song command or a program entry stores a byte into the record at an offset the song gives. Offsets past the
// record would land in the next record or the module; no song does that.
static void poke_byte(Voice* v, uint32_t offset, uint8_t value) {
    if (offset >= RECORD_SIZE) {
        return;
    }
    uint8_t record[RECORD_SIZE];
    pack_record(v, record);
    record[offset] = value;
    unpack_record(v, record);
}

// The command word (command byte, argument) is stored at the record offset (command byte - 0x80), the 68020 stores
// it at an odd offset as well
static void store_command(void* voice, uint16_t command) {
    const uint32_t offset = (uint32_t)(command >> 8) - 0x80u;
    poke_byte(voice, offset, (uint8_t)(command >> 8));
    poke_byte(voice, offset + 1, (uint8_t)command);
}

static void load_record(const JoModule* m, uint32_t offset, Voice* v) {
    uint8_t record[RECORD_SIZE];
    jom_record(m, offset, record, RECORD_SIZE);
    *v = (Voice) { 0 };
    unpack_record(v, record);
}

// The instrument record

typedef struct Instrument {
    JoCProgram programs[3];
    uint8_t volume;
    bool no_dma_stop; // a trigger does not stop the DMA
    bool reset_wave;  // the wave number goes back to 0 when the note starts
    bool tie;         // a trigger while tied keeps the sound
    bool multi_wave;  // the wave number selects a block of the sample
    uint32_t wave;    // sample offset; WAVE_IS_SVX: the sample is an IFF 8SVX file and `wave` points at its body
    uint16_t length;  // words
    uint32_t loop_start;
    uint16_t loop_length;
} Instrument;

static Instrument read_instrument(const JoModule* m, uint32_t offset) {
    Instrument i = { 0 };
    for (size_t p = 0; p < nuade_sizeof_array(i.programs); p++) {
        const uint32_t at = offset + 4 * (uint32_t)p;
        i.programs[p] = (JoCProgram) { .position = jom_u16(m, at),
                                       .counter = (int8_t)jom_u8(m, at + 2),
                                       .reload = jom_u8(m, at + 3) };
    }
    const uint8_t flags = jom_u8(m, offset + 0xd);
    i.volume = jom_u8(m, offset + 0xc);
    i.no_dma_stop = (flags & 0x01) != 0;
    i.reset_wave = (flags & 0x02) != 0;
    i.tie = (flags & 0x04) != 0;
    i.multi_wave = (flags & 0x80) != 0;
    i.wave = jom_u32(m, offset + 0xe);
    i.length = jom_u16(m, offset + 0x12);
    i.loop_start = jom_u32(m, offset + 0x14);
    i.loop_length = jom_u16(m, offset + 0x18);
    return i;
}

static Instrument voice_instrument(const JoModule* m, const Voice* v) {
    const uint32_t entry = v->instrument_table + 2u * (v->instrument & 0xff);
    return read_instrument(m, jom_u16(m, entry));
}

// The header of an 8SVX file that precedes the sample body: the lengths of the one-shot and the repeat part
typedef struct SvxHeader {
    uint16_t one_shot; // bytes
    uint16_t repeat;
} SvxHeader;

static SvxHeader read_svx_header(const JoModule* m, uint32_t body) {
    const uint32_t header = body - FORM_HEADER_SIZE;
    return (SvxHeader) { .one_shot = jom_u16(m, header + 0x16), .repeat = jom_u16(m, header + 0x1a) };
}

// Song start

static Voice* find_record(JoC* c, const JoModule* m, uint32_t offset) {
    for (int i = 0; i < c->record_count; i++) {
        if (c->record_offsets[i] == offset) {
            return &c->records[i];
        }
    }
    if (c->record_count == JO_C_RECORDS) {
        return nullptr;
    }
    const int index = c->record_count++;
    c->record_offsets[index] = offset;
    load_record(m, offset, &c->records[index]);
    return &c->records[index];
}

static uint32_t list_offset(const JoModule* m, int list) {
    return jom_u16(m, 2u * (uint32_t)list);
}

// An entry of a song list: the voice it starts
typedef struct ListEntry {
    uint32_t record;           // offset of the voice record
    uint32_t instrument_table; // offset of the instrument table
    uint16_t track;            // offset of the first track entry; 0 = the voice is silent
    uint8_t channel;
    uint8_t speed;
    uint8_t priority;
    uint8_t volume;
} ListEntry;

#define LIST_ENTRY_SIZE 10

static ListEntry read_list_entry(const JoModule* m, uint32_t offset) {
    return (ListEntry) { .record = jom_offset16(jom_u16(m, offset)),
                         .instrument_table = jom_offset16(jom_u16(m, offset + 2)),
                         .track = jom_u16(m, offset + 4),
                         .channel = jom_u8(m, offset + 6),
                         .speed = jom_u8(m, offset + 7),
                         .priority = jom_u8(m, offset + 8),
                         .volume = jom_u8(m, offset + 9) };
}

// Starts the song list `list`: every entry initialises the voice record it names, unless a voice playing a song of
// higher priority owns it
static void start_list(JoC* c, const JoModule* m, int list) {
    c->master = 0x3f;
    memset(c->dma_off_next, 0, sizeof(c->dma_off_next));
    memset(c->dma_on, 0, sizeof(c->dma_on));
    uint32_t at = list_offset(m, list);
    for (int n = 0; n < 4 * JO_VOICES && jom_u16(m, at) != JoWord_End; n++, at += LIST_ENTRY_SIZE) {
        const ListEntry entry = read_list_entry(m, at);
        Voice* v = find_record(c, m, entry.record);
        if (v == nullptr || entry.channel >= JO_VOICES || entry.priority < v->priority) {
            continue;
        }
        c->slots[entry.channel] = v;
        v->instrument_table = entry.instrument_table;
        v->track_position = entry.track;
        v->channel = entry.channel;
        nuade_paula_channel_dma_off(&c->paula[v->channel]);
        v->speed = entry.speed;
        v->priority = entry.track != 0 ? entry.priority : 0;
        v->volume_a = entry.volume;
        v->volume_b = 0x3f;
        v->volume_c = 0x3f;
        v->note = JoNote_Silent;
        v->duration = 1;
        v->pattern_position = 0;
        v->repeat = 0;
        v->base_note = 0;
        v->fixed_pitch = false;
        v->transpose = 0;
        v->slide_sum = 0;
        v->slide = 0;
        v->tempo = 0xff;
        v->pitch_b = 0;
        v->no_pitch_a = false;
        v->tie = false;
        v->started = false;
    }
}

bool jo_c_detect(const JoModule* m) {
    const uint16_t size = jom_u16(m, 0);
    if (size > 0x200 || size < 4 || (size & 1) != 0) {
        return false;
    }
    for (uint32_t at = 2; at < 2u + size; at += 2) {
        const uint16_t list = jom_u16(m, at);
        if (list == 0 || (list & 0x8000) != 0 || (list & 1) != 0 || jom_u16(m, list - 2u) != JoWord_End) {
            return false;
        }
    }
    return true;
}

// The eagleplayer's subsong range: the lists after list 0, minus one more when the voice records follow an empty
// slot or a 0x7f00 marker
int jo_c_subsong_count(const JoModule* m) {
    int count = jom_u16(m, 0) / 2 - 1;
    const uint32_t records = jom_u16(m, jom_u16(m, 0));
    if (count != 1 && (jom_u16(m, records - 6u) == 0 || jom_u16(m, records - 4u) == 0x7f00)) {
        count--;
    }
    return count;
}

void jo_c_start(JoC* c, const JoModule* m, int subsong) {
    *c = (JoC) { 0 };
    start_list(c, m, 0);
    start_list(c, m, subsong + 1);
    bool has_track[JO_VOICES];
    for (int i = 0; i < JO_VOICES; i++) {
        has_track[i] = c->slots[i] != nullptr && c->slots[i]->track_position != 0;
    }
    jo_song_end_start(&c->end, has_track);
}

// Tick

// One of the voice's three programs: every time its counter runs out it stores a value into a field of the record
static void run_program(const JoModule* m, Voice* v, int index) {
    JoCProgram* p = &v->programs[index];
    uint32_t at;
    if (p->position == 0 || !jo_counter_expired(&p->counter)) {
        return;
    }
    p->counter++;
    const JoPair hold = jo_read_pair(m, jom_offset16(p->position));
    if (hold.first == PROGRAM_HOLD) {
        if ((hold.second & PROGRAM_WAIT) == 0) {
            p->reload = hold.second;
        } else if ((v->row_duration & ROW_TIED) != 0) {
            p->counter = 0;
            return;
        }
        p->position += 2;
        p->counter = (int8_t)p->reload;
        return;
    }
    if (jo_follow(m, &p->position, &at) == JoFollow_End) {
        return;
    }
    const JoPair entry = jo_read_pair(m, at);
    poke_byte(v, 1u + (entry.first & 0x7fu), entry.second);
    p->position += 2;
    p->counter = (int8_t)p->reload;
}

// Starts the instrument of a note row: loads its programs and stops the channel for the restart
static void trigger(JoC* c, const JoModule* m, Voice* v) {
    const Instrument instrument = voice_instrument(m, v);
    v->new_note = true;
    for (size_t i = 0; i < nuade_sizeof_array(v->programs); i++) {
        v->programs[i] = instrument.programs[i];
    }
    v->slide_sum = 0;
    if (instrument.tie) {
        if (v->tie) {
            return;
        }
        v->tie = true;
    } else {
        v->tie = false;
    }
    c->dma_off_next[v->channel] = true;
    if (!instrument.no_dma_stop) {
        nuade_paula_channel_dma_off(&c->paula[v->channel]);
    }
}

// The row of the pattern the voice plays: field commands, then a note row, a rest row or a hold row
static void step_pattern(JoC* c, const JoModule* m, Voice* v, uint32_t pattern) {
    const uint32_t start = pattern + jom_offset16(v->pattern_position);
    v->slide = 0;
    uint32_t at = jo_run_commands(m, start, v, store_command);
    const JoRow row = jo_read_row(m, at);
    at += 2;
    if (row.kind == JoRowKind_Note) {
        if (!row.tied) {
            trigger(c, m, v);
        }
        v->note = (uint8_t)(row.first + v->transpose);
        v->duration = row.second;
        v->slide_sum = 0;
        v->started = true;
    } else {
        v->row_duration = row.second;
        v->duration = row.second;
        if (row.kind == JoRowKind_Rest) {
            v->note = JoNote_Silent;
            c->dma_off_next[v->channel] = true;
        } else {
            v->row_duration &= 0x80;
        }
    }
    v->pattern_position = (uint16_t)(v->pattern_position + (at - start));
    jo_pattern_advance(m, at, &v->pattern_position, &v->repeat, &v->track_position);
}

// The tempo accumulator carried and the row ran out: the track gives the next pattern
static void step_track(JoC* c, const JoModule* m, Voice* v) {
    uint32_t at;
    const JoFollow follow = jo_follow(m, &v->track_position, &at);
    if (follow == JoFollow_End) {
        v->track_position = 0;
        v->priority = 0;
        jo_voice_stopped(&c->end, &c->paula[v->channel], v->channel);
        return;
    }
    if (follow == JoFollow_Jumped) {
        jo_voice_passed(&c->end, v->channel);
    }
    const uint32_t start = at;
    at = jo_run_commands(m, at, v, store_command);
    v->track_position = (uint16_t)(v->track_position + (at - start));
    step_pattern(c, m, v, jom_offset16(jom_u16(m, at)));
}

// The sample pointer and length of a note: the one-shot part when it has just started, the loop part afterwards
static void write_sample(const JoModule* m, const Voice* v, const Instrument* instrument, NuadePaulaChannel* paula,
                         bool start) {
    if ((instrument->wave & WAVE_IS_SVX) != 0) {
        // an 8SVX file: the body is at `wave`, its header FORM_HEADER_SIZE bytes before it
        const uint32_t body = instrument->wave & ~WAVE_IS_SVX;
        const SvxHeader svx = read_svx_header(m, body);
        if (start) {
            jo_set_sample(m, paula, body, (uint16_t)(svx.one_shot + svx.repeat) >> 1);
        } else {
            jo_set_sample(m, paula, body + jom_offset16(svx.one_shot), svx.repeat >> 1);
        }
        return;
    }
    uint32_t block = 0;
    if (instrument->multi_wave) {
        block = jom_offset16((uint16_t)(2u * (uint16_t)(v->wave * instrument->length)));
    }
    if (start) {
        jo_set_sample(m, paula, instrument->wave + block, instrument->length);
    } else {
        jo_set_sample(m, paula, instrument->loop_start + block, instrument->loop_length);
    }
}

// The replayer's multiplication table: level a (0..63) times level b (0..63) over 63, 64 entries per level
static uint8_t table_product(uint16_t index) {
    return index < 64 * 64 ? (uint8_t)((index >> 6) * (index & 0x3f) / 63) : 0;
}

// The table offset of the row of a level: 0x40 per level. A level of 64 and above reads the next words of the
// replayer, which are the table itself.
static uint16_t level_row(uint8_t level) {
    const unsigned x = level & 0x7fu;
    if (x < 64) {
        return (uint16_t)(0x40 * x);
    }
    return (uint16_t)((table_product((uint16_t)(2 * (x - 64))) << 8) | table_product((uint16_t)(2 * (x - 64) + 1)));
}

// The replayer scales a level through the table with a byte add that wraps inside the row group
static uint8_t scaled(uint8_t a, uint8_t b) {
    const uint16_t row = level_row(a);
    return table_product((uint16_t)((row & 0xff00u) | (uint8_t)(row + b)));
}

// The voice plays on: sample, instrument programs, volume mix and period
static void run_voice(JoC* c, const JoModule* m, Voice* v) {
    NuadePaulaChannel* paula = &c->paula[v->channel];
    const Instrument instrument = voice_instrument(m, v);
    c->dma_on[v->channel] = true;
    const bool starting = v->new_note;
    if (starting) {
        v->new_note = false;
        v->pitch_a = 0;
        v->base_note = 0;
        v->fixed_pitch = false;
        v->row_duration = 0xff;
        if (instrument.reset_wave) {
            v->wave = 0;
        }
    }
    write_sample(m, v, &instrument, paula, starting);
    for (size_t i = 0; i < nuade_sizeof_array(v->programs); i++) {
        run_program(m, v, (int)i);
    }
    v->slide_sum = (uint16_t)(v->slide_sum + (int16_t)v->slide);

    uint8_t level = scaled(v->volume_a, v->volume_b);
    level = scaled(level, v->volume_c);
    level = scaled(level, c->master);
    level = scaled(level, instrument.volume);
    paula->volume = level;

    const uint8_t note = v->fixed_pitch ? v->base_note : (uint8_t)(v->base_note + v->note);
    const int pitch_a = v->no_pitch_a ? 0 : v->pitch_a;
    paula->period = (uint16_t)(k_periods[note & 0x7f] - v->pitch_b - pitch_a - v->slide_sum);
}

JoTickResult jo_c_tick(JoC* c, const JoModule* m, NuadeVoices* voices) {
    for (int i = 0; i < JO_VOICES; i++) {
        if (c->dma_off_next[i]) {
            nuade_paula_channel_dma_off(&c->paula[i]);
        }
        c->dma_off_next[i] = false;
        c->dma_on[i] = false;
    }
    for (int slot = 0; slot < JO_VOICES; slot++) {
        Voice* v = c->slots[slot];
        if (v == nullptr || v->track_position == 0) {
            continue;
        }
        if (jo_accumulate(&v->tempo, v->speed)) {
            v->duration = (uint8_t)((v->duration & 0x7f) - 1);
            if (v->duration == 0) {
                step_track(c, m, v);
                continue;
            }
        }
        if (v->note != JoNote_Silent) {
            run_voice(c, m, v);
        }
    }
    bool voices_play = false;
    for (int i = 0; i < JO_VOICES; i++) {
        if (c->dma_on[i]) {
            nuade_paula_channel_dma_on(&c->paula[i]);
        }
        nuade_paula_channel_sync(&c->paula[i], (uint32_t)i, voices);
        voices_play |= c->slots[i] != nullptr && c->slots[i]->track_position != 0;
    }
    return jo_song_end_result(&c->end, voices_play);
}

JoVoiceState jo_c_voice_state(const JoC* c, int voice) {
    const JoCVoice* v = c->slots[voice];
    return jo_shown_state(v != nullptr && v->started && v->note != JoNote_Silent, c->paula[voice].period,
                          v != nullptr ? v->instrument & 0xff : -1);
}

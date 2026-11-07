// SPDX-License-Identifier: MIT
// Jesper Olsen player, replayer B: the data modules of the Harald Hardtand games, translated from the replayer in the
// eagleplayer.
//
// A module starts with a long, then 0x00017fff, then the song records (0x1a bytes: the start of each voice's track,
// the tempo, a flags byte and the offsets of the four voice records in the module). A voice steps when the global
// tick counter and its own row counter run out: the track gives a pattern (with field commands in front), the pattern
// rows give notes (again with field commands) and an instrument record that holds three programs: sample blocks,
// pitch and volume. Every tick the programs advance and the voice writes volume, sample, period and DMA.

#include "jo_internal.h"
#include <string.h>

typedef JoBVoice Voice;

#define JO_B_RECORD_SIZE 0x7a

// The master volume of every voice. The replayer's own init takes it from the song record; the eagleplayer's InitSound
// overwrites it after that.
#define MASTER_VOLUME 0x20
#define TRACK_STEP_NOW 0x8000

// The replayer's period table. The last five entries are not periods: the table is followed by the replayer's code.
static const uint16_t k_periods[64] = {
    0x0d5c, 0x0c9c, 0x0be8, 0x0b3c, 0x0a9a, 0x0a02, 0x0972, 0x08ea, 0x086a, 0x07f2, 0x0780, 0x0714, 0x06ae,
    0x064e, 0x05f4, 0x059e, 0x054d, 0x0501, 0x04b9, 0x0475, 0x0435, 0x03f9, 0x03c0, 0x038a, 0x0357, 0x0327,
    0x02fa, 0x02cf, 0x02a7, 0x0281, 0x025d, 0x023b, 0x021b, 0x01fc, 0x01e0, 0x01c5, 0x01ac, 0x0194, 0x017d,
    0x0168, 0x0153, 0x0140, 0x012e, 0x011d, 0x010d, 0x00fe, 0x00f0, 0x00e2, 0x00d6, 0x00ca, 0x00be, 0x00b4,
    0x00aa, 0x00a0, 0x0097, 0x008f, 0x0087, 0x007f, 0x0078, 0x0000, 0x203c, 0x0000, 0x0400, 0x2f08,
};

// The voice record

enum {
    Record_Instrument = 0x00,
    Record_Note = 0x02,
    Record_Counter = 0x03,
    Record_TrackPosition = 0x04,
    Record_PatternPosition = 0x06,
    Record_SamplePosition = 0x08,
    Record_PitchPosition = 0x0a,
    Record_VolumePosition = 0x0c,
    Record_SlideSum = 0x0e,
    Record_PortamentoFlags = 0x31,
    Record_SlideSpeed = 0x41,
    Record_Volume = 0x50,
    Record_RowDuration = 0x53,
    Record_NoteOffset = 0x55,
    Record_Repeat = 0x57,
    Record_Master = 0x58,
    Record_TempoCommand = 0x5a,
    Record_TempoValue = 0x5b,
    Record_PortamentoPosition = 0x5e,
    Record_PitchFlags = 0x60,
    Record_PitchNote = 0x61,
    Record_PortamentoCount = 0x62,
    Record_PortamentoValue = 0x63,
    Record_VolumeLevel = 0x64,
    Record_PitchCounter = 0x66,
    Record_PortamentoWait = 0x67,
    Record_VolumeCounter = 0x68,
    Record_SampleCounter = 0x69,
    Record_Detune = 0x6a,
};

// Bits of the flag bytes of the record
enum {
    PortamentoFlag_NoWait = 0x01,
    PortamentoFlag_Off = 0x02,
    PitchFlag_NoPortamento = 0x10,
    PitchFlag_AddSlide = 0x20,
    PitchFlag_Absolute = 0x40,
    PitchFlag_Hold = 0x80,
};

static void set_pitch_flags(Voice* v, uint8_t flags) {
    v->pitch_no_portamento = (flags & PitchFlag_NoPortamento) != 0;
    v->pitch_add_slide = (flags & PitchFlag_AddSlide) != 0;
    v->pitch_absolute = (flags & PitchFlag_Absolute) != 0;
    v->pitch_hold = (flags & PitchFlag_Hold) != 0;
}

static void unpack_record(Voice* v, const uint8_t* record) {
    const uint16_t track = nuade_read_be16(record + Record_TrackPosition);
    const uint8_t portamento_flags = record[Record_PortamentoFlags];
    const uint8_t pitch_flags = record[Record_PitchFlags];
    v->instrument = nuade_read_be16(record + Record_Instrument);
    v->note = record[Record_Note];
    v->counter = record[Record_Counter];
    v->track_position = track & ~TRACK_STEP_NOW;
    v->step_now = (track & TRACK_STEP_NOW) != 0;
    v->pattern_position = nuade_read_be16(record + Record_PatternPosition);
    v->sample_position = nuade_read_be16(record + Record_SamplePosition);
    v->pitch_position = nuade_read_be16(record + Record_PitchPosition);
    v->volume_position = nuade_read_be16(record + Record_VolumePosition);
    v->slide_sum = nuade_read_be16(record + Record_SlideSum);
    v->portamento_no_wait = (portamento_flags & PortamentoFlag_NoWait) != 0;
    v->portamento_off = (portamento_flags & PortamentoFlag_Off) != 0;
    v->slide_speed = record[Record_SlideSpeed];
    v->volume = nuade_read_be16(record + Record_Volume);
    v->row_duration = record[Record_RowDuration];
    v->note_offset = record[Record_NoteOffset];
    v->repeat = (int8_t)record[Record_Repeat];
    v->master = nuade_read_be16(record + Record_Master);
    v->tempo_command = record[Record_TempoCommand];
    v->tempo_value = record[Record_TempoValue];
    v->portamento_position = nuade_read_be16(record + Record_PortamentoPosition);
    set_pitch_flags(v, pitch_flags);
    v->pitch_note = record[Record_PitchNote];
    v->portamento_count = (int8_t)record[Record_PortamentoCount];
    v->portamento_value = (int8_t)record[Record_PortamentoValue];
    v->volume_level = nuade_read_be16(record + Record_VolumeLevel);
    v->pitch_counter = (int8_t)record[Record_PitchCounter];
    v->portamento_wait = (int8_t)record[Record_PortamentoWait];
    v->volume_counter = (int8_t)record[Record_VolumeCounter];
    v->sample_counter = (int8_t)record[Record_SampleCounter];
    v->detune = nuade_read_be16(record + Record_Detune);
}

static void pack_record(const Voice* v, uint8_t* record) {
    memset(record, 0, JO_B_RECORD_SIZE);
    jor_put16(record, Record_Instrument, v->instrument);
    record[Record_Note] = v->note;
    record[Record_Counter] = v->counter;
    jor_put16(record, Record_TrackPosition, (uint16_t)(v->track_position | (v->step_now ? TRACK_STEP_NOW : 0)));
    jor_put16(record, Record_PatternPosition, v->pattern_position);
    jor_put16(record, Record_SamplePosition, v->sample_position);
    jor_put16(record, Record_PitchPosition, v->pitch_position);
    jor_put16(record, Record_VolumePosition, v->volume_position);
    jor_put16(record, Record_SlideSum, v->slide_sum);
    record[Record_PortamentoFlags]
        = (uint8_t)((v->portamento_no_wait ? PortamentoFlag_NoWait : 0) | (v->portamento_off ? PortamentoFlag_Off : 0));
    record[Record_SlideSpeed] = v->slide_speed;
    jor_put16(record, Record_Volume, v->volume);
    record[Record_RowDuration] = v->row_duration;
    record[Record_NoteOffset] = v->note_offset;
    record[Record_Repeat] = (uint8_t)v->repeat;
    jor_put16(record, Record_Master, v->master);
    record[Record_TempoCommand] = v->tempo_command;
    record[Record_TempoValue] = v->tempo_value;
    jor_put16(record, Record_PortamentoPosition, v->portamento_position);
    record[Record_PitchFlags]
        = (uint8_t)((v->pitch_no_portamento ? PitchFlag_NoPortamento : 0)
                    | (v->pitch_add_slide ? PitchFlag_AddSlide : 0) | (v->pitch_absolute ? PitchFlag_Absolute : 0)
                    | (v->pitch_hold ? PitchFlag_Hold : 0));
    record[Record_PitchNote] = v->pitch_note;
    record[Record_PortamentoCount] = (uint8_t)v->portamento_count;
    record[Record_PortamentoValue] = (uint8_t)v->portamento_value;
    jor_put16(record, Record_VolumeLevel, v->volume_level);
    record[Record_PitchCounter] = (uint8_t)v->pitch_counter;
    record[Record_PortamentoWait] = (uint8_t)v->portamento_wait;
    record[Record_VolumeCounter] = (uint8_t)v->volume_counter;
    record[Record_SampleCounter] = (uint8_t)v->sample_counter;
    jor_put16(record, Record_Detune, v->detune);
}

// The voice's track position as the record holds it: bit 15 is the step-now flag
static void set_track(Voice* v, uint16_t word) {
    v->track_position = word & ~TRACK_STEP_NOW;
    v->step_now = (word & TRACK_STEP_NOW) != 0;
}

static bool track_active(const Voice* v) {
    return v->track_position != 0 || v->step_now;
}

// A command stores its word (command byte, argument) into the record. Offsets past the record would land in the next
// record or the module; no song does that.
static void poke_command(Voice* v, uint32_t offset, uint16_t command) {
    if (offset + 1 >= JO_B_RECORD_SIZE) {
        return;
    }
    uint8_t record[JO_B_RECORD_SIZE];
    pack_record(v, record);
    jor_put16(record, offset, command);
    unpack_record(v, record);
}

// Track commands store at the record offset (command byte - 0x2c)
static void store_track_command(void* voice, uint16_t command) {
    poke_command(voice, (uint32_t)(command >> 8) - 0x2cu, command);
}

// Pattern commands store at the record offset (command byte & 0x70)
static void store_pattern_command(void* voice, uint16_t command) {
    poke_command(voice, (uint32_t)(command >> 8) & 0x70u, command);
}

// Song start

bool jo_b_detect(const JoModule* m) {
    return jom_u32(m, 4) == 0x00017fff;
}

// A song record: the voices' track starts (1 = the voice is not started, its record stays as the file has it), the
// tempo and the offsets of the voice records
typedef struct SongRecord {
    uint16_t tracks[JO_VOICES];
    uint8_t tempo;
    uint32_t records[JO_VOICES];
} SongRecord;

static SongRecord read_song(const JoModule* m, int subsong) {
    const uint32_t at = 8 + 0x1a * (uint32_t)subsong;
    return (SongRecord) { .tracks = { jom_u16(m, at), jom_u16(m, at + 2), jom_u16(m, at + 4), jom_u16(m, at + 6) },
                          .tempo = jom_u8(m, at + 8),
                          .records = { jom_u32(m, at + 0xa), jom_u32(m, at + 0xe), jom_u32(m, at + 0x12),
                                       jom_u32(m, at + 0x16) } };
}

void jo_b_start(JoB* b, const JoModule* m, int subsong) {
    *b = (JoB) { .counter = 1, .high_nibble = true };
    const SongRecord song = read_song(m, subsong);
    b->tempo = song.tempo;
    bool has_track[JO_VOICES];
    for (int i = 0; i < JO_VOICES; i++) {
        Voice* v = &b->voices[i];
        uint8_t record[JO_B_RECORD_SIZE];
        jom_record(m, song.records[i], record, JO_B_RECORD_SIZE);
        unpack_record(v, record);
        v->channel = (uint8_t)(JO_VOICES - 1 - i);
        if (song.tracks[i] != 1) {
            *v = (Voice) { .channel = v->channel, .counter = 2 };
            set_track(v, song.tracks[i]);
            nuade_paula_channel_dma_off(&b->paula[v->channel]);
        }
        v->master = MASTER_VOLUME;
        has_track[v->channel] = song.tracks[i] > 1;
    }
    jo_song_end_start(&b->end, has_track);
}

// Tick

// The instrument record the voice plays: three program starts, the portamento wait and the volume
typedef struct Instrument {
    uint16_t pitch_position;
    uint16_t volume_position;
    uint16_t portamento_position;
    uint8_t portamento_wait;
    uint8_t volume;
} Instrument;

static Instrument read_instrument(const JoModule* m, uint32_t offset) {
    return (Instrument) { .pitch_position = jom_u16(m, offset),
                          .volume_position = jom_u16(m, offset + 2),
                          .portamento_position = jom_u16(m, offset + 4),
                          .portamento_wait = jom_u8(m, offset + 6),
                          .volume = jom_u8(m, offset + 7) };
}

// A row starts the instrument: the programs begin at the start of the instrument's data
static void start_instrument(Voice* v, const JoModule* m) {
    const Instrument instrument = read_instrument(m, jom_offset16(v->instrument));
    v->sample_position = (uint16_t)(v->instrument + 8);
    v->volume_position = instrument.volume_position;
    v->pitch_position = instrument.pitch_position;
    v->portamento_count = 0;
    v->portamento_value = 0;
    v->volume_counter = 0;
    v->sample_counter = 0;
    v->row_duration = 0xff;
    v->portamento_position = instrument.portamento_position;
    v->portamento_wait = (int8_t)instrument.portamento_wait;
}

// The row of the pattern the voice plays: field commands, then a note row, a rest row or a hold row
static void step_pattern(JoB* b, const JoModule* m, Voice* v, uint32_t pattern) {
    NuadePaulaChannel* paula = &b->paula[v->channel];
    const uint32_t start = pattern + jom_offset16(v->pattern_position);
    v->volume = 0x10;
    v->slide_speed = 0;
    uint32_t at = jo_run_commands(m, start, v, store_pattern_command);
    v->instrument &= 0x7fff;
    v->volume &= 0x1f;
    v->master &= 0x2f;
    const JoRow row = jo_read_row(m, at);
    at += 2;
    if (row.kind == JoRowKind_Note) {
        v->slide_sum = 0;
        if (!row.tied) {
            nuade_paula_channel_dma_off(paula);
            start_instrument(v, m);
        }
        v->note = row.first;
        v->counter = row.second;
        v->started = true;
    } else {
        v->row_duration = row.second;
        v->counter = row.second;
        if (row.kind == JoRowKind_Rest) {
            v->note = JoNote_Silent;
            nuade_paula_channel_dma_off(paula);
        } else {
            v->row_duration &= 0x80;
        }
    }
    v->pattern_position = (uint16_t)(v->pattern_position + (at - start));
    jo_pattern_advance(m, at, &v->pattern_position, &v->repeat, &v->track_position);
}

// The row ran out: the track gives the next pattern
static void step_track(JoB* b, const JoModule* m, Voice* v) {
    uint32_t at;
    uint16_t position = v->track_position;
    const JoFollow follow = jo_follow(m, &position, &at);
    if (follow == JoFollow_End) {
        v->track_position = 0;
        v->step_now = false;
        jo_voice_stopped(&b->end, &b->paula[v->channel], v->channel);
        return;
    }
    if (follow == JoFollow_Jumped) {
        set_track(v, position);
        jo_voice_passed(&b->end, v->channel);
    }
    const uint32_t start = at;
    at = jo_run_commands(m, at, v, store_track_command);
    v->track_position = (uint16_t)(v->track_position + (at - start));
    step_pattern(b, m, v, jom_offset16(jom_u16(m, at)));
}

// Between rows

// An entry of the portamento program: how many ticks the value is added, and the value
typedef struct PortamentoEntry {
    int8_t count; // negative: the entry holds while a note plays
    int8_t value;
} PortamentoEntry;

static PortamentoEntry read_portamento(const JoModule* m, uint32_t at) {
    const JoPair pair = jo_read_pair(m, at);
    return (PortamentoEntry) { .count = (int8_t)pair.first, .value = (int8_t)pair.second };
}

// The pitch slide of the portamento program: the signed value added to the period this tick
static int portamento(const JoModule* m, Voice* v) {
    if (v->slide_speed != 0) {
        v->slide_sum = (uint16_t)(v->slide_sum + (int8_t)v->slide_speed);
        return 0;
    }
    if (v->portamento_position == 0 || v->portamento_off) {
        return 0;
    }
    if (!v->portamento_no_wait) {
        if (!jo_counter_expired(&v->portamento_wait)) {
            return 0;
        }
        v->portamento_wait = 0;
    }
    v->portamento_count &= 0x0f;
    v->portamento_value = 0;
    if (jo_counter_expired(&v->portamento_count)) {
        uint32_t at;
        if (jo_follow(m, &v->portamento_position, &at) != JoFollow_End) {
            const PortamentoEntry entry = read_portamento(m, at);
            v->portamento_count = entry.count;
            v->portamento_value = entry.value;
            if (entry.count < 0 && v->row_duration != 0) {
                return 0;
            }
        }
    }
    v->portamento_position += 2;
    return v->portamento_value;
}

// An entry of the volume program: a level held for a number of ticks
typedef struct VolumeEntry {
    int8_t wait;
    bool hold; // the entry holds while a note plays
    uint8_t level;
} VolumeEntry;

static VolumeEntry read_volume_entry(const JoModule* m, uint32_t at) {
    const JoPair pair = jo_read_pair(m, at);
    return (
        VolumeEntry) { .wait = (int8_t)(pair.first & 0x7f), .hold = (pair.first & 0x80) != 0, .level = pair.second };
}

// The volume program: a level per step. Returns the mixed volume.
static uint8_t mix_volume(const JoModule* m, Voice* v, const Instrument* instrument) {
    uint16_t level = instrument->volume & 0x3f;
    if (v->volume_position != 0) {
        uint32_t at;
        if (jo_counter_expired(&v->volume_counter) && jo_follow(m, &v->volume_position, &at) != JoFollow_End) {
            const VolumeEntry entry = read_volume_entry(m, at);
            v->volume_level = (uint16_t)((v->volume_level & 0xff00) | entry.level);
            v->volume_counter = entry.wait;
            if (!entry.hold || v->row_duration == 0) {
                v->volume_position += 2;
            }
        }
        level = (uint16_t)(level * v->volume_level) >> 5;
    }
    level = (uint16_t)((uint16_t)((int16_t)level * (int16_t)v->master) >> 5);
    level = (uint16_t)((uint16_t)(level * v->volume) >> 4);
    return (uint8_t)level;
}

// An entry of the sample program: a block of the instrument's sample
typedef struct SampleBlock {
    uint32_t start;
    uint16_t length; // words
    int8_t wait;     // ticks to the next step
    bool hold;       // the program stays on this block while a note plays
} SampleBlock;

static SampleBlock read_block(const JoModule* m, uint32_t at) {
    const uint32_t head = jom_u32(m, at);
    return (SampleBlock) { .start = head & 0xffffff,
                           .length = jom_u16(m, at + 4),
                           .wait = (int8_t)((head >> 24) & 0x0f),
                           .hold = (head & 0x80000000u) != 0 };
}

// The sample program: blocks of the instrument's sample, one per step
static void step_samples(const JoModule* m, Voice* v, NuadePaulaChannel* paula) {
    uint32_t at;
    if (!jo_counter_expired(&v->sample_counter) || jo_follow(m, &v->sample_position, &at) == JoFollow_End) {
        return;
    }
    const SampleBlock block = read_block(m, at);
    jo_set_sample(m, paula, block.start, block.length);
    v->sample_counter = block.wait;
    if (!block.hold || v->row_duration == 0) {
        v->sample_position += 6;
    }
}

// An entry of the pitch program: flags, a note, and the ticks to the next step
typedef struct PitchEntry {
    int8_t wait;
    uint8_t flags;
    uint8_t note;
} PitchEntry;

static PitchEntry read_pitch_entry(const JoModule* m, uint32_t at) {
    const JoPair pair = jo_read_pair(m, at);
    return (PitchEntry) { .wait = (int8_t)(pair.first & 0x0f), .flags = pair.first, .note = pair.second };
}

// The pitch program: a flags byte and a note per step
static void step_pitch(const JoModule* m, Voice* v) {
    if (v->pitch_position == 0) {
        set_pitch_flags(v, 0);
        v->pitch_note = 0;
        return;
    }
    uint32_t at;
    if (!jo_counter_expired(&v->pitch_counter) || jo_follow(m, &v->pitch_position, &at) == JoFollow_End) {
        return;
    }
    const PitchEntry entry = read_pitch_entry(m, at);
    v->pitch_counter = entry.wait;
    set_pitch_flags(v, entry.flags);
    v->pitch_note = entry.note;
    if (!v->pitch_hold || v->row_duration == 0) {
        v->pitch_position += 2;
    }
}

// The voice plays on within its row: the programs advance and the registers are written
static void play_voice(JoB* b, const JoModule* m, Voice* v) {
    NuadePaulaChannel* paula = &b->paula[v->channel];
    if (v->note == JoNote_Silent) {
        return;
    }
    if (v->tempo_command == 0x86) {
        b->tempo = v->tempo_value;
    }
    const Instrument instrument = read_instrument(m, jom_offset16(v->instrument));
    int slide = portamento(m, v);
    paula->volume = mix_volume(m, v, &instrument);
    step_samples(m, v, paula);
    step_pitch(m, v);
    if (v->pitch_no_portamento) {
        slide = 0;
    }
    if (v->pitch_add_slide) {
        slide = (int16_t)(slide + (int16_t)v->slide_sum);
    }
    uint8_t note = v->pitch_note;
    if (!v->pitch_absolute) {
        note = (uint8_t)(note + v->note + v->note_offset);
    }
    paula->period = (uint16_t)(k_periods[note & 0x3f] - v->slide_sum - slide - v->detune);
    nuade_paula_channel_dma_on(paula);
}

JoTickResult jo_b_tick(JoB* b, const JoModule* m, NuadeVoices* voices) {
    if (jo_counter_expired(&b->counter)) {
        b->counter = (int8_t)(b->high_nibble ? b->tempo >> 4 : b->tempo & 0x0f);
        b->high_nibble = !b->high_nibble;
    }
    for (int i = 0; i < JO_VOICES; i++) {
        Voice* v = &b->voices[i];
        if (!track_active(v)) {
            continue;
        }
        if (!v->step_now && b->counter != 0) {
            play_voice(b, m, v);
            continue;
        }
        v->counter = (uint8_t)((v->counter & 0x7f) - 1);
        if (v->counter != 0) {
            play_voice(b, m, v);
            continue;
        }
        step_track(b, m, v);
    }
    bool voices_play = false;
    for (int i = 0; i < JO_VOICES; i++) {
        nuade_paula_channel_sync(&b->paula[i], (uint32_t)i, voices);
        voices_play |= track_active(&b->voices[i]);
    }
    return jo_song_end_result(&b->end, voices_play);
}

JoVoiceState jo_b_voice_state(const JoB* b, int voice) {
    const JoBVoice* v = &b->voices[JO_VOICES - 1 - voice];
    return jo_shown_state(v->started && v->note != JoNote_Silent, b->paula[voice].period, v->instrument);
}

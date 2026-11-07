// SPDX-License-Identifier: MIT
// Jesper Olsen player, replayers A1 and A2: the replayers in the modules of the Guldkornsexpressen title music (A1) and
// game music (A2), translated from their 68k code.
//
// A module starts with branches to the replayer's routines (A1: four, A2: eight), then the replayer's variables, code
// and tables, then the song data. A tick counts the tempo down; when the row of a voice has run out at a tempo step,
// the voice reads its next row: the track gives a pattern, the pattern words give rests, sample changes, row lengths,
// effects and a note. Between rows a voice runs four programs (sample blocks, note, volume and pitch), a portamento and
// a slide, and writes its period and volume. A2 is the later version of the same design; what differs is in the
// version record (table places, sizes) and in track_commands, read_row, portamento_reached and the volume entry.

#include "jo_internal.h"

typedef JoAVoice Voice;

#define SAMPLE_RECORD_SIZE 0x14

static const JoAVersion k_versions[] = {
    { .kind = JoKind_A1,
      .signature = 0x5300d9c1u,
      .periods = 0x6a8,
      .sample_entries = 0x768,
      .samples = 0x778,
      .silence = 0x800,
      .songs = 0x812,
      .song_size = 0xe,
      .patterns = 0x90a,
      .first_entry = 0,
      .level_mask = 0xff,
      .counter_bias = -1,
      .signed_offsets = true },
    { .kind = JoKind_A2,
      .signature = 0xb3030e37u,
      .periods = 0x878,
      .samples = (uint32_t)-0x56c8,
      .silence = 0x1158,
      .songs = 0x1422,
      .song_size = 0x10,
      .patterns = 0x191c,
      .first_entry = 0x6000,
      .level_mask = 0x0f,
      .counter_bias = 0,
      .signed_offsets = false },
};

// Words and bytes of a row: each is recognised by its value and consumes its argument
enum {
    Row_Rest = 0x7001,          // and above: a rest of that many ticks
    Row_Sample = 0x6000,        // and above: a sample number
    Row_SampleProgram = 0x5003, // A1: followed by an offset added to the position of the sample program
    Row_VolumeProgram = 0x5002, // ... the volume program
    Row_NoteProgram = 0x5001,   // ... the note program
    Row_Reload = 0x4001,        // and above (also from 0x3001): the length of the row
    Row_Reload2 = 0x3001,
    Row_Effect = 0x2000,        // and above: effect flags, portamento note and slide speed
    Row_Pitch = 0x1000,         // and above: pitch flags
    Row_Master = 2,             // A1, as a byte: the master volume
    Row_Volume = 1,             // as a byte: the instrument volume
    Track_TableBase = 0xe000,   // A1, in a track entry: and above: added to the sample numbers of the pattern
    Reload_KeepSample = 0x4000, // a row length from here keeps the sample of the previous row
    Note_Fixed = 0x80,          // a note byte with bit 7 set is not transposed
    Pattern_EndMark = 0x7f,     // the second byte of the word after the last row of a pattern
};

// The sample record

typedef struct Sample {
    uint32_t start;
    uint16_t length;                    // words
    uint16_t programs[JoProgram_Count]; // start positions of the sample, note, volume and pitch programs
    uint8_t slide_limit;                // the pitch program starts after this many steps
    uint8_t sample_wait;                // ticks between steps of the sample program
    uint8_t note_wait;                  // ... the note program
    uint8_t volume_wait;                // ... the volume program
    uint8_t pitch_wait;                 // ... the pitch program
    uint8_t volume_hold;                // A1: a volume entry that holds lets go when the row counter is below this
    uint8_t note_offset;                // A2: added to every note (the same byte)
    uint8_t volume;
    uint8_t level;
} Sample;

static Sample read_sample(const JoModule* m, uint32_t offset) {
    const uint8_t waits_a = jom_u8(m, offset + 0xf);
    const uint8_t waits_b = jom_u8(m, offset + 0x10);
    return (Sample) { .start = jom_u32(m, offset),
                      .length = jom_u16(m, offset + 4),
                      .programs = { jom_u16(m, offset + 6), jom_u16(m, offset + 8), jom_u16(m, offset + 0xa),
                                    jom_u16(m, offset + 0xc) },
                      .slide_limit = jom_u8(m, offset + 0xe),
                      .sample_wait = waits_a >> 4,
                      .note_wait = waits_a & 0x0f,
                      .volume_wait = waits_b >> 4,
                      .pitch_wait = waits_b & 0x0f,
                      .volume_hold = jom_u8(m, offset + 0x11),
                      .note_offset = jom_u8(m, offset + 0x11),
                      .volume = jom_u8(m, offset + 0x12),
                      .level = jom_u8(m, offset + 0x13) };
}

static uint32_t sample_offset(const JoAVersion* version, const Voice* v) {
    return version->samples + jom_offset16(v->sample_entry);
}

static uint16_t period_at(const JoAVersion* version, const JoModule* m, uint32_t index) {
    return jom_u16(m, version->periods + 2 * index);
}

const JoAVersion* jo_a_detect(const JoModule* m) {
    if (jom_u16(m, 0) != 0x6000) {
        return nullptr;
    }
    for (size_t i = 0; i < nuade_sizeof_array(k_versions); i++) {
        if (jom_hash(m, 0, k_versions[i].periods) == k_versions[i].signature) {
            return &k_versions[i];
        }
    }
    return nullptr;
}

// The eagleplayer counts the song records between the table and the first track list
int jo_a_subsong_count(const JoAVersion* version, const JoModule* m) {
    const int count = (int)((jom_u16(m, version->songs) - version->songs) / version->song_size);
    return count > 0 ? count : 1;
}

// A record of the song table
typedef struct SongRecord {
    uint16_t tracks[JO_VOICES]; // the first track entry of each voice; 0 = no track
    uint8_t tempo;
    uint8_t voices; // the voices that play (low nibble)
    uint8_t master;
} SongRecord;

static SongRecord read_song(const JoAVersion* version, const JoModule* m, int subsong) {
    const uint32_t at = version->songs + version->song_size * (uint32_t)subsong;
    return (SongRecord) { .tracks = { jom_u16(m, at), jom_u16(m, at + 2), jom_u16(m, at + 4), jom_u16(m, at + 6) },
                          .tempo = jom_u8(m, at + 8),
                          .voices = jom_u8(m, at + 9) & 0x0f,
                          .master = jom_u8(m, at + 0xa) };
}

// The tempo byte: A1 alternates between its two nibbles, A2 uses the low one
static void set_tempo(JoA* a, uint8_t tempo) {
    a->tempo[0] = a->version->kind == JoKind_A1 ? tempo >> 4 : tempo & 0x0f;
    a->tempo[1] = tempo & 0x0f;
}

void jo_a_start(JoA* a, const JoAVersion* version, const JoModule* m, int subsong) {
    *a = (JoA) { .version = version };
    const SongRecord song = read_song(version, m, subsong);
    set_tempo(a, song.tempo);
    // A2 songs with a negative master volume only set tempo and master: the song continues in the state the module
    // file holds for its work area, which is clear
    if (version->kind == JoKind_A2 && (song.master & 0x80) != 0) {
        a->master = song.master & 0x0f;
        return;
    }
    a->master = song.master;
    bool has_track[JO_VOICES];
    for (int i = 0; i < JO_VOICES; i++) {
        Voice* v = &a->voices[i];
        v->track_position = song.tracks[i];
        v->transpose = version->kind == JoKind_A1 ? 0x0b : 0;
        v->counter = version->kind == JoKind_A2 ? 1 : 0;
        v->sample_entry = version->first_entry;
        a->active[i] = (song.voices & (1u << i)) != 0;
        has_track[i] = v->track_position != 0;
        if (version->kind == JoKind_A2) {
            jo_set_sample(m, &a->paula[i], version->silence, 1);
        }
        nuade_paula_channel_dma_off(&a->paula[i]);
    }
    jo_song_end_start(&a->end, has_track);
}

// Row fetch

// The field commands in front of the pattern of an A1 track entry, in this order: repeat count, transpose, master
// volume (high byte), tempo, sample table base. Returns the position after them and the value the transpose command
// gave (the replayer adds it to this tick's period).
static uint32_t track_commands_a1(JoA* a, const JoModule* m, Voice* v, uint32_t at, uint16_t* adjust) {
    if (jom_u8(m, at) == 0xff) {
        v->repeat = (int8_t)jom_u8(m, at + 1);
        at += 2;
    }
    if (jom_u8(m, at) == 0xfe) {
        v->transpose = (uint8_t)(jom_u8(m, at + 1) + 0x0b);
        *adjust = v->transpose;
        at += 2;
    }
    if (jom_u8(m, at) == 0xfd) {
        a->master_high = jom_u8(m, at + 1);
        at += 2;
    }
    if (jom_u8(m, at) == 0xfc) {
        set_tempo(a, jom_u8(m, at + 1));
        at += 2;
    }
    if (jom_u16(m, at) >= Track_TableBase) {
        v->table_base = jom_u16(m, at);
        at += 2;
    }
    return at;
}

// The field commands in front of the pattern of an A2 track entry, in this order: global transpose, voice transpose,
// master volume, tempo. Returns the position after them.
static uint32_t track_commands_a2(JoA* a, const JoModule* m, Voice* v, uint32_t at) {
    if (jom_u8(m, at) == 0xfe) {
        a->transpose = jom_u8(m, at + 1);
        at += 2;
    }
    if (jom_u8(m, at) == 0xfd) {
        v->transpose = jom_u8(m, at + 1);
        at += 2;
    }
    if (jom_u8(m, at) == 0xfc) {
        a->master = jom_u8(m, at + 1);
        at += 2;
    }
    if (jom_u8(m, at) == 0xfb) {
        set_tempo(a, jom_u8(m, at + 1));
        at += 2;
    }
    return at;
}

// The effect word of a row: flags, the portamento value and the slide speed
static void set_effect(Voice* v, JoKind kind, uint8_t flags) {
    if (kind == JoKind_A1) {
        v->no_pitch_program = (flags & 0x01) != 0;
        v->keep_slide_speed = (flags & 0x02) != 0;
        v->keep_slide_sum = (flags & 0x04) != 0;
    } else {
        v->no_pitch_program = (flags & 0x01) != 0;
        v->keep_slide_speed = (flags & 0x01) != 0;
        v->keep_slide_sum = (flags & 0x02) != 0;
    }
}

// The sample number of a row: A1 maps it through a table, A2 uses it as it is
static uint16_t row_sample_entry(const JoAVersion* version, const JoModule* m, const Voice* v, uint16_t number) {
    if (version->kind == JoKind_A2) {
        return number;
    }
    const uint32_t index = (uint32_t)((uint16_t)(number + v->table_base) & 0xfffu) * 2;
    return jom_u16(m, version->sample_entries + index);
}

// The program offsets of an A1 row: three words, each behind its own marker
static uint32_t program_offsets(const JoModule* m, Voice* v, uint32_t at) {
    static const struct {
        uint16_t marker;
        int program;
    } k_offsets[] = { { Row_SampleProgram, JoProgram_Sample },
                      { Row_VolumeProgram, JoProgram_Volume },
                      { Row_NoteProgram, JoProgram_Note } };
    for (size_t i = 0; i < nuade_sizeof_array(k_offsets); i++) {
        if (jom_u16(m, at) == k_offsets[i].marker) {
            v->offsets[k_offsets[i].program] = jom_u16(m, at + 2);
            at += 4;
        }
    }
    return at;
}

// The words of one row of the pattern: a rest, or sample change, offsets, row length, effects and the note.
// Returns the position after the row.
static uint32_t read_row(JoA* a, const JoModule* m, Voice* v, uint32_t at, NuadePaulaChannel* paula) {
    const JoAVersion* version = a->version;
    const bool a1 = version->kind == JoKind_A1;
    v->rest = false;
    if (jom_u16(m, at) >= Row_Rest) {
        v->counter = (int16_t)jom_u16(m, at);
        jo_set_sample(m, paula, version->silence, 1);
        v->rest = true;
        return at + 2;
    }
    if (jom_u16(m, at) >= Row_Sample) {
        v->sample_entry = row_sample_entry(version, m, v, jom_u16(m, at));
        at += 2;
    }
    const Sample sample = read_sample(m, sample_offset(version, v));
    if (v->reload < Reload_KeepSample) {
        jo_set_sample(m, paula, sample.start, sample.length);
        v->instrument_volume = sample.volume;
        v->volume_level = sample.level & version->level_mask;
        v->step = 0;
    }
    if (a1) {
        at = program_offsets(m, v, at);
    }
    if (jom_u16(m, at) >= Row_Reload) {
        v->reload = jom_u16(m, at);
        at += 2;
    }
    if (jom_u16(m, at) >= Row_Reload2) {
        v->reload = jom_u16(m, at);
        at += 2;
    }
    if (!v->keep_slide_speed) {
        v->slide_speed = 0;
    }
    if (!v->keep_slide_sum) {
        v->slide_position = 0;
        if (!a1) {
            v->portamento = 0;
        }
    }
    if (jom_u16(m, at) >= Row_Effect) {
        set_effect(v, version->kind, jom_u8(m, at));
        v->portamento = jom_u8(m, at + 1);
        v->slide_speed = jom_u16(m, at + 2);
        at += 4;
    }
    if (jom_u16(m, at) >= Row_Pitch) {
        const uint8_t flags = jom_u8(m, at + 1);
        v->pitch_ignores_step = (flags & 0x01) != 0;
        v->pitch_off = (flags & 0x02) != 0;
        at += 2;
    }
    if (a1 && jom_u8(m, at) == Row_Master) {
        a->master = jom_u8(m, at + 1);
        at += 2;
    }
    if (jom_u8(m, at) == Row_Volume) {
        v->instrument_volume = jom_u8(m, at + 1);
        at += 2;
    }
    uint8_t note = jom_u8(m, at + 1);
    if ((note & Note_Fixed) == 0) {
        note = (uint8_t)(note + v->transpose + (a1 ? 0 : a->transpose));
    }
    if (!a1) {
        note = (uint8_t)(note + sample.note_offset);
    }
    v->note = note & 0x7f;
    v->played_note = v->note;
    v->started = true;
    v->counter = (int16_t)v->reload;
    return at + 2;
}

// What a fetch gives the tick
typedef struct Fetch {
    bool ended;      // the track ended: the voice stops
    uint16_t adjust; // added to the period of this tick
} Fetch;

// The row of a voice has run out: the track gives the pattern, the pattern the next row
static Fetch fetch(JoA* a, const JoModule* m, Voice* v, int index, NuadePaulaChannel* paula) {
    const JoAVersion* version = a->version;
    const bool a1 = version->kind == JoKind_A1;
    Fetch result = { 0 };
    uint32_t at = v->track_position;
    if (jom_u16(m, at) == JoWord_End) {
        a->active[index] = false;
        result.ended = true;
        if (!a1) {
            jo_voice_stopped(&a->end, paula, index);
        }
        return result;
    }
    if (jom_u16(m, at) == JoWord_Jump) {
        v->track_position = jom_u16(m, at + 2);
        at = version->signed_offsets ? jom_offset16(v->track_position) : v->track_position;
        jo_voice_passed(&a->end, index);
    }
    const uint32_t start = at;
    at = a1 ? track_commands_a1(a, m, v, at, &result.adjust) : track_commands_a2(a, m, v, at);
    v->track_position = (uint16_t)(v->track_position + (at - start));
    const uint32_t pattern_offset = version->signed_offsets ? jom_offset16(jom_u16(m, at)) : jom_u16(m, at);
    const uint32_t pattern = version->patterns + pattern_offset + v->pattern_position;
    const uint32_t row_end = read_row(a, m, v, pattern, paula);
    v->counter = (int16_t)((v->counter & 0xfff) + version->counter_bias);
    a->dma_on[index] = true;
    v->pattern_position = (uint16_t)(v->pattern_position + (row_end - pattern));
    if (jom_u8(m, row_end + 1) == Pattern_EndMark) {
        v->pattern_position = 0;
        if (!a1 || jo_counter_expired(&v->repeat)) {
            v->repeat = 0;
            v->track_position += 2;
        }
    }
    return result;
}

// Between rows

// Pitch changes of one tick: the slide moves the period, the pitch program offsets it
typedef struct Pitch {
    uint16_t slide;
    uint16_t offset;
} Pitch;

// The portamento moves the note towards a target note; reaching it ends the slide
static bool portamento_reached(const JoAVersion* version, const JoModule* m, Voice* v) {
    if (version->kind == JoKind_A1) {
        const uint8_t target = (uint8_t)(v->portamento + v->transpose);
        const bool below = (int8_t)(target - v->note) < 0;
        const bool lower = period_at(version, m, target) < v->last_period;
        if (below != lower) {
            return false;
        }
        v->note = target;
        v->played_note = target;
        v->portamento = 0xff;
    } else {
        const uint8_t target = (uint8_t)(v->portamento + v->note);
        const int16_t distance = (int16_t)(period_at(version, m, target & 0x7fu) - v->last_period);
        const bool reached = (v->portamento & 0x80) != 0 ? distance < 0 : distance >= 0;
        if (!reached) {
            return false;
        }
        v->note = target & 0x7f;
        v->played_note = v->note;
        v->portamento = 0;
    }
    v->no_pitch_program = false;
    v->keep_slide_speed = false;
    v->keep_slide_sum = false;
    v->slide_position = 0;
    v->slide_speed = 0;
    return true;
}

static Pitch slide_and_pitch(const JoAVersion* version, const JoModule* m, Voice* v, const Sample* sample) {
    Pitch pitch = { 0 };
    const bool has_portamento = version->kind == JoKind_A1 ? (v->portamento & 0x80) == 0 : v->portamento != 0;
    if (has_portamento && portamento_reached(version, m, v)) {
        return pitch;
    }
    v->slide_position = (uint16_t)(v->slide_position - v->slide_speed);
    pitch.slide = v->slide_position;
    // The original's bpl on the 16-bit word difference limit - step
    const bool waits = !v->pitch_ignores_step && (int16_t)(sample->slide_limit - v->step) >= 0;
    if (v->pitch_off || v->no_pitch_program || waits) {
        return pitch;
    }
    uint32_t at;
    uint16_t* position = &v->programs[JoProgram_Pitch];
    if (jo_follow(m, position, &at) == JoFollow_End) {
        return pitch;
    }
    pitch.offset = jom_u16(m, at);
    if (v->counters[JoProgram_Pitch] == sample->pitch_wait) {
        v->counters[JoProgram_Pitch] = 0;
        *position += 2;
    } else {
        v->counters[JoProgram_Pitch]++;
    }
    return pitch;
}

// An entry of the sample program: a block of the sample
typedef struct SampleBlock {
    uint32_t start;
    uint16_t length; // words
} SampleBlock;

static SampleBlock read_block(const JoModule* m, uint32_t at) {
    return (SampleBlock) { .start = jom_u32(m, at), .length = jom_u16(m, at + 4) };
}

// Whether the volume program moves on after the entry `volume`: some entries hold until the row is nearly over
static bool volume_entry_moves_on(JoKind kind, const Voice* v, const Sample* sample, JoPair volume) {
    if (kind == JoKind_A1) {
        return (volume.first & 1) == 0 || sample->volume_hold >= (uint16_t)v->counter;
    }
    if ((volume.first & 0x80) == 0) {
        return true;
    }
    return v->reload < Reload_KeepSample && (volume.first & 0x7f) >= (uint16_t)v->counter;
}

// The volume, note and sample programs: each steps when its counter runs out
static void run_programs(const JoAVersion* version, const JoModule* m, Voice* v, const Sample* sample,
                         NuadePaulaChannel* paula) {
    uint32_t at;
    if (jo_counter_expired(&v->counters[JoProgram_Volume])) {
        v->counters[JoProgram_Volume] = (int8_t)sample->volume_wait;
        if (jo_follow(m, &v->programs[JoProgram_Volume], &at) != JoFollow_End) {
            const JoPair entry = jo_read_pair(m, at);
            v->volume_level = entry.second;
            if (volume_entry_moves_on(version->kind, v, sample, entry)) {
                v->programs[JoProgram_Volume] += 2;
            }
        }
    }
    if (jo_counter_expired(&v->counters[JoProgram_Note])) {
        v->counters[JoProgram_Note] = (int8_t)sample->note_wait;
        if (jo_follow(m, &v->programs[JoProgram_Note], &at) != JoFollow_End) {
            v->played_note = (uint8_t)(v->note + jom_u8(m, at + 1));
            v->programs[JoProgram_Note] += 2;
        }
    }
    if (jo_counter_expired(&v->counters[JoProgram_Sample])) {
        v->counters[JoProgram_Sample] = (int8_t)sample->sample_wait;
        if (jo_follow(m, &v->programs[JoProgram_Sample], &at) != JoFollow_End) {
            const SampleBlock block = read_block(m, at);
            jo_set_sample(m, paula, block.start, block.length);
            v->programs[JoProgram_Sample] += 6;
        }
    }
    v->step++;
}

// The first tick of a note loads the program positions of the sample record; A1 adds the offsets of the row, a carry
// of the note program position runs into the sample program position
static void start_programs(Voice* v, const Sample* sample) {
    const uint32_t note = (uint32_t)sample->programs[JoProgram_Note] + v->offsets[JoProgram_Note];
    v->programs[JoProgram_Sample]
        = (uint16_t)(sample->programs[JoProgram_Sample] + v->offsets[JoProgram_Sample] + (note >> 16));
    v->programs[JoProgram_Note] = (uint16_t)note;
    v->programs[JoProgram_Volume] = (uint16_t)(sample->programs[JoProgram_Volume] + v->offsets[JoProgram_Volume]);
    v->programs[JoProgram_Pitch] = sample->programs[JoProgram_Pitch];
    for (size_t i = 0; i < nuade_sizeof_array(v->counters); i++) {
        v->counters[i] = 0;
    }
}

// Registers

// The period and volume registers of a voice
static void write_registers(const JoA* a, const JoModule* m, Voice* v, NuadePaulaChannel* paula, uint16_t slide,
                            uint16_t offset) {
    const JoAVersion* version = a->version;
    const uint16_t period = (uint16_t)(period_at(version, m, v->played_note & 0x7fu) + slide);
    paula->period = (uint16_t)(period - offset);
    v->last_period = version->kind == JoKind_A1 ? period : paula->period;
    const uint16_t master = (uint16_t)((a->master_high << 8) | a->master);
    const uint16_t level = (uint16_t)((uint16_t)(v->instrument_volume * v->volume_level) >> 4);
    paula->volume = (uint8_t)((uint16_t)(level * master) >> 4);
}

static void run_voice(JoA* a, const JoModule* m, int index) {
    const JoAVersion* version = a->version;
    Voice* v = &a->voices[index];
    NuadePaulaChannel* paula = &a->paula[index];
    if (!a->active[index]) {
        return;
    }
    if (a->counter == 0) {
        v->counter--;
        if (version->kind == JoKind_A1 ? v->counter < 0 : v->counter == 0) {
            const Fetch f = fetch(a, m, v, index, paula);
            // A2 leaves the period and volume alone on a rest row
            if (!f.ended && (version->kind == JoKind_A1 || !v->rest)) {
                write_registers(a, m, v, paula, f.adjust, 0);
            }
            return;
        }
    } else if (v->reload < Reload_KeepSample && v->counter == 1 + version->counter_bias) {
        a->dma_off[index] = true;
    }
    if (v->rest) {
        return;
    }
    const Sample sample = read_sample(m, sample_offset(version, v));
    Pitch pitch = { 0 };
    if (v->step == 0) {
        start_programs(v, &sample);
    } else {
        pitch = slide_and_pitch(version, m, v, &sample);
    }
    run_programs(version, m, v, &sample, paula);
    write_registers(a, m, v, paula, pitch.slide, pitch.offset);
}

JoTickResult jo_a_tick(JoA* a, const JoModule* m, NuadeVoices* voices) {
    if (a->counter == 1) {
        for (int i = 0; i < JO_VOICES; i++) {
            if (a->dma_off[i]) {
                nuade_paula_channel_dma_off(&a->paula[i]);
            }
        }
    }
    if (jo_counter_expired(&a->counter)) {
        a->counter = (int8_t)a->tempo[a->second_tempo ? 1 : 0];
        a->second_tempo = !a->second_tempo;
    }
    for (int i = 0; i < JO_VOICES; i++) {
        a->dma_off[i] = false;
        a->dma_on[i] = false;
    }
    for (int i = 0; i < JO_VOICES; i++) {
        run_voice(a, m, i);
    }
    bool voices_play = false;
    for (int i = 0; i < JO_VOICES; i++) {
        if (a->dma_on[i]) {
            nuade_paula_channel_dma_on(&a->paula[i]);
        }
        nuade_paula_channel_sync(&a->paula[i], (uint32_t)i, voices);
        voices_play |= a->active[i];
    }
    return jo_song_end_result(&a->end, voices_play);
}

JoVoiceState jo_a_voice_state(const JoA* a, int voice) {
    const JoAVoice* v = &a->voices[voice];
    if (a->version == nullptr) {
        return jo_shown_state(false, 0, 0);
    }
    return jo_shown_state(v->started && !v->rest, a->paula[voice].period,
                          (uint16_t)(v->sample_entry - a->version->first_entry) / SAMPLE_RECORD_SIZE);
}

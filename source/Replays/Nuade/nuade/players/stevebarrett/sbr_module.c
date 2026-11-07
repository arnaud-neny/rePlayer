// SPDX-License-Identifier: MIT
// Steve Barrett: the module check, the records of the module and the pattern byte code.

#include "sbr_internal.h"

#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define FORM_ID 0x464f524du // 'FORM'
#define BODY_ID 0x424f4459u // 'BODY'

// The offsets of an 8SVX file's VHDR chunk that the init reads: oneShotHiSamples and repeatHiSamples.
#define VHDR_ONE_SHOT 0x14u
#define VHDR_REPEAT 0x18u
#define VHDR_END 0x1cu

#define SONG_BYTES 8u
#define MAX_WORK 4000000u

// The builds the port models. Each is the sound driver of one game; the offsets are what its code addresses.
static const SbrBuild s_builds[] = {
    { .name = "Advanced Ski Simulator",
      .vars_hash = 0x3a2ede1b0c535498ull,
      .code_hash = 0x6b3f9f4a4a7dd209ull,
      .code_start = 0x70,
      .code_end = 0x8fc,
      .entry = { 0x7a, 0x70, 0xc8, 0x67a },
      .extended = true,
      .reads_vhdr = true,
      .loop_style = SbrLoop_EveryVoice,
      .max_song = 5,
      .live_periods = 0x98a,
      .record = 0x99a,
      .period = 0x8fe,
      .chord = 0xa2b,
      .instruments = 0xabe,
      .wave = 0xb12,
      .env = 0xb82,
      .songs = 0xc74,
      .patterns = 0xc94,
      .form = 0x1512 },
    { .name = "Artificial Dreams",
      .vars_hash = 0xcd9590454a4e1e16ull,
      .code_hash = 0xfd5f3915379291adull,
      .code_start = 0x6c,
      .code_end = 0x8e2,
      .entry = { 0x76, 0x6c, 0xae, 0x65e },
      .extended = true,
      .reads_vhdr = false,
      .loop_style = SbrLoop_EveryVoice,
      .max_song = 5,
      .live_periods = 0x970,
      .record = 0x980,
      .period = 0x8e4,
      .chord = 0xa10,
      .instruments = 0xaa2,
      .wave = 0xafc,
      .env = 0xb7c,
      .songs = 0xc04,
      .patterns = 0xc14,
      .form = 0xff4 },
    { .name = "Dragonscape",
      .vars_hash = 0xb0e313f11f512858ull,
      .code_hash = 0x35907401a287dfc8ull,
      .code_start = 0x66,
      .code_end = 0x7fa,
      .entry = { 0x70, 0x66, 0xbe, 0x614 },
      .extended = true,
      .reads_vhdr = true,
      .loop_style = SbrLoop_EveryVoice,
      .max_song = 5,
      .live_periods = 0x8b0,
      .record = 0x8c0,
      .period = 0x7fc,
      .chord = 0x951,
      .instruments = 0x9c6,
      .wave = 0xa08,
      .env = 0xa60,
      .songs = 0xb10,
      .patterns = 0xb18,
      .form = 0xe90 },
    { .name = "Kelly X",
      .vars_hash = 0x55a17cc5d8866598ull,
      .code_hash = 0xa6fb51ca0f545ef9ull,
      .code_start = 0x66,
      .code_end = 0x696,
      .entry = { 0x70, 0x66, 0xbe, 0x4fe },
      .extended = false,
      .reads_vhdr = true,
      .loop_style = SbrLoop_EveryVoice,
      .max_song = 5,
      .live_periods = 0x72a,
      .record = 0x73a,
      .period = 0x698,
      .chord = 0,
      .instruments = 0x82e,
      .wave = 0x858,
      .env = 0x890,
      .songs = 0x908,
      .patterns = 0x910,
      .form = 0xe0c },
    { .name = "Chronicles of Omega",
      .vars_hash = 0x55a17cc5d8866598ull,
      .code_hash = 0xa60735b7b4e8779dull,
      .code_start = 0x46,
      .code_end = 0x676,
      .entry = { 0x50, 0x46, 0x9e, 0x4de },
      .extended = false,
      .reads_vhdr = true,
      .loop_style = SbrLoop_EveryVoice,
      .max_song = 5,
      .live_periods = 0x70a,
      .record = 0x71a,
      .period = 0x678,
      .chord = 0,
      .instruments = 0x80e,
      .wave = 0x83e,
      .env = 0x87e,
      .songs = 0x916,
      .patterns = 0x92e,
      .form = 0xfda },
    { .name = "Street Fighter",
      .vars_hash = 0x636f532534b87ff6ull,
      .code_hash = 0x21c154dec85122d6ull,
      .code_start = 0x38,
      .code_end = 0x8a6,
      .entry = { 0x42, 0x38, 0x7e, 0x622 },
      .extended = true,
      .reads_vhdr = false,
      .loop_style = SbrLoop_VoiceZero,
      .max_song = 6,
      .live_periods = 0x934,
      .record = 0x93c,
      .period = 0x8a8,
      .chord = 0x9cc,
      .instruments = 0xa5e,
      .wave = 0xab8,
      .env = 0xb08,
      .songs = 0xb98,
      .patterns = 0xbc0,
      .form = 0xfaa },
};

#define HASH_SEED 0xcbf29ce484222325ull

static uint64_t hash_bytes(uint64_t hash, const uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        hash = (hash ^ data[i]) * 0x100000001b3ull;
    }
    return hash;
}

// The records of the module. Every read is bounded; the module check has proven the offsets the songs reach.

uint8_t sbr_module_byte(const SbrModule* m, int64_t offset) {
    if (offset < 0 || (uint64_t)offset >= m->size) {
        return 0;
    }
    return m->bytes[offset];
}

static uint16_t read_word(const SbrModule* m, uint64_t offset) {
    if (offset > m->size || m->size - offset < 2) {
        return 0;
    }
    return nuade_read_be16(m->bytes + offset);
}

bool sbr_module_wave(const SbrModule* m, uint8_t offset, SbrWave* out) {
    const uint64_t at = (uint64_t)m->build->wave + offset;
    *out = (SbrWave) { 0 };
    if (at > m->size || m->size - at < SBR_WAVE_BYTES) {
        return false;
    }
    const uint16_t word2 = nuade_read_be16(m->bytes + at + 2);
    const uint16_t word4 = nuade_read_be16(m->bytes + at + 4);
    // the build that keeps the repeat's length in words swaps the two fields
    const bool swapped = m->build->loop_style == SbrLoop_VoiceZero;
    *out = (SbrWave) { .instrument = m->bytes[at],
                       .loop_start = swapped ? word4 : word2,
                       .loop_length = swapped ? word2 : word4 };
    return true;
}

bool sbr_module_env(const SbrModule* m, uint8_t offset, SbrEnv* out) {
    const uint64_t at = (uint64_t)m->build->env + offset;
    *out = (SbrEnv) { 0 };
    if (at > m->size || m->size - at < SBR_ENV_BYTES) {
        return false;
    }
    const uint8_t* r = m->bytes + at;
    *out = (SbrEnv) { .volume = r[0],
                      .target = r[1],
                      .step = r[2],
                      .release = r[3],
                      .sustain_shift = r[4],
                      .vibrato = r[5],
                      .chord = r[6],
                      .flags = r[7] };
    return true;
}

int32_t sbr_module_list(const SbrModule* m, uint32_t song, uint32_t voice) {
    // adda.w: the offset is signed
    return (int16_t)read_word(m, (uint64_t)m->build->songs + (uint64_t)song * SONG_BYTES + 2u * voice);
}

uint16_t sbr_module_pattern(const SbrModule* m, uint8_t index) {
    return read_word(m, (uint64_t)m->build->patterns + 2u * index);
}

uint8_t sbr_module_chord(const SbrModule* m, unsigned bit) {
    return sbr_module_byte(m, (int64_t)m->build->chord + bit);
}

// The fields of the voice record that the port keeps. The slide and glide fields ($24, $28, $2c, $30) stay 0 in the
// songs the module check lets through. $74, $78 and $7c hold bytes of the wave record that nothing reads.

static bool record_field_modelled(unsigned byte) {
    const unsigned field = byte & ~3u;
    return field <= 0x70;
}

SbrWordKind sbr_module_period_kind(const SbrModule* m, unsigned index) {
    const SbrBuild* b = m->build;
    const uint64_t at = (uint64_t)b->period + 2u * index;
    if (at > m->size || m->size - at < 2) {
        return SbrWord_Unmodelled;
    }
    if (at < b->live_periods) {
        return SbrWord_Image;
    }
    if (at < b->live_periods + 2u * SBR_VOICES) {
        return SbrWord_Live;
    }
    if (at >= b->record && at - b->record + 2u <= SBR_RECORD_BYTES) {
        const unsigned byte = (unsigned)(at - b->record);
        return record_field_modelled(byte) && record_field_modelled(byte + 1) ? SbrWord_Record : SbrWord_Unmodelled;
    }
    if (at < b->instruments) {
        return SbrWord_Unmodelled;
    }
    if (at < b->instruments + (uint64_t)SBR_INSTRUMENT_BYTES * m->instrument_count) {
        return SbrWord_Instrument;
    }
    return SbrWord_Image;
}

uint16_t sbr_module_period_word(const SbrModule* m, unsigned index) {
    const SbrBuild* b = m->build;
    const uint64_t at = (uint64_t)b->period + 2u * index;
    switch (sbr_module_period_kind(m, index)) {
        case SbrWord_Instrument: {
            const uint64_t rel = at - b->instruments;
            const SbrInstrument* entry = &m->instruments[rel / SBR_INSTRUMENT_BYTES];
            const uint32_t address = SBR_LOAD_ADDRESS + entry->offset;
            switch ((rel % SBR_INSTRUMENT_BYTES) / 2u) {
                case 0:
                    return entry->length_words;
                case 1:
                    return (uint16_t)(address >> 16);
                default:
                    return (uint16_t)address;
            }
        }
        case SbrWord_Image:
            return read_word(m, at);
        case SbrWord_Live:
        case SbrWord_Record:
        case SbrWord_Unmodelled:
            break;
    }
    return 0;
}

unsigned sbr_module_period_voice(const SbrModule* m, unsigned index) {
    return (unsigned)(((uint64_t)m->build->period + 2u * index - m->build->live_periods) / 2u);
}

unsigned sbr_module_period_record(const SbrModule* m, unsigned index) {
    return (unsigned)((uint64_t)m->build->period + 2u * index - m->build->record);
}

static bool next_byte(const SbrModule* m, int64_t* pos, uint8_t* b) {
    if (*pos < 0 || (uint64_t)*pos >= m->size) {
        return false;
    }
    *b = m->bytes[(*pos)++];
    return true;
}

static SbrEvent truncated(SbrEvent e, int64_t consumed) {
    e.kind = SbrEvent_Truncated;
    e.consumed = (uint32_t)consumed;
    return e;
}

// The pattern byte code. The driver reads, in this order and each at most once: a tempo byte ($f0..), a wave byte
// ($e0..), an envelope byte ($c0..), a length byte ($80..), a command ($7a..$7f), the note. An $ff behind the wave or
// the envelope byte ends the pattern too.

SbrEvent sbr_decode_event(const SbrModule* m, int64_t offset) {
    SbrEvent e = { .kind = SbrEvent_Truncated };
    int64_t pos = offset;
    uint8_t b = 0;

    if (!next_byte(m, &pos, &b)) {
        return truncated(e, pos - offset);
    }
    if (b == 0xff || b == 0xfe) {
        e.kind = b == 0xff ? SbrEvent_PatternEnd : SbrEvent_Stop;
        e.consumed = 1;
        return e;
    }
    if (b > 0xef) {
        e.has_tempo = true;
        e.tempo = b & 0xf;
        if (!next_byte(m, &pos, &b)) {
            return truncated(e, pos - offset);
        }
    }
    if (b > 0xdf) {
        e.has_wave = true;
        e.wave = (uint8_t)((b & 0xf) << 3);
        if (!next_byte(m, &pos, &b)) {
            return truncated(e, pos - offset);
        }
        if (b == 0xff) {
            e.kind = SbrEvent_PatternEnd;
            return e;
        }
    }
    if (b > 0xbf) {
        e.has_env = true;
        e.env = (uint8_t)((b & 0x1f) << 3);
        if (!next_byte(m, &pos, &b)) {
            return truncated(e, pos - offset);
        }
        if (b == 0xff) {
            e.kind = SbrEvent_PatternEnd;
            return e;
        }
    }
    if (b > 0x7f) {
        e.has_duration = true;
        e.duration = b & 0x3f;
        if (!next_byte(m, &pos, &b)) {
            return truncated(e, pos - offset);
        }
    }
    if (b > 0x5f) {
        switch (b) {
            case 0x7a:
                e.command = SbrCommand_VibratoDelay;
                if (!next_byte(m, &pos, &b)) {
                    return truncated(e, pos - offset);
                }
                e.parameter = b;
                if (!next_byte(m, &pos, &b)) {
                    return truncated(e, pos - offset);
                }
                break;
            case 0x7d:
            case 0x7c:
                if (m->build->extended) {
                    e.command = b == 0x7d ? SbrCommand_Chord : SbrCommand_ChordHold;
                    if (!next_byte(m, &pos, &b)) {
                        return truncated(e, pos - offset);
                    }
                    e.parameter = b;
                    if (!next_byte(m, &pos, &b)) {
                        return truncated(e, pos - offset);
                    }
                }
                break;
            case 0x7b:
            case 0x7e:
            case 0x7f:
                if (m->build->extended) {
                    e.command = SbrCommand_Unmodelled;
                    if (!next_byte(m, &pos, &b)) {
                        return truncated(e, pos - offset);
                    }
                }
                break;
            default:
                break;
        }
    }
    e.kind = SbrEvent_Note;
    e.note = b;
    e.consumed = (uint32_t)(pos - offset);
    return e;
}

// The init: the chain of 8SVX files behind the driver.

static bool find_instruments(SbrModule* m) {
    const SbrBuild* b = m->build;
    uint64_t form = b->form;
    uint32_t count = 0;
    while (m->size >= 4 && form <= m->size - 4 && nuade_read_be32(m->bytes + form) == FORM_ID) {
        if (count == SBR_MAX_INSTRUMENTS) {
            return false;
        }
        if (b->reads_vhdr && (m->size < VHDR_END || form > m->size - VHDR_END)) {
            return false;
        }
        // The init looks for BODY on every fourth byte from the start of the file.
        uint64_t chunk = form + 4;
        while (m->size >= 8 && chunk <= m->size - 8 && nuade_read_be32(m->bytes + chunk) != BODY_ID) {
            chunk += 4;
        }
        if (m->size < 8 || chunk > m->size - 8) {
            return false;
        }
        const uint32_t body_size = nuade_read_be32(m->bytes + chunk + 4);
        const uint64_t body = chunk + 8;
        if (body_size < 2 || body_size > m->size - body) {
            return false;
        }
        m->instruments[count++] = (SbrInstrument) { .length_words = (uint16_t)(body_size >> 1),
                                                    .offset = (uint32_t)body,
                                                    .form_offset = (uint32_t)form };
        form = body + body_size;
    }
    m->instrument_count = count;
    // the eagleplayer runs a module whose FORM chain, with 4 bytes, is $F3A4 long at 100 Hz
    m->tick_length = form - b->form + 4 == 0xf3a4 ? 7093 : 14187;
    return count > 0 && (uint64_t)count * SBR_INSTRUMENT_BYTES <= (uint64_t)b->wave - b->instruments
           && (!b->reads_vhdr || (uint64_t)count * SBR_WAVE_BYTES <= (uint64_t)b->env - b->wave);
}

// The proof that the songs stay inside the port: a walk over every note of every voice of every song, with the state
// of the driver that decides what the note reads: the transposition, the envelope, the chord. A list is walked twice,
// the second round with the state the first leaves.

typedef struct Walk {
    uint8_t transpose;
    uint8_t env; // the envelope record's byte offset
    SbrChordMode mode;
    uint8_t chord;
    bool fresh;     // the next event starts a note: the driver resets the chord of a note that does not hold it
    uint32_t* work; // the events walked in the whole module
} Walk;

static bool check_wave(const SbrModule* m, uint8_t offset) {
    SbrWave wave;
    return sbr_module_wave(m, offset, &wave) && wave.instrument < m->instrument_count;
}

// The driver reads the envelope's bytes 6 and 7 for a chord and a bend that the port does not model.

static bool check_env(const SbrModule* m, uint8_t offset) {
    SbrEnv env;
    return sbr_module_env(m, offset, &env) && env.chord == 0 && (env.flags & 1u) == 0;
}

// A period table word the driver reads must be one the port models.

static bool period_modelled(const SbrModule* m, unsigned index) {
    return sbr_module_period_kind(m, index) != SbrWord_Unmodelled;
}

static uint8_t env_vibrato(const SbrModule* m, uint8_t offset) {
    SbrEnv env;
    sbr_module_env(m, offset, &env);
    return env.vibrato;
}

// What a note reads: its period, and then either the chord's notes (the bend may add one to the note first) or, with
// the envelope's vibrato, the pair of words the vibrato takes from the note doubled in a byte.

static bool check_note(const SbrModule* m, const Walk* walk, uint8_t note) {
    if (walk->chord != 0) {
        for (unsigned bend = 0; bend < (m->build->extended ? 2u : 1u); bend++) {
            const unsigned bent = (note + bend) & 0xffu;
            if (!period_modelled(m, bent)) {
                return false;
            }
            for (unsigned bit = 0; bit < 8; bit++) {
                if (((walk->chord >> bit) & 1u) && !period_modelled(m, (bent + sbr_module_chord(m, bit)) & 0xffu)) {
                    return false;
                }
            }
        }
        return true;
    }
    if (!period_modelled(m, note)) {
        return false;
    }
    if (walk->mode == SbrChordMode_None && env_vibrato(m, walk->env) != 0) {
        const unsigned vibrato = note & 0x7fu;
        return period_modelled(m, vibrato) && period_modelled(m, vibrato + 1);
    }
    return true;
}

static bool walk_pattern(const SbrModule* m, Walk* walk, uint8_t pattern) {
    const int64_t start = sbr_offset(sbr_module_pattern(m, pattern));
    // memory below the wave table changes at run time (code hooks, variables, records, the sample table)
    if (start < (int64_t)m->build->wave || (uint64_t)start >= m->size) {
        return false;
    }
    int64_t pos = start;
    for (;;) {
        if (++*walk->work > MAX_WORK) {
            return false;
        }
        const SbrEvent e = sbr_decode_event(m, pos);
        if (e.kind == SbrEvent_Truncated) {
            return false;
        }
        if (walk->fresh && walk->mode != SbrChordMode_Hold) {
            walk->mode = SbrChordMode_None;
            walk->chord = 0;
        }
        walk->fresh = false;
        if (e.has_wave && !check_wave(m, e.wave)) {
            return false;
        }
        if (e.has_env) {
            if (!check_env(m, e.env)) {
                return false;
            }
            walk->env = e.env;
        }
        if (e.kind != SbrEvent_Note) {
            return true;
        }
        if (e.command == SbrCommand_Unmodelled) {
            return false;
        }
        if (e.command == SbrCommand_Chord || e.command == SbrCommand_ChordHold) {
            walk->mode = e.command == SbrCommand_Chord ? SbrChordMode_Note : SbrChordMode_Hold;
            walk->chord = e.parameter;
        }
        if (!check_note(m, walk, (uint8_t)(e.note + walk->transpose))) {
            return false;
        }
        walk->fresh = true;
        pos += e.consumed;
    }
}

// The driver's list reader: a byte position that wraps at 256, $ff restarts the list, $80..$fe set the transposition,
// the rest is a pattern number.

static bool walk_list(const SbrModule* m, int64_t list, uint32_t* work) {
    if (list < (int64_t)m->build->wave) {
        return false;
    }
    Walk walk = { .fresh = true, .work = work };
    if (!check_wave(m, 0) || !check_env(m, 0)) {
        return false;
    }
    uint8_t position = 0;
    uint32_t rounds = 0;
    bool has_pattern = false;
    for (uint32_t step = 0; step < 1024 && rounds < 2; step++) {
        const int64_t at = list + position;
        if (at < 0 || (uint64_t)at >= m->size) {
            return false;
        }
        const uint8_t entry = m->bytes[at];
        position++;
        if (entry == 0xff) {
            if (!has_pattern) {
                return false;
            }
            position = 0;
            rounds++;
        } else if (entry & 0x80) {
            walk.transpose = (uint8_t)(entry + 0x40);
        } else {
            has_pattern = true;
            if (!walk_pattern(m, &walk, entry)) {
                return false;
            }
        }
    }
    return rounds == 2;
}

static bool check_songs(const SbrModule* m) {
    uint32_t work = 0;
    for (uint32_t song = 0; song < m->song_count; song++) {
        for (uint32_t voice = 0; voice < SBR_VOICES; voice++) {
            if (!walk_list(m, sbr_offset(sbr_module_list(m, song, voice)), &work)) {
                return false;
            }
        }
    }
    return true;
}

static uint64_t vars_hash(const uint8_t* data, const SbrBuild* b) {
    if (b->chord == 0) {
        return hash_bytes(HASH_SEED, data + b->live_periods, b->instruments - b->live_periods);
    }
    const uint64_t before = hash_bytes(HASH_SEED, data + b->live_periods, b->chord - b->live_periods);
    return hash_bytes(before, data + b->chord + 8, b->instruments - b->chord - 8);
}

// The title between the entries and the code. Check2 and InitPlayer take the first $41FA word of the module for the
// first FORM, so the title holds none. InitPlayer's scan from $10 (player.asm $560) looks for "tt" and writes a $0A
// behind it, then clears the byte two before the next $FA; the scan stops at an $FA it meets first. No write may reach
// the code, nor may the scan run off the build's code.

static bool title_ok(const uint8_t* data, const SbrBuild* b) {
    for (uint32_t i = 0; i < b->code_start; i += 2) {
        if (nuade_read_be16(data + i) == 0x41fa) {
            return false;
        }
    }
    uint32_t at = 0x10;
    while (at < b->code_end) {
        if (data[at] == 0xfa) {
            return true;
        }
        if (data[at++] != 0x74 || at >= b->code_end) {
            continue;
        }
        if (data[at++] != 0x74) {
            continue;
        }
        const uint32_t newline = at++;
        while (at < b->code_end && data[at++] != 0xfa) {}
        return newline < b->code_start && at >= 3 && at - 3 < b->code_start && data[at - 1] == 0xfa;
    }
    return false;
}

// The eagleplayer's module check: four `bra.w`, the same entries the build has.

static const SbrBuild* identify(const uint8_t* data, size_t size) {
    if (size < 8) {
        return nullptr;
    }
    for (size_t i = 0; i < nuade_sizeof_array(s_builds); i++) {
        const SbrBuild* b = &s_builds[i];
        if (size < b->form + 4u || size < b->code_end) {
            continue;
        }
        bool entries = true;
        for (uint32_t k = 0; k < SBR_ENTRY_COUNT; k++) {
            const uint8_t* bra = data + 4u * k;
            entries
                = entries && nuade_read_be16(bra) == 0x6000 && 4u * k + 2u + nuade_read_be16(bra + 2) == b->entry[k];
        }
        if (entries && hash_bytes(HASH_SEED, data + b->code_start, b->code_end - b->code_start) == b->code_hash
            && vars_hash(data, b) == b->vars_hash && title_ok(data, b)) {
            return b;
        }
    }
    return nullptr;
}

// What the init does to the module: it clears the first word of every sample (the repeat of a sample without a loop
// then plays a silent word) and of every file header, and sets the loop of the sample's wave record from its VHDR.

static void patch_vhdr(SbrModule* m) {
    uint8_t* image = m->image;
    for (uint32_t k = 0; k < m->instrument_count; k++) {
        const SbrInstrument* inst = &m->instruments[k];
        const uint32_t one_shot = nuade_read_be32(image + inst->form_offset + VHDR_ONE_SHOT);
        const uint32_t repeat = nuade_read_be32(image + inst->form_offset + VHDR_REPEAT);
        // The record's words 2 and 4 are the loop's start and length.
        uint8_t* record = image + m->build->wave + (size_t)k * SBR_WAVE_BYTES + 2;
        record[0] = 0;
        record[1] = 0;
        record[2] = 0;
        record[3] = 2;
        if (repeat != 0) {
            record[0] = (uint8_t)(one_shot >> 8);
            record[1] = (uint8_t)one_shot;
            record[2] = (uint8_t)(repeat >> 8);
            record[3] = (uint8_t)repeat;
        }
        image[inst->form_offset] = 0;
        image[inst->form_offset + 1] = 0;
        image[inst->offset] = 0;
        image[inst->offset + 1] = 0;
    }
}

// The module check. The proof is made on the module as the driver's init leaves it: the walk of the songs reads the
// copy that the init patched.

SbrStatus sbr_module_open(SbrModule* m, const uint8_t* data, size_t size) {
    memset(m, 0, sizeof(*m));
    m->build = identify(data, size);
    if (m->build == nullptr) {
        return SbrStatus_InvalidData;
    }
    const SbrBuild* b = m->build;
    m->bytes = data;
    m->size = size;
    // The tables sit in this order; the sample table and the wave table hold one record per file.
    if (!(b->period < b->live_periods && b->live_periods + 2u * SBR_VOICES <= b->record
          && b->record + SBR_RECORD_BYTES <= b->instruments && b->instruments < b->wave && b->wave < b->env
          && b->env < b->songs && b->songs < b->patterns && b->patterns < b->form && b->form < size)) {
        return SbrStatus_InvalidData;
    }
    if (b->extended && b->chord + 8u > size) {
        return SbrStatus_InvalidData;
    }
    const uint32_t songs = (b->patterns - b->songs) / SONG_BYTES;
    if ((b->patterns - b->songs) % SONG_BYTES != 0 || songs == 0 || songs > b->max_song) {
        return SbrStatus_InvalidData;
    }
    m->song_count = songs;
    if (!find_instruments(m)) {
        return SbrStatus_InvalidData;
    }
    m->image = nuade_paula_padded_copy(data, size);
    if (m->image == nullptr) {
        return SbrStatus_OutOfMemory;
    }
    m->bytes = m->image;
    if (b->reads_vhdr) {
        patch_vhdr(m);
    }
    if (!check_songs(m)) {
        sbr_module_close(m);
        return SbrStatus_InvalidData;
    }
    return SbrStatus_Ok;
}

void sbr_module_close(SbrModule* m) {
    free(m->image);
    memset(m, 0, sizeof(*m));
}

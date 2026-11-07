// SPDX-License-Identifier: MIT
// Audio Sculpture player, translated from the UADE eagleplayer "AudioSculpture / Startrekker 4AM player" (ASREPLAY by
// The Synchron Assembly / Expose Software, adapted for UADE).
//
// The replayer is a ProTracker one with two additions: instrument n is a synth voice when the bank has an "AM" entry
// for it (Paula loops a 32 byte wave form while the player moves the volume through an envelope and the period through
// a slide and a vibrato), and the E command calls 68k code from the bank, which is not ported. The first row is played
// after one full speed. The eagleplayer also finds the subsongs and detects the end of the song.

#include <nuade/players/audiosculpture.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

// Module layout.
#define MOD_TITLE_SIZE 20
#define MOD_SAMPLE_TABLE 0x14
#define MOD_SAMPLE_HEADER_SIZE 30
#define MOD_SAMPLE_NAME_SIZE 22
#define MOD_SONG_LENGTH 0x3B6
#define MOD_RESTART 0x3B7
#define MOD_ORDERS 0x3B8
#define MOD_TAG 0x438
#define MOD_PATTERNS 0x43C
#define MOD_PATTERN_SIZE 1024
#define MOD_ROW_SIZE 16
#define MOD_CELL_SIZE 4
#define TAG_EXO4 0x45584F34u
#define TAG_FLT4 0x464C5434u

// Check2 looks at 32 instrument headers: the 32nd one lies over the order list.
#define CHECK_HEADERS 32

// Bank layout: a 24 byte header, then 120 bytes for each instrument number (instrument n is at 24 + n * 120).
#define BANK_HEADER_SIZE 24
#define BANK_INSTRUMENT_SIZE 120
#define BANK_AM_MAGIC 0x414D // "AM"

#define WAVE_NOISE 3
#define WAVE_WORDS 16
#define WAVE_NUMBER_SHIFT 5 // the wave number times 32 is the offset, as a 16 bit value

#define PERIOD_MIN 0x71
#define PERIOD_MAX 0x358
#define PERIOD_MASK 0x0FFF
#define VOLUME_MAX 64
#define SPEED_DEFAULT 6
#define SPEED_MAX 31
#define ORDER_MASK 0x7F
#define PATTERN_ROW_END (ASC_PATTERN_ROWS * MOD_ROW_SIZE)

// The AM vibrato table is the first half of a sine, one word for each 2 degrees (180 entries); the phase runs over the
// whole wave and the second half uses the table again, negated.
#define AM_SINE_SIZE 180
#define AM_VIBRATO_HALF 0x168
#define AM_VIBRATO_PHASE_END 0x2D0

#define NOISE_SEED 0x7327
#define NOISE_XOR 0x7C
#define BEAM_STEP 27 // colour clocks per noise byte: the stand-in for the beam position the original reads
#define BEAM_LINE 227

// Tables

// The first half of a sine, 32 steps (the vibrato effect; the second half is the same, negated).
static const uint8_t k_vibrato[32]
    = { 0x00, 0x18, 0x31, 0x4a, 0x61, 0x78, 0x8d, 0xa1, 0xb4, 0xc5, 0xd4, 0xe0, 0xeb, 0xf4, 0xfa, 0xfd,
        0xff, 0xfd, 0xfa, 0xf4, 0xeb, 0xe0, 0xd4, 0xc5, 0xb4, 0xa1, 0x8d, 0x78, 0x61, 0x4a, 0x31, 0x18 };

// 36 periods and a terminating 0
#define PERIOD_COUNT 37
static const uint16_t k_periods[PERIOD_COUNT]
    = { 0x358, 0x328, 0x2fa, 0x2d0, 0x2a6, 0x280, 0x25c, 0x23a, 0x21a, 0x1fc, 0x1e0, 0x1c5, 0x1ac,
        0x194, 0x17d, 0x168, 0x153, 0x140, 0x12e, 0x11d, 0x10d, 0xfe,  0xf0,  0xe2,  0xd6,  0xca,
        0xbe,  0xb4,  0xaa,  0xa0,  0x97,  0x8f,  0x87,  0x7f,  0x78,  0x71,  0 };

// The three fixed wave forms.
static const int8_t k_waves[(ASC_WAVE_COUNT - 1) * ASC_WAVE_SIZE] = {
    0,    25,   49,   71,   90,   106,  117,  125,  127,  125,  117,  106,  90,   71,   49,   25,
    0,    -25,  -49,  -71,  -90,  -106, -117, -125, -127, -125, -117, -106, -90,  -71,  -49,  -25,
    -128, -120, -112, -104, -96,  -88,  -80,  -72,  -64,  -56,  -48,  -40,  -32,  -24,  -16,  -8,
    0,    8,    16,   24,   32,   40,   48,   56,   64,   72,   80,   88,   96,   104,  112,  120,
    -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128,
    127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,  127,
};

// The AM vibrato table (see AM_SINE_SIZE): 0..128..0, indexed in steps of 2 bytes.
static const int16_t k_am_sine[AM_SINE_SIZE]
    = { 0,    2,    4,    6,    8,    0xb,  0xd,  0xf,  0x11, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e, 0x21, 0x23, 0x25,
        0x27, 0x29, 0x2b, 0x2d, 0x2f, 0x32, 0x34, 0x36, 0x38, 0x3a, 0x3c, 0x3e, 0x3f, 0x41, 0x43, 0x45, 0x47, 0x49,
        0x4b, 0x4d, 0x4e, 0x50, 0x52, 0x53, 0x55, 0x57, 0x58, 0x5a, 0x5c, 0x5d, 0x5f, 0x60, 0x62, 0x63, 0x64, 0x66,
        0x67, 0x68, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x71, 0x72, 0x73, 0x74, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
        0x79, 0x7a, 0x7b, 0x7b, 0x7c, 0x7c, 0x7d, 0x7d, 0x7e, 0x7e, 0x7e, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
        0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7e, 0x7e, 0x7e, 0x7d, 0x7d, 0x7c, 0x7c, 0x7b, 0x7b, 0x7a,
        0x79, 0x79, 0x78, 0x77, 0x76, 0x75, 0x74, 0x73, 0x72, 0x71, 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x67,
        0x66, 0x64, 0x63, 0x62, 0x60, 0x5f, 0x5d, 0x5c, 0x5a, 0x58, 0x57, 0x55, 0x53, 0x52, 0x50, 0x4e, 0x4d, 0x4b,
        0x49, 0x47, 0x45, 0x43, 0x41, 0x40, 0x3e, 0x3c, 0x3a, 0x38, 0x36, 0x34, 0x32, 0x2f, 0x2d, 0x2b, 0x29, 0x27,
        0x25, 0x23, 0x21, 0x1e, 0x1c, 0x1a, 0x18, 0x16, 0x14, 0x11, 0xf,  0xd,  0xb,  0x8,  0x6,  0x4,  0x2,  0 };

// Types

typedef enum Effect {
    Effect_Arpeggio = 0x0,
    Effect_SlideUp = 0x1,
    Effect_SlideDown = 0x2,
    Effect_TonePortamento = 0x3,
    Effect_Vibrato = 0x4,
    Effect_TonePortamentoVolumeSlide = 0x5,
    Effect_VibratoVolumeSlide = 0x6,
    Effect_VolumeSlide = 0xA,
    Effect_PositionJump = 0xB,
    Effect_SetVolume = 0xC,
    Effect_PatternBreak = 0xD,
    Effect_Extended = 0xE,
    Effect_SetSpeed = 0xF,
} Effect;

// File access

static uint8_t blob_u8(const AscBlob* blob, size_t offset) {
    return offset < blob->size ? blob->data[offset] : 0;
}

static uint16_t blob_u16(const AscBlob* blob, size_t offset) {
    if (offset + 2 <= blob->size) {
        return nuade_read_be16(blob->data + offset);
    }
    return (uint16_t)((uint16_t)blob_u8(blob, offset) << 8 | blob_u8(blob, offset + 1));
}

static uint32_t blob_u32(const AscBlob* blob, size_t offset) {
    if (offset + 4 <= blob->size) {
        return nuade_read_be32(blob->data + offset);
    }
    return (uint32_t)blob_u16(blob, offset) << 16 | blob_u16(blob, offset + 2);
}

static AscSampleHeader read_sample_header(const AscBlob* module, int index) {
    const size_t start = MOD_SAMPLE_TABLE + (size_t)index * MOD_SAMPLE_HEADER_SIZE;
    const size_t at = start + MOD_SAMPLE_NAME_SIZE;
    AscSampleHeader header = { .length_words = blob_u16(module, at),
                               .volume = blob_u8(module, at + 3),
                               .loop_start_words = blob_u16(module, at + 4),
                               .loop_length_words = blob_u16(module, at + 6) };
    for (int i = 0; i < MOD_SAMPLE_NAME_SIZE; i++) {
        header.name[i] = (char)blob_u8(module, start + (size_t)i);
    }
    return header;
}

static AscRawCell read_cell(const AscBlob* module, size_t offset) {
    const uint8_t b0 = blob_u8(module, offset);
    const uint8_t b2 = blob_u8(module, offset + 2);
    return (AscRawCell) { .period = (uint16_t)(blob_u16(module, offset) & PERIOD_MASK),
                          .instrument = (uint8_t)((b0 & 0xF0) | (b2 >> 4)),
                          .effect = (uint8_t)(b2 & 0x0F),
                          .param = blob_u8(module, offset + 3) };
}

// Fields of the 120 bytes, as byte offsets: 6 start level, 8 + 4 * n and 10 + 4 * n level and rate of stage n, 0x14
// hold, 0x18 release rate, 0x1A wave form, 0x1C period slide, 0x1E vibrato depth, 0x20 vibrato speed, 0x22 octave
// shift.
static AscAmInstrument read_am_instrument(const AscBlob* bank, uint32_t number) {
    const size_t at = BANK_HEADER_SIZE + (size_t)number * BANK_INSTRUMENT_SIZE;
    AscAmInstrument am = { .present = blob_u16(bank, at) == BANK_AM_MAGIC };
    if (!am.present) {
        return am;
    }
    am.start_level = blob_u16(bank, at + 6);
    for (int i = 0; i < 3; i++) {
        am.stages[i] = (AscAmStage) { .level = (int16_t)blob_u16(bank, at + 8 + (size_t)i * 4),
                                      .rate = (int16_t)blob_u16(bank, at + 10 + (size_t)i * 4) };
    }
    am.hold_ticks = (int16_t)blob_u16(bank, at + 0x14);
    am.release_rate = (int16_t)blob_u16(bank, at + 0x18);
    am.wave = blob_u16(bank, at + 0x1A);
    am.period_slide = (int16_t)blob_u16(bank, at + 0x1C);
    am.vibrato_depth = blob_u16(bank, at + 0x1E);
    am.vibrato_speed = blob_u16(bank, at + 0x20);
    am.octave_shift = blob_u16(bank, at + 0x22);
    return am;
}

// Offset in the module of a cell. The row is addressed with 16 bit arithmetic (a pattern number of 64 or more wraps)
// and each voice reads 4 bytes after the one before.
static size_t cell_offset(uint8_t pattern, uint16_t row_offset, int voice) {
    const uint16_t row_start = (uint16_t)((uint16_t)(pattern << 10) + row_offset);
    return MOD_PATTERNS + (size_t)row_start + (size_t)voice * MOD_CELL_SIZE;
}

// Paula memory

typedef struct Span {
    const int8_t* data;
    size_t size;
} Span;

static Span memory_span(const AscPlayer* p, AscMemory memory) {
    switch (memory) {
        case AscMemory_Module:
            return (Span) { (const int8_t*)p->module.data, p->module.size };
        case AscMemory_Waves:
            return (Span) { p->waves, sizeof(p->waves) };
        case AscMemory_None:
            break;
    }
    return (Span) { nullptr, 0 };
}

static const int8_t* address_data(const AscPlayer* p, AscAddress a) {
    const Span span = memory_span(p, a.memory);
    return a.offset < span.size ? span.data + a.offset : nullptr;
}

// AUDxLEN for an AUDxLC at `a`. The module is a padded copy, so any length plays there (zeros past its end), and an
// address outside its memory has a null AUDxLC. Only the 128 bytes of wave forms need a bound: a wave goes in with its
// 16 words, which fit, and write_lc keeps whatever length the channel holds, so a longer one (or 0) is cut to the waves
// for memory safety (the original would read the replayer memory that follows them).
static uint16_t readable_words(const AscPlayer* p, AscAddress a, uint16_t words) {
    if (a.memory != AscMemory_Waves || a.offset >= sizeof(p->waves)) {
        return words;
    }
    const uint16_t room = (uint16_t)((sizeof(p->waves) - a.offset) / 2);
    return words != 0 && words <= room ? words : room;
}

static AscAddress address_add(AscAddress a, uint32_t bytes) {
    return (AscAddress) { .memory = a.memory, .offset = a.offset + bytes };
}

// AUDxLC and AUDxLEN.
static void write_lc_len(const AscPlayer* p, NuadePaulaChannel* paula, AscAddress a, uint16_t words) {
    paula->lc = address_data(p, a);
    paula->len_words = readable_words(p, a, words);
}

// AUDxLC alone: the length stays (cut to the wave forms, see readable_words).
static void write_lc(const AscPlayer* p, NuadePaulaChannel* paula, AscAddress a) {
    paula->lc = address_data(p, a);
    paula->len_words = readable_words(p, a, paula->len_words);
}

// AscVoice helpers

// `level` scaled by the voice's volume (a 16 bit product; only its low word is shifted).
static uint16_t scaled_volume(const AscVoice* v, uint16_t level) {
    return (uint16_t)((uint16_t)(level * v->volume) >> 6);
}

// The N flag of the 16 bit subtraction `a - b`. The 68k code clamps with `bpl` / `bmi` after it, which differs from a
// signed comparison where the subtraction overflows (periods of 0x8000 and above).
static bool word_difference_negative(uint16_t a, uint16_t b) {
    return (int16_t)(uint16_t)(a - b) < 0;
}

// The period register and the base the AM vibrato is added to.
static void set_period(AscVoice* v, uint16_t period) {
    v->paula.period = period;
    v->base_period = period;
}

static bool is_tone_portamento(uint8_t effect) {
    return effect == Effect_TonePortamento || effect == Effect_TonePortamentoVolumeSlide;
}

// Effects, one tick after the row

// The arpeggio looks up the period in the table, and a note high in the table plus the semitones reads past its end
// into the replayer's variables: the tick counter, the row offset, the order. This maps such an index to those fields;
// the sample pointer table that follows is an Amiga address and reads as 0.
static uint16_t period_table_word(const AscPlayer* p, uint32_t index) {
    if (index < PERIOD_COUNT) {
        return k_periods[index];
    }
    switch (index - PERIOD_COUNT) {
        case 0:
            return (uint16_t)((uint16_t)p->speed << 8 | p->counter);
        case 1:
            return p->row_offset;
        case 2:
            return (uint16_t)((uint16_t)p->order << 8 | (p->break_pending ? 1 : 0));
        case 3: {
            uint16_t started = 0;
            for (int i = 0; i < ASC_VOICES; i++) {
                started |= p->voices[i].dma_started ? (uint16_t)(1u << i) : 0u;
            }
            return started;
        }
        default:
            return 0;
    }
}

static void arpeggio(const AscPlayer* p, AscVoice* v) {
    // The original's list of 32 ticks (the counter stays below the speed, 31 at most): none, high nibble, low nibble.
    const unsigned step = p->counter % 3;
    if (step != 0) {
        const uint8_t semitones = step == 2 ? (v->cell.param & 0x0F) : (uint8_t)(v->cell.param >> 4);
        const uint16_t period = v->period & PERIOD_MASK;
        for (uint32_t i = 0; i < PERIOD_COUNT; i++) {
            if ((int16_t)period >= (int16_t)k_periods[i]) {
                set_period(v, period_table_word(p, i + semitones));
                return;
            }
        }
    }
    set_period(v, v->period);
}

static void slide_up(AscVoice* v) {
    v->period = (uint16_t)(v->period - v->cell.param);
    if (word_difference_negative(v->period, PERIOD_MIN)) {
        v->period = PERIOD_MIN;
    }
    set_period(v, v->period);
}

static void slide_down(AscVoice* v) {
    v->period = (uint16_t)(v->period + v->cell.param);
    if (!word_difference_negative(v->period, PERIOD_MAX)) {
        v->period = PERIOD_MAX;
    }
    set_period(v, v->period);
}

// A speed in the cell sets the portamento speed and is used up.
static void tone_portamento(AscVoice* v, bool take_speed) {
    if (take_speed && v->cell.param != 0) {
        v->porta_speed = v->cell.param;
        v->cell.param = 0;
    }
    if (v->porta_target == 0) {
        return;
    }
    const int16_t target = (int16_t)v->porta_target;
    if (!v->porta_period_falls) {
        v->period = (uint16_t)(v->period + v->porta_speed);
        if (target <= (int16_t)v->period) {
            v->period = v->porta_target;
            v->porta_target = 0;
        }
    } else {
        v->period = (uint16_t)(v->period - v->porta_speed);
        if (target >= (int16_t)v->period) {
            v->period = v->porta_target;
            v->porta_target = 0;
        }
    }
    set_period(v, v->period);
}

static void vibrato(AscVoice* v, bool take_param) {
    if (take_param && v->cell.param != 0) {
        v->vibrato_speed = v->cell.param >> 4;
        v->vibrato_depth = v->cell.param & 0x0F;
    }
    const uint16_t delta = (uint16_t)(k_vibrato[(v->vibrato_pos >> 2) & 0x1F] * v->vibrato_depth) >> 7;
    const uint16_t period
        = (v->vibrato_pos & 0x80) != 0 ? (uint16_t)(v->period - delta) : (uint16_t)(v->period + delta);
    set_period(v, period);
    v->vibrato_pos = (uint8_t)(v->vibrato_pos + v->vibrato_speed * 4);
}

static void volume_slide(AscVoice* v) {
    const uint8_t up = v->cell.param >> 4;
    if (up != 0) {
        v->volume = (uint8_t)(v->volume + up);
        if ((int8_t)(v->volume - VOLUME_MAX) >= 0) {
            v->volume = VOLUME_MAX;
        }
    } else {
        v->volume = (uint8_t)(v->volume - (v->cell.param & 0x0F));
        if ((int8_t)v->volume < 0) {
            v->volume = 0;
        }
    }
    if (!v->am) {
        v->paula.volume = v->volume;
    }
}

// The effects of the ticks between two rows.
static void tick_effect(const AscPlayer* p, AscVoice* v) {
    if (v->cell.effect == Effect_Arpeggio && v->cell.param == 0) {
        set_period(v, v->period);
        return;
    }
    switch (v->cell.effect) {
        case Effect_Arpeggio:
            arpeggio(p, v);
            return;
        case Effect_SlideUp:
            slide_up(v);
            return;
        case Effect_SlideDown:
            slide_down(v);
            return;
        case Effect_Extended:
            return; // calls 68k code from the bank file
        case Effect_TonePortamento:
            tone_portamento(v, true);
            return;
        case Effect_Vibrato:
            vibrato(v, true);
            return;
        case Effect_TonePortamentoVolumeSlide:
            tone_portamento(v, false);
            volume_slide(v);
            return;
        case Effect_VibratoVolumeSlide:
            vibrato(v, false);
            volume_slide(v);
            return;
        default:
            break;
    }
    if (!v->am) {
        v->paula.period = v->period;
    }
    if (v->cell.effect == Effect_VolumeSlide) {
        volume_slide(v);
    }
}

// The row

// Is this row played for the second time? Marks it. The original calls SongEnd on every row from the first repeat on;
// here the marks start over at the repeat, so that each pass through the song is one loop.
static bool mark_row_visited(AscPlayer* p) {
    bool* seen = &p->visited[p->order][p->row_offset / MOD_ROW_SIZE];
    if (*seen) {
        memset(p->visited, 0, sizeof(p->visited));
        *seen = true;
        return true;
    }
    *seen = true;
    return false;
}

// A cell with an instrument number loads the instrument into the voice and writes its volume.
static void assign_instrument(const AscPlayer* p, AscVoice* v, uint8_t number) {
    const int index = number - 1;
    const AscAmInstrument* am = &p->am[number];
    const AscSampleHeader header = p->samples[index];
    const uint16_t start_volume = am->present ? (uint16_t)(am->start_level >> 2) : VOLUME_MAX;

    v->instrument = number;
    v->am = am->present;
    v->sample = (AscAddress) { AscMemory_Module, p->sample_offsets[index] };
    v->length_words = header.length_words;
    v->volume = header.volume;
    if (header.loop_start_words != 0) {
        v->loop = address_add(v->sample, (uint32_t)header.loop_start_words * 2);
        v->length_words = (uint16_t)(header.loop_start_words + header.loop_length_words);
    } else {
        v->loop = v->sample;
    }
    v->loop_words = header.loop_length_words;
    v->paula.volume = (uint8_t)scaled_volume(v, start_volume);
}

// A note on a synth instrument: a wave form in a loop at the period shifted by the octave shift; the envelope starts.
static void start_am_note(AscPlayer* p, AscVoice* v) {
    const AscAmInstrument* am = &p->am[v->instrument];
    const AscAddress wave = { AscMemory_Waves, (uint16_t)(am->wave << WAVE_NUMBER_SHIFT) };

    write_lc_len(p, &v->paula, wave, WAVE_WORDS);
    v->loop = wave;
    v->loop_words = WAVE_WORDS;
    v->am_level = (int16_t)am->start_level;
    v->am_vibrato_pos = 0;
    v->am_phase = AscAmPhase_Attack;
    const unsigned shift = am->octave_shift & 0x3F; // the shift count register takes 6 bits
    v->period = shift >= 16 ? 0 : (uint16_t)(v->period << shift);
    set_period(v, v->period);
}

// A note: DMA stops and Paula is loaded; the DMA starts once all the voices of the row are set.
static void start_note(AscPlayer* p, AscVoice* v) {
    v->period = v->cell.period;
    nuade_paula_channel_dma_off(&v->paula);
    v->vibrato_pos = 0;
    if (v->am) {
        start_am_note(p, v);
    } else {
        write_lc_len(p, &v->paula, v->sample, v->length_words);
        v->paula.period = v->period;
    }
    v->dma_started = true;
}

// A tone portamento row sets the target; the note itself does not start. The portamento slides towards it unless the
// period is already there.
static void start_tone_portamento(AscVoice* v) {
    v->porta_target = v->cell.period;
    v->porta_period_falls = false;
    if ((int16_t)v->cell.period == (int16_t)v->period) {
        v->porta_target = 0;
    } else if ((int16_t)v->cell.period < (int16_t)v->period) {
        v->porta_period_falls = true;
    }
}

// The effects that act on the row itself.
static void row_effect(AscPlayer* p, AscVoice* v) {
    switch (v->cell.effect) {
        case Effect_PatternBreak:
            p->break_pending = true;
            break;
        case Effect_PositionJump:
            p->break_pending = true;
            p->order = (uint8_t)(v->cell.param - 1);
            break;
        case Effect_SetVolume: {
            v->volume = v->cell.param > VOLUME_MAX ? VOLUME_MAX : v->cell.param;
            v->paula.volume = v->volume;
            break;
        }
        case Effect_SetSpeed: {
            const uint8_t speed = v->cell.param > SPEED_MAX ? SPEED_MAX : v->cell.param;
            p->speed = speed == 0 ? 1 : speed;
            break;
        }
        default:
            break; // E calls 68k code from the bank file
    }
}

static void play_voice_row(AscPlayer* p, AscVoice* v, uint8_t pattern, int index) {
    v->cell = read_cell(&p->module, cell_offset(pattern, p->row_offset, index));
    // An instrument number past the 31 of the module reads the replayer's variables in the original; it is skipped.
    if (v->cell.instrument != 0 && v->cell.instrument <= ASC_INSTRUMENTS) {
        assign_instrument(p, v, v->cell.instrument);
    }
    if (v->cell.period != 0) {
        if (v->am) {
            start_note(p, v);
        } else if (v->length_words == 0) {
            nuade_paula_channel_dma_off(&v->paula);
        } else if (is_tone_portamento(v->cell.effect)) {
            start_tone_portamento(v);
        } else {
            start_note(p, v);
        }
        if (v->dma_started) {
            v->started_period = v->cell.period;
            v->started_instrument = v->instrument;
        }
    }
    row_effect(p, v);
}

// After the DMA of the row's notes is on, every voice gets its repeat registers; the volume goes with them for a plain
// sample, and a synth voice on the last channel keeps its length.
static void write_repeats(AscPlayer* p) {
    for (int i = 0; i < ASC_VOICES; i++) {
        AscVoice* v = &p->voices[i];
        const bool last_voice = i == ASC_VOICES - 1;
        if (last_voice && v->am) {
            write_lc(p, &v->paula, v->loop);
        } else {
            write_lc_len(p, &v->paula, v->loop, v->loop_words);
        }
        if (!v->am) {
            v->paula.volume = v->volume;
        }
    }
}

// Next order position; the last one restarts at the song's restart position.
static void advance_order(AscPlayer* p) {
    p->break_pending = false;
    p->order = (uint8_t)((p->order + 1) & ORDER_MASK);
    if (p->order == p->song_length) {
        p->order = p->restart;
    }
}

// Plays the row; true if it was played before (the eagleplayer's SongEnd).
static bool play_row(AscPlayer* p) {
    const bool again = mark_row_visited(p);
    p->shown = (AscPosition) { .order = p->order, .row = p->row_offset / MOD_ROW_SIZE, .speed = p->speed };
    const uint8_t pattern = p->orders[p->order];

    for (int i = 0; i < ASC_VOICES; i++) {
        p->voices[i].dma_started = false;
    }
    for (int i = 0; i < ASC_VOICES; i++) {
        play_voice_row(p, &p->voices[i], pattern, i);
    }
    for (int i = 0; i < ASC_VOICES; i++) {
        if (p->voices[i].dma_started) {
            nuade_paula_channel_dma_on(&p->voices[i].paula);
        }
    }
    write_repeats(p);

    p->row_offset = (uint16_t)(p->row_offset + MOD_ROW_SIZE);
    if (p->row_offset == PATTERN_ROW_END || p->break_pending) {
        p->row_offset = 0;
        advance_order(p);
    }
    return again;
}

// The synth voices

// One step of an envelope slide towards `stage.level`; true once it is there.
static bool slide_level(AscVoice* v, AscAmStage stage) {
    if (v->am_level == stage.level) {
        return true;
    }
    if (v->am_level > stage.level) {
        v->am_level = (int16_t)(v->am_level - stage.rate);
        if (v->am_level > stage.level) {
            return false;
        }
    } else {
        v->am_level = (int16_t)(v->am_level + stage.rate);
        if (v->am_level < stage.level) {
            return false;
        }
    }
    v->am_level = stage.level;
    return true;
}

// The envelope of one tick. A voice whose release ends stops its DMA and is no longer a synth voice.
static void envelope_step(AscVoice* v, const AscAmInstrument* am) {
    switch (v->am_phase) {
        case AscAmPhase_Attack:
        case AscAmPhase_Decay:
        case AscAmPhase_Sustain: {
            const int stage = v->am_phase - AscAmPhase_Attack;
            if (slide_level(v, am->stages[stage])) {
                v->am_phase = (AscAmPhase)(v->am_phase + 1);
                if (v->am_phase == AscAmPhase_Hold) {
                    v->am_hold = am->hold_ticks;
                }
            }
            break;
        }
        case AscAmPhase_Hold:
            v->am_hold--;
            if (v->am_hold < 0) {
                v->am_phase = AscAmPhase_Release;
            }
            break;
        case AscAmPhase_Release:
            v->am_level = (int16_t)(v->am_level - am->release_rate);
            if (v->am_level < 0) {
                v->am = false;
                v->am_level = 0;
                v->am_phase = AscAmPhase_Off;
                nuade_paula_channel_dma_off(&v->paula);
            }
            break;
        case AscAmPhase_Off:
            break;
    }
}

// The vibrato of a synth voice: the depth times a sine, over a whole wave of the table.
static int16_t am_vibrato(const AscVoice* v, uint16_t depth) {
    uint16_t phase = v->am_vibrato_pos;
    bool negative = false;
    if (phase >= AM_VIBRATO_HALF) {
        phase = (uint16_t)(phase - AM_VIBRATO_HALF);
        negative = true;
    }
    // The table has 180 entries; a phase past it (a speed of more than a whole wave per tick) reads the replayer's
    // other data in the original and 0 here.
    const int16_t sine = phase / 2 < AM_SINE_SIZE ? k_am_sine[phase / 2] : 0;
    const int16_t value = (int16_t)((int16_t)(sine * (int16_t)depth) >> 7);
    return negative ? (int16_t)-value : value;
}

static void synth_voice_tick(AscVoice* v, const AscAmInstrument* am) {
    envelope_step(v, am);

    v->paula.volume = (uint8_t)scaled_volume(v, (uint16_t)v->am_level >> 2);

    v->base_period = (uint16_t)(v->base_period + am->period_slide);
    v->period = (uint16_t)(v->period + am->period_slide);

    int16_t vibrato_delta = 0;
    if (am->vibrato_depth != 0) {
        vibrato_delta = am_vibrato(v, am->vibrato_depth);
    }
    v->paula.period = (uint16_t)(v->base_period + vibrato_delta);

    v->am_vibrato_pos = (uint16_t)(v->am_vibrato_pos + am->vibrato_speed * 2);
    if (v->am_vibrato_pos >= AM_VIBRATO_PHASE_END) {
        v->am_vibrato_pos = (uint16_t)(v->am_vibrato_pos - AM_VIBRATO_PHASE_END);
    }
}

static void synth_voices_tick(AscPlayer* p) {
    for (int i = 0; i < ASC_VOICES; i++) {
        AscVoice* v = &p->voices[i];
        if (v->am && v->am_phase != AscAmPhase_Off) {
            synth_voice_tick(v, &p->am[v->instrument]);
        }
    }
}

// The noise wave form is made again each tick from the beam position the original reads at each byte; the beam is
// stood in for by a counter, so the noise is a different one.
static void regenerate_noise(AscPlayer* p) {
    int8_t* noise = &p->waves[WAVE_NOISE * ASC_WAVE_SIZE];
    uint16_t state = NOISE_SEED;
    for (int i = 0; i < ASC_WAVE_SIZE; i++) {
        noise[i] = (int8_t)(state & 0xFF);
        p->beam = (uint8_t)((p->beam + BEAM_STEP) % BEAM_LINE);
        state = (uint16_t)((state & 0xFF00) | (uint8_t)((state & 0xFF) + p->beam));
        state ^= NOISE_XOR;
        state = (uint16_t)((state << 3) | (state >> 13));
    }
}

// Creation

// Check2.
bool asc_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size < MOD_PATTERNS) {
        return false;
    }
    const AscBlob module = { data, size };
    const uint32_t tag = blob_u32(&module, MOD_TAG);
    if (tag != TAG_EXO4 && tag != TAG_FLT4) {
        return false;
    }
    for (int i = 0; i < CHECK_HEADERS; i++) {
        const AscSampleHeader header = read_sample_header(&module, i);
        if (header.length_words == 0 && header.volume != 0) {
            return true;
        }
    }
    return false;
}

// Does the pattern have a position jump (effect B)? The parameter of the first one is returned in `*param`. The scan
// addresses the pattern in 32 bit arithmetic, so unlike cell_offset it does not wrap.
static bool pattern_jump(const AscBlob* module, uint8_t pattern, uint8_t* param) {
    for (size_t cell = 0; cell < MOD_PATTERN_SIZE / MOD_CELL_SIZE; cell++) {
        const AscRawCell c
            = read_cell(module, MOD_PATTERNS + (size_t)pattern * MOD_PATTERN_SIZE + cell * MOD_CELL_SIZE);
        if (c.effect == Effect_PositionJump) {
            *param = c.param;
            return true;
        }
    }
    return false;
}

// The eagleplayer's subsong detection: the order after each position whose pattern has a position jump starts a
// subsong. A second jump to the same position as the first one means the song is a single loop.
static void find_subsongs(AscPlayer* p) {
    uint32_t count = 0;
    int8_t first_target = -1; // negative: none yet (a first target of 0x80 or more stays "none")
    p->subsong_starts[0] = 0;
    for (uint32_t position = 0; position < asc_position_count(p); position++) {
        uint8_t target = 0;
        if (pattern_jump(&p->module, p->orders[position], &target)) {
            if (first_target < 0) {
                first_target = (int8_t)target;
            } else if ((int8_t)target == first_target) {
                count = 1;
                break;
            }
            count++;
            p->subsong_starts[count] = (uint8_t)(position + 1);
        }
    }
    p->subsong_count = count == 0 ? 1 : count;
}

// The sample data follows the highest pattern of the order list (the original compares the order bytes as signed);
// each sample starts with 4 cleared bytes.
static void locate_samples(AscPlayer* p) {
    int8_t last_pattern = 0;
    for (int i = 0; i < ASC_MOD_ORDER_COUNT; i++) {
        last_pattern = (int8_t)p->orders[i] > last_pattern ? (int8_t)p->orders[i] : last_pattern;
    }
    uint32_t offset = MOD_PATTERNS + ((uint32_t)last_pattern + 1) * MOD_PATTERN_SIZE;
    for (int i = 0; i < ASC_INSTRUMENTS; i++) {
        p->sample_offsets[i] = offset;
        if ((size_t)offset + 4 <= p->module.size) {
            memset(p->module_copy + offset, 0, 4);
        }
        offset += (uint32_t)p->samples[i].length_words * 2;
    }
}

// The header of the module, decoded once.
static void read_header(AscPlayer* p) {
    for (int i = 0; i < MOD_TITLE_SIZE; i++) {
        p->title[i] = (char)blob_u8(&p->module, (size_t)i);
    }
    for (int i = 0; i < ASC_INSTRUMENTS; i++) {
        p->samples[i] = read_sample_header(&p->module, i);
    }
    for (int i = 0; i < ASC_ORDER_READ_COUNT; i++) {
        p->orders[i] = blob_u8(&p->module, MOD_ORDERS + (size_t)i);
    }
    p->song_length = blob_u8(&p->module, MOD_SONG_LENGTH);
    p->restart = blob_u8(&p->module, MOD_RESTART);
}

AscPlayer* asc_create(const uint8_t* module, size_t module_size, const uint8_t* bank, size_t bank_size) {
    if (!asc_probe(module, module_size) || bank == nullptr || bank_size == 0) {
        return nullptr;
    }
    AscPlayer* p = calloc(1, sizeof(AscPlayer));
    if (p == nullptr) {
        return nullptr;
    }
    p->module_copy = nuade_paula_padded_copy(module, module_size);
    if (p->module_copy == nullptr) {
        asc_destroy(p);
        return nullptr;
    }
    p->module = (AscBlob) { p->module_copy, module_size };
    const AscBlob bank_file = { bank, bank_size };
    for (uint32_t number = 1; number <= ASC_INSTRUMENTS; number++) {
        p->am[number] = read_am_instrument(&bank_file, number);
    }
    read_header(p);
    locate_samples(p);
    find_subsongs(p);
    return p;
}

void asc_destroy(AscPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module_copy);
    free(player);
}

uint32_t asc_subsong_count(const AscPlayer* player) {
    return player->subsong_count;
}

// Playing

// InitSound.
bool asc_start(AscPlayer* player, uint32_t subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong >= player->subsong_count) {
        return false;
    }
    memset(player->voices, 0, sizeof(player->voices));
    memset(player->visited, 0, sizeof(player->visited));
    memcpy(player->waves, k_waves, sizeof(k_waves));
    memset(&player->waves[WAVE_NOISE * ASC_WAVE_SIZE], 0, ASC_WAVE_SIZE);
    player->beam = 0;
    player->speed = SPEED_DEFAULT;
    player->counter = 0;
    player->row_offset = 0;
    player->break_pending = false;
    player->order = player->subsong_starts[subsong];
    player->shown = (AscPosition) { .order = player->order, .row = 0, .speed = SPEED_DEFAULT };
    for (uint32_t i = 0; i < ASC_VOICES; i++) {
        nuade_voice_stop(voices, i);
    }
    return true;
}

// The interrupt: effects between the rows, the row, then the synth voices and the noise.
AscTickResult asc_tick(AscPlayer* player, NuadeVoices* voices) {
    bool looped = false;
    player->counter++;
    if (player->counter < player->speed) {
        for (int i = 0; i < ASC_VOICES; i++) {
            tick_effect(player, &player->voices[i]);
        }
    } else {
        player->counter = 0;
        looped = play_row(player);
    }
    synth_voices_tick(player);
    regenerate_noise(player);
    for (int i = 0; i < ASC_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, (uint32_t)i, voices);
    }
    return (AscTickResult) { .looped = looped };
}

// Display

void asc_title(const AscPlayer* player, char out[ASC_TITLE_SIZE]) {
    memcpy(out, player->title, ASC_TITLE_SIZE);
}

void asc_sample_name(const AscPlayer* player, int index, char out[ASC_NAME_SIZE]) {
    out[0] = '\0';
    if (index < 0 || index >= ASC_INSTRUMENTS) {
        return;
    }
    memcpy(out, player->samples[index].name, ASC_NAME_SIZE);
}

AscPosition asc_position(const AscPlayer* player) {
    return player->shown;
}

uint32_t asc_position_count(const AscPlayer* player) {
    return player->song_length < ASC_MOD_ORDER_COUNT ? player->song_length : ASC_MOD_ORDER_COUNT;
}

AscVoiceState asc_voice_state(const AscPlayer* player, int voice) {
    const AscVoice* v = &player->voices[voice];
    return (AscVoiceState) { .period = v->started_period, .instrument = v->started_instrument };
}

AscCell asc_pattern_cell(const AscPlayer* player, uint32_t order, uint32_t channel, uint32_t row) {
    const uint8_t pattern = player->orders[order & (ASC_ORDER_READ_COUNT - 1)];
    const AscRawCell cell
        = read_cell(&player->module, cell_offset(pattern, (uint16_t)(row * MOD_ROW_SIZE), (int)channel));
    return (AscCell) {
        .period = cell.period, .instrument = cell.instrument, .effect = cell.effect, .effect_arg = cell.param
    };
}

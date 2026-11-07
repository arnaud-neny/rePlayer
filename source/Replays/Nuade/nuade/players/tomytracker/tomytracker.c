// SPDX-License-Identifier: MIT
// Tomy Tracker player, translated from the UADE eagleplayer "Tomy Tracker" (Wanted Team).
//
// TmyModule: long total size, long offset of the sample block, 31 instruments of 14 bytes, a word restart entry, a word
// order count, then the order list (words: byte offsets of 1 KB patterns) at 0x1C0 and the patterns from 0x2C0. The
// sample block follows the patterns. A row is 4 voices of 4 bytes: effect (4 * number), parameter, instrument * 7,
// note (2 * period table index). A row lasts `speed` ticks; the other ticks run the voice's effect.

#include <nuade/players/tomytracker.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_tracker.h"
#include <stdlib.h>
#include <string.h>

#define HEADER_SIZE_MIN 0x6C0 // the module must be larger
#define LENGTH_MAX 0x200000
#define SAMPLES_OFFSET_FIELD 4
#define INSTRUMENT_COUNT 31
#define INSTRUMENTS_OFFSET 8
#define INSTRUMENT_SIZE 14
#define RESTART_OFFSET 0x1BA
#define ORDER_COUNT_OFFSET 0x1BC // a word whose high byte must be 0 and whose low byte must be below 0x80
#define ORDER_LIST_OFFSET 0x1C0
#define ORDER_COUNT_MAX 0x7F
#define PATTERNS_OFFSET 0x2C0
#define PATTERN_SIZE 0x400
#define ROW_SIZE 16
#define CELL_SIZE 4
#define INSTRUMENT_STRIDE 7 // the instrument byte of a cell is the number * 7
#define EFFECT_STRIDE 4     // the effect byte is the number * 4
#define NOTE_STRIDE 2       // the note byte is the period table entry * 2

#define DEFAULT_SPEED 6
#define PATTERN_BREAK_OFFSET (PATTERN_SIZE - ROW_SIZE) // the row offset a pattern break sets

#define NOTE_COUNT 36
#define NOTE_MAX (NOTE_COUNT * NOTE_STRIDE) // the largest note byte
#define PERIOD_SLIDE_MIN 0x71
#define PERIOD_SLIDE_MAX 0x358
#define VOLUME_MAX 0x40
#define VOLUME_MASK 0x7F
#define VIBRATO_STEPS 32
#define ARPEGGIO_TICKS 32 // the arpeggio phase table the replayer reads has 32 entries
#define SAMPLE_WORDS_MAX 0x10000
#define MODULE_ADDRESS 0x10000 // where UADE loads the module
#define CHIP_MEMORY_SIZE 0x200000

// Walk limits of the open-time proof: rows simulated in total, and laps until the instrument state settles.
#define WALK_ROW_BUDGET 200000
#define WALK_LAP_MAX 8

// Periods of note bytes 2, 4 .. 72; byte 0 is no note.
static const uint16_t k_periods[NOTE_COUNT + 1] = {
    0,   856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320,
    302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
};

// A quarter sine, half a period, scaled to 255.
static const uint8_t k_vibrato[VIBRATO_STEPS] = {
    0x00, 0x18, 0x31, 0x4A, 0x61, 0x78, 0x8D, 0xA1, 0xB4, 0xC5, 0xD4, 0xE0, 0xEB, 0xF4, 0xFA, 0xFD,
    0xFF, 0xFD, 0xFA, 0xF4, 0xEB, 0xE0, 0xD4, 0xC5, 0xB4, 0xA1, 0x8D, 0x78, 0x61, 0x4A, 0x31, 0x18,
};

typedef struct Instrument {
    uint16_t volume;
    uint32_t start; // offsets from the sample block
    uint16_t length_words;
    uint32_t loop_start;
    uint16_t loop_length_words;
} Instrument;

// A cell as the pattern holds it (the effect and instrument bytes are scaled).
typedef struct RawCell {
    uint8_t effect;
    uint8_t param;
    uint8_t instrument;
    uint8_t note;
} RawCell;

// The period of a note byte (0 = none).
static int16_t period_of_note(uint8_t note_byte) {
    return (int16_t)k_periods[note_byte / NOTE_STRIDE];
}

static int semitone_of_note(uint8_t note_byte) {
    return (int)nuade_period_to_note((uint32_t)period_of_note(note_byte));
}

// TmyModule readers. Offsets are bounded by the open-time checks; a read outside the file gives 0.

static uint8_t module_u8(const TmyModule* m, uint64_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static uint16_t module_u16(const TmyModule* m, uint64_t offset) {
    return (uint16_t)(module_u8(m, offset) << 8 | module_u8(m, offset + 1));
}

static uint32_t module_u32(const TmyModule* m, uint64_t offset) {
    return (uint32_t)module_u16(m, offset) << 16 | module_u16(m, offset + 2);
}

static uint32_t module_samples_offset(const TmyModule* m) {
    return module_u32(m, SAMPLES_OFFSET_FIELD);
}

static uint32_t module_order_count(const TmyModule* m) {
    return module_u16(m, ORDER_COUNT_OFFSET);
}

static uint32_t module_restart(const TmyModule* m) {
    return module_u16(m, RESTART_OFFSET);
}

// The byte offset of the pattern of an order list entry, from the start of the pattern block.
static uint32_t module_order_pattern(const TmyModule* m, uint32_t order) {
    return module_u16(m, ORDER_LIST_OFFSET + (uint64_t)order * 2);
}

// `instrument` is 1-based.
static Instrument module_instrument(const TmyModule* m, uint32_t instrument) {
    const uint64_t o = INSTRUMENTS_OFFSET + (uint64_t)(instrument - 1) * INSTRUMENT_SIZE;
    return (Instrument) { .volume = module_u16(m, o),
                          .start = module_u32(m, o + 2),
                          .length_words = module_u16(m, o + 6),
                          .loop_start = module_u32(m, o + 8),
                          .loop_length_words = module_u16(m, o + 12) };
}

static RawCell module_cell(const TmyModule* m, uint32_t pattern, uint32_t row, uint32_t voice) {
    const uint64_t o = PATTERNS_OFFSET + (uint64_t)pattern + row * ROW_SIZE + voice * CELL_SIZE;
    return (RawCell) { .effect = module_u8(m, o),
                       .param = module_u8(m, o + 1),
                       .instrument = module_u8(m, o + 2),
                       .note = module_u8(m, o + 3) };
}

// The sample area an AUDxLC / AUDxLEN pair of the sample block reads. UADE loads the module at MODULE_ADDRESS and the
// chip memory behind it is zero, so a sample may run past the end of the file, but must start inside it and end
// inside the chip memory.
static bool sample_readable(const TmyModule* m, uint32_t offset, uint16_t length_words) {
    const uint64_t bytes = (length_words != 0 ? length_words : SAMPLE_WORDS_MAX) * 2ull;
    const uint64_t start = (uint64_t)module_samples_offset(m) + offset;
    return start <= m->size && MODULE_ADDRESS + start + bytes <= CHIP_MEMORY_SIZE;
}

// Paula ignores bit 0 of AUDxLC.
static const int8_t* sample_at(const TmyModule* m, uint32_t offset) {
    return (const int8_t*)m->data + ((module_samples_offset(m) + (uint64_t)offset) & ~1ull);
}

// The eagleplayer's check, plus the size the InitPlayer compares with the file.

static bool header_valid(const TmyModule* m) {
    if (m->size <= HEADER_SIZE_MIN) {
        return false;
    }
    const uint32_t length = module_u32(m, 0);
    const uint32_t samples = module_samples_offset(m);
    if (length == 0 || length > LENGTH_MAX || (length & 1) != 0 || length > m->size) {
        return false;
    }
    if (samples > length || (samples & 1) != 0 || samples < PATTERNS_OFFSET + PATTERN_SIZE
        || (samples - PATTERNS_OFFSET) % PATTERN_SIZE != 0) {
        return false;
    }
    if (module_u8(m, ORDER_COUNT_OFFSET) != 0) {
        return false;
    }
    const uint32_t count = module_order_count(m);
    if (count == 0 || count > ORDER_COUNT_MAX || module_restart(m) == 0 || module_restart(m) > count) {
        return false;
    }
    // The largest order list entry is the last pattern.
    uint32_t largest = 0;
    for (uint32_t i = 0; i < count; i++) {
        const uint32_t pattern = module_order_pattern(m, i);
        largest = pattern > largest ? pattern : largest;
    }
    return largest == (samples - PATTERNS_OFFSET) - PATTERN_SIZE;
}

// The song walk. The replayer reads tables by cell bytes without bounds, so a cell is checked before it is played.

typedef struct Walk {
    uint8_t instrument[TMY_VOICES]; // 1-based, 0 = none
    uint32_t rows_left;
    bool arpeggio;
    bool long_speed; // a speed above the arpeggio phase table
} Walk;

static bool effect_from_byte(uint8_t effect_byte, TmyEffect* out) {
    if (effect_byte % EFFECT_STRIDE != 0 || effect_byte / EFFECT_STRIDE >= TmyEffect_Count) {
        return false;
    }
    *out = (TmyEffect)(effect_byte / EFFECT_STRIDE);
    return true;
}

static bool is_tone_portamento(TmyEffect effect) {
    return effect == TmyEffect_TonePortamento || effect == TmyEffect_TonePortamento2;
}

// One row: updates the instruments and returns whether it proves out. `*pattern_break` tells that the row ends the
// pattern.

static bool walk_row(const TmyModule* m, Walk* w, uint32_t pattern, uint32_t row, bool* pattern_break) {
    *pattern_break = false;
    for (uint32_t v = 0; v < TMY_VOICES; v++) {
        const RawCell c = module_cell(m, pattern, row, v);
        TmyEffect effect;
        if (!effect_from_byte(c.effect, &effect) || c.note % NOTE_STRIDE != 0 || c.note > NOTE_MAX
            || c.instrument % INSTRUMENT_STRIDE != 0 || c.instrument / INSTRUMENT_STRIDE > INSTRUMENT_COUNT) {
            return false;
        }
        if (c.instrument != 0) {
            w->instrument[v] = c.instrument / INSTRUMENT_STRIDE;
        }
        if (c.note != 0 && !is_tone_portamento(effect)) {
            if (w->instrument[v] == 0) {
                return false;
            }
            const Instrument in = module_instrument(m, w->instrument[v]);
            if (!sample_readable(m, in.start, in.length_words)
                || !sample_readable(m, in.loop_start, in.loop_length_words)) {
                return false;
            }
        }
        switch (effect) {
            case TmyEffect_Arpeggio:
                w->arpeggio = true;
                break;
            case TmyEffect_PatternBreak:
                *pattern_break = true;
                break;
            case TmyEffect_Speed:
                if (c.param == 0) {
                    return false;
                }
                w->long_speed = w->long_speed || c.param > ARPEGGIO_TICKS;
                break;
            default:
                break;
        }
    }
    return true;
}

static bool walk_lap(const TmyModule* m, Walk* w, uint32_t first_order) {
    for (uint32_t order = first_order; order < module_order_count(m); order++) {
        const uint32_t pattern = module_order_pattern(m, order);
        for (uint32_t row = 0; row < TMY_ROWS; row++) {
            if (w->rows_left == 0) {
                return false;
            }
            w->rows_left--;
            bool pattern_break;
            if (!walk_row(m, w, pattern, row, &pattern_break)) {
                return false;
            }
            if (pattern_break) {
                break;
            }
        }
    }
    return true;
}

// Plays the song until the voices' instruments are the same at two loop points: the cells are the same from then on.

static bool song_valid(const TmyModule* m) {
    Walk w = { .rows_left = WALK_ROW_BUDGET };
    if (!walk_lap(m, &w, 0)) {
        return false;
    }
    const uint32_t restart_order = module_restart(m) - 1;
    for (int lap = 0; lap < WALK_LAP_MAX; lap++) {
        const Walk before = w;
        if (!walk_lap(m, &w, restart_order)) {
            return false;
        }
        if (memcmp(before.instrument, w.instrument, sizeof(w.instrument)) == 0) {
            return !(w.arpeggio && w.long_speed);
        }
    }
    return false;
}

bool tmy_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size > UINT32_MAX) {
        return false;
    }
    const TmyModule m = { .data = data, .size = (uint32_t)size };
    return header_valid(&m) && song_valid(&m);
}

TmyPlayer* tmy_create(const uint8_t* data, size_t size) {
    if (!tmy_probe(data, size)) {
        return nullptr;
    }
    TmyPlayer* p = calloc(1, sizeof(TmyPlayer));
    if (p == nullptr) {
        return nullptr;
    }
    p->owned = nuade_paula_padded_copy(data, size);
    if (p->owned == nullptr) {
        free(p);
        return nullptr;
    }
    p->module = (TmyModule) { .data = p->owned, .size = (uint32_t)size };
    return p;
}

void tmy_destroy(TmyPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->owned);
    free(player);
}

static void write_volume(TmyVoice* v) {
    v->paula.volume = (uint8_t)(v->volume & VOLUME_MASK);
}

static void write_period(TmyVoice* v, int16_t period) {
    v->paula.period = (uint16_t)period;
}

// Tick effects (the replayer's handlers run for each voice on the ticks between rows).

// Tone portamento: step the period towards the target and stop on it.
static void tick_tone_portamento(TmyVoice* v) {
    if (v->param != 0) {
        v->porta_speed = v->param;
        v->param = 0;
    }
    if (v->porta_target == 0) {
        return;
    }
    if (!v->porta_down) {
        v->period = (int16_t)(v->period + v->porta_speed);
        if (v->porta_target <= v->period) {
            v->period = v->porta_target;
            v->porta_target = 0;
        }
    } else {
        v->period = (int16_t)(v->period - v->porta_speed);
        if (v->porta_target >= v->period) {
            v->period = v->porta_target;
            v->porta_target = 0;
        }
    }
    write_period(v, v->period);
}

static void tick_slide_up(TmyVoice* v) {
    int16_t period = (int16_t)(v->period - v->param);
    if (period < PERIOD_SLIDE_MIN) {
        period = PERIOD_SLIDE_MIN;
    }
    v->period = period;
    write_period(v, period);
}

static void tick_slide_down(TmyVoice* v) {
    int16_t period = (int16_t)(v->period + v->param);
    if (period >= PERIOD_SLIDE_MAX) {
        period = PERIOD_SLIDE_MAX;
    }
    v->period = period;
    write_period(v, period);
}

static void tick_vibrato(TmyVoice* v) {
    if (v->param != 0) {
        v->vibrato_param = v->param;
    }
    const unsigned step = (v->vibrato_pos >> 2) & (VIBRATO_STEPS - 1);
    const int delta = (k_vibrato[step] * (v->vibrato_param & 0x0F)) >> 7;
    write_period(v, (int16_t)((v->vibrato_pos & 0x80) != 0 ? v->period - delta : v->period + delta));
    v->vibrato_pos = (uint8_t)(v->vibrato_pos + ((v->vibrato_param >> 2) & 0x3C));
}

// The low nibble of the parameter slides down, otherwise the high nibble slides up (to 64).
static void tick_volume_slide(TmyVoice* v) {
    const unsigned down = v->param & 0x0F;
    int16_t volume = (int16_t)v->volume;
    if (down != 0) {
        volume = (int16_t)(volume - (int)down);
        if (volume < 0) {
            volume = 0;
        }
    } else {
        volume = (int16_t)(volume + (v->param >> 4));
        // The replayer clamps on the sign bit of volume - 64, which differs from volume >= 64 above 0x7FFF.
        if (((uint16_t)(volume - VOLUME_MAX) & 0x8000) == 0) {
            volume = VOLUME_MAX;
        }
    }
    v->volume = (uint16_t)volume;
    write_volume(v);
}

// `since_row` ticks have passed since the row; phase 0 plays the note, 1 and 2 the nibbles of the parameter.
static void tick_arpeggio(TmyVoice* v, unsigned since_row) {
    const unsigned phase = since_row % 3; // the replayer's table repeats 0, 1, 2
    if (phase == 0) {
        write_period(v, v->period);
        return;
    }
    const unsigned semitones = phase == 1 ? v->param >> 4 : v->param & 0x0F;
    unsigned note = semitones * NOTE_STRIDE + v->note;
    if (note > NOTE_MAX) {
        note = NOTE_MAX;
    }
    write_period(v, period_of_note((uint8_t)note));
}

static void tick_effects(TmySong* s) {
    for (uint32_t i = 0; i < TMY_VOICES; i++) {
        TmyVoice* v = &s->voices[i];
        switch (v->effect) {
            case TmyEffect_TonePortamento:
            case TmyEffect_TonePortamento2:
                tick_tone_portamento(v);
                break;
            case TmyEffect_SlideUp:
                tick_slide_up(v);
                break;
            case TmyEffect_SlideDown:
                tick_slide_down(v);
                break;
            case TmyEffect_VibratoVolumeSlide:
                tick_vibrato(v);
                tick_volume_slide(v);
                break;
            case TmyEffect_Vibrato:
                tick_vibrato(v);
                break;
            case TmyEffect_VolumeSlide:
                tick_volume_slide(v);
                break;
            case TmyEffect_Arpeggio:
                tick_arpeggio(v, (unsigned)(s->speed - s->counter));
                break;
            default:
                break;
        }
    }
}

// Row effects that act while the row is decoded.

static void row_effect(TmySong* s, TmyVoice* v) {
    switch (v->effect) {
        case TmyEffect_SetVolume:
            v->volume = (uint16_t)((v->volume & 0xFF00) | v->param);
            break;
        case TmyEffect_PatternBreak:
            s->row_offset = PATTERN_BREAK_OFFSET;
            break;
        case TmyEffect_Speed:
            s->counter = v->param;
            s->speed = v->param;
            break;
        default:
            break; // Nop and Filter
    }
}

// A note: the sample goes to the channel registers and the loop is kept for after the DMA start.

static void start_note(const TmyModule* m, TmyVoice* v, uint8_t note) {
    const Instrument in = module_instrument(m, v->instrument);
    v->paula.lc = sample_at(m, in.start);
    v->paula.len_words = in.length_words;
    v->loop_start = sample_at(m, in.loop_start);
    v->loop_length_words = in.loop_length_words;
    v->period = period_of_note(note);
    v->note = note;
    v->vibrato_pos = 0;
    v->start_dma = true;
    v->started = true;
}

// A tone portamento note sets the target and leaves the sound running.

static void set_portamento(TmyVoice* v, uint8_t note) {
    const int16_t target = period_of_note(note);
    v->porta_down = false;
    v->porta_speed = (uint8_t)(v->volume >> 8);
    v->porta_target = target;
    if (target == v->period) {
        v->porta_target = 0;
    } else if (target < v->period) {
        v->porta_down = true;
    }
}

static void decode_voice(const TmyModule* m, TmySong* s, uint32_t pattern, uint32_t row, uint32_t voice) {
    TmyVoice* v = &s->voices[voice];
    const RawCell c = module_cell(m, pattern, row, voice);
    TmyEffect effect = TmyEffect_None;
    effect_from_byte(c.effect, &effect);
    v->effect = effect;
    v->param = c.param;
    if (c.instrument != 0) {
        v->instrument = c.instrument / INSTRUMENT_STRIDE;
        v->volume = module_instrument(m, v->instrument).volume;
    }
    if (c.note == 0) {
        row_effect(s, v);
        write_period(v, v->period);
        write_volume(v);
        return;
    }
    if (is_tone_portamento(effect)) {
        set_portamento(v, c.note);
        write_volume(v);
        return;
    }
    start_note(m, v, c.note);
    row_effect(s, v);
    write_period(v, v->period);
    write_volume(v);
}

// The end of a pattern: the next order list entry, or the restart entry after the last one.
static void next_pattern(const TmyModule* m, TmySong* s) {
    s->row_offset = 0;
    if (s->order + 1 == module_order_count(m)) {
        s->order = module_restart(m) - 1;
        s->looped = true;
    } else {
        s->order++;
    }
}

static void play_row(const TmyModule* m, TmySong* s) {
    s->counter = s->speed;
    s->shown_order = (int)s->order;
    const uint32_t row = s->row_offset / ROW_SIZE; // a pattern break in this row only moves the next one
    s->shown_row = (int)row;
    const uint32_t pattern = module_order_pattern(m, s->order);
    for (uint32_t i = 0; i < TMY_VOICES; i++) {
        s->voices[i].start_dma = false;
        decode_voice(m, s, pattern, row, i);
    }
    // The DMA of the new notes is switched off, then on again after the registers are set; the loop follows.
    for (uint32_t i = 0; i < TMY_VOICES; i++) {
        if (s->voices[i].start_dma) {
            nuade_paula_channel_dma_off(&s->voices[i].paula);
        }
    }
    s->row_offset += ROW_SIZE;
    if (s->row_offset == PATTERN_SIZE) {
        next_pattern(m, s);
    }
    for (uint32_t i = 0; i < TMY_VOICES; i++) {
        TmyVoice* v = &s->voices[i];
        if (v->start_dma) {
            nuade_paula_channel_dma_on(&v->paula);
        }
        v->paula.lc = v->loop_start;
        v->paula.len_words = v->loop_length_words;
    }
}

// InitSound
bool tmy_start(TmyPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    TmySong* s = &player->song;
    *s = (TmySong) { .counter = 1, .speed = DEFAULT_SPEED };
    for (uint32_t i = 0; i < TMY_VOICES; i++) {
        nuade_voice_stop(voices, i);
    }
    return true;
}

TmyTickResult tmy_tick(TmyPlayer* player, NuadeVoices* voices) {
    TmySong* s = &player->song;
    s->looped = false;
    s->counter--;
    if (s->counter == 0) {
        play_row(&player->module, s);
    } else {
        tick_effects(s);
    }
    for (uint32_t i = 0; i < TMY_VOICES; i++) {
        nuade_paula_channel_sync(&s->voices[i].paula, i, voices);
    }
    return (TmyTickResult) { .looped = s->looped };
}

TmyPosition tmy_position(const TmyPlayer* player) {
    const TmySong* s = &player->song;
    return (TmyPosition) { .order = s->shown_order, .row = s->shown_row, .speed = s->speed };
}

int tmy_order_count(const TmyPlayer* player) {
    return (int)module_order_count(&player->module);
}

TmyVoiceState tmy_voice_state(const TmyPlayer* player, int voice) {
    const TmyVoice* v = &player->song.voices[voice];
    return (TmyVoiceState) { .note = v->started ? semitone_of_note(v->note) : -1,
                             .instrument = v->instrument != 0 ? v->instrument - 1 : -1 };
}

TmyCell tmy_pattern_cell(const TmyPlayer* player, int order, int voice, int row) {
    const TmyModule* m = &player->module;
    const RawCell c = module_cell(m, module_order_pattern(m, (uint32_t)order), (uint32_t)row, (uint32_t)voice);
    TmyEffect effect = TmyEffect_None;
    effect_from_byte(c.effect, &effect);
    return (TmyCell) { .note = c.note != 0 && c.note <= NOTE_MAX ? semitone_of_note(c.note) : -1,
                       .instrument = c.instrument / INSTRUMENT_STRIDE,
                       .effect = (uint8_t)effect,
                       .param = c.param };
}

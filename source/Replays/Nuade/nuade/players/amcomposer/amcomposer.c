// SPDX-License-Identifier: MIT
// AM-Composer player (Thomas Porschen), translated from the UADE eagleplayer "AM-Composer" (Wanted Team).
//
// AmcModule (all values big-endian):
//   0x00  "AMC V1.2 REPLAY!"
//   0x10  word  steps in the song
//   0x18  8 longs: the note stream of each voice, then the effect stream of each voice (offsets in the file)
//   0x38  byte at 0x39: the CIA timer of the tick (see tick_length)
//   0x48  the instruments, 16 bytes each: sample offset (long), length in words, loop offset (long), loop length in
//         words, and 4 unused bytes. Sample and loop offsets count from the start of the file; the samples follow.
//
// A note stream entry is {step word, instrument byte, volume byte, period word}; an effect stream entry is {step
// word, command byte, arguments}. Both are sorted by step. A stream is only read when its next entry has the step
// being played, so an entry that is behind the song (equal steps, a missing terminator) stops its stream until the
// song wraps. The song advances one step every fourth tick.

#include <nuade/players/amcomposer.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>
#include <string.h>

#define MAGIC "AMC V1.2 REPLAY!"
#define MAGIC_SIZE 16
#define STEPS_OFFSET 0x10
#define STREAMS_OFFSET 0x18
#define TIMER_OFFSET 0x39
#define INSTRUMENTS_OFFSET 0x48
#define INSTRUMENT_SIZE 16

// The eagleplayer stores the timer byte in the high byte of DeliTracker's timer word: it counts in 256 CIA cycles.
#define TIMER_UNIT 256

typedef enum EffectCommand {
    EffectCommand_SetPeriod = 1,
    EffectCommand_Slide = 2,
    EffectCommand_SetVolume = 3,
    EffectCommand_Arpeggio = 4,
    EffectCommand_Filter = 7,
} EffectCommand;

// One instrument record as the notes address it. The loop is the sample the channel repeats after its first pass.
typedef struct Instrument {
    const int8_t* sample;
    uint16_t length_words;
    const int8_t* loop;
    uint16_t loop_words;
} Instrument;

// A read position in the module. A read past the end gives 0xFF bytes: the step 0xFFFF that no step matches.
typedef struct Cursor {
    const AmcModule* module;
    uint32_t pos;
} Cursor;

static uint16_t read_u16_at(const AmcModule* m, size_t pos) {
    if (pos > m->size || m->size - pos < 2) {
        return 0xFFFF;
    }
    return (uint16_t)((m->data[pos] << 8) | m->data[pos + 1]);
}

static uint32_t read_u32_at(const AmcModule* m, size_t pos) {
    return ((uint32_t)read_u16_at(m, pos) << 16) | read_u16_at(m, pos + 2);
}

static uint8_t cursor_u8(Cursor* c) {
    const uint32_t pos = c->pos++;
    return pos < c->module->size ? c->module->data[pos] : 0xFF;
}

static uint16_t cursor_u16(Cursor* c) {
    const uint16_t value = read_u16_at(c->module, c->pos);
    c->pos += 2;
    return value;
}

static uint32_t cursor_u32(Cursor* c) {
    const uint32_t value = read_u32_at(c->module, c->pos);
    c->pos += 4;
    return value;
}

// AUDxLC for a sample at a file offset, null when it starts past the file (the original would play whatever memory is
// there). Any AUDxLEN can follow it: what runs past the file plays the zeros behind the padded copy.
static const int8_t* sample_at(const AmcModule* m, uint32_t offset) {
    return offset <= m->size ? (const int8_t*)(m->data + offset) : nullptr;
}

static Instrument instrument_at(const AmcModule* m, uint32_t offset) {
    Cursor c = { m, offset };
    const uint32_t sample_offset = cursor_u32(&c);
    const uint16_t length = cursor_u16(&c);
    const uint32_t loop_offset = cursor_u32(&c);
    const uint16_t loop_length = cursor_u16(&c);
    return (Instrument) { .sample = sample_at(m, sample_offset),
                          .length_words = length,
                          .loop = sample_at(m, loop_offset),
                          .loop_words = loop_length };
}

// The offset of instrument `number` (1-based) as the replayer forms it: 16-bit arithmetic, so 0 reads far past the
// table.

static uint32_t instrument_offset(uint8_t number) {
    const uint16_t index = (uint16_t)(number - 1);
    return INSTRUMENTS_OFFSET + (uint16_t)(index << 4);
}

// The replayer's volume scaling (balance and master volume at their default 64, so the value passes through) keeps
// the product's low word: only ten bits of a 16-bit effect volume survive. The result is the AUDxVOL write; the
// channel applies Paula's 7-bit rule.

static uint8_t scaled_volume(uint16_t value) {
    return (uint8_t)(((uint32_t)value * 64u & 0xFFFFu) >> 6);
}

// Number of 16-byte instruments from the table start to the first stream, or -1 if the table does not fit.
static int instrument_table_count(const uint8_t* data, size_t size) {
    if (size < INSTRUMENTS_OFFSET) {
        return -1;
    }
    const AmcModule probe = { .data = data, .size = size };
    const uint32_t table_end = read_u32_at(&probe, STREAMS_OFFSET);
    if (table_end < INSTRUMENTS_OFFSET || table_end > size || (table_end - INSTRUMENTS_OFFSET) % INSTRUMENT_SIZE != 0) {
        return -1;
    }
    return (int)((table_end - INSTRUMENTS_OFFSET) / INSTRUMENT_SIZE);
}

bool amc_probe(const uint8_t* data, size_t size) {
    if (size < STREAMS_OFFSET + 4 || memcmp(data, MAGIC, MAGIC_SIZE) != 0) {
        return false;
    }
    // Check2 also wants the high word of the first stream offset to be zero
    const AmcModule probe = { .data = data, .size = size };
    return (read_u32_at(&probe, STREAMS_OFFSET) >> 16) == 0;
}

AmcPlayer* amc_create(const uint8_t* data, size_t size) {
    if (!amc_probe(data, size)) {
        return nullptr;
    }
    const int count = instrument_table_count(data, size);
    if (count < 0) {
        return nullptr;
    }
    AmcPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    player->chip = nuade_paula_padded_copy(data, size);
    if (player->chip == nullptr) {
        free(player);
        return nullptr;
    }
    AmcModule* m = &player->module;
    m->data = player->chip;
    m->size = size;
    m->instrument_count = count;
    m->steps = read_u16_at(m, STEPS_OFFSET);
    m->timer = data[TIMER_OFFSET];
    for (int i = 0; i < AMC_STREAMS_PER_KIND; i++) {
        m->note_streams[i] = read_u32_at(m, STREAMS_OFFSET + 4 * (uint32_t)i);
        m->effect_streams[i] = read_u32_at(m, STREAMS_OFFSET + 4 * (uint32_t)(AMC_STREAMS_PER_KIND + i));
    }
    // InitPlayer: no sample may end past the file. A zero timer never ticks.
    for (int i = 0; i < count; i++) {
        const uint32_t offset = INSTRUMENTS_OFFSET + INSTRUMENT_SIZE * (uint32_t)i;
        const uint64_t end = (uint64_t)read_u32_at(m, offset) + 2u * read_u16_at(m, offset + 4);
        if (end > size) {
            amc_destroy(player);
            return nullptr;
        }
    }
    if (m->timer == 0) {
        amc_destroy(player);
        return nullptr;
    }
    return player;
}

void amc_destroy(AmcPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->chip);
    free(player);
}

int amc_instrument_count(const AmcPlayer* player) {
    return player->module.instrument_count;
}

AmcVoiceState amc_voice_state(const AmcPlayer* player, int voice) {
    const AmcVoice* v = &player->voice[voice];
    return (AmcVoiceState) { .period = v->last_period, .instrument = v->last_instrument };
}

// The streams start over: at the song start and when the step counter reaches the end.
static void rewind_streams(AmcPlayer* p) {
    for (int i = 0; i < AMC_VOICES; i++) {
        p->voice[i].note_pos = p->module.note_streams[i];
        p->voice[i].effect_pos = p->module.effect_streams[i];
    }
    p->step = 0;
    p->step_ticks = 0;
}

// InitSound

bool amc_start(AmcPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    memset(player->voice, 0, sizeof(player->voice));
    rewind_streams(player);
    for (int i = 0; i < AMC_VOICES; i++) {
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

// Effect 2 per tick: the period steps every `rate` ticks.
static void run_slide(AmcVoice* v) {
    AmcSlide* s = &v->slide;
    if (s->remaining == 0) {
        return;
    }
    s->remaining--;
    if (s->remaining == 0) {
        return;
    }
    s->countdown--;
    if (s->countdown == 0) {
        s->countdown = s->rate;
        s->period = (uint16_t)(s->period - s->delta);
        v->paula.period = s->period;
    }
}

// Effect 4 per tick: the next of the eight periods.

static void run_arpeggio(AmcVoice* v) {
    AmcArpeggio* a = &v->arpeggio;
    if (a->ticks == 0) {
        return;
    }
    a->ticks--;
    v->paula.period = a->periods[a->index];
    a->index = (uint8_t)((a->index + 1) % AMC_ARPEGGIO_NOTES);
}

// A note: the channel is switched off with the new sample, volume and period written; its DMA starts on the next tick.
static void start_note(AmcVoice* v, const AmcModule* m, Cursor* c) {
    v->arpeggio.ticks = 0;
    v->slide.remaining = 0;
    nuade_paula_channel_dma_off(&v->paula);
    v->started = true;

    const uint8_t number = cursor_u8(c);
    const Instrument instrument = instrument_at(m, instrument_offset(number));
    v->paula.lc = instrument.sample;
    v->paula.len_words = instrument.length_words;
    v->loop = instrument.loop;
    v->loop_words = instrument.loop_words;
    v->paula.volume = scaled_volume(cursor_u8(c));
    v->paula.period = cursor_u16(c);
    v->last_instrument = number;
    v->last_period = v->paula.period;
}

static void run_note_stream(AmcVoice* v, const AmcModule* m, uint16_t step) {
    Cursor c = { m, v->note_pos };
    if (cursor_u16(&c) != step) {
        return;
    }
    start_note(v, m, &c);
    v->note_pos = c.pos;
}

static void run_effect(AmcVoice* v, Cursor* c) {
    switch (cursor_u8(c)) {
        case EffectCommand_SetPeriod:
            cursor_u8(c);
            v->paula.period = cursor_u16(c);
            break;
        case EffectCommand_Slide:
            v->arpeggio.ticks = 0;
            v->slide.rate = cursor_u8(c);
            v->slide.countdown = v->slide.rate;
            v->slide.delta = cursor_u16(c);
            v->slide.remaining = cursor_u16(c);
            v->slide.period = cursor_u16(c);
            break;
        case EffectCommand_SetVolume:
            cursor_u8(c);
            v->paula.volume = scaled_volume(cursor_u16(c));
            break;
        case EffectCommand_Arpeggio:
            v->slide.remaining = 0;
            v->arpeggio.ticks = cursor_u8(c);
            for (int i = 0; i < AMC_ARPEGGIO_NOTES; i++) {
                v->arpeggio.periods[i] = cursor_u16(c);
            }
            v->arpeggio.index = 1; // the first period is skipped
            break;
        case EffectCommand_Filter:
            // The LED filter bit of CIA A. The nuade voices have no filter, and UADE runs with it off.
            cursor_u8(c);
            break;
        default:
            // Any other command has no arguments and the stream goes on after its byte.
            break;
    }
}

static void run_effect_stream(AmcVoice* v, const AmcModule* m, uint16_t step) {
    Cursor c = { m, v->effect_pos };
    if (cursor_u16(&c) != step) {
        return;
    }
    run_effect(v, &c);
    v->effect_pos = c.pos;
}

// The step part of the tick: the streams of every voice for the new step. True when the song wrapped.
static bool run_step(AmcPlayer* p) {
    bool looped = false;
    p->step++;
    if (p->step == p->module.steps) {
        looped = true;
        rewind_streams(p);
    }
    for (int i = 0; i < AMC_VOICES; i++) {
        run_note_stream(&p->voice[i], &p->module, p->step);
    }
    for (int i = 0; i < AMC_VOICES; i++) {
        run_effect_stream(&p->voice[i], &p->module, p->step);
    }
    return looped;
}

// Interrupt. Order of work: DMA on for the notes started on the last step, slides and arpeggios, then (after the
// replayer's raster wait) the repeat of those notes, then the sequencer.
static bool play_tick(AmcPlayer* p) {
    for (int i = 0; i < AMC_VOICES; i++) {
        if (p->voice[i].started) {
            nuade_paula_channel_dma_on(&p->voice[i].paula);
        }
    }
    for (int i = 0; i < AMC_VOICES; i++) {
        run_slide(&p->voice[i]);
        run_arpeggio(&p->voice[i]);
    }
    for (int i = 0; i < AMC_VOICES; i++) {
        AmcVoice* v = &p->voice[i];
        if (v->started) {
            v->paula.lc = v->loop;
            v->paula.len_words = v->loop_words;
            v->started = false;
        }
    }
    p->step_ticks++;
    if (p->step_ticks != AMC_TICKS_PER_STEP) {
        return false;
    }
    p->step_ticks = 0;
    return run_step(p);
}

AmcTickResult amc_tick(AmcPlayer* player, NuadeVoices* voices) {
    const bool looped = play_tick(player);
    for (int i = 0; i < AMC_VOICES; i++) {
        nuade_paula_channel_sync(&player->voice[i].paula, (uint32_t)i, voices);
    }
    return (AmcTickResult) { .length = (uint32_t)player->module.timer * TIMER_UNIT, .looped = looped };
}

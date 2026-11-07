// SPDX-License-Identifier: MIT
// Andrew Parton player, translated from the UADE eagleplayer "Andrew Parton player module V1.1" (Wanted Team).
//
// A module ("BANK" file) is a header of six tables with 20 entries each (sample start flags, sample lengths, loop
// lengths, loop offsets, then 4 envelope bytes and 4 vibrato bytes per instrument), the samples (a 16 byte header
// and the data each, in table order, only for the entries whose flag is set) and the event stream.
//
// The events are MIDI-like: a status byte (kept as running status), one or two data bytes and a delay byte in ticks.
// 0x9n is a note on (note, velocity; velocity 0 is a note off), 0x8n a note off, 0xCn sets the instrument of channel
// n (program 0x7E in 0xCn restarts the song), 0xFF ends it. Channel n plays on voice n & 3. A voice runs a note
// through attack, decay to the sustain level and release, with a vibrato after a delay.

#include <nuade/players/andrewparton.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>

#define PROGRAM_MASK 0x1F
#define NOTE_MASK 0x7F

// ApModule layout. The tables follow the magic one after the other; the sample area starts after them.
#define MAGIC 0x42414E4Bu // "BANK"
#define TABLES_OFFSET 4
#define TABLE_ENTRY_BYTES \
    4 // every table has 4 bytes per instrument (envelope: attack, decay, sustain, release;
      // vibrato: delay, depth, speed, unused)
#define SAMPLE_HEADER_SIZE 0x10

// Check2 limits.
#define CHECK_START_LIMIT 0x200000u
#define CHECK_LENGTH_LIMIT 0x10000u
#define CHECK_SIZE (TABLES_OFFSET + AP_INSTRUMENT_COUNT * TABLE_ENTRY_BYTES * 3)

// The event stream ends at the first 0xFF byte, which must be followed by '/'.
#define STREAM_END_BYTE 0xFF
#define STREAM_END_MARK '/'

// The first byte of the event stream is not an event; the loop point is the byte after it, and a loop restarts one
// byte past that.
#define STREAM_FIRST_EVENT 1
#define LOOP_RESTART_SKIP 1

// Events.
#define STATUS_BIT 0x80
#define STATUS_END 0xFF // the end event, compared with the whole status byte
#define KIND_MASK 0xF0
#define KIND_NOTE_OFF 0x80
#define KIND_NOTE_ON 0x90
#define KIND_PROGRAM 0xC0
#define KIND_SKIP_DATA 0xE0 // one more data byte, ignored
#define PROGRAM_RESTART 0x7E
#define VELOCITY_SHIFT 1

// A tick that runs more events than this (zero delays in a loop) is a broken stream: the sequencer stops.
#define MAX_EVENTS_PER_TICK 65536

// ApEnvelope levels are signed bytes; the volume register takes the low 6 bits.
#define LEVEL_MAX 0x3F
#define SUSTAIN_SHIFT 6

#define PERIOD_COUNT 72
#define FIRST_NOTE 0x30
#define NOTE_C0 51 // the note byte of C-0 (period 1712)
#define VIBRATO_STEPS 32

#define SILENCE_WORDS 2

// Periods from note 0x30. A note outside the table makes the original read its own variables and code; no song does,
// and such a note plays silent here.
static const uint16_t k_periods[PERIOD_COUNT] = {
    2032, 1920, 1808, 1712, 1616, 1520, 1440, 1360, 1280, 1200, 1136, 1072, 1016, 960, 904, 856, 808, 760,
    720,  680,  640,  600,  568,  536,  508,  480,  452,  428,  404,  380,  360,  340, 320, 300, 284, 268,
    254,  240,  226,  214,  202,  190,  180,  170,  160,  150,  142,  134,  127,  120, 113, 107, 101, 95,
    90,   85,   80,   75,   71,   67,   63,   60,   56,   53,   50,   47,   45,   42,  40,  37,  35,  33,
};

// One vibrato cycle in period steps per unit of depth.
static const int8_t k_vibrato[VIBRATO_STEPS] = {
    0, 1, 1, 2, 3, 4, 4, 5, 5, 4, 3, 2, 2, 1, 1, 0, 0, 0, -1, -1, -2, -3, -4, -4, -5, -4, -4, -3, -2, -2, -1, -1,
};

// What a sample without a loop repeats: the four zero bytes where the module's magic was (the original clears it).
static const int8_t k_silence[SILENCE_WORDS * 2] = { 0 };

// The header tables, each one entry per instrument; a table follows the one before it.
typedef enum Table {
    Table_Present,
    Table_Length,
    Table_LoopLength,
    Table_LoopStart,
    Table_Envelope,
    Table_Vibrato,
    Table_Count,
} Table;

#define HEADER_SIZE (TABLES_OFFSET + Table_Count * AP_INSTRUMENT_COUNT * TABLE_ENTRY_BYTES)

// ApModule reads; the original reads whatever memory follows the module, here it is zero.
static uint8_t module_u8(const ApModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static uint32_t module_u32(const ApModule* m, uint32_t offset) {
    return (uint64_t)offset + 4 <= m->size ? nuade_read_be32(m->data + offset) : 0;
}

bool ap_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size < CHECK_SIZE || nuade_read_be32(data) != MAGIC) {
        return false;
    }
    const uint8_t* p = data + TABLES_OFFSET;
    for (int i = 0; i < AP_INSTRUMENT_COUNT; i++, p += 4) {
        if (nuade_read_be32(p) >= CHECK_START_LIMIT) {
            return false;
        }
    }
    for (int i = 0; i < 2 * AP_INSTRUMENT_COUNT; i++, p += 4) {
        if (nuade_read_be32(p) >= CHECK_LENGTH_LIMIT) {
            return false;
        }
    }
    return true;
}

static uint32_t table_offset(Table table, int instrument) {
    return TABLES_OFFSET + ((uint32_t)table * AP_INSTRUMENT_COUNT + (uint32_t)instrument) * TABLE_ENTRY_BYTES;
}

// One instrument of the header; its sample data starts at `sample` if it has one.
static ApInstrument read_instrument(const ApModule* m, int index, uint32_t sample) {
    const uint32_t envelope = table_offset(Table_Envelope, index);
    const uint32_t vibrato = table_offset(Table_Vibrato, index);
    return (ApInstrument) { .has_sample = module_u32(m, table_offset(Table_Present, index)) != 0,
                            .sample = sample,
                            .length = module_u32(m, table_offset(Table_Length, index)),
                            .loop_length = module_u32(m, table_offset(Table_LoopLength, index)),
                            .loop_start = module_u32(m, table_offset(Table_LoopStart, index)),
                            .envelope = { .attack = module_u8(m, envelope),
                                          .decay = module_u8(m, envelope + 1),
                                          .sustain = module_u8(m, envelope + 2),
                                          .release = module_u8(m, envelope + 3) },
                            .vibrato = { .delay = module_u8(m, vibrato),
                                         .depth = module_u8(m, vibrato + 1),
                                         .speed = module_u8(m, vibrato + 2) } };
}

// InitPlayer: the samples must lie inside the module, and the event stream after them ends in 0xFF '/'.
static bool load_module(ApModule* m, const uint8_t* data, size_t size) {
    if (size < HEADER_SIZE || size > UINT32_MAX / 2) {
        return false;
    }
    m->data = nuade_paula_padded_copy(data, size);
    if (m->data == nullptr) {
        return false;
    }
    m->size = (uint32_t)size;

    uint64_t at = HEADER_SIZE;
    for (int i = 0; i < AP_INSTRUMENT_COUNT; i++) {
        const bool has_sample = module_u32(m, table_offset(Table_Present, i)) != 0;
        if (has_sample) {
            at += SAMPLE_HEADER_SIZE;
        }
        m->instruments[i] = read_instrument(m, i, (uint32_t)at);
        if (has_sample) {
            at += m->instruments[i].length;
        }
        if (at > m->size) {
            return false;
        }
    }
    m->stream = (uint32_t)at;
    for (uint32_t i = m->stream; i + 1 < m->size; i++) {
        if (m->data[i] == STREAM_END_BYTE) {
            return m->data[i + 1] == STREAM_END_MARK;
        }
    }
    return false;
}

ApPlayer* ap_create(const uint8_t* data, size_t size) {
    if (!ap_probe(data, size)) {
        return nullptr;
    }
    ApPlayer* p = calloc(1, sizeof(ApPlayer));
    if (p == nullptr) {
        return nullptr;
    }
    if (!load_module(&p->module, data, size)) {
        ap_destroy(p);
        return nullptr;
    }
    return p;
}

void ap_destroy(ApPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module.data);
    free(player);
}

// The instrument a program names. Programs past the 20 table entries read the header's other tables in the original
// (no song sets one); here they are an instrument without a sample.
static const ApInstrument* instrument_of(const ApModule* m, uint8_t program) {
    static const ApInstrument k_empty = { 0 };
    const uint8_t index = program & PROGRAM_MASK;
    return index < AP_INSTRUMENT_COUNT ? &m->instruments[index] : &k_empty;
}

// AUDxLC for a sample (or its loop) at a module offset, null if it starts past the module. Any AUDxLEN can follow it:
// what runs past the module plays the zeros behind the padded copy.
static const int8_t* module_samples(const ApModule* m, uint64_t offset) {
    return offset <= m->size ? (const int8_t*)m->data + offset : nullptr;
}

static uint16_t period_of_note(uint8_t note) {
    return note >= FIRST_NOTE && note - FIRST_NOTE < PERIOD_COUNT ? k_periods[note - FIRST_NOTE] : 0;
}

// A note with velocity 0 (or 1): the voice goes to the release phase.
static void note_off(ApVoice* v) {
    v->phase = ApPhase_Release;
}

// Starts a note: the voice restarts on the instrument's sample; once DMA has latched it, the loop (or silence) is
// queued. The original waits 9 raster lines after the DMA start before it writes the loop registers.
static void note_on(const ApModule* m, ApVoice* v, uint8_t note, uint8_t half_velocity, uint8_t program) {
    const ApInstrument* inst = instrument_of(m, program);
    v->phase = ApPhase_Attack;
    v->level = 0;
    v->peak = (int8_t)half_velocity;
    v->envelope = inst->envelope;
    v->envelope.sustain = (uint8_t)((inst->envelope.sustain * half_velocity) >> SUSTAIN_SHIFT);
    v->vibrato = inst->vibrato;
    v->vibrato_index = 0;
    v->period = period_of_note(note);
    v->note = note > NOTE_C0 ? note - NOTE_C0 : 0;
    v->instrument = program & PROGRAM_MASK;

    nuade_paula_channel_dma_off(&v->paula);
    v->paula.lc = inst->has_sample ? module_samples(m, inst->sample) : nullptr;
    v->paula.len_words = (uint16_t)(inst->length >> 1);
    v->paula.period = v->period;
    nuade_paula_channel_dma_on(&v->paula);
    if (inst->loop_length == 0) {
        v->paula.lc = k_silence;
        v->paula.len_words = SILENCE_WORDS;
    } else {
        v->paula.lc = inst->has_sample ? module_samples(m, (uint64_t)inst->sample + inst->loop_start) : nullptr;
        v->paula.len_words = (uint16_t)(inst->loop_length >> 1);
    }
}

// A note event: the note and the velocity are 7 bit; velocity 0 or 1 releases the voice.
static void note_event(ApPlayer* p, uint8_t channel, uint8_t note, uint8_t velocity) {
    ApVoice* v = &p->voices[channel % AP_VOICES];
    const uint8_t half_velocity = (velocity & 0x7F) >> VELOCITY_SHIFT;
    if (half_velocity == 0) {
        note_off(v);
    } else {
        note_on(&p->module, v, note & NOTE_MASK, half_velocity, p->sequencer.programs[channel]);
    }
}

// The events up to the next one with a delay. The 0xB0 events are no-ops here: the original steps its data base
// pointer by one for them and corrupts its own variables; no song uses one. Returns true if the song is over: the end
// event ran, or the stream ran out (or never delays) and the sequencer stops.
static bool run_events(ApPlayer* p) {
    const ApModule* m = &p->module;
    ApSequencer* s = &p->sequencer;
    uint32_t at = s->position;
    bool delayed = false;
    bool ended = false;
    for (int events = 0; !delayed && events < MAX_EVENTS_PER_TICK && at < m->size; events++) {
        uint8_t data = module_u8(m, at++);
        if (data & STATUS_BIT) {
            s->status = data;
            data = module_u8(m, at++);
        }
        const uint8_t kind = s->status & KIND_MASK;
        const uint8_t channel = s->status & (AP_CHANNEL_COUNT - 1);
        if (kind == KIND_NOTE_ON) {
            note_event(p, channel, data, module_u8(m, at++));
        } else if (kind == KIND_NOTE_OFF) {
            at++;
            note_off(&p->voices[channel % AP_VOICES]);
        } else if (kind == KIND_PROGRAM) {
            if (data == PROGRAM_RESTART) {
                at = s->loop_position + LOOP_RESTART_SKIP;
                continue;
            }
            s->programs[channel] = data;
        } else if (s->status == STATUS_END) {
            s->running = false;
            ended = true;
        } else if (kind == KIND_SKIP_DATA) {
            at++;
        }
        s->wait = module_u8(m, at++);
        delayed = s->wait != 0;
    }
    s->position = at;
    if (!delayed) {
        s->running = false; // the stream ran out without a delay (or never stops): nothing more to play
        ended = true;
    }
    return ended;
}

// Returns true on the tick the song ends.
static bool sequencer_tick(ApPlayer* p) {
    ApSequencer* s = &p->sequencer;
    if (!s->running) {
        return false;
    }
    s->wait--;
    return s->wait == 0 && run_events(p);
}

static int8_t level_add(int8_t level, uint8_t rate) {
    return (int8_t)(uint8_t)(level + rate);
}

// A level that fell below zero stops at zero.
static int8_t level_sub(int8_t level, uint8_t rate) {
    const int8_t result = (int8_t)(uint8_t)(level - rate);
    return result < 0 ? 0 : result;
}

// One tick of the envelope. Returns true if the volume register is written: the sustain and off phases leave it.
static bool envelope_tick(ApVoice* v) {
    const int8_t sustain = (int8_t)v->envelope.sustain;
    switch (v->phase) {
        case ApPhase_Attack:
            v->level = level_add(v->level, v->envelope.attack);
            if (v->level > LEVEL_MAX) {
                v->level = LEVEL_MAX;
            }
            if (v->peak <= v->level) {
                v->phase = ApPhase_Decay;
                v->level = v->peak;
            }
            return true;
        case ApPhase_Decay:
            if (sustain == v->level) {
                v->phase = ApPhase_Sustain;
                return false;
            }
            if (sustain > v->level) {
                v->level = level_add(v->level, v->envelope.decay);
                if (sustain <= v->level) {
                    v->level = sustain;
                }
            } else {
                v->level = level_sub(v->level, v->envelope.decay);
                if (sustain >= v->level) {
                    v->level = sustain;
                }
            }
            return true;
        case ApPhase_Release:
            v->level = level_sub(v->level, v->envelope.release);
            if (v->level == 0) {
                v->phase = ApPhase_Off;
            }
            return true;
        case ApPhase_Sustain:
        case ApPhase_Off:
            break;
    }
    return false;
}

// After its delay the vibrato writes the period every tick, with or without depth.
static void vibrato_tick(ApVoice* v) {
    if (v->vibrato.delay != 0) {
        v->vibrato.delay--;
        return;
    }
    v->paula.period = (uint16_t)(v->period + k_vibrato[v->vibrato_index] * v->vibrato.depth);
    v->vibrato_index = (uint8_t)((v->vibrato_index + v->vibrato.speed) % VIBRATO_STEPS);
}

static void voice_tick(ApVoice* v) {
    vibrato_tick(v);
    if (envelope_tick(v)) {
        v->paula.volume = (uint8_t)(v->level & LEVEL_MAX);
    }
}

// InitSound
bool ap_start(ApPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    player->sequencer = (ApSequencer) { .running = true,
                                        .position = player->module.stream + STREAM_FIRST_EVENT,
                                        .loop_position = player->module.stream + STREAM_FIRST_EVENT,
                                        .wait = 1 };
    for (int i = 0; i < AP_VOICES; i++) {
        player->voices[i] = (ApVoice) { .note = -1, .instrument = -1 };
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

// The interrupt: the sequencer, then the voices.
ApTickResult ap_tick(ApPlayer* player, NuadeVoices* voices) {
    const bool ended = sequencer_tick(player);
    for (int i = 0; i < AP_VOICES; i++) {
        voice_tick(&player->voices[i]);
        nuade_paula_channel_sync(&player->voices[i].paula, (uint32_t)i, voices);
    }
    return (ApTickResult) { .ended = ended };
}

ApVoiceState ap_voice_state(const ApPlayer* player, int voice) {
    const ApVoice* v = &player->voices[voice];
    return (ApVoiceState) { .note = v->note, .instrument = v->instrument };
}

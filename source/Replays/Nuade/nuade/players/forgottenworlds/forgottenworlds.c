// SPDX-License-Identifier: MIT
// Forgotten Worlds player, translated from the UADE eagleplayer "ForgottenWorlds_Game" (the sound driver of the
// Amiga game; the replayer is in the eagleplayer, the modules hold data only).
//
// FwModule: the magic "FWMP", three longs (offsets from DATA_BASE of the pattern table, the instrument table and the
// sample table), then at DATA_BASE the song table (four words, one per voice: the distance from DATA_BASE to the
// voice's command stream), and the tables and streams that the offsets name.
//
// A voice reads commands from its stream: a note (a byte below 0x80: pitch, then a length in ticks), or a command
// byte 0x80..0x8a (end, loop, instrument, rest, call a pattern, return). A note starts the instrument's sample and its
// volume envelope; the envelope then runs once per tick: attack to 0x40, decay to the sustain level, hold, release.
//
// The tick starts the voices in order. The end command leaves the tick at once, so the voices after the one that
// ended do not run in that tick.
//
// The eagleplayer's volume and balance scaling is the identity at its default settings and left out (UADE writes
// level * 64 >> 6). The eagleplayer never calls SongEnd; looped and ended are reported on the commands (see
// FwTickResult).

#include <nuade/players/forgottenworlds.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

// FwModule layout.
#define MAGIC_HEADER 0x46574D50u // "FWMP"
#define DATA_BASE 0x10
#define SONG_TABLE_SIZE (2 * FW_VOICES)

// Guards: commands one voice may read in a tick (the original loops for ever on a stream without a note or a rest).
#define COMMANDS_PER_TICK 4096

#define INSTRUMENT_SIZE 8 // the instrument number is a multiple of this
#define SAMPLE_BYTE_MASK 0xFF
#define PATTERN_ENTRY_SIZE 2

#define ATTACK_LIMIT 0x40
#define LEVEL_RELEASED 0x80 // bit 7 of the level after a release step: the sound has faded out

// The Paula registers InitSound leaves: the silent block that every voice plays, with this period.
#define IDLE_PERIOD 0x7C
#define IDLE_WORDS 2

typedef enum Command {
    Command_End = 0x80,
    Command_Loop = 0x82,
    Command_Instrument = 0x84,
    Command_Rest = 0x86,
    Command_Call = 0x88,
    Command_Return = 0x8A,
} Command;

// The periods of notes 0..46 (C-0 is 1712; the last entry is 0).
static const uint16_t k_periods[] = {
    0x6B0, 0x650, 0x5F0, 0x5A0, 0x550, 0x500, 0x4B8, 0x478, 0x438, 0x3F8, 0x3C0, 0x388, 0x358, 0x328, 0x2F8, 0x2D0,
    0x2A8, 0x280, 0x25C, 0x23C, 0x21C, 0x1FC, 0x1E0, 0x1C4, 0x1AC, 0x194, 0x17C, 0x168, 0x154, 0x140, 0x12E, 0x11E,
    0x10E, 0x0FE, 0x0F0, 0x0E2, 0x0D6, 0x0CA, 0x0BE, 0x0B4, 0x0AA, 0x0A0, 0x097, 0x08F, 0x087, 0x07F, 0x000,
};

// The block the voices play when idle.
static const int8_t k_idle_block[IDLE_WORDS * 2];

// FwModule reads. A read outside the file gives 0.

static uint8_t module_u8(const FwModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static uint16_t module_u16(const FwModule* m, uint32_t offset) {
    return (uint16_t)((module_u8(m, offset) << 8) | module_u8(m, offset + 1));
}

// `base` moved by the signed word at `offset` (the original adds the word to an address register, sign extended).
static uint32_t word_relative(const FwModule* m, uint32_t base, uint32_t offset) {
    return base + (uint32_t)(int32_t)(int16_t)module_u16(m, offset);
}

// The sample data at `offset`; null if it starts outside the file.
static const int8_t* module_sample(const FwModule* m, uint32_t offset) {
    return offset < m->size ? (const int8_t*)(m->data + offset) : nullptr;
}

bool fw_probe(const uint8_t* data, size_t size) {
    return size >= DATA_BASE + SONG_TABLE_SIZE && nuade_read_be32(data) == MAGIC_HEADER;
}

FwPlayer* fw_create(const uint8_t* data, size_t size) {
    if (!fw_probe(data, size)) {
        return nullptr;
    }
    FwPlayer* player = (FwPlayer*)calloc(1, sizeof(FwPlayer));
    if (player == nullptr) {
        return nullptr;
    }
    FwModule* m = &player->module;
    m->data = nuade_paula_padded_copy(data, size);
    if (m->data == nullptr) {
        free(player);
        return nullptr;
    }
    m->original = malloc(size);
    if (m->original == nullptr) {
        free(m->data);
        free(player);
        return nullptr;
    }
    memcpy(m->original, data, size);
    m->size = size;
    m->patterns = DATA_BASE + nuade_read_be32(data + 4);
    m->instruments = DATA_BASE + nuade_read_be32(data + 8);
    m->samples = DATA_BASE + nuade_read_be32(data + 12);
    return player;
}

void fw_destroy(FwPlayer* player) {
    if (player != nullptr) {
        free(player->module.data);
        free(player->module.original);
    }
    free(player);
}

// Records

static FwInstrument read_instrument(const FwModule* m, uint32_t at) {
    return (FwInstrument) { .sample = module_u8(m, at),
                            .sustain = module_u8(m, at + 1),
                            .attack = module_u8(m, at + 2),
                            .decay = module_u8(m, at + 3),
                            .hold = module_u8(m, at + 4),
                            .release = module_u8(m, at + 5) };
}

// The word at 0 is the distance of the sample from the sample table, the word at 4 the distance of the repeat block
// from the sample (both signed).
static FwSample read_sample(const FwModule* m, uint32_t at) {
    const uint32_t start = word_relative(m, m->samples, at);
    return (FwSample) { .start = start,
                        .words = module_u16(m, at + 2),
                        .loop = word_relative(m, start, at + 4),
                        .loop_words = module_u16(m, at + 6) };
}

// Paula

// Restarts the channel with the voice's sample: the sample plays once, then its repeat block. The original waits for
// the audio interrupt of the DMA start before it writes the repeat.
static void start_sample(const FwModule* m, FwVoice* v) {
    nuade_paula_channel_dma_off(&v->paula);
    v->paula.lc = module_sample(m, v->sample.start);
    v->paula.len_words = v->sample.words;
    v->paula.period = v->period;
    v->paula.volume = v->level;
    nuade_paula_channel_dma_on(&v->paula);
    v->paula.lc = module_sample(m, v->sample.loop);
    v->paula.len_words = v->sample.loop_words;
}

// Envelope

static void run_envelope_stage(FwVoice* v) {
    const FwInstrument* ins = &v->instrument;
    switch (v->stage) {
        case FwStage_Attack:
            v->level = (uint8_t)(v->level + ins->attack);
            if (v->level >= ATTACK_LIMIT) {
                v->level = ATTACK_LIMIT;
                v->stage = FwStage_Decay;
            }
            break;
        case FwStage_Decay:
            v->level = (uint8_t)(v->level + ins->decay);
            if (v->level <= ins->sustain) {
                v->level = ins->sustain;
                v->hold_left = ins->hold;
                v->stage = FwStage_Hold;
            }
            break;
        case FwStage_Hold:
            if (--v->hold_left == 0) {
                v->stage = FwStage_Release;
            }
            break;
        case FwStage_Release:
            v->level = (uint8_t)(v->level + ins->release);
            if ((v->level & LEVEL_RELEASED) != 0) {
                v->level = 0;
                v->stage = FwStage_Done;
            }
            break;
        case FwStage_Done:
            break;
    }
}

// An instrument with a hold time of 0 has no envelope: its level stays and the volume register is not written.
static void run_envelope(FwVoice* v) {
    if (v->instrument.hold == 0) {
        return;
    }
    run_envelope_stage(v);
    v->paula.volume = v->level;
}

// Commands

typedef enum StepResult {
    StepResult_Continue, // read the next command
    StepResult_Done,     // the voice is done for this tick
    StepResult_EndTick,  // the end command: the tick stops here, for the voices after this one too
    StepResult_Stall,    // the stream is broken
} StepResult;

// A note: pitch, then the length. The instrument's start level and the attack stage restart.
static StepResult start_note(const FwModule* m, FwVoice* v, uint8_t note, uint32_t* pos) {
    // The original reads past its period table (the eagleplayer's own image) for a note above the table, and the
    // instrument record at address 0 for a note before the first instrument command; no module does either.
    if (note >= nuade_sizeof_array(k_periods) || !v->has_instrument) {
        return StepResult_Stall;
    }
    v->shown_note = note;
    v->period = k_periods[note];
    v->ticks = module_u8(m, *pos);
    v->stream = *pos + 1;
    v->level = v->instrument.attack;
    v->stage = FwStage_Attack;
    start_sample(m, v);
    return StepResult_Done;
}

// The instrument command: the instrument number (a signed byte), then the sample it names. The instrument number's
// word (number * 8) keeps its high byte when the sample offset byte is loaded, so numbers of 32 and above add it to
// the sample offset.
static void select_instrument(FwPlayer* p, FwVoice* v, uint32_t* pos) {
    const FwModule* m = &p->module;
    const int8_t number = (int8_t)module_u8(m, (*pos)++);
    const int16_t table_offset = (int16_t)(number * INSTRUMENT_SIZE);
    v->has_instrument = true;
    v->instrument_number = number;
    v->instrument = read_instrument(m, m->instruments + (uint32_t)(int32_t)table_offset);
    const int16_t sample_offset = (int16_t)((table_offset & ~SAMPLE_BYTE_MASK) | v->instrument.sample);
    v->sample = read_sample(m, m->samples + (uint32_t)(int32_t)sample_offset);
    p->end_target_known = true;
    p->end_target = v->sample.loop;
}

// The call command: a signed pattern number; the pattern table holds the distance of each pattern from the table.
// There is one return address.
static void call_pattern(const FwModule* m, FwVoice* v, uint32_t* pos) {
    const int8_t number = (int8_t)module_u8(m, (*pos)++);
    v->has_return = true;
    v->return_to = *pos;
    *pos = word_relative(m, m->patterns, m->patterns + (uint32_t)(int32_t)(number * PATTERN_ENTRY_SIZE));
}

// The end command writes a zero word at the repeat block of the last instrument command run in this tick. An odd
// address is an address error on the 68000 and is not modelled.
static void clear_end_target(FwPlayer* p) {
    FwModule* m = &p->module;
    if (p->end_target_known && (p->end_target & 1) == 0 && p->end_target <= m->size - 2) {
        m->data[p->end_target] = 0;
        m->data[p->end_target + 1] = 0;
    }
}

static StepResult step_command(FwPlayer* p, FwVoice* v, uint32_t* pos) {
    const FwModule* m = &p->module;
    const uint8_t first = module_u8(m, (*pos)++);
    if (first < Command_End) {
        return start_note(m, v, first, pos);
    }
    switch (first) {
        case Command_End:
            v->ended = true;
            v->wrapped = true;
            clear_end_target(p);
            return StepResult_EndTick;
        case Command_Loop:
            v->wrapped = true;
            *pos = v->loop_start;
            return StepResult_Continue;
        case Command_Instrument:
            select_instrument(p, v, pos);
            return StepResult_Continue;
        case Command_Rest:
            v->ticks = module_u8(m, (*pos)++);
            v->stream = *pos;
            return StepResult_Done;
        case Command_Call:
            call_pattern(m, v, pos);
            return StepResult_Continue;
        case Command_Return:
            if (!v->has_return) {
                return StepResult_Stall;
            }
            *pos = v->return_to;
            return StepResult_Continue;
        default:
            // The original jumps through an odd or out-of-range table entry
            return StepResult_Stall;
    }
}

// The tick

// Runs one voice; false when the tick ends here.
static bool run_voice(FwPlayer* p, FwVoice* v) {
    if (v->stalled) {
        return true;
    }
    if (--v->ticks != 0) {
        run_envelope(v);
        return true;
    }
    uint32_t pos = v->stream;
    StepResult result = StepResult_Continue;
    for (int steps = 0; result == StepResult_Continue; steps++) {
        result = steps < COMMANDS_PER_TICK ? step_command(p, v, &pos) : StepResult_Stall;
    }
    if (result == StepResult_Stall) {
        v->stalled = true;
    }
    return result != StepResult_EndTick;
}

FwTickResult fw_tick(FwPlayer* player, NuadeVoices* voices) {
    player->end_target_known = false;
    // The tick starts with DMACON set for all four channels.
    for (int i = 0; i < FW_VOICES; i++) {
        nuade_paula_channel_dma_on(&player->voices[i].paula);
    }
    for (int i = 0; i < FW_VOICES; i++) {
        if (!run_voice(player, &player->voices[i])) {
            break;
        }
    }
    bool all_wrapped = true;
    bool all_ended = true;
    for (int i = 0; i < FW_VOICES; i++) {
        FwVoice* v = &player->voices[i];
        nuade_paula_channel_sync(&v->paula, (uint32_t)i, voices);
        all_wrapped = all_wrapped && (v->wrapped || v->stalled);
        all_ended = all_ended && (v->ended || v->stalled);
    }
    if (all_wrapped) {
        for (int i = 0; i < FW_VOICES; i++) {
            player->voices[i].wrapped = false;
        }
    }
    return (FwTickResult) { .looped = all_wrapped, .ended = all_ended };
}

void fw_start(FwPlayer* player, NuadeVoices* voices) {
    FwModule* m = &player->module;
    memcpy(m->data, m->original, m->size);
    for (int i = 0; i < FW_VOICES; i++) {
        const uint32_t start = DATA_BASE + module_u16(m, DATA_BASE + 2u * (uint32_t)i);
        // The idle block is what InitSound writes to the channels
        player->voices[i]
            = (FwVoice) { .loop_start = start,
                          .stream = start,
                          .ticks = 1,
                          .instrument_number = -1,
                          .shown_note = -1,
                          .paula = { .lc = k_idle_block, .len_words = IDLE_WORDS, .period = IDLE_PERIOD } };
        nuade_voice_stop(voices, (uint32_t)i);
    }
}

FwVoiceState fw_voice_state(const FwPlayer* player, int voice) {
    const FwVoice* v = &player->voices[voice];
    return (FwVoiceState) { .note = v->shown_note, .instrument = v->instrument_number };
}

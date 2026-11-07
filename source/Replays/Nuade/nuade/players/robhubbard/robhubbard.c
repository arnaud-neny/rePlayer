// SPDX-License-Identifier: MIT
// Rob Hubbard player, translated from the replayers inside the "*.rh" modules and the UADE eagleplayer "RobHubbard",
// which checks the module, patches its sequence data and calls the replayer's init and tick.
//
// The replayer is a small sample player: four voices, each reading a list of tracks (streams of notes and commands)
// and playing instruments (a sample with a repeat, a volume, a base period, and in the later builds a vibrato and a
// "ferris wheel" that rewrites the sample while it plays). It exists in several builds that differ in the commands they
// know and in details of the note start; rh_module.c recognises the build and describes it as a RhVariant.
//
// What the eagleplayer does that changes the replayer's output:
//   - every 0x85 byte in the sequence data becomes 0x84, so the stop command never runs from the data;
//   - the next-track command is wrapped: when a voice runs off the end of its track list, the eagleplayer clears that
//     voice's flag, and when all four are clear it calls SongEnd and sets them again;
//   - the instrument volume writes go through a master volume that is 1 at full volume.
// The remaining patches only record what the replayer writes to the audio registers.
//
// Order of work in a tick: the voices from 3 down to 0. Each voice first writes its repeat registers if its note was
// started in the tick before, then counts its length down. At 0 it reads commands until a note or a rest; at 1 it
// switches its DMA off (the gap between two notes); otherwise it runs its effects (slide, vibrato, ferris wheel).

#include <nuade/players/robhubbard.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_tracker.h"
#include "rh_module.h"
#include <stdlib.h>

#define PERIOD_SHIFT 10  // period = table period * base period >> 10
#define FERRIS_FILL 0x3C // what the ferris wheel writes when it turns and on the way down; 0 on the way up
#define VOLUME_MASK 0x7F // the volume the eagleplayer's master volume passes on

bool rh_probe(const uint8_t* data, size_t size) {
    RhModule* module = rh_module_open(data, size);
    rh_module_close(module);
    return module != nullptr;
}

RhPlayer* rh_create(const uint8_t* data, size_t size) {
    RhPlayer* player = (RhPlayer*)calloc(1, sizeof(RhPlayer));
    if (player == nullptr) {
        return nullptr;
    }
    player->module = rh_module_open(data, size);
    if (player->module == nullptr) {
        free(player);
        return nullptr;
    }
    return player;
}

void rh_destroy(RhPlayer* player) {
    if (player != nullptr) {
        rh_module_close(player->module);
    }
    free(player);
}

int rh_subsong_count(const RhPlayer* player) {
    return (int)player->module->subsong_count;
}

int rh_subsong_tempo(const RhPlayer* player, int subsong) {
    const RhModule* m = player->module;
    return rh_module_read_subsong(m, rh_module_played_subsong(m, (uint32_t)subsong)).tempo;
}

// Module reads

static const int8_t* sample_at(const RhModule* m, uint32_t offset) {
    return offset < m->size ? (const int8_t*)(m->data + offset) : nullptr;
}

static void set_silence(const RhModule* m, NuadePaulaChannel* paula) {
    paula->lc = sample_at(m, m->silence);
    paula->len_words = m->variant->silence_words;
}

// Starting a subsong

bool rh_start(RhPlayer* player, int subsong, NuadeVoices* voices) {
    RhModule* m = player->module;
    if (subsong < 0 || (uint32_t)subsong >= m->subsong_count) {
        return false;
    }
    rh_module_reset(m);
    // The default instrument comes from the subsong that was asked for, the music from the one the init plays
    const RhSubsong played = rh_module_read_subsong(m, rh_module_played_subsong(m, (uint32_t)subsong));
    player->subsong = (uint32_t)subsong;
    player->tempo = played.tempo;
    for (int i = 0; i < RH_VOICES; i++) {
        const uint32_t list = played.track_list[i];
        player->voices[i] = (RhVoice) {
            .stream = rh_module_u32(m, list),
            .track_list = list,
            .track_offset = 4,
            .length = 1,
            .shown_note = -1,
            .shown_instrument = -1,
        };
        nuade_voice_stop(voices, (uint32_t)i);
    }
    player->running = true;
    player->song_ended = false;
    return true;
}

// Note start and commands

// The period of a note: the period table entry scaled by the instrument's base period.
static uint16_t note_period(const RhModule* m, uint8_t note, uint16_t base_period) {
    const uint32_t table_period = rh_module_u16(m, m->periods + 2u * note);
    return (uint16_t)((table_period * base_period) >> PERIOD_SHIFT);
}

static void start_note(RhPlayer* p, RhVoice* v, uint8_t length) {
    const RhModule* m = p->module;
    const RhInstrument ins = rh_module_read_instrument(m, v->instrument);
    v->length = (uint16_t)(length * p->tempo);
    v->note = rh_module_u8(m, v->stream++);
    v->ferris_at = ins.ferris_min;
    v->wave_at = v->wave_start;
    v->paula.lc = sample_at(m, ins.sample);
    v->paula.len_words = ins.words;
    v->paula.volume = (uint8_t)((v->volume_override != 0 ? v->volume_override : ins.volume) & VOLUME_MASK);
    v->period = note_period(m, v->note, ins.base_period);
    v->paula.period = v->period;
    nuade_paula_channel_dma_on(&v->paula);
    v->repeat_pending = true;
    v->shown_note = nuade_period_to_note(v->period);
}

// Command 0x80: the voice's instrument, and in the extended builds its vibrato wave.
static void select_instrument(RhPlayer* p, RhVoice* v, uint8_t operand) {
    const RhModule* m = p->module;
    const RhVariant* variant = m->variant;
    const uint32_t index = rh_module_resolve_instrument(m, p->subsong, operand);
    v->instrument = rh_module_instrument(m, index);
    v->shown_instrument = (int)index;
    if (variant->extended_instruments) {
        v->wave_start = m->waves + (uint32_t)(int32_t)rh_module_read_instrument(m, v->instrument).wave;
        v->wave_at = v->wave_start;
    }
}

// The eagleplayer's SongEnd: reached when every voice has run off the end of its track list once.
static void report_wrap(RhPlayer* p, RhVoice* v) {
    v->wrapped = true;
    for (int i = 0; i < RH_VOICES; i++) {
        if (!p->voices[i].wrapped) {
            return;
        }
    }
    p->song_ended = true;
    for (int i = 0; i < RH_VOICES; i++) {
        p->voices[i].wrapped = false;
    }
}

// Command 0x84: the next stream of the track list; the end of the list starts it over.
static void next_track(RhPlayer* p, RhVoice* v) {
    const RhModule* m = p->module;
    uint32_t entry = v->track_list + v->track_offset;
    v->track_offset += 4;
    if (rh_module_u32(m, entry) == 0) {
        report_wrap(p, v);
        entry = v->track_list;
        v->track_offset = 4;
    }
    v->stream = rh_module_u32(m, entry);
}

typedef enum StepResult {
    StepResult_Continue, // read the next command
    StepResult_Done,     // the voice is done for this tick
    StepResult_Stop,     // the replayer has stopped
} StepResult;

static StepResult step_command(RhPlayer* p, RhVoice* v, uint8_t byte) {
    const RhModule* m = p->module;
    const RhVariant* variant = m->variant;
    if (byte < RH_COMMAND_FIRST) {
        start_note(p, v, byte);
        return StepResult_Done;
    }
    switch (byte) {
        case RhCommand_Instrument:
            select_instrument(p, v, rh_module_u8(m, v->stream++));
            return StepResult_Continue;
        case RhCommand_Slide:
            v->slide_step = (int8_t)rh_module_u8(m, v->stream++);
            v->slide = true;
            return StepResult_Continue;
        case RhCommand_Rest:
            v->length = (uint16_t)(rh_module_u8(m, v->stream++) * p->tempo);
            set_silence(m, &v->paula);
            return StepResult_Done;
        case RhCommand_NextTrack:
            next_track(p, v);
            return StepResult_Continue;
        default:
            break;
    }
    if (byte == RhCommand_Legato && variant->legato_command) {
        v->legato = true;
    } else if (byte == RhCommand_Stop && variant->stop_command) {
        return StepResult_Stop;
    } else if (byte == variant->volume_command) {
        v->volume_override = rh_module_u8(m, v->stream++);
    }
    return StepResult_Continue;
}

static StepResult read_commands(RhPlayer* p, RhVoice* v) {
    v->slide = false;
    v->ferris_down = false;
    v->legato = false;
    for (int steps = 0; steps < RH_COMMANDS_PER_TICK; steps++) {
        const StepResult result = step_command(p, v, rh_module_u8(p->module, v->stream++));
        if (result != StepResult_Continue) {
            return result;
        }
    }
    // The replayer would read commands for ever; rh_module_open refuses the modules that do
    v->halted = true;
    return StepResult_Done;
}

// Effects

// The vibrato adds period / divisor times the next wave byte to the period.
static void run_vibrato(const RhModule* m, RhVoice* v, const RhInstrument* ins) {
    if (ins->vibrato_divisor == 0) {
        return;
    }
    const uint16_t step = (uint16_t)(v->period / ins->vibrato_divisor);
    uint8_t wave = rh_module_u8(m, v->wave_at++);
    if (wave == RH_VIBRATO_RESTART) {
        v->wave_at = v->wave_start;
        wave = rh_module_u8(m, v->wave_at++);
    }
    const int32_t offset = (int32_t)(int8_t)wave * (int32_t)(int16_t)step;
    v->paula.period = (uint16_t)(v->period + (uint16_t)offset);
}

// The ferris wheel moves a write position up and down between the instrument's limits and writes into the sample:
// 0 on the way up, 0x3C on the way down.
static void run_ferris_wheel(RhModule* m, RhVoice* v, const RhInstrument* ins) {
    if (ins->ferris_max == 0) {
        return;
    }
    uint16_t at = v->ferris_at;
    uint8_t fill = 0;
    if (!v->ferris_down) {
        at++;
        if (at > ins->ferris_max) {
            v->ferris_down = true;
            fill = FERRIS_FILL;
        }
    } else {
        at--;
        if (at > ins->ferris_min) {
            fill = FERRIS_FILL;
        } else {
            v->ferris_down = false;
        }
    }
    rh_module_store_u8(m, ins->sample + (uint32_t)(int32_t)(int16_t)at, fill);
    v->ferris_at = at;
}

static void run_effects(RhPlayer* p, RhVoice* v) {
    RhModule* m = p->module;
    if (v->slide) {
        v->period = (uint16_t)(v->period + v->slide_step);
        v->paula.period = v->period;
    }
    if (!m->variant->extended_instruments) {
        return;
    }
    const RhInstrument ins = rh_module_read_instrument(m, v->instrument);
    run_vibrato(m, v, &ins);
    run_ferris_wheel(m, v, &ins);
}

// The tick

// The repeat registers of the note that started in the tick before: from the instrument's repeat offset on, or silence.
static void set_repeat(const RhModule* m, RhVoice* v) {
    const RhVariant* variant = m->variant;
    v->repeat_pending = false;
    const RhInstrument ins = rh_module_read_instrument(m, v->instrument);
    if (ins.repeat == 0) {
        set_silence(m, &v->paula);
    } else if (ins.repeat > 0 || !variant->skip_negative_loop) {
        v->paula.lc = sample_at(m, ins.sample + (uint32_t)ins.repeat);
        v->paula.len_words = (uint16_t)(ins.words - (((uint32_t)ins.repeat & 0xFFFF) >> 1));
    }
}

static StepResult run_voice(RhPlayer* p, RhVoice* v) {
    const RhModule* m = p->module;
    if (v->halted) {
        return StepResult_Done;
    }
    if (v->repeat_pending) {
        set_repeat(m, v);
    }
    v->length--;
    if (v->length == 0) {
        return read_commands(p, v);
    }
    if (v->length == 1 && !v->legato) {
        nuade_paula_channel_dma_off(&v->paula);
    }
    run_effects(p, v);
    return StepResult_Done;
}

RhTickResult rh_tick(RhPlayer* player, NuadeVoices* voices) {
    player->song_ended = false;
    for (int i = RH_VOICES - 1; i >= 0 && player->running; i--) {
        if (run_voice(player, &player->voices[i]) == StepResult_Stop) {
            // The stop routine switches all four channels off, and the tick returns
            player->running = false;
            for (int k = 0; k < RH_VOICES; k++) {
                nuade_paula_channel_dma_off(&player->voices[k].paula);
            }
        }
    }
    for (int i = 0; i < RH_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, (uint32_t)i, voices);
    }
    return (RhTickResult) { .looped = player->song_ended, .ended = !player->running };
}

RhVoiceState rh_voice_state(const RhPlayer* player, int voice) {
    const RhVoice* v = &player->voices[voice];
    return (RhVoiceState) { .note = v->shown_note, .instrument = v->shown_instrument };
}

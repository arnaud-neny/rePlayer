// SPDX-License-Identifier: MIT
// Jochen Hippel ".hip" player, engine A (Wings of Death, Dragonflight, Endless Piracy, Tower FRA): the commands of
// Hippel's COSO replayer on the TFMX-header layout. Frequency sequence commands run on until one yields a value,
// samples can be swept with a moving window, and the repeat registers are written when a channel was restarted.

#include "hh_internal.h"

// The names engine A gives the commands that the engines do not share
enum {
    Command_Sweep = HhCommand_E5,
    Command_SweepChange = HhCommand_E6,
    Command_SampleIfNew = HhCommand_E7,
};

// How a frequency sequence run ended
typedef enum FseqEnd {
    FseqEnd_Value = 0,
    FseqEnd_Hold,
    FseqEnd_Wait,
} FseqEnd;

// E9: one sample of a sample pack. The replayer copies the first byte of the sample over the second.
static void load_packed_sample(const HhModule* m, HhVoice* v, uint8_t number, uint8_t record) {
    const HhPack pack = hh_sample_pack(m, number);
    const HhPackEntry entry = hh_pack_entry(m, &pack, record);
    const uint32_t start = entry.start & ~1u;
    const uint32_t end = entry.end & ~1u;
    const uint32_t at = pack.data + start;
    hh_write_registers(&v->paula, hh_registers(m, at, (uint16_t)((end - start) >> 1)));
    v->repeat = hh_registers(m, at, 1);
    if (at < m->size - 1) {
        m->data[at + 1] = m->data[at];
    }
    hh_restart_volume(v);
}

// E5: start a sweep through a sample: a window of `length` words that moves by `delta` words every `speed` ticks
static void start_sweep(const HhModule* m, HhVoice* v, uint32_t at) {
    const HhSample sample = hh_sample(m, hhm_u8(m, at + 1));
    HhSweep* sweep = &v->sweep;
    sweep->start = sample.start;
    sweep->end = sample.start + (uint32_t)sample.length * 2;
    const uint16_t pos = hhm_u16(m, at + 2);
    sweep->pos = pos == 0xffff ? sample.length : pos;
    sweep->length = hhm_u16(m, at + 4);
    sweep->delta = (int16_t)hhm_u16(m, at + 6);
    sweep->counter = 0;
    sweep->speed = hhm_u8(m, at + 8);
    sweep->stopped = false;
    sweep->started = false;
    sweep->active = true;
}

// E6: change the length, step and speed of the sweep
static void change_sweep(const HhModule* m, HhVoice* v, uint32_t at) {
    HhSweep* sweep = &v->sweep;
    sweep->length = hhm_u16(m, at + 1);
    sweep->delta = (int16_t)hhm_u16(m, at + 3);
    sweep->counter = 0;
    sweep->speed = hhm_u8(m, at + 5);
    sweep->started = false;
    sweep->stopped = false;
}

// Read the frequency sequence until it yields a value, holds or waits.
static FseqEnd run_fseq(const HhModule* m, const HhEngine* engine, HhVoice* v) {
    for (int guard = 0; guard < 256; guard++) {
        if (v->fseq_wait != 0) {
            v->fseq_wait--;
            return FseqEnd_Wait;
        }
        const uint32_t at = v->fseq + (uint32_t)v->fseq_pos;
        const uint8_t command = hhm_u8(m, at);
        if (command < HhCommand_FirstCommand) {
            v->fseq_value = command;
            v->fseq_pos++;
            return FseqEnd_Value;
        }
        const uint8_t arg = hhm_u8(m, at + 1);
        switch (command) {
            case HhCommand_Jump:
                v->fseq_pos = arg & engine->jump_mask;
                break;
            case HhCommand_Hold:
                return FseqEnd_Hold;
            case HhCommand_Sample:
                hh_command_sample(m, v, arg);
                break;
            case HhCommand_Vibrato:
                hh_command_vibrato(m, v, at);
                break;
            case HhCommand_SampleNoReset:
                hh_command_sample_no_reset(m, v, arg);
                break;
            case Command_Sweep:
                hh_channel_restart(v);
                start_sweep(m, v, at);
                hh_restart_volume(v);
                v->fseq_pos += 9;
                break;
            case Command_SweepChange:
                change_sweep(m, v, at);
                v->fseq_pos += 6;
                break;
            case Command_SampleIfNew:
                hh_command_sample_if_new(m, v, arg);
                break;
            case HhCommand_Wait:
                v->fseq_wait = arg;
                v->fseq_pos += 2;
                break;
            case HhCommand_PackedSample:
                v->current_sample = 0xff;
                hh_channel_restart(v);
                load_packed_sample(m, v, arg, hhm_u8(m, at + 2));
                v->sweep.active = false;
                v->fseq_pos += 3;
                break;
            default:
                return FseqEnd_Hold; // the replayer would jump through the bytes behind its command table
        }
    }
    return FseqEnd_Hold;
}

// Move the sample window: AUDxLC/AUDxLEN become the repeat, no restart.
static void run_sweep(const HhModule* m, HhVoice* v) {
    HhSweep* sweep = &v->sweep;
    if (!sweep->active || sweep->stopped) {
        return;
    }
    sweep->counter--;
    if (sweep->counter >= 0) {
        return;
    }
    sweep->counter = (int8_t)sweep->speed;
    int32_t pos = sweep->pos;
    if (!sweep->started) {
        sweep->started = true;
    } else {
        pos += sweep->delta;
        if (pos < 0 || sweep->start + (uint32_t)pos * 2 + (uint32_t)sweep->length * 2 > sweep->end) {
            sweep->stopped = true;
            pos -= sweep->delta;
        }
    }
    sweep->pos = (uint16_t)pos;
    v->repeat = hh_registers(m, m->sample_data + sweep->start + (uint32_t)pos * 2, sweep->length);
    hh_write_registers(&v->paula, v->repeat);
}

static void run_frequency(const HhModule* m, const HhVersion* version, HhVoice* v) {
    const FseqEnd end = run_fseq(m, version->engine, v);
    if (end != FseqEnd_Wait || !version->a_wait_skips_sweep) {
        run_sweep(m, v);
    }
}

static void init_voice(HhVoice* v) {
    v->current_sample = 0xff;
    v->sweep.active = false;
    v->repeat = (HhRegisters) { .lc = nullptr, .len_words = 0 }; // no repeat until a sample sets one
}

// The repeat registers of every channel, when the commands stopped a channel
static void write_repeat(HhSong* song, const HhModule* m, const HhVersion* version) {
    (void)m;
    bool restarted = false;
    for (int i = 0; i < version->voices; i++) {
        restarted = restarted || song->voices[i].dma_restart;
    }
    if (!restarted) {
        return;
    }
    for (int i = 0; i < version->voices; i++) {
        hh_write_registers(&song->voices[i].paula, song->voices[i].repeat);
    }
}

const HhEngine k_hh_engine_a = {
    .name = "engine A",
    .record_size = 0x52,
    .record_note_offset = 0x42,
    .first_step_in_first_tick = true,
    .new_note_stops_channel = false,
    .alt_fseq_allowed = true,
    .flat_vibrato = true,
    .stop_keeps_periods = false,
    .jump_mask = 0xff,
    .vseq_commands_hold = true,
    .init_voice = init_voice,
    .run_frequency = run_frequency,
    .write_repeat = write_repeat,
};

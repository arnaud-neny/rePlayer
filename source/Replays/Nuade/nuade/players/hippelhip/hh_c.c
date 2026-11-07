// SPDX-License-Identifier: MIT
// Jochen Hippel ".hip" player, engine C (Shaolin, Warp, Battle Valley, Astaroth 2, Cybernoid 2, Thalion intro,
// Hurricane): a new note switches the channel off, a sample command writes the sample and, a tick later (or in the
// same tick), the repeat registers. Frequency sequence commands are a subset of engine B's, and E7 switches the
// sequence.

#include "hh_internal.h"

// E4 writes the repeat one tick after the sample start
#define SWITCH_DELAY 1

// The name engine C gives the command that the engines do not share
enum {
    Command_SwitchFseq = HhCommand_E7,
};

// AUDxLC/AUDxLEN = the sample. The repeat is written when `delay` ticks have passed, if the high word of the sample's
// address is not zero; it is written in the same tick otherwise.
static void load_sample(const HhModule* m, HhVoice* v, uint8_t number, uint8_t delay) {
    const HhSample sample = hh_sample(m, number);
    const uint32_t start = m->sample_data + sample.start;
    hh_write_registers(&v->paula, hh_registers(m, start, sample.length));
    v->delayed.sample = start;
    v->delayed.offset = sample.loop_offset;
    v->delayed.length = sample.loop_length;
    const bool high_word_zero = ((HH_LOAD_ADDRESS + start) >> 16) == 0;
    v->delayed.countdown = (uint8_t)((high_word_zero ? 0 : delay) + 1);
}

// The commands that are not common to the engines
static HhNext run_command(const HhModule* m, const HhVersion* version, HhVoice* v, uint8_t command, uint32_t at) {
    const uint8_t arg = hhm_u8(m, at + 1);
    if (command == HhCommand_Sample) {
        hh_channel_restart(v);
        load_sample(m, v, arg, version->c_start_delay);
        hh_restart_volume(v);
        v->fseq_pos += 2;
    } else if (command == HhCommand_SampleNoReset && version->c_has_switch_command) {
        load_sample(m, v, arg, SWITCH_DELAY);
        v->fseq_pos += 2;
    } else if (command == Command_SwitchFseq) {
        v->fseq = hhm_record(m->fseqs, arg, HH_SEQUENCE_RECORD_SIZE);
        v->fseq_pos = 0;
        return HhNext_Switch;
    }
    return HhNext_Value;
}

static void run_frequency(const HhModule* m, const HhVersion* version, HhVoice* v) {
    hh_run_fseq_chain(m, version, v, run_command);
}

static void init_voice(HhVoice* v) {
    v->note = 0;
    v->portamento = false;
    v->delayed.offset = 0;
    v->delayed.countdown = 0;
}

// The repeat of a new sample is written a few ticks after its start
static void write_repeat(HhSong* song, const HhModule* m, const HhVersion* version) {
    for (int i = 0; i < version->voices; i++) {
        HhVoice* v = &song->voices[i];
        if (v->delayed.countdown != 0) {
            v->delayed.countdown--;
            if (v->delayed.countdown == 0) {
                hh_write_registers(&v->paula,
                                   hh_registers(m, v->delayed.sample + v->delayed.offset, v->delayed.length));
            }
        }
    }
}

const HhEngine k_hh_engine_c = {
    .name = "engine C",
    .record_size = 0,
    .first_step_in_first_tick = false,
    .new_note_stops_channel = true,
    .alt_fseq_allowed = false,
    .flat_vibrato = false,
    .stop_keeps_periods = true,
    .jump_mask = 0x3f,
    .vseq_commands_hold = false,
    .init_voice = init_voice,
    .run_frequency = run_frequency,
    .write_repeat = write_repeat,
};

// SPDX-License-Identifier: MIT
// Jochen Hippel ".hip" player, engine B (Leaving Teramis, Seven Gates of Jambala, Crazy Comets, Quartex, Rings of
// Medusa): the frequency sequence commands are tried one after the other, the repeat registers are written every tick.

#include "hh_internal.h"

// The names engine B gives the commands that the engines do not share
enum {
    Command_SampleBank = HhCommand_E5,
    Command_SampleNoReset = HhCommand_E6, // the same as E4
    Command_SampleIfNew = HhCommand_E7,
};

// E5: record `index` of a sample made of records that are each as long as its loop
static void load_sample_bank(const HhModule* m, HhVoice* v, uint8_t number, uint8_t index) {
    const HhSample sample = hh_sample(m, number);
    const uint32_t start = m->sample_data + sample.start + (uint32_t)sample.loop_length * index;
    hh_write_registers(&v->paula, hh_registers(m, start, sample.loop_length));
    v->repeat = hh_registers(m, start + sample.loop_offset, sample.loop_length);
}

// E9: one sample of a sample pack. A block without the magic is left alone.
static void load_packed_sample(const HhModule* m, HhVoice* v, uint8_t number, uint8_t record) {
    const HhPack pack = hh_sample_pack(m, number);
    if (!pack.magic) {
        return;
    }
    const HhPackEntry entry = hh_pack_entry(m, &pack, record);
    hh_write_registers(&v->paula, hh_registers(m, pack.data + entry.start, (uint16_t)((entry.end - entry.start) >> 1)));
    v->repeat = hh_registers(m, pack.data + entry.start, 1);
    hh_restart_volume(v);
}

// The commands that are not common to the engines: after any of them the next byte of the sequence is the value
static HhNext run_command(const HhModule* m, const HhVersion* version, HhVoice* v, uint8_t command, uint32_t at) {
    (void)version;
    const uint8_t arg = hhm_u8(m, at + 1);
    switch (command) {
        case HhCommand_Sample:
            hh_command_sample(m, v, arg);
            break;
        case HhCommand_PackedSample:
            v->current_sample = 0xff;
            hh_channel_restart(v);
            load_packed_sample(m, v, arg, hhm_u8(m, at + 2));
            v->fseq_pos += 3;
            break;
        case Command_SampleBank:
            hh_channel_restart(v);
            load_sample_bank(m, v, arg, hhm_u8(m, at + 2));
            hh_restart_volume(v);
            v->fseq_pos += 3;
            break;
        case HhCommand_SampleNoReset:
        case Command_SampleNoReset:
            hh_command_sample_no_reset(m, v, arg);
            break;
        case Command_SampleIfNew:
            hh_command_sample_if_new(m, v, arg);
            break;
        default:
            break; // anything else is a value
    }
    return HhNext_Value;
}

static void run_frequency(const HhModule* m, const HhVersion* version, HhVoice* v) {
    hh_run_fseq_chain(m, version, v, run_command);
}

static void init_voice(HhVoice* v) {
    v->current_sample = 0xff;
}

// The repeat registers of every channel
static void write_repeat(HhSong* song, const HhModule* m, const HhVersion* version) {
    (void)m;
    for (int i = 0; i < version->voices; i++) {
        hh_write_registers(&song->voices[i].paula, song->voices[i].repeat);
    }
}

const HhEngine k_hh_engine_b = {
    .name = "engine B",
    .record_size = 0x48,
    .record_note_offset = 0x08,
    .first_step_in_first_tick = true,
    .new_note_stops_channel = false,
    .alt_fseq_allowed = true,
    .flat_vibrato = false,
    .stop_keeps_periods = false,
    .jump_mask = 0xff,
    .vseq_commands_hold = false,
    .init_voice = init_voice,
    .run_frequency = run_frequency,
    .write_repeat = write_repeat,
};

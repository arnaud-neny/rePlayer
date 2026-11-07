// SPDX-License-Identifier: MIT
// Sean Conran player, translated from the replayers inside the "*.scr" modules and the UADE eagleplayer "SeanConran",
// which checks the module, patches the replayer to report the end of the song and calls its init and tick.
//
// The replayer keeps its variables inside its own instructions (a flag is the immediate of a `moveq`, a pointer the
// operand of a `lea`), writes to them with ordinary moves, and has one copy of its code for each of the four voices.
// The port models those variables as the fields of ScrVoice. The code exists in a few builds that differ in the
// commands each voice knows and in a few starting values; scr_module.c recognises the build and describes it.
//
// A voice reads three streams. The command stream holds notes (a length in ticks), settings and sample starts. The
// volume sequence and the pitch sequence run once a tick while a note plays: the first gives an index into the volume
// table, the second an index into the period table.
//
// What the eagleplayer does that changes the replayer's output: the audio interrupts that one build chains sample
// repeats with are servers installed with SetIntVector (and `rte` becomes `rts`), the first voice's track list calls
// SongEnd when it reaches a marker, and a stub that does so is written over the copyright text (scr_module.c).
//
// Order of work in a tick: the voices 0 to 3, each with its command stream, then its volume and pitch sequences; then
// the volumes of all four voices.
//
// The replayer reads whatever its data makes it read, and writes its variables into the code that the module also
// holds. scr_create refuses a module unless a dry run of each voice (see `run_dry`) proves that the voice reads only
// data the port models.

#include <nuade/players/seanconran.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "sc_module.h"
#include <stdlib.h>

#define MAX_COMMANDS_PER_TICK 4096 // a voice that reads this many without a note would hang the replayer
#define MASTER_VOLUME_MAX 15
#define PITCH_FREE_RUNNING 0xFFFF   // what a note start writes to AUDxPER until the pitch sequence has run
#define SEQUENCE_JUMP_MARKER 0x80   // a sequence byte of 0x80 is followed by how far back to jump
#define PEEK_NOTE_LIMIT 8           // a command byte below this is a note
#define PEEK_TRANSPARENT_COMMAND 12 // the pitch shift command does not end a note
#define VOLUME_TABLE_ENTRIES 16
#define VOLUME_NIBBLE_MASK 0x0F
#define PITCH_SHIFT_LEFT 1 // bit 0 of the opcode byte that holds the shift
#define PITCH_SHIFT_OPCODE_MASK 0xF0
#define PITCH_SHIFT_OPCODE 0xE0 // the first byte of `lsr.w #n,d1` and `lsl.w #n,d1`
#define MAX_MARKER_JUMPS 256    // a track list that jumps back this often without reaching an entry loops for ever
#define NO_OFFSET 0xFFFFFFFFu   // a pointer the replayer has not been given: not an offset of the module
// Ticks the dry runs of a whole module may take; a module that needs more is left to UADE.
#define PROOF_TICKS 4000000u

// The one place that sets the dry run's verdict; playing never does.
static void mark_unmodelled(ScrPlayer* p) {
    if (p->proving) {
        p->unmodelled = true;
    }
}

// Module reads

// A read in a dry run is of modelled data when it lies inside the module and outside the replayer's own bytes, which
// the replayer writes to.
static bool is_modelled_data(const ScrModule* m, uint32_t at, uint32_t length) {
    if (at > m->size || m->size - at < length) {
        return false;
    }
    return at >= m->written_to || (uint64_t)at + length <= m->written_from;
}

static uint8_t read_u8(ScrPlayer* p, uint32_t at) {
    if (!is_modelled_data(p->module, at, 1)) {
        mark_unmodelled(p);
    }
    return scr_module_u8(p->module, at);
}

static uint16_t read_u16(ScrPlayer* p, uint32_t at) {
    if (!is_modelled_data(p->module, at, 2)) {
        mark_unmodelled(p);
    }
    return scr_module_u16(p->module, at);
}

static uint32_t read_u32(ScrPlayer* p, uint32_t at) {
    return ((uint32_t)read_u16(p, at) << 16) | read_u16(p, at + 2);
}

// An offset the replayer forms from a signed word: `movea.w` sign-extends it, the module base is 0.
static uint32_t read_word_offset(ScrPlayer* p, uint32_t at) {
    return (uint32_t)(int32_t)(int16_t)read_u16(p, at);
}

// AUDxLC for an offset into the module. An address past the module is memory the original program does not use: it
// plays zeros, like the end of the padded copy.
static const int8_t* chip_pointer(const ScrModule* m, uint32_t offset) {
    const uint32_t even = offset & ~1u; // Paula ignores bit 0
    return (const int8_t*)m->data + (even <= m->size ? even : m->size);
}

// A sample start from the sequence data: a dry run requires it to lie behind the replayer's bytes and inside the
// module.
static const int8_t* sample_at(ScrPlayer* p, uint32_t offset) {
    const ScrModule* m = p->module;
    if (offset < m->written_to || offset > m->size) {
        mark_unmodelled(p);
    }
    return chip_pointer(m, offset);
}

static uint8_t next_byte(ScrPlayer* p, ScrVoice* v) {
    return read_u8(p, v->commands_at++);
}

// A command's offset parameter: a word, sign-extended.
static uint32_t next_word_offset(ScrPlayer* p, ScrVoice* v) {
    const uint32_t offset = read_word_offset(p, v->commands_at);
    v->commands_at += 2;
    return offset;
}

// A command's sample parameter: a skipped byte and a 24 bit offset.
static uint32_t next_offset24(ScrPlayer* p, ScrVoice* v) {
    v->commands_at++;
    const uint32_t offset = ((uint32_t)read_u8(p, v->commands_at) << 16) | read_u16(p, v->commands_at + 1);
    v->commands_at += 3;
    return offset;
}

// Audio interrupts
//
// All four interrupts run the same handler, which serves the lowest voice that has its interrupt enabled: it writes
// that voice's repeat to AUDxLC / AUDxLEN and disables the interrupt. A voice's interrupt fires at every block start
// (a DMA start, a repeat), so the interrupt of a higher voice serves the lower voices that are enabled first.

// The eagleplayer installs the handler for all four interrupts after the init, and installing enables each of them.
static void arm_audio_interrupts(ScrPlayer* p) {
    const ScrBuild* build = p->module->build;
    for (uint32_t i = 0; i < SCR_VOICES && build->audio_interrupts; i++) {
        p->voices[i].loop_armed = true;
        p->voices[i].loop_start = build->handler_loops[i].start;
        p->voices[i].loop_words = build->handler_loops[i].words;
    }
}

// A block start of voice `index`.
static void serve_audio_interrupt(ScrPlayer* p, uint32_t index) {
    if (!p->voices[index].loop_armed) {
        return;
    }
    for (uint32_t i = 0; i <= index; i++) {
        ScrVoice* v = &p->voices[i];
        if (v->loop_armed) {
            v->paula.lc = chip_pointer(p->module, v->loop_start);
            v->paula.len_words = v->loop_words;
            v->sample_set = true;
            v->loop_armed = false;
        }
    }
}

// Block starts the mixer played since the last tick are interrupts that fired in between.
static void answer_wraps(ScrPlayer* p, uint32_t index, const NuadeVoices* voices) {
    ScrVoice* v = &p->voices[index];
    NuadeVoiceState state;
    nuade_voice_get_state(voices, index, &state);
    for (uint32_t n = state.wraps - v->wraps_seen; n > 0 && v->loop_armed; n--) {
        serve_audio_interrupt(p, index);
    }
    v->wraps_seen = state.wraps;
}

// DMACON on. A DMA start is a block start. A voice that was never given a sample plays memory the port does not know.
static void start_dma(ScrPlayer* p, uint32_t index) {
    ScrVoice* v = &p->voices[index];
    if (!v->sample_set) {
        mark_unmodelled(p);
    }
    const bool was_running = v->paula.dma;
    nuade_paula_channel_dma_on(&v->paula);
    if (!was_running) {
        serve_audio_interrupt(p, index);
    }
}

// Track lists

typedef enum Flow {
    Flow_Continue,  // read the next command
    Flow_Envelopes, // the voice goes on with its volume and pitch sequences
    Flow_NextVoice, // nothing more for this voice in this tick
    Flow_Stop,      // the replayer has stopped or hangs: the tick ends
} Flow;

// The stop routine: all volumes and DMA off, once.
static void run_stop_routine(ScrPlayer* p) {
    if (!p->stop_pending) {
        return;
    }
    p->stop_pending = false;
    for (uint32_t i = 0; i < SCR_VOICES; i++) {
        p->voices[i].paula.volume = 0;
        nuade_paula_channel_dma_off(&p->voices[i].paula);
    }
}

// What a marker in a track list does, by the byte behind it.
typedef enum MarkerAction {
    MarkerAction_JumpBack, // jump back by the byte
    MarkerAction_Stop,
    MarkerAction_Hang,
} MarkerAction;

static MarkerAction marker_action(ScrCommand kind, uint8_t back) {
    switch (kind) {
        case ScrCommand_NextStop:
            return (int8_t)back < 0 ? MarkerAction_Stop : MarkerAction_JumpBack;
        case ScrCommand_NextStopFF:
            return back == 0xFF ? MarkerAction_Stop : MarkerAction_JumpBack;
        case ScrCommand_NextEnd:
            return MarkerAction_Stop;
        case ScrCommand_NextStopOrHang:
            return back == 0xFF ? MarkerAction_Stop : MarkerAction_Hang;
        case ScrCommand_NextHang:
            return MarkerAction_Hang;
        default:
            return MarkerAction_JumpBack;
    }
}

// The next stream of the voice's track list. A byte with the sign bit set is a marker: the byte behind it says how far
// back in the list to jump (or, in some builds, to stop). The first voice's marker is where the eagleplayer calls
// SongEnd.
static Flow next_stream(ScrPlayer* p, ScrVoice* v, uint32_t index, ScrCommand kind) {
    const ScrBuild* build = p->module->build;
    uint32_t at = v->track_at;
    uint8_t entry = read_u8(p, at++);
    for (int jumps = 0; (int8_t)entry < 0 && kind != ScrCommand_NextPlain; jumps++) {
        if (index == 0) {
            p->looped = true;
        }
        const uint8_t back = read_u8(p, at);
        const MarkerAction action = jumps >= MAX_MARKER_JUMPS ? MarkerAction_Hang : marker_action(kind, back);
        if (action == MarkerAction_Stop) {
            p->playing = false;
            run_stop_routine(p);
            return Flow_Stop;
        }
        if (action == MarkerAction_Hang) {
            p->hung = true;
            return Flow_Stop;
        }
        at -= back;
        entry = read_u8(p, at++);
        if (!build->marker_rechecked) {
            break;
        }
    }
    v->track_at = at;
    const uint32_t scale = build->track_entries_doubled ? 2 : 1;
    const uint32_t entry_at = build->track_tables[index] + scale * entry;
    if ((entry_at & 1) != 0) {
        mark_unmodelled(p); // the 68000 reads a word at an odd address: an address error
    }
    v->commands_at = read_word_offset(p, entry_at);
    return Flow_Continue;
}

// Starting a subsong

static ScrPitchShift initial_pitch_shift(const ScrBuild* build, uint32_t voice) {
    return (ScrPitchShift) { .count = (uint8_t)(build->pitch_shifts_ascend ? 5 + voice : 8 - voice) };
}

// The track list of a voice: from the subsong's record of the song table, or the one list of a build without.
static uint32_t track_list_start(ScrPlayer* p, uint32_t index, uint32_t subsong) {
    const ScrBuild* build = p->module->build;
    return build->song_table != 0 ? read_word_offset(p, build->song_table + 8 * subsong + 2 * index)
                                  : build->track_lists[index];
}

// The init starts each voice with the stream that the first entry of its track list names. The builds that read the
// list with a loop test that entry for a marker like any other.
static void start_voice(ScrPlayer* p, uint32_t index, uint32_t subsong) {
    const ScrBuild* build = p->module->build;
    ScrVoice* v = &p->voices[index];
    *v = (ScrVoice) {
        .track_at = track_list_start(p, index, subsong),
        .length = build->length_ends_at_zero ? 1 : 0,
        .legato = true,
        .volumes.restart = NO_OFFSET,
        .pitches.restart = NO_OFFSET,
        .pitch_bias = build->start_pitch_bias,
        .pitch_shift = initial_pitch_shift(build, index),
        .volume_index = (uint8_t)(index + (build->volume_start_zero ? 0 : 1)),
    };
    const ScrCommand next = (ScrCommand)build->commands[index][SCR_COMMAND_SLOT_NEXT];
    next_stream(p, v, index, build->marker_rechecked ? next : ScrCommand_NextPlain);
}

static void reset_player(ScrPlayer* p) {
    p->master_volume = p->module->build->start_master_volume;
    p->playing = true;
    p->stop_pending = true;
    p->hung = false;
    p->looped = false;
    p->unmodelled = false;
}

static void start_player(ScrPlayer* p, uint32_t subsong) {
    reset_player(p);
    for (uint32_t i = 0; i < SCR_VOICES; i++) {
        start_voice(p, i, subsong);
    }
    arm_audio_interrupts(p);
    p->looped = false;
}

bool scr_start(ScrPlayer* player, int subsong, NuadeVoices* voices) {
    if (subsong < 0 || (uint32_t)subsong >= player->module->subsong_count) {
        return false;
    }
    start_player(player, (uint32_t)subsong);
    for (uint32_t i = 0; i < SCR_VOICES; i++) {
        nuade_voice_stop(voices, i);
        NuadeVoiceState state;
        nuade_voice_get_state(voices, i, &state);
        player->voices[i].wraps_seen = state.wraps;
    }
    return true;
}

// Commands

// A note: does the command stream go on with a note? The gate stays open (the DMA is not switched off at the end of
// the length) if it does; the pitch shift commands are looked through.
static void peek_legato(ScrPlayer* p, ScrVoice* v) {
    uint32_t at = v->commands_at;
    for (uint32_t steps = 0; steps < p->module->size + 1; steps++) {
        const uint8_t next = read_u8(p, at);
        if ((int8_t)next < PEEK_NOTE_LIMIT) {
            v->legato = true;
            return;
        }
        if (next != PEEK_TRANSPARENT_COMMAND) {
            break;
        }
        at += 2;
    }
    v->legato = false;
}

static void start_rest(ScrPlayer* p, ScrVoice* v) {
    v->length = next_byte(p, v);
    peek_legato(p, v);
}

static void start_note(ScrPlayer* p, ScrVoice* v) {
    v->pitch_bias = next_byte(p, v);
    start_rest(p, v);
}

// The command stores the first byte of the `lsr.w #n,d1` or `lsl.w #n,d1` instruction: the count in bits 1..3 (0 is 8)
// and the direction in bit 0. Any other byte would turn the instruction into another one.
static void set_pitch_shift(ScrPlayer* p, ScrVoice* v, uint8_t opcode) {
    if ((opcode & PITCH_SHIFT_OPCODE_MASK) != PITCH_SHIFT_OPCODE) {
        mark_unmodelled(p);
    }
    const uint8_t count = (opcode >> 1) & 7;
    v->pitch_shift = (ScrPitchShift) { .count = count == 0 ? 8 : count, .left = (opcode & PITCH_SHIFT_LEFT) != 0 };
}

static void restart_envelopes(ScrVoice* v) {
    v->volumes.restart_pending = true;
    v->pitches.restart_pending = true;
}

// A sample and its repeat, as the module holds them: AUDxLC and AUDxLEN of the start, then of the repeat.
typedef struct SampleRecord {
    uint32_t start;
    uint16_t words;
    uint32_t loop_start;
    uint16_t loop_words;
} SampleRecord;

static SampleRecord read_sample_record(ScrPlayer* p, uint32_t at) {
    if ((at & 1) != 0) {
        mark_unmodelled(p); // the 68000 reads longs at an odd address: an address error
    }
    return (SampleRecord) { .start = read_u32(p, at),
                            .words = read_u16(p, at + 4),
                            .loop_start = read_u32(p, at + 6),
                            .loop_words = read_u16(p, at + 10) };
}

// A sample start with the repeat chained by the audio interrupt.
static void start_sample_loop(ScrPlayer* p, ScrVoice* v, uint32_t index, uint32_t record_at) {
    const SampleRecord record = read_sample_record(p, record_at);
    v->paula.lc = sample_at(p, record.start);
    v->paula.len_words = record.words;
    v->sample_set = true;
    sample_at(p, record.loop_start); // a dry run checks the repeat as well
    v->loop_start = record.loop_start;
    v->loop_words = record.loop_words;
    v->loop_armed = true;
    v->paula.period = PITCH_FREE_RUNNING;
    start_dma(p, index);
}

static void set_master_volume(ScrPlayer* p, uint8_t adjust) {
    const uint8_t sum = (uint8_t)(p->master_volume + adjust);
    if ((int8_t)sum < 0) {
        p->master_volume = 0;
    } else {
        p->master_volume = (int8_t)sum > MASTER_VOLUME_MAX ? MASTER_VOLUME_MAX : sum;
    }
}

static Flow run_command(ScrPlayer* p, ScrVoice* v, uint32_t index, ScrCommand command) {
    switch (command) {
        case ScrCommand_Note:
            start_note(p, v);
            return Flow_Envelopes;
        case ScrCommand_Rest:
            start_rest(p, v);
            return Flow_Envelopes;
        case ScrCommand_VolumeBase:
            v->volume_base = (int8_t)next_byte(p, v);
            return Flow_Continue;
        case ScrCommand_PitchShift:
            set_pitch_shift(p, v, next_byte(p, v));
            return Flow_Continue;
        case ScrCommand_VolumeRestart:
            v->volumes.restart = next_word_offset(p, v);
            return Flow_Continue;
        case ScrCommand_PitchRestart:
            v->pitches.restart = next_word_offset(p, v);
            return Flow_Continue;
        case ScrCommand_Return:
            if (v->call_armed) {
                v->call_armed = false;
                v->commands_at = v->call_return;
            }
            return Flow_Continue;
        case ScrCommand_Call: {
            const uint32_t target = next_word_offset(p, v);
            v->call_return = v->commands_at;
            v->commands_at = target;
            v->call_armed = true;
            return Flow_Continue;
        }
        case ScrCommand_SampleLoop:
            start_sample_loop(p, v, index, next_word_offset(p, v));
            return Flow_Continue;
        case ScrCommand_Sample:
            v->paula.lc = sample_at(p, next_offset24(p, v));
            v->sample_set = true;
            return Flow_Continue;
        case ScrCommand_SampleOpen:
            v->paula.lc = sample_at(p, next_offset24(p, v));
            v->paula.len_words = 0;
            v->sample_set = true;
            return Flow_Continue;
        case ScrCommand_MasterVolume:
            set_master_volume(p, next_byte(p, v));
            return Flow_Continue;
        case ScrCommand_Mute:
            v->envelope_on = false;
            v->length = next_byte(p, v);
            return Flow_NextVoice;
        case ScrCommand_Retrigger:
            restart_envelopes(v);
            start_dma(p, index);
            start_rest(p, v);
            return Flow_Envelopes;
        case ScrCommand_NoteOn:
            v->envelope_on = true;
            restart_envelopes(v);
            v->paula.period = PITCH_FREE_RUNNING;
            start_dma(p, index);
            start_note(p, v);
            return Flow_Envelopes;
        case ScrCommand_NextLoop:
        case ScrCommand_NextStop:
        case ScrCommand_NextStopFF:
        case ScrCommand_NextEnd:
        case ScrCommand_NextPlain:
        case ScrCommand_NextStopOrHang:
        case ScrCommand_NextHang:
            return next_stream(p, v, index, command);
        case ScrCommand_Unmodelled:
            break;
    }
    v->halted = true; // the sequence data is not one the port models
    return Flow_NextVoice;
}

// Reads commands until one ends the voice's work for the tick. The command byte indexes a jump table of 4 byte
// entries.
static Flow read_commands(ScrPlayer* p, ScrVoice* v, uint32_t index) {
    for (int steps = 0; steps < MAX_COMMANDS_PER_TICK; steps++) {
        const uint8_t byte = next_byte(p, v);
        const uint32_t slot = byte / SCR_COMMAND_STEP;
        if (byte % SCR_COMMAND_STEP != 0 || slot >= SCR_COMMAND_SLOTS) {
            break;
        }
        const Flow flow = run_command(p, v, index, (ScrCommand)p->module->build->commands[index][slot]);
        if (flow != Flow_Continue) {
            return flow;
        }
    }
    v->halted = true;
    return Flow_NextVoice;
}

// Sequences

// The sequence's next byte. A marker byte jumps back by the byte behind it, and a restart starts the sequence over.
static uint8_t next_sequence_byte(ScrPlayer* p, ScrSequence* s) {
    uint32_t at = s->at;
    if (s->restart_pending) {
        s->restart_pending = false;
        at = s->restart;
    } else if (read_u8(p, at) == SEQUENCE_JUMP_MARKER) {
        at -= read_u8(p, at + 1);
    }
    const uint8_t value = read_u8(p, at);
    s->at = at + 1;
    return value;
}

// The period table, which the module starts with: a word index, or a byte offset that must be even.
static uint16_t read_period_table(ScrPlayer* p, uint8_t note) {
    if (p->module->build->pitch_index_bytes) {
        if ((note & 1) != 0) {
            mark_unmodelled(p); // the 68000 would take an address error
        }
        return read_u16(p, note);
    }
    return read_u16(p, 2u * note);
}

static void run_envelopes(ScrPlayer* p, ScrVoice* v) {
    const ScrBuild* build = p->module->build;
    if (!v->envelope_on) {
        return;
    }
    // Volume: the base plus the sequence's byte, 0 when that is negative
    const uint8_t volume = (uint8_t)((uint8_t)v->volume_base + next_sequence_byte(p, &v->volumes));
    v->volume_index = (int8_t)volume < 0 ? 0 : volume;
    if (build->volume_nibble) {
        v->volume_index &= VOLUME_NIBBLE_MASK;
    }
    if (v->volume_index >= VOLUME_TABLE_ENTRIES) {
        mark_unmodelled(p); // the volume table is 16 entries: the replayer reads on into its code
    }
    // Pitch: the byte plus the bias indexes the period table, which the shift scales
    const uint8_t note = (uint8_t)(v->pitch_bias + next_sequence_byte(p, &v->pitches));
    const uint16_t base = read_period_table(p, note);
    v->paula.period
        = v->pitch_shift.left ? (uint16_t)(base << v->pitch_shift.count) : (uint16_t)(base >> v->pitch_shift.count);
}

// The tick

static Flow run_voice(ScrPlayer* p, uint32_t index) {
    ScrVoice* v = &p->voices[index];
    if (v->halted) {
        return Flow_NextVoice;
    }
    v->length--;
    const bool zero_ends = p->module->build->length_ends_at_zero;
    Flow flow = Flow_Envelopes;
    if (zero_ends ? v->length == 0 : (int8_t)v->length < 0) {
        flow = read_commands(p, v, index);
    } else if (v->length == (zero_ends ? 1 : 0) && !v->legato) {
        nuade_paula_channel_dma_off(&v->paula);
        if (p->module->build->release_clears_volume) {
            v->volume_index = 0;
        }
        flow = Flow_NextVoice;
    }
    if (flow == Flow_Envelopes) {
        run_envelopes(p, v);
    }
    return flow == Flow_Stop ? Flow_Stop : Flow_NextVoice;
}

// The volume of each voice: its index moved down by the master volume, through the volume table.
static void write_volumes(ScrPlayer* p) {
    const ScrModule* m = p->module;
    const uint8_t shift = (uint8_t)(p->master_volume - MASTER_VOLUME_MAX);
    for (uint32_t i = 0; i < SCR_VOICES; i++) {
        const uint8_t index = (uint8_t)(p->voices[i].volume_index + shift);
        const bool silent = (int8_t)index < 0 || index >= VOLUME_TABLE_ENTRIES;
        p->voices[i].paula.volume = silent ? 0 : (uint8_t)scr_module_u16(m, m->build->volume_table + 2u * index);
    }
}

ScrTickResult scr_tick(ScrPlayer* p, NuadeVoices* voices) {
    p->looped = false;
    for (uint32_t i = 0; i < SCR_VOICES && !p->hung; i++) {
        answer_wraps(p, i, voices);
    }
    if (p->hung) {
        // the tick never returned: nothing is written
    } else if (!p->playing) {
        run_stop_routine(p);
    } else {
        for (uint32_t i = 0; i < SCR_VOICES; i++) {
            if (run_voice(p, i) == Flow_Stop) {
                break;
            }
        }
        if (p->playing && !p->hung) {
            write_volumes(p);
        }
    }
    for (uint32_t i = 0; i < SCR_VOICES; i++) {
        nuade_paula_channel_sync(&p->voices[i].paula, i, voices);
    }
    return (ScrTickResult) { .looped = p->looped, .ended = !p->playing };
}

// The dry run
//
// What a voice does depends on its own state and on the module, apart from the master volume, which only lowers the
// volumes, and the audio interrupts, which only write sample pointers. A voice is run alone, tick after tick, until it
// stops the replayer, reads something the port does not model, or its state repeats (Brent's cycle detection, with one
// saved state): from a repeat on it does what it did in the ticks before, so every read it can ever make has been
// checked. A voice that stops the replayer ends it for the others too, so what they would read after that moment does
// not count: in the first subsong of Top Cat the other voices jump back out of their track lists, but the first voice
// stops at the same marker before they get there.

typedef struct DryRun {
    bool stops;     // the voice stops the replayer
    bool violates;  // the voice reads data the port does not model, or hangs the replayer
    uint32_t order; // when: 4 * tick + voice, the order the replayer does its work in (the init is tick 0)
} DryRun;

static bool same_sequence(const ScrSequence* a, const ScrSequence* b) {
    return a->at == b->at && a->restart == b->restart && a->restart_pending == b->restart_pending;
}

static bool same_voice_state(const ScrVoice* a, const ScrVoice* b) {
    return a->commands_at == b->commands_at && a->track_at == b->track_at && a->length == b->length
           && a->legato == b->legato && a->call_armed == b->call_armed
           && (!a->call_armed || a->call_return == b->call_return) && a->envelope_on == b->envelope_on
           && same_sequence(&a->volumes, &b->volumes) && same_sequence(&a->pitches, &b->pitches)
           && a->volume_base == b->volume_base && a->pitch_bias == b->pitch_bias
           && a->pitch_shift.count == b->pitch_shift.count && a->pitch_shift.left == b->pitch_shift.left
           && a->sample_set == b->sample_set;
}

static bool voice_cannot_go_on(const ScrPlayer* p, uint32_t index) {
    return p->unmodelled || p->hung || p->voices[index].halted;
}

// Runs voice `index` of `subsong` alone. `ticks_left` is the budget of the whole module; false when it ran out.
static bool run_dry(ScrPlayer* p, uint32_t index, uint32_t subsong, uint32_t* ticks_left, DryRun* result) {
    *result = (DryRun) { .order = index };
    reset_player(p);
    start_voice(p, index, subsong);
    arm_audio_interrupts(p);
    uint32_t tick = 0;
    ScrVoice saved = p->voices[index];
    uint32_t power = 1;
    uint32_t steps = 0;
    for (;;) {
        result->order = SCR_VOICES * tick + index;
        if (voice_cannot_go_on(p, index)) {
            result->violates = true;
            return true;
        }
        if (!p->playing) {
            result->stops = true;
            return true;
        }
        if (tick > 0 && same_voice_state(&p->voices[index], &saved)) {
            return true; // from here on it repeats itself
        }
        if (tick > 0 && ++steps == power) {
            saved = p->voices[index];
            power *= 2;
            steps = 0;
        }
        if (*ticks_left == 0) {
            return false;
        }
        (*ticks_left)--;
        tick++;
        run_voice(p, index);
    }
}

// Whether the voices of `subsong` read only modelled data until the replayer stops, if it ever does.
static bool subsong_is_modelled(ScrPlayer* p, uint32_t subsong, uint32_t* ticks_left) {
    DryRun runs[SCR_VOICES];
    for (uint32_t voice = 0; voice < SCR_VOICES; voice++) {
        if (!run_dry(p, voice, subsong, ticks_left, &runs[voice])) {
            return false;
        }
    }
    uint32_t first_stop = UINT32_MAX;
    for (uint32_t voice = 0; voice < SCR_VOICES; voice++) {
        if (runs[voice].stops && runs[voice].order < first_stop) {
            first_stop = runs[voice].order;
        }
    }
    for (uint32_t voice = 0; voice < SCR_VOICES; voice++) {
        if (runs[voice].violates && runs[voice].order <= first_stop) {
            return false;
        }
    }
    return true;
}

// The track lists and streams of every subsong are of the kinds the port models.
static bool streams_are_modelled(ScrModule* module) {
    ScrPlayer* proof = (ScrPlayer*)calloc(1, sizeof(ScrPlayer));
    if (proof == nullptr) {
        return false;
    }
    proof->module = module;
    proof->proving = true;
    uint32_t ticks_left = PROOF_TICKS;
    bool ok = true;
    for (uint32_t subsong = 0; subsong < module->subsong_count && ok; subsong++) {
        ok = subsong_is_modelled(proof, subsong, &ticks_left);
    }
    free(proof);
    return ok;
}

// Opening

ScrPlayer* scr_create(const uint8_t* data, size_t size) {
    ScrModule* module = scr_module_open(data, size);
    if (module == nullptr) {
        return nullptr;
    }
    ScrPlayer* player = (ScrPlayer*)calloc(1, sizeof(ScrPlayer));
    if (player == nullptr || !streams_are_modelled(module)) {
        scr_module_close(module);
        free(player);
        return nullptr;
    }
    player->module = module;

    return player;
}

bool scr_probe(const uint8_t* data, size_t size) {
    ScrPlayer* player = scr_create(data, size);
    const bool supported = player != nullptr;
    scr_destroy(player);
    return supported;
}

void scr_destroy(ScrPlayer* player) {
    if (player != nullptr) {
        scr_module_close(player->module);
    }
    free(player);
}

int scr_subsong_count(const ScrPlayer* player) {
    return (int)player->module->subsong_count;
}

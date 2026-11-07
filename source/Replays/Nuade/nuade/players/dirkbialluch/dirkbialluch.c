// SPDX-License-Identifier: MIT
// Dirk Bialluch player, translated from the UADE eagleplayer "Dirk Bialluch" (Wanted Team), see db_internal.h for the
// files.
//
// A song is a sequence of steps; a step names one pattern and one transpose for each voice. A pattern is a stream of
// three byte events: notes, waits, volume and slide commands, loops and jumps. A note names a program, a small
// interpreter per voice (four byte commands) that sets the sample, its length, the volume, the pitch bend and the
// arpeggio, and waits. Once per tick: (1) the sequencer; unless the groove (ticks per step) holds it back every voice
// reads its events up to the first wait; (2) the programs that the events requested are started, the priority decides
// whether a program replaces the running one; (3) every voice runs its program up to the first command that ends its
// turn; (4) the fade out; (5) period, volume, sample pointer and length of the four channels are written and the DMA
// of all four channels is switched on (a running channel is not restarted: only a program that switched its DMA off
// restarts, with the pointer and length that were written).
//
// The eagleplayer calls the replayer with a request for a sound effect on a channel in four registers; its Interrupt
// passes 0 for all of them, so the code that starts those requests is never reached and is not ported. The two
// commands that switch the audio filter (the power LED bit) are not modelled: the filter is off in the reference.

#include <nuade/players/dirkbialluch.h>

#include "db_internal.h"
#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_tracker.h"
#include "nuade_utils.h"
#include <stdlib.h>

#define MAX_EVENTS_PER_TURN 2048
#define MAX_COMMANDS_PER_TURN 2048
#define POSITION_MASK 0x0FFFu
#define PATTERN_EVENT_BYTES 3
#define PROGRAM_COMMAND_BYTES 4
#define ARPEGGIO_END 0x80
#define GROOVE_END 0xFF
#define VOLUME_SCALE_SHIFT 6
#define PERIOD_TABLE_NOTES 36
// AUDxLC holds a word address: bit 0 of the pointer the program computes is dropped.
#define LC_ALIGN_MASK (~1u)

// The period table: the entry 0 is 0xFFFF, the entries 1 .. 36 are the periods from C-1 (856) to B-3 (113). The
// replayer indexes it by a byte offset and the indexes can pass the table; what follows it is the pattern command
// classification table.

static const uint16_t k_periods[PERIOD_TABLE_NOTES + 1] = {
    0xFFFF, 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320,
    302,    285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
};

static uint16_t period_at(uint8_t byte_offset) {
    const unsigned index = byte_offset / 2u;
    if (index < nuade_sizeof_array(k_periods)) {
        return k_periods[index];
    }
    // Past the table: the classification of command bytes j and j + 1 (big-endian word).
    const unsigned first = (index - (unsigned)nuade_sizeof_array(k_periods)) * 2u;
    return (uint16_t)(((unsigned)db_pattern_command((uint8_t)first) << 8)
                      | (unsigned)db_pattern_command((uint8_t)(first + 1)));
}

DbCreateResult db_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    DbCreateResult result = { .status = DbStatus_InvalidData };
    DbPlayer* p = calloc(1, sizeof(DbPlayer));
    if (p == nullptr) {
        result.status = DbStatus_OutOfMemory;
        return result;
    }
    result.status = db_module_open(&p->module, data, size, path, io);
    if (result.status == DbStatus_Ok) {
        result.player = p;
        return result;
    }
    free(p);
    return result;
}

void db_destroy(DbPlayer* player) {
    if (player == nullptr) {
        return;
    }
    db_module_close(&player->module);
    free(player);
}

int db_subsong_count(const DbPlayer* player) {
    return (int)player->module.subsong_count;
}

// InitSound: all variables are 0, every voice runs the first program (the idle one), the first turn loads the step.

bool db_start(DbPlayer* player, int subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong < 0 || (uint32_t)subsong >= player->module.subsong_count) {
        return false;
    }
    const DbModule* m = &player->module;
    *player = (DbPlayer) { .module = player->module };
    for (int i = 0; i < DB_VOICES; i++) {
        DbVoice* v = &player->voices[i];
        *v = (DbVoice) { .program = m->program_data, .note = -1, .instrument = -1 };
        nuade_paula_channel_dma_off(&v->paula);
        nuade_voice_stop(voices, (uint32_t)i);
    }
    player->position = db_subsong_position(m, (uint8_t)subsong);
    player->step_pending = true;
    player->running = true;
    return true;
}

// A byte jump: the replayer replaces the low byte of its word offset and keeps the high byte, which only counts the
// bytes the turn went past 255.

static uint16_t with_low_byte(uint16_t position, unsigned low) {
    return (uint16_t)((position & 0xFF00u) | (low & 0xFFu));
}

static uint8_t note_offset_of(uint8_t note, uint8_t transpose) {
    return (uint8_t)((note + transpose) * 2);
}

// -1 for a note the period table does not hold

static int note_value(uint8_t note_offset) {
    const unsigned index = note_offset / 2u;
    return index >= 1 && index <= PERIOD_TABLE_NOTES ? nuade_period_to_note(k_periods[index]) : -1;
}

// Pattern events

typedef enum TurnFlow {
    TurnFlow_Next,  // read the next event or command
    TurnFlow_Store, // the voice's turn ends; its position is stored
    TurnFlow_End,   // the voice's turn ends; its position stays where it was
} TurnFlow;

static void start_note(DbVoice* v, DbPatternEvent e, uint8_t volume, uint8_t code_base) {
    v->volume = volume;
    v->note_offset = note_offset_of((uint8_t)(e.code - code_base), v->transpose);
    v->requested_program = e.second;
    v->note = note_value(v->note_offset);
    v->instrument = e.second;
}

static TurnFlow run_pattern_event(DbPlayer* p, DbVoice* v, DbPatternEvent e, uint16_t* position, DbTickResult* result) {
    switch (db_pattern_command(e.code)) {
        case DbPatternCommand_Note:
            start_note(v, e, e.first, 0);
            *position += PATTERN_EVENT_BYTES;
            return TurnFlow_Next;
        case DbPatternCommand_NoteWait:
            start_note(v, e, 0x40, 0x40);
            v->wait = e.first;
            *position += PATTERN_EVENT_BYTES;
            return TurnFlow_Store;
        case DbPatternCommand_Wait:
            v->wait = e.second;
            *position += PATTERN_EVENT_BYTES;
            return TurnFlow_Store;
        case DbPatternCommand_SetGroove:
            p->groove_offset = e.second;
            p->groove_position = 0;
            *position += PATTERN_EVENT_BYTES;
            return TurnFlow_Next;
        case DbPatternCommand_FilterOn:
        case DbPatternCommand_FilterOff:
        case DbPatternCommand_Unused:
            *position += PATTERN_EVENT_BYTES;
            return TurnFlow_Next;
        case DbPatternCommand_Slide:
            v->slide_target_offset = note_offset_of(e.first, v->transpose);
            v->slide_rate = e.second;
            *position += PATTERN_EVENT_BYTES;
            return TurnFlow_Next;
        case DbPatternCommand_MasterVolume:
            p->master_volume = e.second;
            *position += PATTERN_EVENT_BYTES;
            return TurnFlow_Next;
        case DbPatternCommand_VolumeWait:
            v->volume = e.second;
            v->wait = e.first;
            *position += PATTERN_EVENT_BYTES;
            return TurnFlow_Store;
        case DbPatternCommand_VolumeAddWait:
            v->volume = (uint8_t)(v->volume + e.second);
            v->wait = e.first;
            *position += PATTERN_EVENT_BYTES;
            return TurnFlow_Store;
        case DbPatternCommand_VolumeSubWait:
            v->volume = (uint8_t)(v->volume - e.second);
            v->wait = e.first;
            *position += PATTERN_EVENT_BYTES;
            return TurnFlow_Store;
        case DbPatternCommand_Loop:
            // The count in `first` is how often the jump back to event `second` is taken.
            if (v->loop_count != 0) {
                v->loop_count--;
                if (v->loop_count == 0) {
                    *position += PATTERN_EVENT_BYTES;
                    return TurnFlow_Store;
                }
            } else {
                v->loop_count = e.first;
            }
            *position = with_low_byte(*position, e.second * PATTERN_EVENT_BYTES);
            return TurnFlow_Store;
        case DbPatternCommand_Signal:
            v->signal = e.second;
            *position += PATTERN_EVENT_BYTES;
            return TurnFlow_Next;
        case DbPatternCommand_SignalWait:
            v->signal = e.second;
            v->wait = e.first;
            *position += PATTERN_EVENT_BYTES;
            return TurnFlow_Store;
        case DbPatternCommand_Arpeggio:
            v->arpeggio_start = e.second;
            v->arpeggio_position = 0;
            *position += PATTERN_EVENT_BYTES;
            return TurnFlow_Next;
        case DbPatternCommand_GotoPosition:
            p->position = (uint16_t)((e.first << 8) | e.second);
            p->step_pending = true;
            result->looped = true;
            return TurnFlow_End;
        case DbPatternCommand_Stop:
            p->running = false;
            result->looped = true;
            result->ended = true;
            return TurnFlow_End;
        case DbPatternCommand_NextStep:
            p->step_pending = true;
            return TurnFlow_End;
        case DbPatternCommand_JumpPattern:
            v->pattern = db_pattern_address(&p->module, e.second);
            *position = with_low_byte(*position, e.first * PATTERN_EVENT_BYTES);
            return TurnFlow_Store;
        case DbPatternCommand_FadeParameters:
            p->fade_step = e.first;
            p->fade_interval = e.second;
            *position += PATTERN_EVENT_BYTES;
            return TurnFlow_Next;
        case DbPatternCommand_FadeToSubsong:
            p->fade_subsong = e.second;
            *position += PATTERN_EVENT_BYTES;
            return TurnFlow_Next;
        case DbPatternCommand_Idle:
            break;
    }
    return TurnFlow_End;
}

// One voice's events up to the first one that ends its turn.

static void run_voice_events(DbPlayer* p, DbVoice* v, DbTickResult* result) {
    if (v->wait != 0) {
        v->wait--;
        return;
    }
    uint16_t position = v->pattern_position;
    for (int guard = 0; guard < MAX_EVENTS_PER_TURN; guard++) {
        const DbPatternEvent e = db_pattern_event(&p->module, v->pattern, position);
        const TurnFlow flow = run_pattern_event(p, v, e, &position, result);
        if (flow == TurnFlow_Store) {
            v->pattern_position = (uint8_t)position;
        }
        if (flow != TurnFlow_Next) {
            return;
        }
    }
}

// The step at `position`: the voices start at the top of their patterns.

static void load_step(DbPlayer* p) {
    for (int i = 0; i < DB_VOICES; i++) {
        DbVoice* v = &p->voices[i];
        v->pattern_position = 0;
        v->wait = 0;
        v->loop_count = 0;
    }
    p->position &= POSITION_MASK;
    const DbSequenceStep step = db_sequence_step(&p->module, p->position);
    for (int i = 0; i < DB_VOICES; i++) {
        p->voices[i].pattern = db_pattern_address(&p->module, step.voice[i].pattern);
        p->voices[i].transpose = step.voice[i].transpose;
    }
    p->position++;
    p->step_pending = false;
}

// The sequencer turn: the groove step gives the number of turns to skip after this one; it ends with 0xFF and starts
// over.

static void next_groove_step(DbPlayer* p) {
    const DbModule* m = &p->module;
    uint8_t at = p->groove_position;
    if (db_groove_step(m, (uint32_t)p->groove_offset + at) == GROOVE_END) {
        at = 0;
    }
    p->row_counter = db_groove_step(m, (uint32_t)p->groove_offset + at);
    p->groove_position = (uint8_t)(at + 1);
}

static void run_sequencer(DbPlayer* p, DbTickResult* result) {
    if (p->step_pending) {
        load_step(p);
    }
    for (int i = DB_VOICES - 1; i >= 0; i--) {
        run_voice_events(p, &p->voices[i], result);
    }
    next_groove_step(p);
}

// A note asked for a program: it replaces the voice's program when its priority (the first byte) is not lower.

static void request_programs(DbPlayer* p) {
    for (int i = DB_VOICES - 1; i >= 0; i--) {
        DbVoice* v = &p->voices[i];
        if (v->requested_program != 0) {
            const uint32_t program = db_program_address(&p->module, v->requested_program);
            if (db_program_priority(&p->module, program) >= db_program_priority(&p->module, v->program)) {
                v->program = program;
                v->fixed_note = 0;
                v->slide_rate = 0;
                v->signal = 0;
                v->program_position = 0;
                v->program_wait = 0;
                v->program_loop_count = 0;
                v->slide_offset = 0;
                v->bend = 0;
                v->volume_offset = 0;
            }
        }
    }
    for (int i = 0; i < DB_VOICES; i++) {
        p->voices[i].requested_program = 0;
    }
}

// AUDxLEN: the 24 bit argument halved, the low 16 bits.

static uint16_t length_argument(DbProgramCommand c) {
    const uint32_t bytes = ((uint32_t)c.first << 16) | ((uint32_t)c.second << 8) | c.third;
    return (uint16_t)(bytes >> 1);
}

// The word argument of the commands that move the sample pointer and the pitch bend.

static uint16_t word_argument(DbProgramCommand c) {
    return (uint16_t)((c.second << 8) | c.third);
}

static TurnFlow run_program_command(DbPlayer* p, DbVoice* v, DbProgramCommand c, uint16_t* position) {
    switch (db_program_op(c.code)) {
        case DbProgramOp_StartSample:
            // The original writes AUDxPER 80 here; the tick writes the period again before the DMA comes on.
            v->sample_address = db_sample_address(&p->module, c.third);
            nuade_paula_channel_dma_off(&v->paula);
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Next;
        case DbProgramOp_SampleAdd:
            v->sample_address += word_argument(c);
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Next;
        case DbProgramOp_SetLength:
            v->length_words = length_argument(c);
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Store;
        case DbProgramOp_SetSample:
            v->sample_address = db_sample_address(&p->module, c.third);
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Next;
        case DbProgramOp_SampleSub:
            v->sample_address -= word_argument(c);
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Next;
        case DbProgramOp_VolumeSetWait:
            v->program_wait = c.second;
            v->volume_offset = c.third;
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Store;
        case DbProgramOp_VolumeAddWait:
            v->program_wait = c.second;
            v->volume_offset = (uint8_t)(v->volume_offset + c.third);
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Store;
        case DbProgramOp_VolumeSubWait:
            v->program_wait = c.second;
            v->volume_offset = (uint8_t)(v->volume_offset - c.third);
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Store;
        case DbProgramOp_VolumeSet:
            v->volume_offset = c.third;
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Next;
        case DbProgramOp_VolumeAdd:
            v->volume_offset = (uint8_t)(v->volume_offset + c.third);
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Next;
        case DbProgramOp_VolumeSub:
            v->volume_offset = (uint8_t)(v->volume_offset - c.third);
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Next;
        case DbProgramOp_BendSubWait:
            v->program_wait = c.second;
            v->bend = (uint16_t)(v->bend - c.third);
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Store;
        case DbProgramOp_BendAddWait:
            v->program_wait = c.second;
            v->bend = (uint16_t)(v->bend + c.third);
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Store;
        case DbProgramOp_BendSub:
            v->bend = (uint16_t)(v->bend - c.third);
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Next;
        case DbProgramOp_BendAdd:
            v->bend = (uint16_t)(v->bend + c.third);
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Next;
        case DbProgramOp_FixedNote:
            v->fixed_note = c.third;
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Next;
        case DbProgramOp_Wait:
            v->program_wait = c.third;
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Store;
        case DbProgramOp_JumpOnSignal:
            if (c.second != v->signal) {
                *position += PROGRAM_COMMAND_BYTES;
                return TurnFlow_Store;
            }
            v->signal = 0;
            *position = with_low_byte(*position, c.third * PROGRAM_COMMAND_BYTES);
            return TurnFlow_Next;
        case DbProgramOp_Arpeggio:
            v->arpeggio_start = c.third;
            v->arpeggio_position = 0;
            *position += PROGRAM_COMMAND_BYTES;
            return TurnFlow_Next;
        case DbProgramOp_Loop:
            // The count in `second` is how often the jump to command `third` is taken.
            if (v->program_loop_count != 0) {
                v->program_loop_count--;
                if (v->program_loop_count == 0) {
                    *position += PROGRAM_COMMAND_BYTES;
                    return TurnFlow_Store;
                }
            } else {
                v->program_loop_count = c.second;
            }
            *position = with_low_byte(*position, c.third * PROGRAM_COMMAND_BYTES);
            return TurnFlow_Store;
        case DbProgramOp_End:
            // Back to the first program, the idle one.
            v->program = p->module.program_data;
            *position = 0;
            return TurnFlow_Store;
        case DbProgramOp_RequestProgram:
            v->requested_program = c.third;
            return TurnFlow_End;
        case DbProgramOp_Idle:
            break;
    }
    return TurnFlow_End;
}

// One voice's program up to the first command that ends its turn.

static void run_program(DbPlayer* p, DbVoice* v) {
    if (v->program_wait != 0) {
        v->program_wait--;
        return;
    }
    uint16_t position = v->program_position;
    for (int guard = 0; guard < MAX_COMMANDS_PER_TURN; guard++) {
        const DbProgramCommand c = db_program_command(&p->module, v->program, position);
        const TurnFlow flow = run_program_command(p, v, c, &position);
        if (flow == TurnFlow_Store) {
            v->program_position = (uint8_t)position;
        }
        if (flow != TurnFlow_Next) {
            return;
        }
    }
}

// The fade out: every interval turns the master volume falls by the step; below 0 the player switches to the fade's
// subsong at once, without calling SongEnd, with the master volume at 0.

static void run_fade(DbPlayer* p) {
    if (p->fade_subsong == 0) {
        return;
    }
    if (p->fade_counter != 0) {
        p->fade_counter--;
        return;
    }
    p->fade_counter = p->fade_interval;
    p->master_volume = (uint8_t)(p->master_volume - p->fade_step);
    if ((int8_t)p->master_volume >= 0) {
        return;
    }
    p->position = db_subsong_position(&p->module, (uint8_t)(p->fade_subsong - 1));
    p->fade_subsong = 0;
    p->fade_counter = 0;
    p->master_volume = 0;
    p->step_pending = true;
    p->row_counter = 0;
}

// AUDxVOL: the voice volume and its offset, a byte that is negative (bit 7) for silence, scaled by the master volume.

static void write_volume(const DbPlayer* p, DbVoice* v) {
    const uint8_t sum = (uint8_t)(v->volume + v->volume_offset);
    if ((sum & 0x80) != 0) {
        v->paula.volume = 0;
        return;
    }
    v->paula.volume = (uint8_t)(((unsigned)sum * p->master_volume) >> VOLUME_SCALE_SHIFT);
}

// The slide moves `slide_offset` towards the distance between the slide target and the note by `slide_rate` per tick
// and stops (rate 0) when it gets there or passes it.

static void run_slide(DbVoice* v, int16_t distance) {
    int16_t offset = v->slide_offset;
    const int16_t remaining = (int16_t)(distance - offset);
    if (remaining < 0) {
        offset = (int16_t)(offset - v->slide_rate);
        if (distance >= offset) {
            v->slide_rate = 0;
            offset = distance;
        }
    } else {
        offset = (int16_t)(offset + v->slide_rate);
        if (distance <= offset) {
            v->slide_rate = 0;
            offset = distance;
        }
    }
    v->slide_offset = offset;
}

// AUDxPER: a fixed note, or the note plus the arpeggio step, the slide and the bend.

static void write_period(const DbPlayer* p, DbVoice* v) {
    if (v->fixed_note != 0) {
        v->paula.period = period_at((uint8_t)(v->fixed_note * 2));
        return;
    }
    const DbModule* m = &p->module;
    uint8_t index = (uint8_t)(v->arpeggio_start + v->arpeggio_position);
    if (db_arpeggio_step(m, index) == ARPEGGIO_END) {
        v->arpeggio_position = 0;
        index = v->arpeggio_start;
    }
    const uint8_t step_offset = (uint8_t)(db_arpeggio_step(m, index) * 2);
    v->arpeggio_position++;
    const uint16_t note_period = period_at((uint8_t)(v->note_offset + step_offset));
    run_slide(v, (int16_t)(period_at(v->slide_target_offset) - note_period));
    v->paula.period = (uint16_t)(note_period + (uint16_t)v->slide_offset + v->bend);
}

// LC and LEN are written before the DMA comes on, so a channel that starts latches them. A song plays AUDxLEN 0 (65536
// words): a note whose program has not set a length plays the sample file after its sample until the next tick
// restarts the channel.

static void write_registers(DbPlayer* p) {
    for (int i = DB_VOICES - 1; i >= 0; i--) {
        write_volume(p, &p->voices[i]);
        write_period(p, &p->voices[i]);
    }
    for (int i = 0; i < DB_VOICES; i++) {
        DbVoice* v = &p->voices[i];
        v->paula.lc = db_sample_lc(&p->module, v->sample_address & LC_ALIGN_MASK);
        v->paula.len_words = v->length_words;
    }
    for (int i = 0; i < DB_VOICES; i++) {
        nuade_paula_channel_dma_on(&p->voices[i].paula);
    }
}

DbTickResult db_tick(DbPlayer* player, NuadeVoices* voices) {
    DbTickResult result = { 0 };
    if (player->running) {
        if (player->row_counter != 0) {
            player->row_counter--;
        } else {
            run_sequencer(player, &result);
        }
        request_programs(player);
        for (int i = DB_VOICES - 1; i >= 0; i--) {
            run_program(player, &player->voices[i]);
        }
        run_fade(player);
        write_registers(player);
    }
    for (int i = 0; i < DB_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, (uint32_t)i, voices);
    }
    return result;
}

uint32_t db_grid_ticks(const DbPlayer* player) {
    return (uint32_t)db_groove_step(&player->module, 0) + 1;
}

DbVoiceState db_voice_state(const DbPlayer* player, int voice) {
    const DbVoice* v = &player->voices[voice];
    return (DbVoiceState) { .note = v->note, .instrument = v->instrument };
}

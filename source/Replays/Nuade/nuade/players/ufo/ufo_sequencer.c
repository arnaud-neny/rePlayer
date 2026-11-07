// SPDX-License-Identifier: MIT
// UFO player: the sequencer. Every call of the replayer's timing routine subtracts the tempo from a counter; when it
// runs out, each logical channel whose delay is up reads events until one is followed by a non-zero delay. An event is
// an optional command byte (high bit set; without one the channel's previous command repeats) and its data bytes:
//   0x80 note off      note, 1 byte skipped
//   0x90 note on       note, velocity
//   0xB0 control       number, value: 0 tempo, 7 velocity scale
//   0xC0 instrument    number; 0x7E restarts the song instead
//   0xE0 bend          value (bias 0x10)
//   0xA0 0xD0 0xF0     1 data byte, no effect
// and the single bytes 0xFE <block> (call a block), 0xFD (return to after the call) and 0xFF (stop the song).

#include "ufo_internal.h"

#define SEQUENCER_ROUND 0x78
// a step that reads this many events without a delay is a loop the replayer never leaves
#define MAX_EVENTS_PER_STEP 65536

#define COMMAND_FIRST 0x80
#define COMMAND_MASK 0xF0
#define BLOCK_CALL 0xFE
#define BLOCK_RETURN 0xFD
#define SONG_STOP 0xFF
#define LOOP_MARK 0x7E
#define BEND_BIAS 0x10
#define TEMPO_SCALE_NUMERATOR 48
#define TEMPO_SCALE_DENOMINATOR 60

typedef enum UfoCommand {
    UfoCommand_NoteOff = 0x80,
    UfoCommand_NoteOn = 0x90,
    UfoCommand_Control = 0xB0,
    UfoCommand_Instrument = 0xC0,
    UfoCommand_Bend = 0xE0,
} UfoCommand;

typedef enum UfoControl {
    UfoControl_Tempo = 0,
    UfoControl_VelocityScale = 7,
} UfoControl;

typedef struct StepResult {
    uint16_t delay;
    bool restart; // the loop command: the whole song goes back to its start
} StepResult;

static uint8_t read_byte(const UfoSong* song, UfoState* state, uint32_t* cursor) {
    const uint8_t byte = ufo_song_u8(song, *cursor, &state->fault);
    (*cursor)++;
    state->work++;
    return byte;
}

static void apply_control(UfoState* state, unsigned channel, uint8_t number, uint8_t value) {
    switch (number) {
        case UfoControl_Tempo:
            state->seq.bpm = (uint8_t)((uint32_t)value * TEMPO_SCALE_NUMERATOR / TEMPO_SCALE_DENOMINATOR);
            break;
        case UfoControl_VelocityScale:
            state->channels[channel].volume_scale = value;
            break;
        default:
            break; // the replayer stores a few more numbers, and never reads them
    }
}

// One event with its command. The loop command sets result->restart: the step ends without a delay.

static void run_event(const UfoSong* song, UfoState* state, unsigned channel, uint32_t* cursor, uint8_t command,
                      uint8_t first, StepResult* result) {
    UfoChannel* chan = &state->channels[channel];
    switch (command & COMMAND_MASK) {
        case UfoCommand_NoteOff:
            read_byte(song, state, cursor);
            ufo_voice_note(song, state, channel, first, 0);
            break;
        case UfoCommand_NoteOn: {
            const uint8_t velocity = read_byte(song, state, cursor);
            ufo_voice_note(song, state, channel, first, (uint8_t)((velocity * chan->volume_scale) >> 8));
            break;
        }
        case UfoCommand_Control:
            apply_control(state, channel, first, read_byte(song, state, cursor));
            break;
        case UfoCommand_Instrument:
            if (first == LOOP_MARK) {
                state->looped = true;
                result->restart = true;
                return;
            }
            chan->instrument = first;
            break;
        case UfoCommand_Bend:
            chan->bend = (int8_t)(first - BEND_BIAS);
            ufo_voice_update_bend(state, channel);
            break;
        default:
            break;
    }
}

// Reads the events of a channel up to the next non-zero delay (the replayer's u5e78).

static StepResult channel_step(const UfoSong* song, UfoState* state, unsigned channel) {
    UfoChannel* chan = &state->channels[channel];
    UfoSequencer* seq = &state->seq;
    StepResult result = { 0 };
    uint32_t cursor = seq->position[channel];
    for (unsigned events = 0; events < MAX_EVENTS_PER_STEP && !state->fault; events++) {
        uint8_t byte = read_byte(song, state, &cursor);
        uint16_t delay = 0;
        if (byte == BLOCK_CALL) {
            const uint8_t block = read_byte(song, state, &cursor);
            chan->return_position = cursor;
            if (block >= song->block_count) {
                state->fault = true;
                break;
            }
            cursor = song->blocks[block];
            delay = ufo_song_read_delay(song, &cursor, &state->fault, &state->work);
        } else if (byte == BLOCK_RETURN) {
            cursor = chan->return_position;
            if (cursor == 0) { // no block was called: the replayer jumps to address 0
                state->fault = true;
                break;
            }
            seq->position[channel] = cursor;
            delay = ufo_song_read_delay(song, &cursor, &state->fault, &state->work);
        } else if (byte == SONG_STOP) {
            state->ended = true;
            seq->playing = false;
            return result;
        } else {
            if (byte >= COMMAND_FIRST) {
                seq->command[channel] = byte;
                byte = read_byte(song, state, &cursor);
            }
            run_event(song, state, channel, &cursor, seq->command[channel], byte, &result);
            if (result.restart) {
                return result;
            }
            delay = ufo_song_read_delay(song, &cursor, &state->fault, &state->work);
        }
        if (delay != 0) {
            seq->position[channel] = cursor;
            result.delay = delay;
            return result;
        }
    }
    state->fault = true;
    return result;
}

static void restart_song(UfoSequencer* seq) {
    for (unsigned i = 0; i < UFO_CHANNELS; i++) {
        seq->position[i] = seq->saved_position[i];
        seq->delay[i] = seq->saved_delay[i];
    }
}

// One call of the timing routine (u5d5a).
void ufo_sequencer_call(const UfoSong* song, UfoState* state) {
    UfoSequencer* seq = &state->seq;
    if (!seq->playing) {
        return;
    }
    seq->count -= seq->bpm;
    if (seq->count > 0) {
        return;
    }
    seq->count += SEQUENCER_ROUND;
    for (unsigned channel = 0; channel < UFO_CHANNELS; channel++) {
        if (seq->position[channel] == 0) {
            continue;
        }
        if (seq->delay[channel] == 0) {
            const StepResult step = channel_step(song, state, channel);
            seq->delay[channel] = step.delay;
            if (step.restart) {
                restart_song(seq);
                return;
            }
        }
        seq->delay[channel]--; // 16 bit: a stopped channel (delay 0) wraps to 65535
    }
}

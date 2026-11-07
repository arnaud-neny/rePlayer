// SPDX-License-Identifier: MIT
// UFO player: the running state, one interrupt of the eagleplayer, and the comparison the proof run uses.

#include "ufo_internal.h"

#include <string.h>

#define DEFAULT_VELOCITY_SCALE 0x7F
// The work a tick costs the proof run; a sequencer step costs one unit per byte it reads.
#define TICK_WORK 8

void ufo_state_init(UfoState* state, const UfoSong* song) {
    memset(state, 0, sizeof(*state));
    UfoSequencer* seq = &state->seq;
    for (unsigned i = 0; i < UFO_CHANNELS; i++) {
        seq->position[i] = song->channel_start[i];
        seq->saved_position[i] = song->channel_start[i];
        seq->delay[i] = song->channel_delay[i];
        seq->saved_delay[i] = song->channel_delay[i];
        state->channels[i].volume_scale = DEFAULT_VELOCITY_SCALE;
    }
    seq->bpm = song->bpm;
    seq->count = song->bpm;
    seq->playing = true;
    for (unsigned i = 0; i < UFO_VOICES; i++) {
        state->voices[i].display_note = -1;
        state->voices[i].display_instrument = -1;
    }
}

// The timing routine runs twice per interrupt, then the volumes follow.

void ufo_state_tick(const UfoSong* song, UfoState* state) {
    state->looped = false;
    state->ended = false;
    state->work += TICK_WORK;
    ufo_sequencer_call(song, state);
    ufo_sequencer_call(song, state);
    ufo_voices_update_volumes(state);
}

// Field by field (padding bytes have no defined value).

static bool same_sequencer(const UfoSequencer* a, const UfoSequencer* b) {
    return a->count == b->count && a->bpm == b->bpm && a->playing == b->playing
           && memcmp(a->position, b->position, sizeof(a->position)) == 0
           && memcmp(a->saved_position, b->saved_position, sizeof(a->saved_position)) == 0
           && memcmp(a->delay, b->delay, sizeof(a->delay)) == 0
           && memcmp(a->saved_delay, b->saved_delay, sizeof(a->saved_delay)) == 0
           && memcmp(a->command, b->command, sizeof(a->command)) == 0;
}

static bool same_channel(const UfoChannel* a, const UfoChannel* b) {
    return a->instrument == b->instrument && a->volume_scale == b->volume_scale && a->bend == b->bend
           && a->base_period == b->base_period && a->return_position == b->return_position;
}

// What the sequencer reads depends on the sequencer state and the channel records (instrument and note checks); the
// voices only follow.

bool ufo_state_same_sequence(const UfoState* a, const UfoState* b) {
    if (!same_sequencer(&a->seq, &b->seq)) {
        return false;
    }
    for (unsigned i = 0; i < UFO_CHANNELS; i++) {
        if (!same_channel(&a->channels[i], &b->channels[i])) {
            return false;
        }
    }
    return true;
}

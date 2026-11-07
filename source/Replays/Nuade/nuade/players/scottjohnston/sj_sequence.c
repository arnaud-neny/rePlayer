// SPDX-License-Identifier: MIT
// Scott Johnston player: the walk of a track over the rows of the tune, and the proof that the walk stays inside the
// data the port models.

#include "sj_internal.h"

// Rows a track steps in the proof, for the whole module; a tune that does not repeat within them is not accepted.
#define PROOF_STEP_BUDGET (1u << 22)

void sj_track_init(SjTrack* track) {
    track->row = -1;
    track->loop_row = SJ_ROW_UNSET;
    track->loop_count = SJ_NO_LOOP;
    track->wait = 0;
}

static void advance_row(SjTrack* track) {
    track->row++;
}

bool sj_track_step(SjTrack* track, const SjModule* module, uint32_t voice, SjCell* cell, bool* read) {
    // A wait holds the row for its count of steps; the step that ends it moves on.
    if (track->wait == 0) {
        advance_row(track);
    } else {
        track->wait--;
        if (track->wait == 0) {
            advance_row(track);
        }
    }
    *read = track->wait == 0;
    return !*read || sj_module_cell(module, track->row, voice, cell);
}

void sj_track_apply(SjTrack* track, SjCommand command) {
    switch (command.kind) {
        case SjCommand_Wait:
            if (track->wait == 0) {
                track->wait = command.param;
            }
            break;
        case SjCommand_LoopBegin:
            if (track->loop_count == SJ_NO_LOOP) {
                track->loop_row = track->row;
                track->loop_count = command.param;
            }
            break;
        case SjCommand_LoopEnd:
            if (track->loop_count == 0) {
                track->loop_count = SJ_NO_LOOP;
            } else {
                track->loop_count--;
                track->row = track->loop_row;
            }
            break;
        case SjCommand_RepeatRow:
            track->row--;
            break;
        case SjCommand_Restart:
            track->row = -1;
            break;
        default:
            break;
    }
}

static bool same_state(const SjTrack* a, const SjTrack* b) {
    return a->row == b->row && a->loop_row == b->loop_row && a->loop_count == b->loop_count && a->wait == b->wait;
}

// One step of a track as the replayer takes it: the walk, then the command of the cell it read.

static bool walk_step(SjTrack* track, const SjModule* module, uint32_t voice) {
    SjCell cell = { 0 };
    bool read = false;
    if (!sj_track_step(track, module, voice, &cell, &read)) {
        return false;
    }
    if (read) {
        if (!sj_cell_is_modelled(&cell)) {
            return false;
        }
        sj_track_apply(track, sj_decode_command(cell.command));
    }
    return true;
}

// Brent's cycle search on one track: the track's state after each step against a saved one. The tracks do not affect
// each other, so the states the tune can reach are in the product of the tracks' own.

static bool track_is_closed(const SjModule* module, uint32_t voice, uint32_t* budget) {
    SjTrack track;
    sj_track_init(&track);
    SjTrack saved = track;
    uint32_t power = 1;
    uint32_t distance = 0;
    while (*budget > 0) {
        (*budget)--;
        if (!walk_step(&track, module, voice)) {
            return false;
        }
        if (same_state(&track, &saved)) {
            return true;
        }
        distance++;
        if (distance == power) {
            saved = track;
            power *= 2;
            distance = 0;
        }
    }
    return false;
}

bool sj_tune_is_closed(const SjModule* module) {
    uint32_t budget = PROOF_STEP_BUDGET;
    for (uint32_t voice = 0; voice < SJ_VOICES; voice++) {
        if (sj_module_voice_is_armed(module, voice) && !track_is_closed(module, voice, &budget)) {
            return false;
        }
    }
    return true;
}

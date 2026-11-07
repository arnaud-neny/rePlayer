// SPDX-License-Identifier: MIT
// Synth Dream player: the proof that a song only reads memory the port models. The replayer reads module offsets that
// the song computes (instrument numbers shifted into a signed word, table offsets that wrap), and some songs point
// them outside the module. A voice does not depend on the other voices, so each is run alone: it ends, or its state
// repeats when its track starts over, and from then on it reads the same bytes again.

#include "sdr_internal.h"
#include <stdlib.h>

#define PROOF_WORK 0x1000000u // steps of all the voices of all the subsongs together
#define SNAPSHOTS 16          // track starts compared against

static bool same_cursor(const SdrCursor* a, const SdrCursor* b) {
    return a->at == b->at && a->left == b->left && a->repeats == b->repeats && a->length == b->length;
}

static bool same_switch(const SdrSwitch* a, const SdrSwitch* b) {
    return a->target == b->target && a->delay == b->delay;
}

static bool same_instrument(const SdrInstrument* a, const SdrInstrument* b) {
    return a->muted == b->muted && a->fixed_pitch == b->fixed_pitch && a->detune == b->detune
           && a->volume_table == b->volume_table && a->pitch_table == b->pitch_table && a->arpeggio == b->arpeggio
           && a->waveform == b->waveform && a->sample.start == b->sample.start
           && a->sample.length_words == b->sample.length_words && a->sample.loop_start == b->sample.loop_start
           && a->sample.loop_length_words == b->sample.loop_length_words;
}

// Everything that decides what the voice reads and does next (not the Paula registers it only writes).

static bool same_state(const SdrVoice* a, const SdrVoice* b) {
    return a->ended == b->ended && a->hold == b->hold && a->track_start == b->track_start
           && a->track_next == b->track_next && a->commands_start == b->commands_start && a->commands == b->commands
           && a->transpose == b->transpose && a->repeat == b->repeat && a->substitute_of == b->substitute_of
           && a->substitute_by == b->substitute_by && a->volume_cut == b->volume_cut && a->left == b->left
           && a->duration == b->duration && a->note == b->note && a->note_cut == b->note_cut && a->rest == b->rest
           && same_instrument(&a->instrument, &b->instrument) && same_cursor(&a->volume_cursor, &b->volume_cursor)
           && same_cursor(&a->pitch_cursor, &b->pitch_cursor) && same_cursor(&a->arpeggio_cursor, &b->arpeggio_cursor)
           && same_cursor(&a->waveform_cursor, &b->waveform_cursor) && same_switch(&a->volume_switch, &b->volume_switch)
           && same_switch(&a->pitch_switch, &b->pitch_switch) && a->arpeggio_offset == b->arpeggio_offset
           && a->slide == b->slide && a->slide_ratio == b->slide_ratio && a->slide_delay == b->slide_delay
           && a->period == b->period && a->volume == b->volume;
}

// Run one voice of one subsong until it ends or its state at the start of the track repeats. False when it cannot be
// shown to, or the song asks for something the port does not model.

static bool prove_voice(const SdrPlayer* player, int subsong, int voice, uint64_t* budget) {
    SdrPlayer* run = malloc(sizeof(SdrPlayer));
    SdrVoice* seen = malloc(SNAPSHOTS * sizeof(SdrVoice));
    if (run == nullptr || seen == nullptr) {
        free(run);
        free(seen);
        return false;
    }
    *run = *player;
    sdr_start(run, subsong);
    run->work = 0;
    int snapshots = 0;
    uint32_t loops = 0;
    bool proven = false;
    while (run->work < *budget && !run->memory.stray && !run->unmodelled) {
        const SdrVoice* v = &run->voices[voice];
        if (v->ended) {
            proven = true;
            break;
        }
        sdr_voice_tick(run, voice);
        if (v->loops == loops) {
            continue;
        }
        loops = v->loops;
        for (int i = 0; i < snapshots && !proven; i++) {
            proven = same_state(&seen[i], v);
        }
        if (proven || snapshots == SNAPSHOTS) {
            break;
        }
        seen[snapshots++] = *v;
    }
    proven = proven && !run->memory.stray && !run->unmodelled;
    *budget -= run->work < *budget ? run->work : *budget;
    free(run);
    free(seen);
    return proven;
}

bool sdr_prove_module(const SdrPlayer* player) {
    uint64_t budget = PROOF_WORK;
    for (int subsong = 0; subsong < player->subsong_count; subsong++) {
        for (int voice = 0; voice < SDR_VOICES; voice++) {
            if (!prove_voice(player, subsong, voice, &budget)) {
                return false;
            }
        }
    }
    return true;
}

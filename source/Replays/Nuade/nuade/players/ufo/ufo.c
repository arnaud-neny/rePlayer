// SPDX-License-Identifier: MIT
// UFO player: the entry points. The eagleplayer's InitPlayer (song, sample bank), InitSound (song start) and
// Interrupt (one tick), and the proof run that decides at open whether the replayer would stay inside the song.

#include "ufo_internal.h"

#include <stdlib.h>
#include <string.h>

// The most work (see TICK_WORK in ufo_state.c) one proof run does before the song is refused.
#define PROOF_WORK_BUDGET 20000000

// Every read of the song is a function of the sequencer state: the song positions, delays, repeat commands and the
// tempo counter, and the channel records the notes check. The run ticks the replayer until that state repeats (all
// channels together, found with Brent's cycle detection) or the song stops; a read outside the song, a block that does
// not exist, a return without call, a note past the period table or an instrument past the record table in that time
// is a fault. After the repeat the replayer repeats too, so a run without a fault proves the whole song.

static bool prove_song(const UfoSong* song) {
    UfoState states[2];
    UfoState* run = &states[0];
    UfoState* mark = &states[1];
    ufo_state_init(run, song);
    *mark = *run;
    uint32_t power = 1;
    uint32_t steps = 0;
    while (run->work < PROOF_WORK_BUDGET) {
        ufo_state_tick(song, run);
        if (run->fault) {
            return false;
        }
        if (!run->seq.playing || ufo_state_same_sequence(run, mark)) {
            return true;
        }
        if (++steps == power) {
            *mark = *run;
            power *= 2;
            steps = 0;
        }
    }
    return false;
}

bool ufo_probe(const uint8_t* data, size_t size) {
    UfoSong song;
    return ufo_song_parse(&song, data, size) && prove_song(&song);
}

void ufo_destroy(UfoPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module_memory);
    free(player->bank_memory);
    free(player);
}

static UfoStatus load_player(UfoPlayer* p, const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    if (!ufo_song_parse(&p->song, data, size)) {
        return UfoStatus_InvalidData;
    }
    p->module_memory = nuade_paula_padded_copy(data, size);
    if (p->module_memory == nullptr) {
        return UfoStatus_OutOfMemory;
    }
    uint32_t bank_size = 0;
    p->bank_memory = ufo_bank_read(path, io, &bank_size);
    if (p->bank_memory == nullptr) {
        return UfoStatus_MissingBank;
    }
    if (!ufo_bank_load(&p->song, p->module_memory, p->bank_memory, bank_size) || !prove_song(&p->song)) {
        return UfoStatus_InvalidData;
    }
    return UfoStatus_Ok;
}

UfoCreateResult ufo_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    UfoCreateResult result = { .status = UfoStatus_OutOfMemory };
    UfoPlayer* p = calloc(1, sizeof(UfoPlayer));
    if (p == nullptr) {
        return result;
    }
    result.status = load_player(p, data, size, path, io);
    if (result.status != UfoStatus_Ok) {
        ufo_destroy(p);
        return result;
    }
    ufo_state_init(&p->state, &p->song);
    result.player = p;
    return result;
}

bool ufo_start(UfoPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    ufo_state_init(&player->state, &player->song);
    for (uint32_t i = 0; i < UFO_VOICES; i++) {
        nuade_voice_stop(voices, i);
    }
    return true;
}

UfoTickResult ufo_tick(UfoPlayer* player, NuadeVoices* voices) {
    UfoState* state = &player->state;
    ufo_state_tick(&player->song, state);
    for (uint32_t i = 0; i < UFO_VOICES; i++) {
        nuade_paula_channel_sync(&state->voices[i].paula, i, voices);
    }
    return (UfoTickResult) { .looped = state->looped, .ended = state->ended };
}

UfoVoiceState ufo_voice_state(const UfoPlayer* player, int voice) {
    if (voice < 0 || voice >= UFO_VOICES) {
        return (UfoVoiceState) { .note = -1, .instrument = -1 };
    }
    const UfoVoice* v = &player->state.voices[voice];
    return (UfoVoiceState) { .note = v->display_note, .instrument = v->display_instrument };
}

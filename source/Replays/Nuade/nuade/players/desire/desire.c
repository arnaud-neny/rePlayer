// SPDX-License-Identifier: MIT
// Desire player: the entry points of the player (create, start, tick, display) around the replayer in dsr_driver.c.

#include "dsr_internal.h"
#include <stdlib.h>

bool dsr_probe(const uint8_t* data, size_t size) {
    DsrModule module;
    return dsr_module_locate(&module, data, size);
}

DsrCreateResult dsr_create(const uint8_t* data, size_t size) {
    DsrPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return (DsrCreateResult) { .player = nullptr, .status = DsrStatus_OutOfMemory };
    }
    const DsrStatus status = dsr_module_load(&player->module, data, size);
    if (status != DsrStatus_Ok) {
        free(player);
        return (DsrCreateResult) { .player = nullptr, .status = status };
    }
    return (DsrCreateResult) { .player = player, .status = DsrStatus_Ok };
}

void dsr_destroy(DsrPlayer* player) {
    if (player == nullptr) {
        return;
    }
    dsr_module_free(&player->module);
    free(player);
}

int dsr_subsong_count(const DsrPlayer* player) {
    return (int)player->module.subsongs;
}

bool dsr_start(DsrPlayer* player, int subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong < 0 || subsong >= dsr_subsong_count(player)) {
        return false;
    }
    dsr_replayer_start(player, subsong);
    for (int i = 0; i < DSR_VOICES; i++) {
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

DsrTickResult dsr_tick(DsrPlayer* player, NuadeVoices* voices) {
    player->song_ended = false;
    dsr_replayer_tick(player);
    for (int i = 0; i < DSR_VOICES; i++) {
        nuade_paula_channel_sync(&player->channels[i], (uint32_t)i, voices);
    }
    return (DsrTickResult) { .ended = player->song_ended };
}

DsrVoiceState dsr_voice_state(const DsrPlayer* player, int voice) {
    if (voice < 0 || voice >= DSR_VOICES) {
        return (DsrVoiceState) { .note = -1, .instrument = -1 };
    }
    const DsrVoice* v = &player->voices[voice];
    return (DsrVoiceState) { .note = v->display_note, .instrument = v->display_instrument };
}

// SPDX-License-Identifier: MIT
// Wally Beben: the player's lifecycle around the replayer, as the eagleplayer's glue drives it.
//
// The eagleplayer's InitSound requests the song and runs the module's reset routine; its interrupt calls the module's
// tick routine 50 times a second. UADE plays on past SongEnd, so a song that reaches its end simply goes on.

#include "wb_replay.h"

#include "nuade_base.h"
#include <stdlib.h>
#include <string.h>

// Loads the module and proves that its replay never leaves what the port models; both draw on one work budget.
static bool open_module(WbModule* module, const uint8_t* data, size_t size) {
    WbBudget budget = { .left = WB_WORK_BUDGET };
    if (!wb_module_load(module, data, size, &budget)) {
        return false;
    }
    if (!wb_replay_prove(module, &budget)) {
        wb_module_free(module);
        return false;
    }
    return true;
}

bool wb_probe(const uint8_t* data, size_t size) {
    WbModule module;
    if (!open_module(&module, data, size)) {
        return false;
    }
    wb_module_free(&module);
    return true;
}

WbPlayer* wb_create(const uint8_t* data, size_t size) {
    WbPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    if (!open_module(&player->module, data, size)) {
        free(player);
        return nullptr;
    }
    player->sample_ptr = calloc(player->module.sample_count, sizeof(uint32_t));
    if (player->sample_ptr == nullptr) {
        wb_module_free(&player->module);
        free(player);
        return nullptr;
    }
    return player;
}

void wb_destroy(WbPlayer* player) {
    if (player != nullptr) {
        free(player->sample_ptr);
        wb_module_free(&player->module);
        free(player);
    }
}

uint32_t wb_subsong_count(const WbPlayer* player) {
    return player->module.subsong_count;
}

const char* wb_version_name(const WbPlayer* player) {
    return player->module.version->name;
}

bool wb_start(WbPlayer* player, uint32_t subsong, NuadeVoices* voices) {
    if (player == nullptr || subsong >= player->module.subsong_count) {
        return false;
    }
    memset(player->paula, 0, sizeof(player->paula));
    wb_replay_init(&player->replay, &player->module, subsong + 1, player->paula, player->sample_ptr);
    for (uint32_t v = 0; v < WB_VOICES; v++) {
        if (voices != nullptr) {
            nuade_voice_stop(voices, v);
        }
    }
    return true;
}

WbTickResult wb_tick(WbPlayer* player, NuadeVoices* voices) {
    wb_replay_tick(&player->replay);
    for (uint32_t v = 0; v < WB_VOICES; v++) {
        nuade_paula_channel_sync(&player->paula[v], v, voices);
    }
    return (WbTickResult) { .looped = player->replay.song_end,
                            .ended = player->replay.stopped || player->replay.fault };
}

WbVoiceState wb_voice_state(const WbPlayer* player, uint32_t voice) {
    return (WbVoiceState) { .period = player->replay.last_period[voice],
                            .instrument = player->replay.last_instrument[voice] };
}

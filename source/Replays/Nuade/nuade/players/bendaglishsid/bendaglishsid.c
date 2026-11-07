// SPDX-License-Identifier: MIT
// Ben Daglish game sound driver: the entry points of the player (create, start, tick, display) around the driver in
// bds_driver.c.

#include "bds_internal.h"
#include <stdlib.h>

#define MAX_STRING 255

BdsCreateResult bds_create(const uint8_t* data, size_t size) {
    BdsPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return (BdsCreateResult) { .player = nullptr, .status = BdsStatus_OutOfMemory };
    }
    if (!bds_module_load(&player->module, data, size)) {
        free(player);
        return (BdsCreateResult) { .player = nullptr, .status = BdsStatus_InvalidData };
    }
    return (BdsCreateResult) { .player = player, .status = BdsStatus_Ok };
}

void bds_destroy(BdsPlayer* player) {
    if (player == nullptr) {
        return;
    }
    bds_module_free(&player->module);
    free(player);
}

int bds_subsong_count(const BdsPlayer* player) {
    return (int)player->module.header.subsongs;
}

bool bds_start(BdsPlayer* player, int subsong, uint32_t sample_rate, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong < 0 || subsong >= bds_subsong_count(player)) {
        return false;
    }
    player->mixer = voices;
    player->sample_rate = (double)sample_rate;
    bds_driver_start(player, subsong);
    for (int i = 0; i < BDS_VOICES; i++) {
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

BdsTickResult bds_tick(BdsPlayer* player, NuadeVoices* voices) {
    player->mixer = voices;
    bds_driver_tick(player);
    bds_driver_sync(player);
    return (BdsTickResult) { .looped = player->looped };
}

static void read_string(const BdsMemory* m, uint32_t offset, char* out, size_t capacity) {
    size_t length = 0;
    while (length + 1 < capacity && length < MAX_STRING) {
        const uint8_t c = bds_u8(m, offset + (uint32_t)length);
        if (c == 0) {
            break;
        }
        out[length++] = (char)c;
    }
    out[length] = '\0';
}

BdsInfo bds_info(const BdsPlayer* player) {
    BdsInfo info = { 0 };
    const BdsMemory* m = &player->module.memory;
    const BdsHeader* h = &player->module.header;
    read_string(m, h->title, info.title, sizeof(info.title));
    read_string(m, h->author, info.author, sizeof(info.author));
    read_string(m, h->notice, info.notice, sizeof(info.notice));
    return info;
}

BdsVoiceState bds_voice_state(const BdsPlayer* player, int voice) {
    if (voice < 0 || voice >= BDS_VOICES) {
        return (BdsVoiceState) { .note = -1, .instrument = -1 };
    }
    const BdsChannel* c = &player->channels[voice];
    return (BdsVoiceState) { .note = c->display_note, .instrument = c->display_instrument };
}

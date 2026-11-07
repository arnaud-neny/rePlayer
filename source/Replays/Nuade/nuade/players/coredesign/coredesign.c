// SPDX-License-Identifier: MIT
// Core Design player: the entry points of the player (create, start, tick, display) around the driver in cd_driver.c.

#include "cd_internal.h"
#include <stdlib.h>

#define MAX_SUBSONGS 256 // the driver takes the subsong number as a byte
#define MAX_STRING 255

CdCreateResult cd_create(const uint8_t* data, size_t size) {
    CdPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return (CdCreateResult) { .player = nullptr, .status = CdStatus_OutOfMemory };
    }
    if (!cd_module_load(&player->module, data, size)) {
        free(player);
        return (CdCreateResult) { .player = nullptr, .status = CdStatus_InvalidData };
    }
    return (CdCreateResult) { .player = player, .status = CdStatus_Ok };
}

void cd_destroy(CdPlayer* player) {
    if (player == nullptr) {
        return;
    }
    cd_module_free(&player->module);
    free(player);
}

int cd_subsong_count(const CdPlayer* player) {
    const uint32_t count = player->module.header.subsongs;
    return (int)(count < MAX_SUBSONGS ? count : MAX_SUBSONGS);
}

bool cd_start(CdPlayer* player, int subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong < 0 || subsong >= cd_subsong_count(player)) {
        return false;
    }
    cd_driver_start(player, subsong);
    for (int i = 0; i < CD_VOICES; i++) {
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

CdTickResult cd_tick(CdPlayer* player, NuadeVoices* voices) {
    cd_driver_tick(player);
    bool any_active = false;
    for (int i = 0; i < CD_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, (uint32_t)i, voices);
        any_active |= player->voices[i].active;
    }
    return (CdTickResult) { .looped = player->looped, .ended = !player->song_playing && !any_active };
}

static void read_string(const CdMemory* m, uint32_t offset, char* out, size_t capacity) {
    size_t length = 0;
    while (length + 1 < capacity && length < MAX_STRING) {
        const uint8_t c = cd_u8(m, offset + (uint32_t)length);
        if (c == 0) {
            break;
        }
        out[length++] = (char)c;
    }
    out[length] = '\0';
}

CdInfo cd_info(const CdPlayer* player) {
    CdInfo info = { 0 };
    const CdMemory* m = &player->module.memory;
    const CdHeader* h = &player->module.header;
    read_string(m, h->title, info.title, sizeof(info.title));
    read_string(m, h->author, info.author, sizeof(info.author));
    read_string(m, h->notice, info.notice, sizeof(info.notice));
    return info;
}

CdVoiceState cd_voice_state(const CdPlayer* player, int voice) {
    if (voice < 0 || voice >= CD_VOICES) {
        return (CdVoiceState) { .note = -1, .instrument = -1 };
    }
    const CdVoice* v = &player->voices[voice];
    return (CdVoiceState) { .note = v->display_note, .instrument = v->display_instrument };
}

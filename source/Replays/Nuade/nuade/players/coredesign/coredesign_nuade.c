// SPDX-License-Identifier: MIT
// Core Design game sound drivers as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/coredesign.h>
#include <stdio.h>

#define MODULE_PREFIX "core"

// Ticks one row of the display grid spans: the tracks run their own note lengths, so the grid is a display choice only.
#define CD_GRID_TICKS 6

static NuadeProbe coredesign_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return cd_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult coredesign_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const CdCreateResult created = cd_create(source->data, source->size);
    if (created.player == nullptr) {
        result.error.status
            = created.status == CdStatus_OutOfMemory ? NuadeStatus_OutOfMemory : NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a known Core Design driver");
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = CD_VOICES,
                                    .subsong_count = (uint32_t)cd_subsong_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = CD_TIMEBASE_HZ };
    return result;
}

static void coredesign_close(void* user_data) {
    cd_destroy((CdPlayer*)user_data);
}

static bool coredesign_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return cd_start((CdPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult coredesign_tick(void* user_data, NuadeVoices* voices) {
    const CdTickResult tick = cd_tick((CdPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = CD_TICK_LENGTH, .flags = flags };
}

static void coredesign_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const CdInfo info = cd_info((const CdPlayer*)user_data);
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Core Design");
    if (info.title[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_TITLE, info.title);
    }
    if (info.author[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_ARTIST, info.author);
    }
    if (info.notice[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_MESSAGE, info.notice);
    }
}

static uint32_t coredesign_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const CdPlayer* player = (const CdPlayer*)user_data;
    const uint32_t count = cap < CD_VOICES ? cap : CD_VOICES;
    for (uint32_t i = 0; i < count; i++) {
        const CdVoiceState v = cd_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return count;
}

static uint32_t coredesign_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return CD_GRID_TICKS;
}

const NuadePlayer nuade_player_coredesign = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "coredesign",
    .name = "Core Design",
    .version = "1",
    .extensions = MODULE_PREFIX,
    .probe = coredesign_probe,
    .open = coredesign_open,
    .close = coredesign_close,
    .start = coredesign_start,
    .tick = coredesign_tick,
    .metadata = coredesign_metadata,
    .voice_info = coredesign_voice_info,
    .grid_ticks = coredesign_grid_ticks,
};

CdPlayer* cd_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_coredesign) {
        return nullptr;
    }
    return (CdPlayer*)nuade_player_data(song);
}

// SPDX-License-Identifier: MIT
// Ben Daglish game sound drivers ("DAGLISH!" modules) as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/bendaglishsid.h>
#include <stdio.h>

#define MODULE_EXTENSION "bds"

// Ticks one row of the display grid spans: the voices run their own note lengths, so the grid is a display choice only.
#define BDS_GRID_TICKS 6

static NuadeProbe bendaglishsid_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return bds_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult bendaglishsid_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const BdsCreateResult created = bds_create(source->data, source->size);
    if (created.player == nullptr) {
        result.error.status
            = created.status == BdsStatus_OutOfMemory ? NuadeStatus_OutOfMemory : NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a known Ben Daglish driver");
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = BDS_VOICES,
                                    .subsong_count = (uint32_t)bds_subsong_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = BDS_TIMEBASE_HZ };
    return result;
}

static void bendaglishsid_close(void* user_data) {
    bds_destroy((BdsPlayer*)user_data);
}

static bool bendaglishsid_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    return bds_start((BdsPlayer*)user_data, (int)subsong, sample_rate, voices);
}

static NuadeTickResult bendaglishsid_tick(void* user_data, NuadeVoices* voices) {
    const BdsTickResult tick = bds_tick((BdsPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = BDS_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void bendaglishsid_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const BdsInfo info = bds_info((const BdsPlayer*)user_data);
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Ben Daglish SID");
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

static uint32_t bendaglishsid_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const BdsPlayer* player = (const BdsPlayer*)user_data;
    const uint32_t count = cap < BDS_VOICES ? cap : BDS_VOICES;
    for (uint32_t i = 0; i < count; i++) {
        const BdsVoiceState v = bds_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return count;
}

static uint32_t bendaglishsid_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return BDS_GRID_TICKS;
}

const NuadePlayer nuade_player_bendaglishsid = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "bendaglishsid",
    .name = "Ben Daglish SID",
    .version = "1",
    .extensions = MODULE_EXTENSION,
    .probe = bendaglishsid_probe,
    .open = bendaglishsid_open,
    .close = bendaglishsid_close,
    .start = bendaglishsid_start,
    .tick = bendaglishsid_tick,
    .metadata = bendaglishsid_metadata,
    .voice_info = bendaglishsid_voice_info,
    .grid_ticks = bendaglishsid_grid_ticks,
};

BdsPlayer* bds_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_bendaglishsid) {
        return nullptr;
    }
    return (BdsPlayer*)nuade_player_data(song);
}

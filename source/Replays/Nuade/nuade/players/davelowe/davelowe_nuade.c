// SPDX-License-Identifier: MIT
// Dave Lowe player as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/davelowe.h>
#include <stdio.h>

// Ticks one row of the display grid spans: the voices run their own note lengths, so the grid is a display choice only
// (4: no two starts of one voice share a row in the test songs).
#define GRID_TICKS 4

static NuadeProbe davelowe_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return dl_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult davelowe_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    DlPlayer* player = dl_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a known Dave Lowe module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = DL_VOICES,
                                    .subsong_count = dl_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = DL_TIMEBASE_HZ };
    return result;
}

static void davelowe_close(void* user_data) {
    dl_destroy((DlPlayer*)user_data);
}

static bool davelowe_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    (void)voices;
    return dl_start((DlPlayer*)user_data, subsong);
}

static NuadeTickResult davelowe_tick(void* user_data, NuadeVoices* voices) {
    const DlTickResult tick = dl_tick((DlPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = DL_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void davelowe_metadata(void* user_data, const NuadeMetadataSink* sink) {
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Dave Lowe");
    sink->set_tag(sink->ctx, NUADE_META_TOOL, dl_kind_name((const DlPlayer*)user_data));
}

static uint32_t davelowe_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const DlPlayer* player = (const DlPlayer*)user_data;
    const uint32_t n = cap < DL_VOICES ? cap : DL_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const DlVoiceState v = dl_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t davelowe_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return GRID_TICKS;
}

const NuadePlayer nuade_player_davelowe = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "davelowe",
    .name = "Dave Lowe",
    .version = "1",
    .extensions = "dl,dl_deli,dln",
    .probe = davelowe_probe,
    .open = davelowe_open,
    .close = davelowe_close,
    .start = davelowe_start,
    .tick = davelowe_tick,
    .metadata = davelowe_metadata,
    .voice_info = davelowe_voice_info,
    .grid_ticks = davelowe_grid_ticks,
};

DlPlayer* dl_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_davelowe) {
        return nullptr;
    }
    return (DlPlayer*)nuade_player_data(song);
}

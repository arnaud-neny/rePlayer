// SPDX-License-Identifier: MIT
// Desire music modules as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/desire.h>
#include <stdio.h>

#define MODULE_PREFIX "dsr"

// Ticks one row of the display grid spans: every voice runs its own speed, so the grid is a display choice only.
#define DSR_GRID_TICKS 6

static NuadeProbe desire_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return dsr_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult desire_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const DsrCreateResult created = dsr_create(source->data, source->size);
    if (created.player == nullptr) {
        result.error.status
            = created.status == DsrStatus_OutOfMemory ? NuadeStatus_OutOfMemory : NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a known Desire module");
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = DSR_VOICES,
                                    .subsong_count = (uint32_t)dsr_subsong_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = DSR_TIMEBASE_HZ };
    return result;
}

static void desire_close(void* user_data) {
    dsr_destroy((DsrPlayer*)user_data);
}

static bool desire_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return dsr_start((DsrPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult desire_tick(void* user_data, NuadeVoices* voices) {
    const DsrTickResult tick = dsr_tick((DsrPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = DSR_TICK_LENGTH, .flags = tick.ended ? NuadeTick_Ended : NuadeTick_None };
}

static void desire_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Desire");
}

static uint32_t desire_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const DsrPlayer* player = (const DsrPlayer*)user_data;
    const uint32_t count = cap < DSR_VOICES ? cap : DSR_VOICES;
    for (uint32_t i = 0; i < count; i++) {
        const DsrVoiceState v = dsr_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return count;
}

static uint32_t desire_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return DSR_GRID_TICKS;
}

const NuadePlayer nuade_player_desire = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "desire",
    .name = "Desire",
    .version = "1",
    .extensions = MODULE_PREFIX,
    .probe = desire_probe,
    .open = desire_open,
    .close = desire_close,
    .start = desire_start,
    .tick = desire_tick,
    .metadata = desire_metadata,
    .voice_info = desire_voice_info,
    .grid_ticks = desire_grid_ticks,
};

DsrPlayer* dsr_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_desire) {
        return nullptr;
    }
    return (DsrPlayer*)nuade_player_data(song);
}

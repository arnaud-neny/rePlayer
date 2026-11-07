// SPDX-License-Identifier: MIT
// Paul Robotham as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/paulrobotham.h>
#include <stdio.h>

#define PR_GRID_TICKS 6 // the voices run on their own timing: a display grid only

static NuadeProbe paulrobotham_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return pr_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult paulrobotham_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const PrCreateResult created = pr_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        switch (created.status) {
            case PrStatus_MissingSamples:
                result.error.status = NuadeStatus_IoError;
                snprintf(result.error.message, sizeof(result.error.message),
                         "the \".ssd\" sample file is missing or too short");
                break;
            case PrStatus_OutOfMemory:
                result.error.status = NuadeStatus_OutOfMemory;
                snprintf(result.error.message, sizeof(result.error.message), "out of memory");
                break;
            default:
                result.error.status = NuadeStatus_InvalidData;
                snprintf(result.error.message, sizeof(result.error.message), "not a Paul Robotham song");
                break;
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = pr_voice_count(created.player),
                                    .subsong_count = 1,
                                    .default_subsong = 0,
                                    .timebase_hz = PR_TIMEBASE_HZ };
    return result;
}

static void paulrobotham_close(void* user_data) {
    pr_destroy((PrPlayer*)user_data);
}

static bool paulrobotham_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)subsong;
    (void)sample_rate;
    return pr_start((PrPlayer*)user_data, voices);
}

static NuadeTickResult paulrobotham_tick(void* user_data, NuadeVoices* voices) {
    const PrTickResult tick = pr_tick((PrPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = PR_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void paulrobotham_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Paul Robotham");
}

static uint32_t paulrobotham_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const PrPlayer* player = (const PrPlayer*)user_data;
    const uint32_t count = pr_voice_count(player);
    const uint32_t n = cap < count ? cap : count;
    for (uint32_t i = 0; i < n; i++) {
        const PrVoiceState v = pr_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t paulrobotham_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return PR_GRID_TICKS;
}

const NuadePlayer nuade_player_paulrobotham = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "paulrobotham",
    .name = "Paul Robotham",
    .version = "1",
    .extensions = "dat",
    .probe = paulrobotham_probe,
    .open = paulrobotham_open,
    .close = paulrobotham_close,
    .start = paulrobotham_start,
    .tick = paulrobotham_tick,
    .metadata = paulrobotham_metadata,
    .voice_info = paulrobotham_voice_info,
    .grid_ticks = paulrobotham_grid_ticks,
};

PrPlayer* pr_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_paulrobotham) {
        return nullptr;
    }
    return (PrPlayer*)nuade_player_data(song);
}

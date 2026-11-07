// SPDX-License-Identifier: MIT
// Mike Davies' sound driver as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_path.h"
#include "nuade_tracker.h"
#include <nuade/players/mikedavies.h>
#include <stdio.h>

// Ticks per row of the grid the framework builds from the voice starts: the voices run on their own durations.
#define MD_GRID_TICKS 6

// The eagleplayer is found by the "md" prefix or suffix; the code check alone would claim files of other formats.

static bool mikedavies_has_name(const char* filename) {
    return filename != nullptr && nuade_path_has_ext(filename, "md");
}

static NuadeProbe mikedavies_probe(const uint8_t* data, size_t size, const char* filename) {
    if (!mikedavies_has_name(filename)) {
        return NuadeProbe_Unsupported;
    }
    MdPlayer* player = md_create(data, size);
    if (player == nullptr) {
        return NuadeProbe_Unsupported;
    }
    md_destroy(player);
    return NuadeProbe_Supported;
}

static NuadePlayerOpenResult mikedavies_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    MdPlayer* player = md_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Mike Davies module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = MD_VOICES,
                                    .subsong_count = md_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = MD_TIMEBASE_HZ };
    return result;
}

static void mikedavies_close(void* user_data) {
    md_destroy((MdPlayer*)user_data);
}

static bool mikedavies_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return md_start((MdPlayer*)user_data, subsong, voices);
}

static NuadeTickResult mikedavies_tick(void* user_data, NuadeVoices* voices) {
    const MdTickResult tick = md_tick((MdPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    flags |= tick.looped ? NuadeTick_Looped : 0u;
    flags |= tick.ended ? NuadeTick_Ended : 0u;
    return (NuadeTickResult) { .length = MD_TICK_LENGTH, .flags = flags };
}

static void mikedavies_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Mike Davies");
}

static uint32_t mikedavies_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const MdPlayer* player = (const MdPlayer*)user_data;
    const uint32_t n = cap < MD_VOICES ? cap : MD_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const MdVoiceState v = md_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t mikedavies_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return MD_GRID_TICKS;
}

const NuadePlayer nuade_player_mikedavies = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "mikedavies",
    .name = "Mike Davies",
    .version = "1",
    .extensions = "md",
    .probe = mikedavies_probe,
    .open = mikedavies_open,
    .close = mikedavies_close,
    .start = mikedavies_start,
    .tick = mikedavies_tick,
    .metadata = mikedavies_metadata,
    .voice_info = mikedavies_voice_info,
    .grid_ticks = mikedavies_grid_ticks,
};

MdPlayer* md_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_mikedavies) {
        return nullptr;
    }
    return (MdPlayer*)nuade_player_data(song);
}

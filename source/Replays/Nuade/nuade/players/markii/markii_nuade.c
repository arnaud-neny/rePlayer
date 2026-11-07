// SPDX-License-Identifier: MIT
// Mark II as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_path.h"
#include "nuade_tracker.h"
#include <nuade/players/markii.h>
#include <stdio.h>

// The eagleplayer is found by the "mii", "mk2" or "mkii" prefix or suffix; the code check alone would claim files of
// other formats.

static bool markii_has_name(const char* filename) {
    return filename != nullptr
           && (nuade_path_has_ext(filename, "mii") || nuade_path_has_ext(filename, "mk2")
               || nuade_path_has_ext(filename, "mkii"));
}

static NuadeProbe markii_probe(const uint8_t* data, size_t size, const char* filename) {
    if (!markii_has_name(filename)) {
        return NuadeProbe_Unsupported;
    }
    Mk2Player* player = mk2_create(data, size);
    if (player == nullptr) {
        return NuadeProbe_Unsupported;
    }
    mk2_destroy(player);
    return NuadeProbe_Supported;
}

static NuadePlayerOpenResult markii_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    Mk2Player* player = mk2_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Mark II module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = MK2_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = MK2_TIMEBASE_HZ
    };
    return result;
}

static void markii_close(void* user_data) {
    mk2_destroy((Mk2Player*)user_data);
}

static bool markii_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)subsong;
    (void)sample_rate;
    return mk2_start((Mk2Player*)user_data, voices);
}

static NuadeTickResult markii_tick(void* user_data, NuadeVoices* voices) {
    const Mk2TickResult tick = mk2_tick((Mk2Player*)user_data, voices);
    return (NuadeTickResult) { .length = MK2_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void markii_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Mark II");
}

static uint32_t markii_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const Mk2Player* player = (const Mk2Player*)user_data;
    const uint32_t n = cap < MK2_VOICES ? cap : MK2_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const Mk2VoiceState v = mk2_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t markii_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return mk2_grid_ticks((const Mk2Player*)user_data);
}

const NuadePlayer nuade_player_markii = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "markii",
    .name = "Mark II",
    .version = "1",
    .extensions = "mii,mk2,mkii",
    .probe = markii_probe,
    .open = markii_open,
    .close = markii_close,
    .start = markii_start,
    .tick = markii_tick,
    .metadata = markii_metadata,
    .voice_info = markii_voice_info,
    .grid_ticks = markii_grid_ticks,
};

Mk2Player* mk2_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_markii) {
        return nullptr;
    }
    return (Mk2Player*)nuade_player_data(song);
}

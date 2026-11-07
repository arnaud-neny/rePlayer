// SPDX-License-Identifier: MIT
// SUN-Tronic as a nuade player: the modules of the SUN-Tronic eagleplayer whose replayer build the port models.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/suntronic.h>
#include <stdio.h>

static NuadeProbe suntronic_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return sun_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult suntronic_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const SunCreateResult created = sun_create(source->data, source->size);
    if (created.player == NULL) {
        if (created.status == SunStatus_OutOfMemory) {
            result.error.status = NuadeStatus_OutOfMemory;
            snprintf(result.error.message, sizeof(result.error.message), "out of memory");
        } else {
            result.error.status = NuadeStatus_InvalidData;
            snprintf(result.error.message, sizeof(result.error.message),
                     "not a SUN-Tronic module with a replayer build the player models");
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) {
        .voice_count = SUN_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = SUN_TIMEBASE_HZ
    };
    return result;
}

static void suntronic_close(void* user_data) {
    sun_destroy((SunPlayer*)user_data);
}

static bool suntronic_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && sun_start((SunPlayer*)user_data, voices);
}

static NuadeTickResult suntronic_tick(void* user_data, NuadeVoices* voices) {
    const SunTickResult tick = sun_tick((SunPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = SUN_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void suntronic_metadata(void* user_data, const NuadeMetadataSink* sink) {
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, sun_format_name((const SunPlayer*)user_data));
}

static uint32_t suntronic_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const SunPlayer* player = (const SunPlayer*)user_data;
    const uint32_t n = cap < SUN_VOICES ? cap : SUN_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const SunVoiceState v = sun_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note(v.period), .instrument = v.instrument };
    }
    return n;
}

const NuadePlayer nuade_player_suntronic = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "suntronic",
    .name = "SUN-Tronic",
    .version = "1",
    .extensions = SUN_EXTENSION,
    .probe = suntronic_probe,
    .open = suntronic_open,
    .close = suntronic_close,
    .start = suntronic_start,
    .tick = suntronic_tick,
    .metadata = suntronic_metadata,
    .voice_info = suntronic_voice_info,
};

SunPlayer* sun_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_suntronic) {
        return nullptr;
    }
    return (SunPlayer*)nuade_player_data(song);
}

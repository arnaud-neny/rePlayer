// SPDX-License-Identifier: MIT
// Activision Pro (Martin Walker) as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/activisionpro.h>
#include <stdio.h>

// Recognising the embedded player code and loading every module table is specific enough to be sure.
static NuadeProbe activisionpro_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return avp_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult activisionpro_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    AvpModule* module = avp_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Activision Pro module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) { .voice_count = AVP_VOICES,
                                    .subsong_count = (uint32_t)avp_subsong_count(module),
                                    .timebase_hz = AVP_TICK_HZ,
                                    .default_subsong = 0 };
    return result;
}

static void activisionpro_close(void* user_data) {
    avp_destroy((AvpModule*)user_data);
}

static bool activisionpro_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return avp_start((AvpModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult activisionpro_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    const bool playing = avp_tick((AvpModule*)user_data);
    return (NuadeTickResult) { .length = 1, .flags = playing ? NuadeTick_None : NuadeTick_Looped };
}

static void activisionpro_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Activision Pro");
}

static uint32_t activisionpro_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const uint32_t n = cap < AVP_VOICES ? cap : AVP_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const AvpVoiceState v = avp_voice_state((const AvpModule*)user_data, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.period > 0 ? nuade_period_to_note(v.period) : -1,
                                    .instrument = v.instrument };
    }
    return n;
}

static uint32_t activisionpro_grid_ticks(void* user_data, uint32_t subsong) {
    return avp_ticks_per_step((const AvpModule*)user_data, (int)subsong);
}

const NuadePlayer nuade_player_activisionpro = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "activisionpro",
    .name = "Activision Pro",
    .version = "1",
    .extensions = "avp,mw",
    .probe = activisionpro_probe,
    .open = activisionpro_open,
    .close = activisionpro_close,
    .start = activisionpro_start,
    .tick = activisionpro_tick,
    .metadata = activisionpro_metadata,
    .voice_info = activisionpro_voice_info,
    .grid_ticks = activisionpro_grid_ticks,
};

AvpModule* avp_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_activisionpro) {
        return nullptr;
    }
    return (AvpModule*)nuade_player_data(song);
}

// SPDX-License-Identifier: MIT
// Sound Control (3.x / 4.0 / 5.0) as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/soundcontrol.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct SoundControlNuade {
    ScModule* module;
    uint32_t subsong;
} SoundControlNuade;

// The header checks are structural only, so a match is "unsure".
static NuadeProbe soundcontrol_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return sc_probe(data, size) ? NuadeProbe_Unsure : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult soundcontrol_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    ScModule* module = sc_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Sound Control module");
        return result;
    }
    SoundControlNuade* nuade = (SoundControlNuade*)calloc(1, sizeof(SoundControlNuade));
    if (nuade == nullptr) {
        sc_destroy(module);
        result.error.status = NuadeStatus_OutOfMemory;
        return result;
    }
    nuade->module = module;
    result.user_data = nuade;
    result.info = (NuadeSongInfo) { .voice_count = SC_CHANNELS,
                                    .subsong_count = (uint32_t)sc_subsong_count(module),
                                    .default_subsong = 0,
                                    .timebase_hz = SC_TICK_HZ };
    return result;
}

static void soundcontrol_close(void* user_data) {
    SoundControlNuade* nuade = (SoundControlNuade*)user_data;
    sc_destroy(nuade->module);
    free(nuade);
}

static bool soundcontrol_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    SoundControlNuade* nuade = (SoundControlNuade*)user_data;
    nuade->subsong = subsong;
    return sc_start(nuade->module, (int)subsong, voices);
}

static NuadeTickResult soundcontrol_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    const bool looped = sc_tick(((SoundControlNuade*)user_data)->module);
    return (NuadeTickResult) { .length = 1, .flags = looped ? NuadeTick_Looped : NuadeTick_None };
}

static uint32_t soundcontrol_grid_ticks(void* user_data, uint32_t subsong) {
    return sc_ticks_per_step(((SoundControlNuade*)user_data)->module, (int)subsong);
}

static uint32_t soundcontrol_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const ScModule* module = ((const SoundControlNuade*)user_data)->module;
    const uint32_t n = cap < SC_CHANNELS ? cap : SC_CHANNELS;
    for (uint32_t i = 0; i < n; i++) {
        const ScVoiceState v = sc_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static void soundcontrol_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Sound Control");
}

const NuadePlayer nuade_player_soundcontrol = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "soundcontrol",
    .name = "Sound Control",
    .version = "1",
    .extensions = "sc,sct",
    .probe = soundcontrol_probe,
    .open = soundcontrol_open,
    .close = soundcontrol_close,
    .start = soundcontrol_start,
    .tick = soundcontrol_tick,
    .metadata = soundcontrol_metadata,
    .voice_info = soundcontrol_voice_info,
    .grid_ticks = soundcontrol_grid_ticks,
};

ScModule* sc_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_soundcontrol) {
        return nullptr;
    }
    return ((SoundControlNuade*)nuade_player_data(song))->module;
}

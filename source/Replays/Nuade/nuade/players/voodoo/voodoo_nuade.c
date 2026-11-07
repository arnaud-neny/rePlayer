// SPDX-License-Identifier: MIT
// Voodoo Supreme Synthesizer as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/voodoo.h>
#include <stdio.h>

#define VOODOO_NUADE_VOICES 4

// The footer mark plus a sane subsong count is specific enough to be sure.
static NuadeProbe voodoo_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return vs_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult voodoo_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    VsModule* module = vs_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Voodoo Supreme module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) { .voice_count = VOODOO_NUADE_VOICES,
                                    .subsong_count = (uint32_t)vs_subsong_count(module),
                                    .default_subsong = 0,
                                    .timebase_hz = VS_TICK_HZ };
    return result;
}

static void voodoo_close(void* user_data) {
    vs_destroy((VsModule*)user_data);
}

static bool voodoo_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return vs_start((VsModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult voodoo_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    return (NuadeTickResult) { .length = 1,
                               .flags = vs_tick((VsModule*)user_data) ? NuadeTick_Looped : NuadeTick_None };
}

// Track waits come in even tick counts, so a note starts on a row boundary.
#define VOODOO_TICKS_PER_ROW 2

static uint32_t voodoo_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return VOODOO_TICKS_PER_ROW;
}

static uint32_t voodoo_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const VsModule* module = (const VsModule*)user_data;
    const uint32_t n = cap < VOODOO_NUADE_VOICES ? cap : VOODOO_NUADE_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const VsVoiceState v = vs_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static void voodoo_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Voodoo Supreme Synthesizer");
}

const NuadePlayer nuade_player_voodoo = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "voodoo",
    .name = "Voodoo Supreme Synthesizer",
    .version = "1",
    .extensions = "vss",
    .probe = voodoo_probe,
    .open = voodoo_open,
    .close = voodoo_close,
    .start = voodoo_start,
    .tick = voodoo_tick,
    .metadata = voodoo_metadata,
    .voice_info = voodoo_voice_info,
    .grid_ticks = voodoo_grid_ticks,
};

VsModule* vs_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_voodoo) {
        return nullptr;
    }
    return (VsModule*)nuade_player_data(song);
}

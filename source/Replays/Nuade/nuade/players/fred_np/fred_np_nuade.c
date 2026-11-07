// SPDX-License-Identifier: MIT
// Fred Editor as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/fred_np.h>
#include <stdio.h>

#define FRED_NP_VOICES 4

// fred_probe accepts the "Fred Editor " signature with its end mark, or the Final replayer's jump table followed by a
// complete conversion; both are specific.
static NuadeProbe fred_np_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return (data != nullptr && fred_probe(data, size)) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult fred_np_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    FredModule* module = fred_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Fred Editor module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) { .voice_count = FRED_NP_VOICES,
                                    .subsong_count = (uint32_t)fred_subsong_count(module),
                                    .default_subsong = 0,
                                    .timebase_hz = FRED_TICK_HZ };
    return result;
}

static void fred_np_close(void* user_data) {
    fred_destroy((FredModule*)user_data);
}

static bool fred_np_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return fred_start((FredModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult fred_np_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    return (NuadeTickResult) { .length = 1,
                               .flags = fred_tick((FredModule*)user_data) ? NuadeTick_Looped : NuadeTick_None };
}

static uint32_t row_ticks(int tempo) {
    return tempo > 0 ? (uint32_t)tempo : 1;
}

// A track line lasts the tempo's worth of ticks, so the subsong's starting tempo is a row.
static uint32_t fred_np_grid_ticks(void* user_data, uint32_t subsong) {
    return row_ticks(fred_start_tempo((const FredModule*)user_data, (int)subsong));
}

static uint32_t fred_np_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const FredModule* module = (const FredModule*)user_data;
    const uint32_t n = cap < FRED_NP_VOICES ? cap : FRED_NP_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const FredVoiceState v = fred_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static void fred_np_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Fred Editor");
}

const NuadePlayer nuade_player_fred_np = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "fred_np",
    .name = "Fred Editor",
    .version = "1",
    .extensions = "fred",
    .probe = fred_np_probe,
    .open = fred_np_open,
    .close = fred_np_close,
    .start = fred_np_start,
    .tick = fred_np_tick,
    .metadata = fred_np_metadata,
    .voice_info = fred_np_voice_info,
    .grid_ticks = fred_np_grid_ticks,
};

FredModule* fred_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_fred_np) {
        return nullptr;
    }
    return (FredModule*)nuade_player_data(song);
}

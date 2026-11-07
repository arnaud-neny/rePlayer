// SPDX-License-Identifier: MIT
// Cinemaware as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/cinemaware.h>
#include <stdio.h>

// The grid of the pattern view: the events come at any tick.
#define GRID_TICKS 6

static NuadeProbe cinemaware_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return cw_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult cinemaware_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const CwCreateResult created = cw_create(source->data, source->size, source->path, source->io);
    switch (created.status) {
        case CwStatus_Ok:
            result.user_data = created.player;
            result.info = (NuadeSongInfo) {
                .voice_count = CW_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = CW_TIMEBASE_HZ
            };
            break;
        case CwStatus_MissingSample:
            result.error.status = NuadeStatus_IoError;
            snprintf(result.error.message, sizeof(result.error.message), "sample \"%s\" not found in Instruments/",
                     created.missing_sample);
            break;
        case CwStatus_OutOfMemory:
            result.error.status = NuadeStatus_OutOfMemory;
            snprintf(result.error.message, sizeof(result.error.message), "out of memory");
            break;
        case CwStatus_InvalidData:
            result.error.status = NuadeStatus_InvalidData;
            snprintf(result.error.message, sizeof(result.error.message), "not a valid Cinemaware module");
            break;
    }
    return result;
}

static void cinemaware_close(void* user_data) {
    cw_destroy((CwPlayer*)user_data);
}

static bool cinemaware_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && cw_start((CwPlayer*)user_data, voices);
}

static NuadeTickResult cinemaware_tick(void* user_data, NuadeVoices* voices) {
    const CwTickResult tick = cw_tick((CwPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = CW_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void cinemaware_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const CwPlayer* player = (const CwPlayer*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Cinemaware");
    for (int i = 0; i < CW_PROGRAMS; i++) {
        const char* name = cw_instrument_name(player, i);
        if (name[0] != '\0') {
            sink->add_instrument(sink->ctx, (uint32_t)i, name);
        }
    }
}

static uint32_t cinemaware_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const CwPlayer* player = (const CwPlayer*)user_data;
    const uint32_t n = cap < CW_VOICES ? cap : CW_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const CwVoiceState v = cw_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t cinemaware_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return GRID_TICKS;
}

const NuadePlayer nuade_player_cinemaware = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "cinemaware",
    .name = "Cinemaware",
    .version = "1",
    .extensions = "cin",
    .probe = cinemaware_probe,
    .open = cinemaware_open,
    .close = cinemaware_close,
    .start = cinemaware_start,
    .tick = cinemaware_tick,
    .metadata = cinemaware_metadata,
    .voice_info = cinemaware_voice_info,
    .grid_ticks = cinemaware_grid_ticks,
};

CwPlayer* cw_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_cinemaware) {
        return nullptr;
    }
    return (CwPlayer*)nuade_player_data(song);
}

// SPDX-License-Identifier: MIT
// Infogrames music as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/infogrames.h>
#include <stdio.h>

#define MODULE_EXTENSION "dum"

// Ticks one row of the display grid spans: the voices run their own note lengths, so the grid is a display choice only.
#define IG_GRID_TICKS 6

static NuadeProbe infogrames_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return ig_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult infogrames_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const IgCreateResult created = ig_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        result.error.status
            = created.status == IgStatus_OutOfMemory ? NuadeStatus_OutOfMemory : NuadeStatus_InvalidData;
        if (created.status == IgStatus_MissingSamples) {
            result.error.status = NuadeStatus_IoError;
            snprintf(result.error.message, sizeof(result.error.message), "the \".ins\" sample file is missing");
        } else {
            snprintf(result.error.message, sizeof(result.error.message), "not an Infogrames song");
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) {
        .voice_count = IG_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = IG_TIMEBASE_HZ
    };
    return result;
}

static void infogrames_close(void* user_data) {
    ig_destroy((IgPlayer*)user_data);
}

static bool infogrames_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && ig_start((IgPlayer*)user_data, voices);
}

static NuadeTickResult infogrames_tick(void* user_data, NuadeVoices* voices) {
    IgPlayer* player = (IgPlayer*)user_data;
    const IgTickResult tick = ig_tick(player, voices);
    return (NuadeTickResult) { .length = ig_tick_length(player),
                               .flags = tick.ended ? NuadeTick_Ended : NuadeTick_None };
}

static void infogrames_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Infogrames");
}

static uint32_t infogrames_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const IgPlayer* player = (const IgPlayer*)user_data;
    const uint32_t count = cap < IG_VOICES ? cap : IG_VOICES;
    for (uint32_t i = 0; i < count; i++) {
        const IgVoiceState v = ig_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return count;
}

static uint32_t infogrames_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return IG_GRID_TICKS;
}

const NuadePlayer nuade_player_infogrames = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "infogrames",
    .name = "Infogrames",
    .version = "1",
    .extensions = MODULE_EXTENSION,
    .probe = infogrames_probe,
    .open = infogrames_open,
    .close = infogrames_close,
    .start = infogrames_start,
    .tick = infogrames_tick,
    .metadata = infogrames_metadata,
    .voice_info = infogrames_voice_info,
    .grid_ticks = infogrames_grid_ticks,
};

IgPlayer* ig_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_infogrames) {
        return nullptr;
    }
    return (IgPlayer*)nuade_player_data(song);
}

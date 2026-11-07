// SPDX-License-Identifier: MIT
// Mark Cooksey (old format) as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/markcooksey_old.h>
#include <stdio.h>

// The grid of the pattern view: the voices play on their own timing.
#define GRID_TICKS 6

static NuadeProbe markcooksey_old_probe(const uint8_t* data, size_t size, const char* filename) {
    return mco_probe(data, size, filename) != McoKind_None ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult markcooksey_old_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const McoCreateResult created = mco_create(source->data, source->size, source->path, source->io);
    switch (created.status) {
        case McoStatus_Ok:
            result.user_data = created.player;
            result.info = (NuadeSongInfo) { .voice_count = MCO_VOICES,
                                            .subsong_count = (uint32_t)mco_subsong_count(created.player),
                                            .default_subsong = 0,
                                            .timebase_hz = MCO_TIMEBASE_HZ };
            break;
        case McoStatus_MissingData:
            result.error.status = NuadeStatus_IoError;
            snprintf(result.error.message, sizeof(result.error.message), "the mcs. data file next to the mcr. file");
            break;
        case McoStatus_OutOfMemory:
            result.error.status = NuadeStatus_OutOfMemory;
            snprintf(result.error.message, sizeof(result.error.message), "out of memory");
            break;
        case McoStatus_InvalidData:
            result.error.status = NuadeStatus_InvalidData;
            snprintf(result.error.message, sizeof(result.error.message), "not a Mark Cooksey (old format) song");
            break;
    }
    return result;
}

static void markcooksey_old_close(void* user_data) {
    mco_destroy((McoPlayer*)user_data);
}

static bool markcooksey_old_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return mco_start((McoPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult markcooksey_old_tick(void* user_data, NuadeVoices* voices) {
    const McoTickResult tick = mco_tick((McoPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = MCO_TICK_LENGTH,
                               .flags = (tick.looped ? NuadeTick_Looped : NuadeTick_None)
                                        | (tick.ended ? NuadeTick_Ended : 0) };
}

static void markcooksey_old_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Mark Cooksey (old)");
}

static uint32_t markcooksey_old_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const McoPlayer* player = (const McoPlayer*)user_data;
    const uint32_t n = cap < MCO_VOICES ? cap : MCO_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const McoVoiceState v = mco_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t markcooksey_old_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return GRID_TICKS;
}

const NuadePlayer nuade_player_markcooksey_old = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "markcooksey_old",
    .name = "Mark Cooksey (old)",
    .version = "1",
    .extensions = "mco,mcr",
    .probe = markcooksey_old_probe,
    .open = markcooksey_old_open,
    .close = markcooksey_old_close,
    .start = markcooksey_old_start,
    .tick = markcooksey_old_tick,
    .metadata = markcooksey_old_metadata,
    .voice_info = markcooksey_old_voice_info,
    .grid_ticks = markcooksey_old_grid_ticks,
};

McoPlayer* mco_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_markcooksey_old) {
        return nullptr;
    }
    return (McoPlayer*)nuade_player_data(song);
}

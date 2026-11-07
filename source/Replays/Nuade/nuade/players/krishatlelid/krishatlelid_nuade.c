// SPDX-License-Identifier: MIT
// Kris Hatlelid as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/krishatlelid.h>
#include <stdio.h>

// Ticks per row of the display grid: the shortest note the songs use is 6 ticks long
#define KH_GRID_TICKS 6

static NuadeProbe krishatlelid_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return kh_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult krishatlelid_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const KhCreateResult created = kh_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        switch (created.status) {
            case KhStatus_OutOfMemory:
                result.error.status = NuadeStatus_OutOfMemory;
                snprintf(result.error.message, sizeof(result.error.message), "out of memory");
                break;
            case KhStatus_MissingDriver:
                result.error.status = NuadeStatus_IoError;
                snprintf(result.error.message, sizeof(result.error.message), "the driver file \"songplay\" is missing");
                break;
            default:
                result.error.status = NuadeStatus_InvalidData;
                snprintf(result.error.message, sizeof(result.error.message), "not a known Kris Hatlelid song");
                break;
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = KH_VOICES,
                                    .subsong_count = kh_subsong_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = KH_TIMEBASE_HZ };
    return result;
}

static void krishatlelid_close(void* user_data) {
    kh_destroy((KhPlayer*)user_data);
}

static bool krishatlelid_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return kh_start((KhPlayer*)user_data, subsong, voices);
}

static NuadeTickResult krishatlelid_tick(void* user_data, NuadeVoices* voices) {
    const KhTickResult tick = kh_tick((KhPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = tick.length, .flags = flags };
}

static void krishatlelid_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Kris Hatlelid");
}

static uint32_t krishatlelid_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const KhPlayer* player = (const KhPlayer*)user_data;
    const uint32_t n = cap < KH_VOICES ? cap : KH_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note(kh_voice_period(player, i)),
                                    .instrument = kh_voice_instrument(player, i) };
    }
    return n;
}

static uint32_t krishatlelid_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return KH_GRID_TICKS;
}

const NuadePlayer nuade_player_krishatlelid = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "krishatlelid",
    .name = "Kris Hatlelid",
    .version = "1",
    .extensions = "kh",
    .probe = krishatlelid_probe,
    .open = krishatlelid_open,
    .close = krishatlelid_close,
    .start = krishatlelid_start,
    .tick = krishatlelid_tick,
    .metadata = krishatlelid_metadata,
    .voice_info = krishatlelid_voice_info,
    .grid_ticks = krishatlelid_grid_ticks,
};

KhPlayer* kh_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_krishatlelid) {
        return nullptr;
    }
    return (KhPlayer*)nuade_player_data(song);
}

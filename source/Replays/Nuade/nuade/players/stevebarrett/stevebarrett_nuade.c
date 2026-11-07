// SPDX-License-Identifier: MIT
// Steve Barrett as a nuade player: the modules of the SteveBarrett eagleplayer whose driver the port models.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/stevebarrett.h>
#include <stdio.h>

static NuadeProbe stevebarrett_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return sbr_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult stevebarrett_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const SbrCreateResult created = sbr_create(source->data, source->size);
    if (created.player == nullptr) {
        if (created.status == SbrStatus_OutOfMemory) {
            result.error.status = NuadeStatus_OutOfMemory;
            snprintf(result.error.message, sizeof(result.error.message), "out of memory");
        } else {
            result.error.status = NuadeStatus_InvalidData;
            snprintf(result.error.message, sizeof(result.error.message),
                     "not a Steve Barrett module with a driver the player models");
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = SBR_VOICES,
                                    .subsong_count = sbr_song_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = SBR_TIMEBASE_HZ };
    return result;
}

static void stevebarrett_close(void* user_data) {
    sbr_destroy((SbrPlayer*)user_data);
}

static bool stevebarrett_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return sbr_start((SbrPlayer*)user_data, subsong, voices);
}

static NuadeTickResult stevebarrett_tick(void* user_data, NuadeVoices* voices) {
    const SbrTickResult tick = sbr_tick((SbrPlayer*)user_data, voices);
    NuadeTickFlags flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = tick.length, .flags = flags };
}

static void stevebarrett_metadata(void* user_data, const NuadeMetadataSink* sink) {
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, sbr_format_name((const SbrPlayer*)user_data));
}

static uint32_t stevebarrett_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const SbrPlayer* player = (const SbrPlayer*)user_data;
    const uint32_t n = cap < SBR_VOICES ? cap : SBR_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const SbrVoiceState v = sbr_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note(v.period), .instrument = v.instrument };
    }
    return n;
}

const NuadePlayer nuade_player_stevebarrett = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "stevebarrett",
    .name = "Steve Barrett",
    .version = "1",
    .extensions = SBR_EXTENSION,
    .probe = stevebarrett_probe,
    .open = stevebarrett_open,
    .close = stevebarrett_close,
    .start = stevebarrett_start,
    .tick = stevebarrett_tick,
    .metadata = stevebarrett_metadata,
    .voice_info = stevebarrett_voice_info,
};

SbrPlayer* sbr_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_stevebarrett) {
        return nullptr;
    }
    return (SbrPlayer*)nuade_player_data(song);
}

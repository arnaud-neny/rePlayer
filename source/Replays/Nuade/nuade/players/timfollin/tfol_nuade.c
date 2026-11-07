// SPDX-License-Identifier: MIT
// Tim Follin player as a nuade player.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/timfollin.h>
#include <stdio.h>

static NuadeProbe tfol_nuade_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return tfol_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult tfol_nuade_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const TfolCreateResult created = tfol_create(source->data, source->size);
    if (created.player == nullptr) {
        if (created.status == TfolStatus_OutOfMemory) {
            result.error.status = NuadeStatus_OutOfMemory;
            snprintf(result.error.message, sizeof(result.error.message), "out of memory");
        } else {
            result.error.status = NuadeStatus_InvalidData;
            snprintf(result.error.message, sizeof(result.error.message), "not a Tim Follin module");
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = TFOL_VOICES,
                                    .subsong_count = tfol_subsong_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = TFOL_TIMEBASE_HZ };
    return result;
}

static void tfol_nuade_close(void* user_data) {
    tfol_destroy((TfolPlayer*)user_data);
}

static bool tfol_nuade_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return tfol_start((TfolPlayer*)user_data, subsong, voices);
}

static NuadeTickResult tfol_nuade_tick(void* user_data, NuadeVoices* voices) {
    const TfolTickResult tick = tfol_tick((TfolPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = TFOL_TICK_LENGTH, .flags = tick.ended ? NuadeTick_Ended : NuadeTick_None };
}

static void tfol_nuade_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Tim Follin");
}

static uint32_t tfol_nuade_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const TfolPlayer* player = (const TfolPlayer*)user_data;
    const uint32_t n = cap < TFOL_VOICES ? cap : TFOL_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const TfolVoiceState v = tfol_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note(v.period), .instrument = v.instrument };
    }
    return n;
}

const NuadePlayer nuade_player_timfollin = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "timfollin",
    .name = "Tim Follin",
    .version = "1",
    .extensions = TFOL_EXTENSION,
    .probe = tfol_nuade_probe,
    .open = tfol_nuade_open,
    .close = tfol_nuade_close,
    .start = tfol_nuade_start,
    .tick = tfol_nuade_tick,
    .metadata = tfol_nuade_metadata,
    .voice_info = tfol_nuade_voice_info,
};

TfolPlayer* tfol_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_timfollin) {
        return nullptr;
    }
    return (TfolPlayer*)nuade_player_data(song);
}

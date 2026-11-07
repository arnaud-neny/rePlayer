// SPDX-License-Identifier: MIT
// Thomas Hermann player as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/thomashermann.h>
#include <stdio.h>

static NuadeProbe thomashermann_probe(const uint8_t* data, size_t size, const char* filename) {
    return thm_probe(data, size, filename) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult thomashermann_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const ThmCreateResult created = thm_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        switch (created.status) {
            case ThmStatus_OutOfMemory:
                result.error.status = NuadeStatus_OutOfMemory;
                snprintf(result.error.message, sizeof(result.error.message), "out of memory");
                break;
            case ThmStatus_MissingSamples:
                result.error.status = NuadeStatus_IoError;
                snprintf(result.error.message, sizeof(result.error.message), "the \"smp.\" sample file is missing");
                break;
            default:
                result.error.status = NuadeStatus_InvalidData;
                snprintf(result.error.message, sizeof(result.error.message),
                         "not a Thomas Hermann module the player models");
                break;
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = THM_VOICES,
                                    .subsong_count = thm_subsong_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = THM_TIMEBASE_HZ };
    return result;
}

static void thomashermann_close(void* user_data) {
    thm_destroy((ThmPlayer*)user_data);
}

static bool thomashermann_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    return thm_start((ThmPlayer*)user_data, subsong, sample_rate, voices);
}

static NuadeTickResult thomashermann_tick(void* user_data, NuadeVoices* voices) {
    const ThmTickResult tick = thm_tick((ThmPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = tick.length, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void thomashermann_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Thomas Hermann");
}

static uint32_t thomashermann_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const ThmPlayer* player = (const ThmPlayer*)user_data;
    const uint32_t n = cap < THM_VOICES ? cap : THM_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const ThmVoiceState v = thm_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note(v.period), .instrument = v.instrument };
    }
    return n;
}

const NuadePlayer nuade_player_thomashermann = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "thomashermann",
    .name = "Thomas Hermann",
    .version = "1",
    .extensions = "thm",
    .probe = thomashermann_probe,
    .open = thomashermann_open,
    .close = thomashermann_close,
    .start = thomashermann_start,
    .tick = thomashermann_tick,
    .metadata = thomashermann_metadata,
    .voice_info = thomashermann_voice_info,
};

ThmPlayer* thm_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_thomashermann) {
        return nullptr;
    }
    return (ThmPlayer*)nuade_player_data(song);
}

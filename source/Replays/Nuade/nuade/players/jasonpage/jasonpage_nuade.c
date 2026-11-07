// SPDX-License-Identifier: MIT
// Jason Page format as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/jasonpage.h>
#include <stdio.h>

// The module is 68k data with fixed table positions; the check of the eagleplayer is specific enough to be sure.
static NuadeProbe jasonpage_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return jp_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult jasonpage_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const JpCreateResult created = jp_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        switch (created.status) {
            case JpStatus_MissingSamples:
                result.error.status = NuadeStatus_IoError;
                snprintf(result.error.message, sizeof(result.error.message), "the \"smp.\" sample file is missing");
                break;
            case JpStatus_OutOfMemory:
                result.error.status = NuadeStatus_OutOfMemory;
                snprintf(result.error.message, sizeof(result.error.message), "out of memory");
                break;
            default:
                result.error.status = NuadeStatus_InvalidData;
                snprintf(result.error.message, sizeof(result.error.message), "not a valid Jason Page module");
                break;
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = JP_VOICES,
                                    .subsong_count = (uint32_t)jp_subsong_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = JP_TIMEBASE_HZ };
    return result;
}

static void jasonpage_close(void* user_data) {
    jp_destroy((JpPlayer*)user_data);
}

static bool jasonpage_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return jp_start((JpPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult jasonpage_tick(void* user_data, NuadeVoices* voices) {
    const JpTickResult tick = jp_tick((JpPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = JP_TICK_LENGTH, .flags = flags };
}

static void jasonpage_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Jason Page");
}

static uint32_t jasonpage_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const JpPlayer* player = (const JpPlayer*)user_data;
    const uint32_t count = cap < JP_VOICES ? cap : JP_VOICES;
    for (uint32_t i = 0; i < count; i++) {
        const JpVoiceState state = jp_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = state.note, .instrument = state.instrument };
    }
    return count;
}

static uint32_t jasonpage_grid_ticks(void* user_data, uint32_t subsong) {
    return jp_grid_ticks((const JpPlayer*)user_data, (int)subsong);
}

const NuadePlayer nuade_player_jasonpage = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "jasonpage",
    .name = "Jason Page",
    .version = "1",
    .extensions = "jpn,jpnd",
    .probe = jasonpage_probe,
    .open = jasonpage_open,
    .close = jasonpage_close,
    .start = jasonpage_start,
    .tick = jasonpage_tick,
    .metadata = jasonpage_metadata,
    .voice_info = jasonpage_voice_info,
    .grid_ticks = jasonpage_grid_ticks,
};

JpPlayer* jp_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_jasonpage) {
        return nullptr;
    }
    return (JpPlayer*)nuade_player_data(song);
}

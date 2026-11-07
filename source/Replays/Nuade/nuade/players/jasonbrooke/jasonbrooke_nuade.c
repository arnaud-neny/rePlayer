// SPDX-License-Identifier: MIT
// Jason C. Brooke player as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/jasonbrooke.h>
#include <stdio.h>

static NuadeProbe jasonbrooke_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return jb_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult jasonbrooke_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    JbPlayer* player = jb_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a known Jason C. Brooke driver");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = JB_VOICES,
                                    .subsong_count = (uint32_t)jb_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = JB_TIMEBASE_HZ };
    return result;
}

static void jasonbrooke_close(void* user_data) {
    jb_destroy((JbPlayer*)user_data);
}

static bool jasonbrooke_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    (void)voices;
    return jb_start((JbPlayer*)user_data, (int)subsong);
}

static NuadeTickResult jasonbrooke_tick(void* user_data, NuadeVoices* voices) {
    const JbTickResult tick = jb_tick((JbPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = JB_TICK_LENGTH,
                               .flags = (tick.looped ? NuadeTick_Looped : NuadeTick_None)
                                        | (tick.ended ? NuadeTick_Ended : 0) };
}

static void jasonbrooke_metadata(void* user_data, const NuadeMetadataSink* sink) {
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Jason C. Brooke");
    sink->set_tag(sink->ctx, NUADE_META_TOOL, jb_driver_name((const JbPlayer*)user_data));
}

static uint32_t jasonbrooke_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const JbPlayer* player = (const JbPlayer*)user_data;
    const uint32_t n = cap < JB_VOICES ? cap : JB_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const JbVoiceState v = jb_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.sample };
    }
    return n;
}

// One grid row is one sequencer step
static uint32_t jasonbrooke_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return (uint32_t)jb_ticks_per_step((const JbPlayer*)user_data);
}

const NuadePlayer nuade_player_jasonbrooke = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "jasonbrooke",
    .name = "Jason C. Brooke",
    .version = "1",
    .extensions = "jb,jcb,jcbo",
    .probe = jasonbrooke_probe,
    .open = jasonbrooke_open,
    .close = jasonbrooke_close,
    .start = jasonbrooke_start,
    .tick = jasonbrooke_tick,
    .metadata = jasonbrooke_metadata,
    .voice_info = jasonbrooke_voice_info,
    .grid_ticks = jasonbrooke_grid_ticks,
};

JbPlayer* jb_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_jasonbrooke) {
        return nullptr;
    }
    return (JbPlayer*)nuade_player_data(song);
}

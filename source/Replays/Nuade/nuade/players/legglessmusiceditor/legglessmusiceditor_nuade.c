// SPDX-License-Identifier: MIT
// Leggless Music Editor as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/legglessmusiceditor.h>
#include <stdio.h>

static NuadeProbe legglessmusiceditor_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return lme_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult legglessmusiceditor_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    LmePlayer* player = lme_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Leggless Music Editor song");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = LME_VOICES,
                                    .subsong_count = lme_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = LME_TIMEBASE_HZ };
    return result;
}

static void legglessmusiceditor_close(void* user_data) {
    lme_destroy((LmePlayer*)user_data);
}

static bool legglessmusiceditor_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return lme_start((LmePlayer*)user_data, subsong, voices);
}

static NuadeTickResult legglessmusiceditor_tick(void* user_data, NuadeVoices* voices) {
    const LmeTickResult tick = lme_tick((LmePlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = LME_TICK_LENGTH, .flags = flags };
}

static void legglessmusiceditor_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Leggless Music Editor");
}

static uint32_t legglessmusiceditor_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const LmePlayer* player = (const LmePlayer*)user_data;
    const uint32_t n = cap < LME_VOICES ? cap : LME_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const LmeVoiceState v = lme_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t legglessmusiceditor_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return lme_grid_ticks((const LmePlayer*)user_data);
}

const NuadePlayer nuade_player_legglessmusiceditor = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "legglessmusiceditor",
    .name = "Leggless Music Editor",
    .version = "1",
    .extensions = "lme",
    .probe = legglessmusiceditor_probe,
    .open = legglessmusiceditor_open,
    .close = legglessmusiceditor_close,
    .start = legglessmusiceditor_start,
    .tick = legglessmusiceditor_tick,
    .metadata = legglessmusiceditor_metadata,
    .voice_info = legglessmusiceditor_voice_info,
    .grid_ticks = legglessmusiceditor_grid_ticks,
};

LmePlayer* lme_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_legglessmusiceditor) {
        return nullptr;
    }
    return (LmePlayer*)nuade_player_data(song);
}

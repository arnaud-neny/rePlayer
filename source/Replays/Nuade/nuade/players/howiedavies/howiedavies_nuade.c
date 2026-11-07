// SPDX-License-Identifier: MIT
// Howie Davies as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/howiedavies.h>
#include <stdio.h>

#define HD_TEXT_BYTES 256

static NuadeProbe howiedavies_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return hd_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult howiedavies_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    HdPlayer* player = hd_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Howie Davies song");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = HD_VOICES,
                                    .subsong_count = hd_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = HD_TIMEBASE_HZ };
    return result;
}

static void howiedavies_close(void* user_data) {
    hd_destroy((HdPlayer*)user_data);
}

static bool howiedavies_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return hd_start((HdPlayer*)user_data, subsong, voices);
}

static NuadeTickResult howiedavies_tick(void* user_data, NuadeVoices* voices) {
    const HdTickResult tick = hd_tick((HdPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = HD_TICK_LENGTH, .flags = flags };
}

static void howiedavies_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const HdPlayer* player = (const HdPlayer*)user_data;
    char text[HD_TEXT_BYTES];
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Howie Davies");
    hd_text(player, HdText_Title, text, sizeof(text));
    if (text[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_TITLE, text);
    }
    hd_text(player, HdText_Artist, text, sizeof(text));
    if (text[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_ARTIST, text);
    }
    hd_text(player, HdText_Message, text, sizeof(text));
    if (text[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_MESSAGE, text);
    }
}

static uint32_t howiedavies_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const HdPlayer* player = (const HdPlayer*)user_data;
    const uint32_t n = cap < HD_VOICES ? cap : HD_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const HdVoiceState v = hd_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t howiedavies_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return hd_grid_ticks((const HdPlayer*)user_data);
}

const NuadePlayer nuade_player_howiedavies = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "howiedavies",
    .name = "Howie Davies",
    .version = "1",
    .extensions = "hd",
    .probe = howiedavies_probe,
    .open = howiedavies_open,
    .close = howiedavies_close,
    .start = howiedavies_start,
    .tick = howiedavies_tick,
    .metadata = howiedavies_metadata,
    .voice_info = howiedavies_voice_info,
    .grid_ticks = howiedavies_grid_ticks,
};

HdPlayer* hd_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_howiedavies) {
        return nullptr;
    }
    return (HdPlayer*)nuade_player_data(song);
}

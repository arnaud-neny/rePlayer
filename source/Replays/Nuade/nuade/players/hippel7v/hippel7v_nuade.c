// SPDX-License-Identifier: MIT
// Jochen Hippel 7 voice player as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/hippel7v.h>
#include <stdio.h>

static NuadeProbe hippel7v_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return h7v_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult hippel7v_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    H7vPlayer* player = h7v_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Jochen Hippel 7V module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = H7V_VOICES,
                                    .subsong_count = (uint32_t)h7v_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = H7V_TIMEBASE_HZ,
                                    // four of the seven tracks are mixed into one Paula voice: show the tracks
                                    .display_voices = H7V_TRACKS };
    return result;
}

static void hippel7v_close(void* user_data) {
    h7v_destroy((H7vPlayer*)user_data);
}

static bool hippel7v_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    return h7v_start((H7vPlayer*)user_data, (int)subsong, sample_rate, voices);
}

static NuadeTickResult hippel7v_tick(void* user_data, NuadeVoices* voices) {
    const H7vTickResult tick = h7v_tick((H7vPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = tick.length, .flags = flags };
}

static void hippel7v_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Jochen Hippel 7V");
}

// One display voice per track.
static uint32_t hippel7v_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const H7vPlayer* player = (const H7vPlayer*)user_data;
    const uint32_t n = cap < H7V_TRACKS ? cap : H7V_TRACKS;
    for (uint32_t i = 0; i < n; i++) {
        const H7vTrackState t = h7v_track_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = t.note, .instrument = t.instrument, .note_starts = t.note_starts };
    }
    return n;
}

// The tracks run their own patterns on one speed: one grid row is one sequencer step.
static uint32_t hippel7v_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return (uint32_t)h7v_speed((const H7vPlayer*)user_data);
}

const NuadePlayer nuade_player_hippel7v = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "hippel7v",
    .name = "Jochen Hippel 7V",
    .version = "1",
    .extensions = "hip7,s7g",
    .probe = hippel7v_probe,
    .open = hippel7v_open,
    .close = hippel7v_close,
    .start = hippel7v_start,
    .tick = hippel7v_tick,
    .metadata = hippel7v_metadata,
    .voice_info = hippel7v_voice_info,
    .grid_ticks = hippel7v_grid_ticks,
};

H7vPlayer* h7v_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_hippel7v) {
        return nullptr;
    }
    return (H7vPlayer*)nuade_player_data(song);
}

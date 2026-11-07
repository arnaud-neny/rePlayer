// SPDX-License-Identifier: MIT
// Scott Johnston's SoundPlayer as a nuade player: the songs of the ScottJohnston and of the SoundPlayer eagleplayer.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/scottjohnston.h>
#include <stdio.h>

static NuadeProbe scottjohnston_probe(const uint8_t* data, size_t size, const char* filename) {
    return sj_probe(data, size, filename) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult scottjohnston_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const SjCreateResult created = sj_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        switch (created.status) {
            case SjStatus_MissingInstruments:
                result.error.status = NuadeStatus_IoError;
                snprintf(result.error.message, sizeof(result.error.message),
                         "the instrument files of the song are missing (\"instruments\" directory or \"smp.\" bundle)");
                break;
            case SjStatus_OutOfMemory:
                result.error.status = NuadeStatus_OutOfMemory;
                snprintf(result.error.message, sizeof(result.error.message), "out of memory");
                break;
            default:
                result.error.status = NuadeStatus_InvalidData;
                snprintf(result.error.message, sizeof(result.error.message),
                         "not a Scott Johnston or SoundPlayer song");
                break;
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) {
        .voice_count = SJ_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = SJ_TIMEBASE_HZ
    };
    return result;
}

static void scottjohnston_close(void* user_data) {
    sj_destroy((SjPlayer*)user_data);
}

static bool scottjohnston_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && sj_start((SjPlayer*)user_data, voices);
}

static NuadeTickResult scottjohnston_tick(void* user_data, NuadeVoices* voices) {
    const SjTickResult tick = sj_tick((SjPlayer*)user_data, voices);
    NuadeTickFlags flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = tick.length, .flags = flags };
}

static void scottjohnston_metadata(void* user_data, const NuadeMetadataSink* sink) {
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, sj_format_name((const SjPlayer*)user_data));
}

static uint32_t scottjohnston_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const SjPlayer* player = (const SjPlayer*)user_data;
    const uint32_t n = cap < SJ_VOICES ? cap : SJ_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const SjVoiceState v = sj_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note(v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t scottjohnston_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return sj_ticks_per_row();
}

const NuadePlayer nuade_player_scottjohnston = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "scottjohnston",
    .name = "Scott Johnston",
    .version = "1",
    .extensions = SJS_EXTENSION "," SJ_EXTENSION, // rePlayer
    .probe = scottjohnston_probe,
    .open = scottjohnston_open,
    .close = scottjohnston_close,
    .start = scottjohnston_start,
    .tick = scottjohnston_tick,
    .metadata = scottjohnston_metadata,
    .voice_info = scottjohnston_voice_info,
    .grid_ticks = scottjohnston_grid_ticks,
};

SjPlayer* sj_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_scottjohnston) {
        return nullptr;
    }
    return (SjPlayer*)nuade_player_data(song);
}

// SPDX-License-Identifier: MIT
// Sound Images as a nuade player: the modules of the SoundImages eagleplayer whose driver the port models.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/soundimages.h>
#include <stdio.h>

static NuadeProbe soundimages_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return si_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult soundimages_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const SiCreateResult created = si_create(source->data, source->size);
    if (created.player == nullptr) {
        if (created.status == SiStatus_OutOfMemory) {
            result.error.status = NuadeStatus_OutOfMemory;
            snprintf(result.error.message, sizeof(result.error.message), "out of memory");
        } else {
            result.error.status = NuadeStatus_InvalidData;
            snprintf(result.error.message, sizeof(result.error.message),
                     "not a Sound Images module with a driver the player models");
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = SI_VOICES,
                                    .subsong_count = si_song_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = SI_TIMEBASE_HZ };
    return result;
}

static void soundimages_close(void* user_data) {
    si_destroy((SiPlayer*)user_data);
}

static bool soundimages_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return si_start((SiPlayer*)user_data, subsong, voices);
}

static NuadeTickResult soundimages_tick(void* user_data, NuadeVoices* voices) {
    const SiTickResult tick = si_tick((SiPlayer*)user_data, voices);
    NuadeTickFlags flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = tick.length, .flags = flags };
}

static void soundimages_metadata(void* user_data, const NuadeMetadataSink* sink) {
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, si_format_name((const SiPlayer*)user_data));
}

static uint32_t soundimages_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const SiPlayer* player = (const SiPlayer*)user_data;
    const uint32_t n = cap < SI_VOICES ? cap : SI_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const SiVoiceState v = si_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note(v.period), .instrument = v.instrument };
    }
    return n;
}

const NuadePlayer nuade_player_soundimages = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "soundimages",
    .name = "Sound Images",
    .version = "1",
    .extensions = SI_EXTENSION,
    .probe = soundimages_probe,
    .open = soundimages_open,
    .close = soundimages_close,
    .start = soundimages_start,
    .tick = soundimages_tick,
    .metadata = soundimages_metadata,
    .voice_info = soundimages_voice_info,
};

SiPlayer* si_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_soundimages) {
        return nullptr;
    }
    return (SiPlayer*)nuade_player_data(song);
}

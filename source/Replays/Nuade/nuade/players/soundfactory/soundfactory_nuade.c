// SPDX-License-Identifier: MIT
// Sound Factory as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/soundfactory.h>
#include <stdio.h>

#define SOUNDFACTORY_NUADE_VOICES 4

// The format has no magic number: a matching header is only "unsure".
static NuadeProbe soundfactory_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return sf_probe(data, size) ? NuadeProbe_Unsure : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult soundfactory_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    SfModule* module = sf_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Sound Factory module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) { .voice_count = SOUNDFACTORY_NUADE_VOICES,
                                    .subsong_count = (uint32_t)sf_subsong_count(module),
                                    .default_subsong = 0,
                                    .timebase_hz = SF_TICK_HZ };
    return result;
}

static void soundfactory_close(void* user_data) {
    sf_destroy((SfModule*)user_data);
}

static bool soundfactory_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return sf_start((SfModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult soundfactory_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    const SfTickEvents events = sf_tick((SfModule*)user_data);

    NuadeTickResult result = { .length = 1, .flags = NuadeTick_None };
    if (events & SfTick_Looped) {
        result.flags |= NuadeTick_Looped;
    }
    if (events & SfTick_Ended) {
        result.flags |= NuadeTick_Ended;
    }
    return result;
}

static void soundfactory_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Sound Factory");
}

// The format has no speed or tempo: every note carries its own length in ticks, and songs use steps of 6, 16 or 20
// ticks. A row of 4 ticks keeps two starts of one voice in separate rows for all of those.
#define SOUNDFACTORY_TICKS_PER_ROW 4

static uint32_t soundfactory_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const uint32_t count = cap < SOUNDFACTORY_NUADE_VOICES ? cap : SOUNDFACTORY_NUADE_VOICES;
    for (uint32_t i = 0; i < count; i++) {
        int note = -1;
        int instrument = -1;
        sf_voice_last_note((const SfModule*)user_data, (int)i, &note, &instrument);
        out[i] = (NuadeVoiceInfo) { .note = note, .instrument = instrument };
    }
    return count;
}

static uint32_t soundfactory_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return SOUNDFACTORY_TICKS_PER_ROW;
}

const NuadePlayer nuade_player_soundfactory = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "soundfactory",
    .name = "Sound Factory",
    .version = "1",
    .extensions = "psf",
    .probe = soundfactory_probe,
    .open = soundfactory_open,
    .close = soundfactory_close,
    .start = soundfactory_start,
    .tick = soundfactory_tick,
    .metadata = soundfactory_metadata,
    .voice_info = soundfactory_voice_info,
    .grid_ticks = soundfactory_grid_ticks,
};

SfModule* sf_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_soundfactory) {
        return nullptr;
    }
    return (SfModule*)nuade_player_data(song);
}

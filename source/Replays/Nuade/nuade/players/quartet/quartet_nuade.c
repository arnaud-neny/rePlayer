// SPDX-License-Identifier: MIT
// Quartet as a nuade player.

#include <nuade/player.h>

#include "nuade_tracker.h"
#include <nuade/players/quartet.h>
#include <stdio.h>

static NuadeProbe quartet_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return qt_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult quartet_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const QtCreateResult created = qt_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        switch (created.status) {
            case QtStatus_MissingSamples:
                result.error.status = NuadeStatus_IoError;
                snprintf(result.error.message, sizeof(result.error.message),
                         "no sample set (SMP.<name> or SMP.set) next to the song, or not a Quartet sample set");
                break;
            case QtStatus_OutOfMemory:
                result.error.status = NuadeStatus_OutOfMemory;
                snprintf(result.error.message, sizeof(result.error.message), "out of memory");
                break;
            default:
                result.error.status = NuadeStatus_InvalidData;
                snprintf(result.error.message, sizeof(result.error.message), "not a Quartet song");
                break;
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) {
        .voice_count = QT_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = QT_TIMEBASE_HZ
    };
    return result;
}

static void quartet_close(void* user_data) {
    qt_destroy((QtPlayer*)user_data);
}

static bool quartet_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)subsong;
    (void)sample_rate;
    return qt_start((QtPlayer*)user_data, voices);
}

static NuadeTickResult quartet_tick(void* user_data, NuadeVoices* voices) {
    const QtTickResult tick = qt_tick((QtPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    flags |= tick.looped ? NuadeTick_Looped : 0u;
    flags |= tick.ended ? NuadeTick_Ended : 0u;
    return (NuadeTickResult) { .length = QT_TICK_LENGTH, .flags = flags };
}

static void quartet_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Quartet");
}

static uint32_t quartet_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const QtPlayer* player = (const QtPlayer*)user_data;
    const uint32_t n = cap < QT_VOICES ? cap : QT_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const QtVoiceState v = qt_voice_state(player, i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t quartet_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return qt_tempo((const QtPlayer*)user_data);
}

const NuadePlayer nuade_player_quartet = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "quartet",
    .name = "Quartet",
    .version = "1",
    .extensions = "qpa",
    .probe = quartet_probe,
    .open = quartet_open,
    .close = quartet_close,
    .start = quartet_start,
    .tick = quartet_tick,
    .metadata = quartet_metadata,
    .voice_info = quartet_voice_info,
    .grid_ticks = quartet_grid_ticks,
};

QtPlayer* qt_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_quartet) {
        return nullptr;
    }
    return (QtPlayer*)nuade_player_data(song);
}

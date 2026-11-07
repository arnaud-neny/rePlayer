// SPDX-License-Identifier: MIT
// Sonix (SNX / SMUS / TINY) as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_path.h"
#include "nuade_utils.h"
#include "sonix.h"
#include "sonix_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SONIX_NUADE_VOICES 4

typedef struct SonixNuade {
    SonixSong* song;
} SonixNuade;

// Modland-style "smus.name" prefix or ".smus" suffix.
static bool has_sonix_name(const char* path) {
    return nuade_path_has_ext(path, "snx") || nuade_path_has_ext(path, "smus") || nuade_path_has_ext(path, "tiny");
}

// SMUS has an IFF signature; SNX and TINY are headerless and only recognised by parsing, which other formats can
// pass, so like UADE they need a Sonix file name.
static NuadeProbe sonix_probe(const uint8_t* data, size_t size, const char* filename) {
    if (data == nullptr || size == 0 || size > UINT32_MAX) {
        return NuadeProbe_Unsupported;
    }
    SonixSong* song = sonix_song_create(data, (uint32_t)size);
    const SonixSongMetadata* meta = song ? sonix_song_get_metadata(song) : nullptr;
    NuadeProbe result = NuadeProbe_Unsupported;
    if (meta != nullptr && meta->valid) {
        result = (meta->format == SONIX_FORMAT_SMUS || has_sonix_name(filename)) ? NuadeProbe_Supported
                                                                                 : NuadeProbe_Unsupported;
    }
    sonix_song_destroy(song);
    return result;
}

static NuadePlayerOpenResult sonix_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    if (source->size > UINT32_MAX) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "file too large");
        return result;
    }

    SonixSong* song = sonix_song_create(source->data, (uint32_t)source->size);
    if (song == nullptr) {
        result.error.status = NuadeStatus_OutOfMemory;
        return result;
    }
    const SonixSongMetadata* meta = sonix_song_get_metadata(song);
    if (meta == nullptr || !meta->valid) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "%s", sonix_song_get_error(song));
        sonix_song_destroy(song);
        return result;
    }

    SonixNuade* nuade = (SonixNuade*)calloc(1, sizeof(SonixNuade));
    if (nuade == nullptr) {
        sonix_song_destroy(song);
        result.error.status = NuadeStatus_OutOfMemory;
        return result;
    }
    nuade->song = song;

    if (source->path != nullptr) {
        sonix_song_load_instruments(song, source->io, source->path);
    }

    result.user_data = nuade;
    result.info = (NuadeSongInfo) {
        .voice_count = SONIX_NUADE_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = SONIX_CIA_PAL_HZ
    };
    return result;
}

static void sonix_close(void* user_data) {
    SonixNuade* nuade = (SonixNuade*)user_data;
    sonix_song_destroy(nuade->song);
    free(nuade);
}

static bool sonix_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    SonixNuade* nuade = (SonixNuade*)user_data;
    if (subsong != 0) {
        return false;
    }
    (void)sample_rate;
    sonix_song_start(nuade->song, voices);
    return true;
}

static NuadeTickResult sonix_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    SonixNuade* nuade = (SonixNuade*)user_data;
    bool looped = false;
    const uint32_t length = sonix_song_tick(nuade->song, &looped);
    return (NuadeTickResult) { .length = length, .flags = looped ? NuadeTick_Looped : NuadeTick_None };
}

// Sonix scores are event streams: no rows, a byte offset into each channel's track.
static NuadePositionResult sonix_position(void* user_data) {
    const SonixSong* song = ((const SonixNuade*)user_data)->song;
    return (NuadePositionResult) {
        .valid = true, .position = { .mode = NuadeScroll_Events, .tick = song->tick_count, .tempo = song->tempo_cur }
    };
}

static uint32_t sonix_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const SonixSong* song = ((const SonixNuade*)user_data)->song;
    const bool smus_stream = song->metadata.format != SONIX_FORMAT_SNX;
    uint32_t n = cap < SONIX_NUM_CHANNELS ? cap : SONIX_NUM_CHANNELS;
    for (uint32_t ch = 0; ch < n; ch++) {
        out[ch] = (NuadeVoiceInfo) { .note = song->shown_note[ch],
                                     .instrument = song->shown_inst[ch],
                                     .track_pos = smus_stream ? song->smus_stream_pos[ch] : song->snx_track_pos[ch] };
    }
    return n;
}

static uint32_t sonix_events(void* user_data, uint32_t voice, uint32_t track_pos, uint32_t before, uint32_t after,
                             NuadeEvent* out, uint32_t cap) {
    const SonixSong* song = ((const SonixNuade*)user_data)->song;
    if (voice >= SONIX_NUM_CHANNELS) {
        return 0;
    }
    // Every entry is one 16-bit word, so the entries before a position are found by stepping back.
    const uint32_t start = song->snx_track_start[voice];
    uint32_t pos = track_pos;
    uint32_t back = 0;
    while (back < before && pos >= start + 2) {
        pos -= 2;
        back++;
    }
    uint32_t n = 0;
    for (uint32_t i = 0; i < back + after && n < cap; i++, pos += 2) {
        if (sonix_song_describe_event(song, (int)voice, pos, &out[n])) {
            n++;
        }
    }
    return n;
}

static void sonix_metadata(void* user_data, const NuadeMetadataSink* sink) {
    SonixNuade* nuade = (SonixNuade*)user_data;
    const SonixSongMetadata* meta = sonix_song_get_metadata(nuade->song);
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, sonix_format_name(meta->format));
    for (uint32_t i = 0; i < 64; i++) {
        const char* name = sonix_song_get_instrument_name(nuade->song, (uint8_t)i);
        if (name != nullptr && name[0] != '\0') {
            sink->add_instrument(sink->ctx, i, name);
        }
    }
}

static bool sonix_dump_state(void* user_data, const char* path) {
    return sonix_set_dump_file(((SonixNuade*)user_data)->song, path);
}

extern const NuadePlayer nuade_player_sonix;

SonixSong* sonix_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_sonix) {
        return nullptr;
    }
    return ((SonixNuade*)nuade_player_data(song))->song;
}

const NuadePlayer nuade_player_sonix = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "sonix",
    .name = "Sonix Music Driver",
    .version = "1",
    .extensions = "snx,smus,tiny",
    .probe = sonix_probe,
    .open = sonix_open,
    .close = sonix_close,
    .start = sonix_start,
    .tick = sonix_tick,
    .metadata = sonix_metadata,
    .position = sonix_position,
    .voice_info = sonix_voice_info,
    .events = sonix_events,
    .dump_state = sonix_dump_state,
};

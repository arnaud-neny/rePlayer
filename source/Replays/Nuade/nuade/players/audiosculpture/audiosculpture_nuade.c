// SPDX-License-Identifier: MIT
// Audio Sculpture as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_tracker.h"
#include "nuade_utils.h"
#include <nuade/players/audiosculpture.h>
#include <stdio.h>

#define PATH_MAX_BYTES 1024
#define NAME_MAX_BYTES 256

// The module alone does not tell an Audio Sculpture song from a plain FLT4 one: the bank file next to it does. The
// extension (or the "adsc." prefix) is the claim of the format itself.
static NuadeProbe audiosculpture_probe(const uint8_t* data, size_t size, const char* filename) {
    if (!asc_probe(data, size)) {
        return NuadeProbe_Unsupported;
    }
    const bool named
        = filename != nullptr && (nuade_path_has_ext(filename, "adsc") || nuade_path_has_ext(filename, "mod_adsc4"));
    return named ? NuadeProbe_Supported : NuadeProbe_Unsure;
}

// The eagleplayer loads "<song file name>.AS", and ".NT" when there is none, from the song's directory, in any case.
static NuadeIoReadResult read_bank(const NuadeSongSource* source) {
    NuadeIoReadResult none = { 0 };
    if (source->path == nullptr) {
        return none;
    }
    char dir[PATH_MAX_BYTES] = { 0 };
    if (!nuade_path_parent(source->path, dir, sizeof(dir))) {
        dir[0] = '\0';
    }
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(source->io, dirs, 1);
    NuadeIoReadResult bank = none;
    const char* extensions[] = { ".as", ".nt" };
    for (size_t i = 0; i < nuade_sizeof_array(extensions) && bank.data == nullptr && index != nullptr; i++) {
        char name[NAME_MAX_BYTES];
        if (snprintf(name, sizeof(name), "%s%s", nuade_path_basename(source->path), extensions[i])
            >= (int)sizeof(name)) {
            continue;
        }
        const char* url = nuade_file_index_find(index, name);
        if (url != nullptr) {
            bank = NuadeIo_read_url_to_memory(source->io, url);
        }
    }
    nuade_file_index_destroy(index);
    return bank;
}

static NuadePlayerOpenResult audiosculpture_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };

    const NuadeIoReadResult bank = read_bank(source);
    if (bank.data == nullptr) {
        result.error.status = NuadeStatus_IoError;
        snprintf(result.error.message, sizeof(result.error.message),
                 "Audio Sculpture needs the .AS file next to the song");
        return result;
    }

    AscPlayer* player = asc_create(source->data, source->size, bank.data, (size_t)bank.size);
    NuadeIo_free_url_to_memory(source->io, bank.data);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Audio Sculpture module");
        return result;
    }

    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = ASC_VOICES,
                                    .subsong_count = asc_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = ASC_TIMEBASE_HZ };
    return result;
}

static void audiosculpture_close(void* user_data) {
    asc_destroy((AscPlayer*)user_data);
}

static bool audiosculpture_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return asc_start((AscPlayer*)user_data, subsong, voices);
}

static NuadeTickResult audiosculpture_tick(void* user_data, NuadeVoices* voices) {
    const AscTickResult tick = asc_tick((AscPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = ASC_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void audiosculpture_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const AscPlayer* player = (const AscPlayer*)user_data;
    char name[ASC_NAME_SIZE];

    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Audio Sculpture");
    asc_title(player, name);
    if (name[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_TITLE, name);
    }
    for (int i = 0; i < ASC_INSTRUMENTS; i++) {
        asc_sample_name(player, i, name);
        if (name[0] != '\0') {
            sink->add_instrument(sink->ctx, (uint32_t)(i + 1), name);
        }
    }
}

static NuadePositionResult audiosculpture_position(void* user_data) {
    const AscPosition p = asc_position((const AscPlayer*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position
                                   = { .mode = NuadeScroll_Rows, .order = p.order, .row = p.row, .speed = p.speed } };
}

static uint32_t audiosculpture_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const AscPlayer* player = (const AscPlayer*)user_data;
    const AscPosition p = asc_position(player);
    const uint32_t n = cap < ASC_VOICES ? cap : ASC_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const AscVoiceState v = asc_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note(v.period),
                                    .instrument = v.instrument != 0 ? v.instrument - 1 : -1,
                                    .order = p.order,
                                    .row = p.row };
    }
    return n;
}

static NuadeTrackerInfoResult audiosculpture_tracker_info(void* user_data, const NuadePosition* at) {
    if (at->order >= asc_position_count((const AscPlayer*)user_data)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = ASC_VOICES,
                                                .rows = ASC_PATTERN_ROWS,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3 },
                                                             { "inst", NuadeColumn_Hex, 2 },
                                                             { "fx", NuadeColumn_Hex, 1 },
                                                             { "arg", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell audiosculpture_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const AscCell c = asc_pattern_cell((const AscPlayer*)ctx, order, channel, row);
    cell.value[0] = nuade_period_to_note(c.period);
    if (c.instrument != 0) {
        cell.value[1] = c.instrument;
    }
    if (c.effect != 0 || c.effect_arg != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.effect_arg;
    }
    return cell;
}

static uint32_t audiosculpture_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                             int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    if (at->order >= asc_position_count((const AscPlayer*)user_data)) {
        return 0;
    }
    return nuade_tracker_fill(at, ASC_VOICES, ASC_PATTERN_ROWS, channel, row_lo, row_hi, out, cap, audiosculpture_cell,
                              user_data);
}

const NuadePlayer nuade_player_audiosculpture = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "audiosculpture",
    .name = "Audio Sculpture",
    .version = "1",
    .extensions = "adsc,mod_adsc4",
    .probe = audiosculpture_probe,
    .open = audiosculpture_open,
    .close = audiosculpture_close,
    .start = audiosculpture_start,
    .tick = audiosculpture_tick,
    .metadata = audiosculpture_metadata,
    .position = audiosculpture_position,
    .voice_info = audiosculpture_voice_info,
    .tracker_info = audiosculpture_tracker_info,
    .tracker_cells = audiosculpture_tracker_cells,
};

AscPlayer* asc_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_audiosculpture) {
        return nullptr;
    }
    return (AscPlayer*)nuade_player_data(song);
}

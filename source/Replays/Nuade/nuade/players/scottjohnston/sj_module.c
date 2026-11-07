// SPDX-License-Identifier: MIT
// Scott Johnston player: the modules of the two eagleplayers, and the song probe.

#include "nuade_path.h"
#include "sj_internal.h"
#include <ctype.h>
#include <string.h>

// The ScottJohnston tune: the tune's own header sits behind the file names.
#define SJ_TUNE_OFFSET 0x2C

// The tune header: the timer's low byte, its high byte, the voice mask.
#define TUNE_TIMER_LOW 0
#define TUNE_TIMER_HIGH 1
#define TUNE_VOICE_MASK 2
#define TIMER_LOW_UNSET 0xFF // the ScottJohnston eagleplayer leaves the timer alone for this
#define MIN_TIMER 0x0400     // a ScottJohnston tick shorter than this is no song

// The SoundPlayer eagleplayer's module check (Check2) looks at the tune's first row: the timer's high byte is a
// plausible one, the voice mask is 7 or 15, and the first row has only commands: no note and no instrument in any
// voice, the same non-zero command in voices 0..2 and, for mask 15, in voice 3.
#define SP_MIN_TIMER_HIGH 0x0B
#define SP_MAX_TIMER_HIGH 0xA0
#define SP_MASK_THREE_VOICES 7
#define SP_MASK_FOUR_VOICES 15

static const SjVariant sj_variants[] = {
    [SjFormat_ScottJohnston] = {
        .format = SjFormat_ScottJohnston,
        .name = "Scott Johnston",
        .tune_offset = SJ_TUNE_OFFSET,
        .init_runs_tick = true,
        .init_dma_on = true,
        .arms_all_voices = false,
        .stop_hits_own_voice = false,
        .restart_ends_song = false,
    },
    [SjFormat_SoundPlayer] = {
        .format = SjFormat_SoundPlayer,
        .name = "SoundPlayer",
        .tune_offset = 0,
        .init_runs_tick = false,
        .init_dma_on = false,
        .arms_all_voices = true,
        .stop_hits_own_voice = true,
        .restart_ends_song = true,
    },
};

const SjVariant* sj_variant(SjFormat format) {
    return &sj_variants[format];
}

// Does the file name start with "sjs."? The SoundPlayer eagleplayer finds the sample bundle by that prefix only.

static bool has_soundplayer_prefix(const char* path) {
    const char* base = nuade_path_basename(path);
    const size_t len = strlen(SJS_EXTENSION);
    for (size_t i = 0; i < len; i++) {
        if (tolower((unsigned char)base[i]) != SJS_EXTENSION[i]) {
            return false;
        }
    }
    return base[len] == '.';
}

bool sj_format_from_path(const char* path, SjFormat* format) {
    if (path == nullptr) {
        return false;
    }
    if (has_soundplayer_prefix(path)) {
        *format = SjFormat_SoundPlayer;
        return true;
    }
    if (nuade_path_has_ext(path, SJ_EXTENSION)) {
        *format = SjFormat_ScottJohnston;
        return true;
    }
    return false;
}

// The ScottJohnston header: 16 file names. The first list entry has no predecessor to repeat; every other name must be
// in the name table.

static bool parse_file_names(SjModule* module) {
    for (uint32_t i = 0; i < SJ_FILE_SLOTS; i++) {
        module->file_index[i] = module->data[SJ_FILE_INDEX_OFFSET + i];
        const bool named = module->file_index[i] != 0;
        if ((i == 0 && !named) || (named && sj_instrument_name(module->file_index[i]) == nullptr)) {
            return false;
        }
    }
    return true;
}

// The SoundPlayer eagleplayer's Check2 (the module must be parsed): the timer, the mask and the first row.

static bool soundplayer_check(const SjModule* module) {
    const uint8_t timer_high = (uint8_t)(module->timer >> 8);
    if (timer_high < SP_MIN_TIMER_HIGH || timer_high > SP_MAX_TIMER_HIGH) {
        return false;
    }
    const uint8_t mask = module->data[module->variant->tune_offset + TUNE_VOICE_MASK];
    if (mask != SP_MASK_THREE_VOICES && mask != SP_MASK_FOUR_VOICES) {
        return false;
    }
    SjCell cells[SJ_VOICES];
    for (uint32_t voice = 0; voice < SJ_VOICES; voice++) {
        if (!sj_module_cell(module, 0, voice, &cells[voice]) || cells[voice].note != 0
            || cells[voice].instrument != 0) {
            return false;
        }
    }
    // The commands of the first row are one and the same; voice 3 is compared only for mask 15.
    const uint8_t command = cells[0].command;
    const uint32_t compared = mask == SP_MASK_FOUR_VOICES ? SJ_VOICES : SJ_VOICES - 1;
    for (uint32_t voice = 0; voice < compared; voice++) {
        if (cells[voice].command != command) {
            return false;
        }
    }
    return command != 0;
}

/////

bool sj_module_parse(SjModule* module, SjFormat format, const uint8_t* data, size_t size) {
    const SjVariant* variant = sj_variant(format);
    if (size > SJ_MAX_MODULE_BYTES || size < variant->tune_offset + SJ_TUNE_HEADER_BYTES + SJ_ROW_BYTES) {
        return false;
    }
    const uint8_t* tune = data + variant->tune_offset;
    module->variant = variant;
    module->data = data;
    module->size = size;
    module->timer = (uint16_t)(tune[TUNE_TIMER_HIGH] << 8 | tune[TUNE_TIMER_LOW]);
    module->voice_mask = tune[TUNE_VOICE_MASK];
    if (variant->arms_all_voices) {
        // The init arms all four voices on top of the tune's mask.
        module->voice_mask = SJ_ALL_VOICES_MASK;
    }
    if (format == SjFormat_SoundPlayer) {
        return soundplayer_check(module);
    }
    return tune[TUNE_TIMER_LOW] != TIMER_LOW_UNSET && module->timer >= MIN_TIMER && parse_file_names(module);
}

bool sj_module_voice_is_armed(const SjModule* module, uint32_t voice) {
    return (module->voice_mask >> voice & 1u) != 0;
}

bool sj_module_cell(const SjModule* module, int32_t row, uint32_t voice, SjCell* cell) {
    const int64_t cell_offset = (int64_t)module->variant->tune_offset + SJ_TUNE_HEADER_BYTES
                                + (int64_t)row * SJ_ROW_BYTES + (int64_t)voice * SJ_CELL_BYTES;
    if (cell_offset < 0 || (uint64_t)cell_offset > module->size
        || module->size - (uint64_t)cell_offset < SJ_CELL_BYTES) {
        return false;
    }
    const uint8_t* p = module->data + cell_offset;
    *cell = (SjCell) { .note = p[0], .instrument = p[1], .command = p[2] };
    return true;
}

bool sj_probe(const uint8_t* data, size_t size, const char* filename) {
    SjFormat format;
    SjModule module;
    return sj_format_from_path(filename, &format) && sj_module_parse(&module, format, data, size)
           && sj_tune_is_closed(&module);
}

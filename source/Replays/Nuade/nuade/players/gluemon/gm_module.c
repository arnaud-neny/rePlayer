// SPDX-License-Identifier: MIT
// GlueMon: the eagleplayer's Check2, the typed readers of the module records and the bounds check that keeps the
// replayer inside the tables it models.

#include "gm_internal.h"
#include "nuade_base.h"
#include "nuade_utils.h"
#include <string.h>

#define GM_MARK 0x474C5545u            // "GLUE"
#define GM_MARK_COMPLEMENT 0xB8B3AABAu // the same four letters, inverted
#define GM_HEADER_BYTES 8u             // the marks; the replayer's base register points behind them
#define GM_ROW_BYTES 4u
// The drum marker "DRM" sits where the envelope of the fourth voice would be: its last two bytes are 'R' and 'M'.
#define GM_PERCUSSION_DECREMENT 'R'
#define GM_PERCUSSION_THRESHOLD 'M'
#define GM_ENVELOPE_BYTES 3u
#define GM_TIMING_BYTES 2u // speed and rows
#define GM_ORDER_SIZE_BYTES 1u

// What the replayer sets for the fourth voice of a percussion song (the commands set the rest).
static const GmEnvelope k_percussion_envelope = { .reload = 0x30, .decrement = 1, .threshold = 0 };

// The period table of the eagleplayer, as indexed by note bytes (the low bit dropped).
static const uint16_t k_periods[GM_PERIOD_COUNT] = {
    0x0D60, 0x0CA0, 0x0BE8, 0x0B40, 0x0A98, 0x0A00, 0x0970, 0x08E8, 0x0868, 0x07F0, 0x0780, 0x0710, 0x06B0,
    0x0650, 0x05F4, 0x05A0, 0x054C, 0x0500, 0x04B8, 0x0474, 0x0434, 0x03F8, 0x03C0, 0x0388, 0x0358, 0x0328,
    0x02FA, 0x02D0, 0x02A6, 0x0280, 0x025C, 0x023A, 0x021A, 0x01FC, 0x01E0, 0x01C4, 0x01AC, 0x0194, 0x017D,
    0x0168, 0x0153, 0x0140, 0x012E, 0x011D, 0x010D, 0x00FE, 0x00F0, 0x00E2, 0x00D6,
};

uint32_t gm_note_index(uint8_t note) {
    return note >> 1;
}

uint16_t gm_period(uint8_t note) {
    const uint32_t index = gm_note_index(note);
    return index < GM_PERIOD_COUNT ? k_periods[index] : 0;
}

bool gm_is_command(uint8_t entry) {
    return entry >= GM_COMMAND_FIRST && entry <= GM_COMMAND_LAST;
}

const int8_t* gm_wave(const GmModule* module, uint32_t voice) {
    return (const int8_t*)(module->bytes + module->waves + (size_t)voice * GM_WAVE_BYTES);
}

uint8_t gm_order(const GmModule* module, uint32_t index) {
    const size_t at = module->orders + index;
    return at < module->size ? module->bytes[at] : GM_ORDER_END;
}

static size_t pattern_offset(const GmModule* module, uint8_t pattern) {
    return module->patterns + (size_t)pattern * module->rows * GM_ROW_BYTES;
}

GmRow gm_row(const GmModule* module, uint8_t pattern, uint32_t row) {
    GmRow result = { { GM_NOTE_NONE, GM_NOTE_NONE, GM_NOTE_NONE, GM_NOTE_NONE } };
    const size_t at = pattern_offset(module, pattern) + (size_t)row * GM_ROW_BYTES;
    if (row < module->rows && at + GM_ROW_BYTES <= module->size) {
        memcpy(result.entry, module->bytes + at, GM_ROW_BYTES);
    }
    return result;
}

const int8_t* gm_pattern_wave(const GmModule* module, uint8_t pattern) {
    return (const int8_t*)(module->bytes + pattern_offset(module, pattern));
}

// Every entry of the patterns the position table names: a note stays inside the period table. On the percussion voice
// only 0xFF and the commands are skipped; any other byte, 0xFE too, plays 32 bytes of its pattern, which must lie in
// the file.

static bool patterns_usable(const GmModule* module) {
    for (uint32_t index = 0; index < module->order_count; index++) {
        const uint8_t pattern = gm_order(module, index);
        for (uint32_t row = 0; row < module->rows; row++) {
            const GmRow cells = gm_row(module, pattern, row);
            for (uint32_t voice = 0; voice < GM_VOICES; voice++) {
                const uint8_t entry = cells.entry[voice];
                if (module->percussion && voice == GM_PERCUSSION_VOICE) {
                    if (entry != GM_NOTE_NONE && !gm_is_command(entry)
                        && pattern_offset(module, pattern) + GM_WAVE_BYTES > module->size) {
                        return false;
                    }
                } else if (entry != GM_NOTE_OFF && entry != GM_NOTE_NONE && gm_note_index(entry) >= GM_PERIOD_COUNT) {
                    return false;
                }
            }
        }
    }
    return true;
}

// The header: name, the envelopes of the voices, speed, rows, four waveforms, the length of the position table.

static bool read_header(GmModule* module, const uint8_t* bytes, size_t size) {
    const size_t fixed = GM_HEADER_BYTES + GM_NAME_LENGTH + GM_VOICES * GM_ENVELOPE_BYTES + GM_TIMING_BYTES
                         + GM_VOICES * GM_WAVE_BYTES + GM_ORDER_SIZE_BYTES;
    if (size < fixed) {
        return false;
    }
    const uint8_t* p = bytes + GM_HEADER_BYTES;
    memcpy(module->name, p, GM_NAME_LENGTH);
    p += GM_NAME_LENGTH;
    for (uint32_t voice = 0; voice < GM_VOICES; voice++) {
        module->envelope[voice] = (GmEnvelope) { .reload = p[0], .decrement = p[1], .threshold = p[2] };
        p += GM_ENVELOPE_BYTES;
    }
    const GmEnvelope* last = &module->envelope[GM_PERCUSSION_VOICE];
    module->percussion = last->decrement == GM_PERCUSSION_DECREMENT && last->threshold == GM_PERCUSSION_THRESHOLD;
    if (module->percussion) {
        module->envelope[GM_PERCUSSION_VOICE] = k_percussion_envelope;
    }
    module->speed = p[0];
    module->rows = p[1];
    p += GM_TIMING_BYTES;
    module->waves = (size_t)(p - bytes);
    p += GM_VOICES * GM_WAVE_BYTES;
    module->order_bytes = *p;
    p += GM_ORDER_SIZE_BYTES;
    module->orders = (size_t)(p - bytes);
    return true;
}

// The position table: the replayer starts at entry order_bytes - 2 and steps first, so entry order_bytes - 1 must end
// the table (it is where the song starts), and the table before the first end mark names patterns of the pointer table.
// Returns the highest pattern number, or -1.

static int check_orders(GmModule* module) {
    const uint8_t order_bytes = module->order_bytes;
    if (order_bytes < 2 || order_bytes > GM_MAX_ORDER_BYTES || module->orders + order_bytes > module->size
        || gm_order(module, order_bytes - 1u) != GM_ORDER_END) {
        return -1;
    }
    int highest = -1;
    uint32_t count = 0;
    while (gm_order(module, count) != GM_ORDER_END) {
        const uint8_t pattern = gm_order(module, count);
        if (pattern >= GM_MAX_PATTERNS) {
            return -1;
        }
        highest = pattern > highest ? pattern : highest;
        count++;
    }
    module->order_count = count;
    module->patterns = module->orders + order_bytes;
    return highest;
}

bool gm_module_parse(GmModule* module, const uint8_t* bytes, size_t size) {
    if (size < GM_HEADER_BYTES || nuade_read_be32(bytes) != GM_MARK
        || nuade_read_be32(bytes + 4) != GM_MARK_COMPLEMENT) {
        return false;
    }
    *module = (GmModule) { .bytes = bytes, .size = size };
    if (!read_header(module, bytes, size) || module->rows == 0) {
        return false;
    }
    const int highest = check_orders(module);
    if (highest < 0 || pattern_offset(module, (uint8_t)(highest + 1)) > size) {
        return false;
    }
    return patterns_usable(module);
}

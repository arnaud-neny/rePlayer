// SPDX-License-Identifier: MIT
// Tronic: locating the data blocks of a module and the typed readers for its records.

#include "tronic_internal.h"

#include "nuade_base.h"
#include "nuade_utils.h"

// The eagleplayer's check adds a block size to the low word of an address only; a module this size cannot overflow it
// whatever address it was loaded at.
#define MAX_MODULE_SIZE 0x10000u

// Block sizes and the instrument data size are added with adda.w: 16 bits, signed.
#define MAX_BLOCK_SIZE 0x7FFFu

#define HEADER_BYTES 16
#define INSTRUMENT_BYTES 0x58
#define FIRST_INSTRUMENT_OFFSET 0x0058
#define SECOND_INSTRUMENT_OFFSET 0x00B0

static const uint16_t k_periods[TRONIC_PERIOD_COUNT] = {
    0,    6848, 6464, 6096, 5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3616, 3424, 3232, 3048, 2880, 2712,
    2560, 2416, 2280, 2152, 2032, 1920, 1808, 1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960,
    904,  856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  452,  428,  404,  381,  360,  339,
    320,  302,  285,  269,  254,  240,  226,  214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,
    113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  0,    113,
};

uint16_t tronic_period(unsigned index) {
    return index < TRONIC_PERIOD_COUNT ? k_periods[index] : 0;
}

uint8_t tronic_byte(TronicModule* module, uint64_t at) {
    if (at >= module->size) {
        module->overrun = true;
        return 0;
    }
    return module->data[at];
}

uint16_t tronic_word(TronicModule* module, uint64_t at) {
    return (uint16_t)((tronic_byte(module, at) << 8) | tronic_byte(module, at + 1));
}

uint32_t tronic_long(TronicModule* module, uint64_t at) {
    return ((uint32_t)tronic_word(module, at) << 16) | tronic_word(module, at + 2);
}

const int8_t* tronic_silence(const TronicModule* module) {
    return (const int8_t*)(module->data + module->size);
}

// A block of the module: a word with the size, then the data.
typedef struct Block {
    uint32_t at;
    uint32_t size;
} Block;

// Reads the block at *cursor and moves the cursor past it. False if it is not inside the file.
static bool read_block(const uint8_t* data, size_t size, uint32_t* cursor, Block* block) {
    if ((uint64_t)*cursor + 2 > size) {
        return false;
    }
    block->size = nuade_read_be16(data + *cursor);
    block->at = *cursor + 2;
    if (block->size > MAX_BLOCK_SIZE || (uint64_t)block->at + block->size > size) {
        return false;
    }
    *cursor = block->at + block->size;
    return true;
}

// The four sequences follow the header, each `length` bytes.
static bool parse_sequences(TronicModule* module, const uint8_t* data, size_t size, uint32_t* cursor) {
    *cursor = HEADER_BYTES;
    for (int i = 0; i < TRONIC_VOICES; i++) {
        const uint16_t restart = nuade_read_be16(data + 4 * i);
        const uint16_t length = nuade_read_be16(data + 4 * i + 2);
        if (length < 2 || length > MAX_BLOCK_SIZE || (length & 1) != 0 || (restart & 1) != 0 || restart >= length
            || (uint64_t)*cursor + length > size) {
            return false;
        }
        module->sequence[i] = (TronicSequence) { .at = *cursor, .restart = restart, .length = length };
        *cursor += length;
    }
    return true;
}

// Wave tables, the instrument table with its records, the waveform index and the waveforms.
static bool parse_tables(TronicModule* module, const uint8_t* data, size_t size, uint32_t* cursor) {
    Block block;
    if (!read_block(data, size, cursor, &block) || (block.size & 1) != 0) {
        return false;
    }
    module->wave_tables = block.at;

    // The eagleplayer's check wants the instrument table to start with the offsets 0x58 and 0xB0.
    if (!read_block(data, size, cursor, &block) || (block.size & 1) != 0 || block.size < 4) {
        return false;
    }
    module->instrument_table = block.at;
    module->instrument_count = block.size / 2;
    if (nuade_read_be16(data + block.at) != FIRST_INSTRUMENT_OFFSET
        || nuade_read_be16(data + block.at + 2) != SECOND_INSTRUMENT_OFFSET) {
        return false;
    }
    const uint32_t last_offset = nuade_read_be16(data + block.at + block.size - 2);
    const uint32_t records_size = last_offset + INSTRUMENT_BYTES;
    if (records_size > MAX_BLOCK_SIZE || (uint64_t)*cursor + records_size > size) {
        return false;
    }
    module->instruments = *cursor;
    *cursor += records_size;

    if (!read_block(data, size, cursor, &block)) {
        return false;
    }
    module->waveform_index = block.at;
    if (!read_block(data, size, cursor, &block)) {
        return false;
    }
    module->waveforms = block.at;
    return true;
}

// The patterns (a long with the size), then the sample table and the samples.
static bool parse_patterns_and_samples(TronicModule* module, const uint8_t* data, size_t size, uint32_t* cursor) {
    if ((uint64_t)*cursor + 4 > size) {
        return false;
    }
    const uint32_t pattern_bytes = nuade_read_be32(data + *cursor);
    if ((uint64_t)*cursor + 4 + pattern_bytes > size) {
        return false;
    }
    module->patterns = *cursor + 4;
    module->pattern_bytes = pattern_bytes;
    *cursor += 4 + pattern_bytes;

    module->sample_table = *cursor;
    module->samples = *cursor + TRONIC_SAMPLE_COUNT * 4;
    return module->samples <= size;
}

bool tronic_module_parse(TronicModule* module, const uint8_t* data, size_t size) {
    *module = (TronicModule) { 0 };
    if (data == nullptr || size < HEADER_BYTES || size > MAX_MODULE_SIZE) {
        return false;
    }
    uint32_t cursor = 0;
    if (!parse_sequences(module, data, size, &cursor) || !parse_tables(module, data, size, &cursor)
        || !parse_patterns_and_samples(module, data, size, &cursor)) {
        return false;
    }
    module->data = nuade_paula_padded_copy(data, size);
    if (module->data == nullptr) {
        return false;
    }
    module->size = size;
    return true;
}

void tronic_module_free(TronicModule* module) {
    free(module->data);
    module->data = nullptr;
}

TronicStep tronic_step(TronicModule* module, int voice, unsigned position) {
    const uint32_t at = module->sequence[voice].at + position;
    return (TronicStep) { .pattern = module->patterns + (uint32_t)tronic_byte(module, at) * TRONIC_PATTERN_BYTES,
                          .transpose = tronic_byte(module, (uint64_t)at + 1) };
}

TronicCell tronic_pattern_row(TronicModule* module, uint32_t pattern, unsigned row) {
    const uint64_t at = (uint64_t)pattern + row * TRONIC_ROW_BYTES;
    return (TronicCell) { .note = tronic_byte(module, at),
                          .instrument = tronic_byte(module, at + 1),
                          .effect = tronic_byte(module, at + 2),
                          .effect_arg = tronic_byte(module, at + 3) };
}

// Field offsets of the fixed part of an instrument record.
enum {
    Instrument_LengthWords = 0x00,
    Instrument_LoopOffset = 0x02,
    Instrument_LoopWords = 0x04,
    Instrument_Detune = 0x24,
    Instrument_SampleFlag = 0x26,
    Instrument_Param = 0x27,
};

bool tronic_instrument(TronicModule* module, unsigned number, TronicInstrument* out) {
    if (number == 0 || number > module->instrument_count) {
        return false;
    }
    const uint32_t at = module->instruments + tronic_word(module, module->instrument_table + 2 * (number - 1));
    if ((uint64_t)at + INSTRUMENT_BYTES > module->size) {
        return false;
    }
    *out = (TronicInstrument) { .at = at,
                                .length_words = tronic_word(module, at + Instrument_LengthWords),
                                .loop_offset = tronic_word(module, at + Instrument_LoopOffset),
                                .loop_words = tronic_word(module, at + Instrument_LoopWords),
                                .detune = tronic_word(module, at + Instrument_Detune),
                                .is_sample = (tronic_byte(module, at + Instrument_SampleFlag) & 0x80) != 0,
                                .param = tronic_byte(module, at + Instrument_Param) };
    return true;
}

uint8_t tronic_instrument_byte(TronicModule* module, const TronicInstrument* instrument, uint32_t field) {
    return tronic_byte(module, (uint64_t)instrument->at + field);
}

TronicEnvelopeStage tronic_envelope_stage(TronicModule* module, const TronicInstrument* instrument, uint16_t pos) {
    const uint32_t at = TronicField_Envelope + pos;
    return (TronicEnvelopeStage) { .step = tronic_instrument_byte(module, instrument, at),
                                   .target = tronic_instrument_byte(module, instrument, at + 1),
                                   .hold = tronic_instrument_byte(module, instrument, at + 2) };
}

TronicVibratoStage tronic_vibrato_stage(TronicModule* module, const TronicInstrument* instrument, uint8_t stage) {
    const uint32_t at = TronicField_Vibrato + stage;
    return (TronicVibratoStage) { .step = tronic_instrument_byte(module, instrument, at),
                                  .ticks = tronic_instrument_byte(module, instrument, at + 1),
                                  .hold = tronic_instrument_byte(module, instrument, at + 2) };
}

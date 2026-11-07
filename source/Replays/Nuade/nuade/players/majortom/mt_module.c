// SPDX-License-Identifier: MIT
// Major Tom: the module reader. Every read goes through mt_read_byte, which knows what Amiga memory holds around the
// song.

#include "mt_internal.h"
#include "nuade_utils.h"

// Unused memory reads as zeros from the end of the song to the CIAs, and everywhere above the 16 MB the chips decode.
#define MT_ZERO_MEMORY_END 0xa00000u
#define MT_ADDRESS_SPACE_END 0x1000000u

#define MT_CELL_BYTES 4
#define MT_TABLE_COUNT 13

// The most a song can be and still sit in the 2 MB of chip memory behind the load address.
#define MT_MAX_SIZE 0x100000u

// The magic, then a word of version 0x100, then the offsets of the tables (the first is the song length).
#define MT_MAGIC 0x4152502eu
#define MT_FIRST_TABLE_OFFSET 6u

static const uint32_t mt_table_offsets[MT_TABLE_COUNT] = {
    MtTable_SongLength,  MtTable_Restart,  MtTable_Speed, MtTable_Order, MtTable_Transpose,
    MtTable_Instruments, MtTable_Volume,   MtTable_Wave,  MtTable_Note,  MtTable_Period,
    MtTable_Waveforms,   MtTable_Patterns, MtTable_End,
};

// The eagleplayer checks the magic only; its InitSound adds the offset table to the song address, so the port accepts
// the one table the songs use.

bool mt_module_check(const uint8_t* data, size_t size) {
    if (size < MtTable_Patterns || size > MT_MAX_SIZE || nuade_read_be32(data) != MT_MAGIC) {
        return false;
    }
    for (uint32_t i = 0; i < MT_TABLE_COUNT; i++) {
        if (nuade_read_be32(data + MT_FIRST_TABLE_OFFSET + i * 4) != mt_table_offsets[i]) {
            return false;
        }
    }
    return true;
}

uint8_t mt_read_byte(const MtModule* module, int64_t offset, bool* unmapped) {
    const uint32_t address = (uint32_t)(MT_SONG_BASE + offset);
    if (address >= MT_SONG_BASE && address - MT_SONG_BASE < module->size) {
        return module->bytes[address - MT_SONG_BASE];
    }
    if ((address >= MT_SONG_BASE && address < MT_ZERO_MEMORY_END) || address >= MT_ADDRESS_SPACE_END) {
        return 0;
    }
    if (unmapped != nullptr) {
        *unmapped = true;
    }
    return 0;
}

uint32_t mt_read_long(const MtModule* module, int64_t offset, bool* unmapped) {
    uint32_t value = 0;
    for (int i = 0; i < 4; i++) {
        value = (value << 8) | mt_read_byte(module, offset + i, unmapped);
    }
    return value;
}

uint8_t mt_song_length(const MtModule* module) {
    return mt_read_byte(module, MtTable_SongLength, nullptr);
}

uint8_t mt_restart(const MtModule* module) {
    return mt_read_byte(module, MtTable_Restart, nullptr);
}

uint16_t mt_speed(const MtModule* module) {
    return nuade_read_be16(module->bytes + MtTable_Speed);
}

// The speed effect writes the word into the song.

void mt_set_speed(MtModule* module, uint16_t speed) {
    module->bytes[MtTable_Speed] = (uint8_t)(speed >> 8);
    module->bytes[MtTable_Speed + 1] = (uint8_t)speed;
}

uint8_t mt_order_pattern(const MtModule* module, uint8_t position) {
    return mt_read_byte(module, MtTable_Order + position, nullptr);
}

uint8_t mt_transpose(const MtModule* module, uint8_t position) {
    return mt_read_byte(module, MtTable_Transpose + position, nullptr);
}

MtInstrument mt_instrument(const MtModule* module, uint8_t number) {
    MtInstrument instrument;
    for (int i = 0; i < MT_SEQUENCES; i++) {
        instrument.block[i] = mt_read_byte(module, MtTable_Instruments + number * MT_SEQUENCES + i, nullptr);
    }
    return instrument;
}

MtBlock mt_block(MtSequence sequence, uint8_t number) {
    static const uint32_t tables[MT_SEQUENCES] = { MtTable_Volume, MtTable_Wave, MtTable_Note, MtTable_Period };
    return (MtBlock) { .zeros = false, .offset = (int64_t)tables[sequence] + (int64_t)number * MT_BLOCK_BYTES };
}

uint8_t mt_block_step(const MtModule* module, MtBlock block, int64_t index, bool* unmapped) {
    return block.zeros ? 0 : mt_read_byte(module, block.offset + index, unmapped);
}

uint32_t mt_waveform_offset(uint8_t number) {
    return MtTable_Waveforms + (uint32_t)number * MT_WAVE_BYTES;
}

MtCell mt_row(const MtModule* module, uint8_t pattern, uint32_t row, int voice, bool* unmapped) {
    const int16_t pattern_offset = (int16_t)(uint16_t)((uint32_t)pattern * MT_PATTERN_BYTES);
    const int64_t cell
        = (int64_t)MtTable_Patterns + pattern_offset + (int64_t)row * MT_ROW_BYTES + voice * MT_CELL_BYTES;
    return (MtCell) { .note = mt_read_byte(module, cell, unmapped),
                      .instrument = mt_read_byte(module, cell + 1, unmapped),
                      .effect = mt_read_byte(module, cell + 2, unmapped),
                      .param = mt_read_byte(module, cell + 3, unmapped) };
}

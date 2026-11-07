// SPDX-License-Identifier: MIT
// MMDC module, translated from the UADE eagleplayer "MMDC" (Check2, InitPlayer).
//
// The file is an OctaMED MMD0 module: a 0x34-byte header, the song structure, an array of block offsets, an array of
// sample record offsets (all file offsets that InitPlayer relocates), then the blocks and the samples. A block is its
// track count and its line count - 1, then every cell of every line as 3 bytes, coded as one run-length stream (see
// MmdcLineReader).

#include "mmdc_module.h"

#include "nuade_utils.h"

#define ID_MMDC 0x4D4D4443u
#define HEADER_SIZE 0x34
#define HEADER_LENGTH 0x04
#define HEADER_SONG 0x08
#define HEADER_BLOCK_ARRAY 0x10
#define HEADER_BLOCK_ARRAY_LOW 0x12
#define HEADER_SAMPLE_ARRAY 0x18

#define MAX_TEMPO_TABLE_INDEX 10 // tempos up to this one index the timer table
#define MISC_EFFECT 0xF
#define SAMPLE_HEADER_SIZE 6 // the length long and the type word
#define BLOCK_HEADER_SIZE 2

// The song structure
#define SONG_SIZE 0x314
#define SONG_INSTRUMENT_SIZE 8
#define SONG_BLOCK_COUNT 0x1F8
#define SONG_LENGTH 0x1FA
#define SONG_PLAY_SEQUENCE 0x1FC
#define SONG_TEMPO 0x2FC
#define SONG_PLAY_TRANSPOSE 0x2FE
#define SONG_FLAGS 0x2FF
#define SONG_SLIDE_LIMIT 0x301
#define SONG_TRACK_VOLUMES 0x302
#define SONG_MASTER_VOLUME 0x312
#define FLAG_ST_SLIDES 0x20

uint8_t mmdc_u8(const MmdcImage* image, uint64_t offset) {
    return offset < image->size ? image->data[offset] : 0;
}

uint16_t mmdc_u16(const MmdcImage* image, uint64_t offset) {
    return offset <= image->size && image->size - offset >= 2 ? nuade_read_be16(&image->data[offset]) : 0;
}

uint32_t mmdc_u32(const MmdcImage* image, uint64_t offset) {
    return offset <= image->size && image->size - offset >= 4 ? nuade_read_be32(&image->data[offset]) : 0;
}

MmdcCell mmdc_read_cell(const uint8_t* bytes) {
    const uint8_t first = bytes[0];
    const uint8_t second = bytes[1];
    return (MmdcCell) { .note = first & 0x3F,
                        .instrument
                        = (uint8_t)((second >> 4) | ((first & 0x80) ? 0x10 : 0) | ((first & 0x40) ? 0x20 : 0)),
                        .effect = second & 0x0F,
                        .param = bytes[2] };
}

void mmdc_line_reader_start(MmdcLineReader* reader, const MmdcImage* image, const MmdcBlock* block) {
    reader->image = image;
    reader->pos = block->offset + BLOCK_HEADER_SIZE;
    reader->run_left = -1;
    reader->literal = false;
}

void mmdc_line_reader_next(MmdcLineReader* reader, uint8_t tracks, uint8_t line[MMDC_MAX_TRACKS * MMDC_CELL_SIZE]) {
    for (uint32_t i = 0; i < (uint32_t)tracks * MMDC_CELL_SIZE; i++) {
        if (reader->run_left < 0) {
            const uint8_t header = mmdc_u8(reader->image, reader->pos++);
            reader->literal = header < 0x80;
            reader->run_left = reader->literal ? header : (uint8_t)~header;
        }
        reader->run_left--;
        line[i] = reader->literal ? mmdc_u8(reader->image, reader->pos++) : 0;
    }
}

MmdcCell mmdc_block_cell(const MmdcImage* image, const MmdcBlock* block, uint32_t line, uint32_t track) {
    if (line >= block->lines || track >= block->tracks) {
        return (MmdcCell) { 0 };
    }
    MmdcLineReader reader;
    mmdc_line_reader_start(&reader, image, block);
    uint8_t cells[MMDC_MAX_TRACKS * MMDC_CELL_SIZE];
    for (uint32_t i = 0; i <= line; i++) {
        mmdc_line_reader_next(&reader, block->tracks, cells);
    }
    return mmdc_read_cell(&cells[track * MMDC_CELL_SIZE]);
}

// Check2: the id, a zero word at 0x10 and a block array at a positive even offset below 0x8000 whose first entry has a
// zero high word.

static bool check_header(const MmdcImage* image) {
    if (image->size < HEADER_SIZE || mmdc_u32(image, 0) != ID_MMDC || mmdc_u16(image, HEADER_BLOCK_ARRAY) != 0) {
        return false;
    }
    const uint16_t array = mmdc_u16(image, HEADER_BLOCK_ARRAY_LOW);
    return array != 0 && (array & 0x8001) == 0 && mmdc_u16(image, array) == 0;
}

// The song structure. A tempo word whose low byte is above 10 gives the timer by division; otherwise it indexes the
// timer table, which only a word below 256 does inside the table.

static bool read_song(const MmdcImage* image, uint32_t offset, MmdcSong* song) {
    if (offset > image->size || image->size - offset < SONG_SIZE) {
        return false;
    }
    for (uint32_t i = 0; i < MMDC_INSTRUMENTS; i++) {
        const uint64_t at = (uint64_t)offset + (uint64_t)i * SONG_INSTRUMENT_SIZE;
        song->instruments[i] = (MmdcInstrument) { .repeat_start = mmdc_u16(image, at),
                                                  .repeat_words = mmdc_u16(image, at + 2),
                                                  .volume = mmdc_u8(image, at + 6),
                                                  .transpose = (int8_t)mmdc_u8(image, at + 7) };
    }
    song->block_count = mmdc_u16(image, (uint64_t)offset + SONG_BLOCK_COUNT);
    song->length = mmdc_u16(image, (uint64_t)offset + SONG_LENGTH);
    for (uint32_t i = 0; i < MMDC_MAX_POSITIONS; i++) {
        song->play_sequence[i] = mmdc_u8(image, (uint64_t)offset + SONG_PLAY_SEQUENCE + i);
    }
    song->tempo = mmdc_u16(image, (uint64_t)offset + SONG_TEMPO);
    song->play_transpose = (int8_t)mmdc_u8(image, (uint64_t)offset + SONG_PLAY_TRANSPOSE);
    song->st_slides = (mmdc_u8(image, (uint64_t)offset + SONG_FLAGS) & FLAG_ST_SLIDES) != 0;
    song->slide_limit = mmdc_u8(image, (uint64_t)offset + SONG_SLIDE_LIMIT);
    for (uint32_t i = 0; i < MMDC_TRACK_VOLUMES; i++) {
        song->track_volumes[i] = mmdc_u8(image, (uint64_t)offset + SONG_TRACK_VOLUMES + i);
    }
    song->master_volume = mmdc_u8(image, (uint64_t)offset + SONG_MASTER_VOLUME);

    const uint8_t tempo_low = (uint8_t)song->tempo;
    const bool tempo_ok = tempo_low > MAX_TEMPO_TABLE_INDEX || (song->tempo >> 8 == 0 && tempo_low >= 1);
    return song->block_count >= 1 && song->block_count <= MMDC_MAX_BLOCKS && song->length >= 1
           && song->length <= MMDC_MAX_POSITIONS && song->play_sequence[0] < song->block_count && tempo_ok;
}

// Where find_arrays found the arrays InitPlayer relocates.
typedef struct Arrays {
    uint32_t blocks;
    uint32_t block_slots;
    uint32_t samples;
    uint32_t sample_slots;
} Arrays;

// The block offsets (their count is the distance to the sample array, as a word) and the sample offsets (up to the
// first block). A voice without an instrument yet uses sample slot 0, so it must exist.

static bool find_arrays(const MmdcImage* image, Arrays* arrays) {
    arrays->blocks = mmdc_u32(image, HEADER_BLOCK_ARRAY);
    arrays->samples = mmdc_u32(image, HEADER_SAMPLE_ARRAY);
    if (arrays->samples <= arrays->blocks || arrays->samples - arrays->blocks > 0xFFFF
        || (arrays->samples - arrays->blocks) % 4 != 0 || arrays->samples > image->size
        || image->size - arrays->samples < 4) {
        return false;
    }
    arrays->block_slots = (arrays->samples - arrays->blocks) / 4;
    const uint32_t first_block = mmdc_u32(image, arrays->blocks);
    if (first_block < arrays->samples || (first_block - arrays->samples) % 4 != 0) {
        return false;
    }
    arrays->sample_slots = (first_block - arrays->samples) / 4;
    return arrays->sample_slots >= 1;
}

// A sample record sits on an even offset with its data inside the module (the data itself may be cut short).

static bool read_samples(const MmdcImage* image, const Arrays* arrays, MmdcModule* module) {
    module->sample_count = arrays->sample_slots < MMDC_INSTRUMENTS ? arrays->sample_slots : MMDC_INSTRUMENTS;
    for (uint32_t slot = 0; slot < module->sample_count; slot++) {
        const uint32_t record = mmdc_u32(image, (uint64_t)arrays->samples + 4u * slot);
        if (record == 0) {
            continue;
        }
        if ((record & 1) != 0 || record > image->size || image->size - record < SAMPLE_HEADER_SIZE) {
            return false;
        }
        module->samples[slot]
            = (MmdcSample) { .present = true, .data = record + SAMPLE_HEADER_SIZE, .length = mmdc_u32(image, record) };
    }
    return true;
}

// A block has 1..4 tracks, decodes inside the module and names only instruments that have a sample slot. A tempo
// command that selects the timer table cannot follow a start tempo of 256 or more (the restart would index past the
// table).

static bool block_is_playable(const MmdcImage* image, const MmdcModule* module, const MmdcBlock* block) {
    if (block->offset > image->size || image->size - block->offset < BLOCK_HEADER_SIZE || block->tracks < 1
        || block->tracks > MMDC_MAX_TRACKS) {
        return false;
    }
    MmdcLineReader reader;
    mmdc_line_reader_start(&reader, image, block);
    for (uint32_t line = 0; line < block->lines; line++) {
        uint8_t cells[MMDC_MAX_TRACKS * MMDC_CELL_SIZE];
        mmdc_line_reader_next(&reader, block->tracks, cells);
        if (reader.pos > image->size) {
            return false;
        }
        for (uint32_t track = 0; track < block->tracks; track++) {
            const MmdcCell cell = mmdc_read_cell(&cells[track * MMDC_CELL_SIZE]);
            const bool slow_tempo
                = cell.effect == MISC_EFFECT && cell.param >= 1 && cell.param <= MAX_TEMPO_TABLE_INDEX;
            if (cell.instrument > module->sample_count || (slow_tempo && module->song.tempo > 0xFF)) {
                return false;
            }
        }
    }
    return true;
}

bool mmdc_parse_module(const MmdcImage* image, MmdcModule* module) {
    *module = (MmdcModule) { 0 };
    Arrays arrays;
    if (!check_header(image) || mmdc_u32(image, HEADER_LENGTH) > image->size || !find_arrays(image, &arrays)
        || !read_song(image, mmdc_u32(image, HEADER_SONG), &module->song)
        || module->song.block_count > arrays.block_slots || !read_samples(image, &arrays, module)) {
        return false;
    }
    for (uint32_t i = 0; i < module->song.block_count; i++) {
        const uint32_t offset = mmdc_u32(image, (uint64_t)arrays.blocks + 4u * i);
        module->blocks[i] = (MmdcBlock) { .offset = offset,
                                          .tracks = mmdc_u8(image, offset),
                                          .lines = (uint16_t)(mmdc_u8(image, (uint64_t)offset + 1) + 1) };
        if (!block_is_playable(image, module, &module->blocks[i])) {
            return false;
        }
    }
    return true;
}

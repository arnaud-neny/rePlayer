// SPDX-License-Identifier: MIT
// MusicMaker V8 player: the instruments. The replayer keeps them in a block of its own. An INST chunk or ".i" file is
// used as it is; a PINS chunk or ".ip" file stores every sample as a delta coded bit stream, which the replayer unpacks
// into that block when it opens the song.
//
// Layout of the block: [the "SEI1XX" header with the record count (optional: 26 records without it)] [records of 8
// bytes: length, first length, loop offset, loop length] [a long, the silent word] [the samples one after the other]
// [padding to an even offset] [15 words: sizes of the tables] [the tables].

#include "mm_internal.h"
#include <stdlib.h>
#include <string.h>

#define HEADER_ID 0x53454931u // "SEI1"
#define HEADER_XX 0x5858u
#define HEADER_SIZE 8
#define HEADER_COUNT_OFFSET 6
#define DEFAULT_COUNT 26
#define RECORD_SIZE 8
#define SILENT_SIZE 4
#define MAX_BLOCK_SIZE (64u << 20)
#define MAX_STEP_BITS 15                  // the step table has 2^bits entries; the replayer reads its index from 8 bits
#define NO_INSTRUMENTS_MARKER 0xFFFFFFFFu // a file that starts with two of these has no instruments

typedef struct Layout {
    uint32_t header; // 8 with the "SEI1XX" header, else 0
    uint32_t count;  // records
} Layout;

static Layout read_layout(const MmBlock* b) {
    Layout layout = { .header = 0, .count = DEFAULT_COUNT };
    if (mm_u32(b, 0) == HEADER_ID && mm_u16(b, 4) == HEADER_XX) {
        layout.header = HEADER_SIZE;
        layout.count = mm_u16(b, HEADER_COUNT_OFFSET);
    }
    return layout;
}

// Reads the bit stream of one sample: bits come most significant first, each output byte is a running sum of the step
// table entry its bits index (the index is the last 8 bits read). A stream that ends exactly on a byte boundary is
// followed by one byte of padding.

typedef struct BitReader {
    const MmBlock* src;
    uint32_t pos;
    uint32_t current;
    int bits_left;
} BitReader;

static uint32_t read_bit(BitReader* r) {
    if (--r->bits_left < 0) {
        r->current = mm_u8(r->src, r->pos);
        r->pos++;
        r->bits_left = 7;
    }
    r->current <<= 1;
    return (r->current >> 8) & 1;
}

static void unpack_sample(BitReader* r, uint32_t steps, uint32_t bits, uint8_t* out, uint32_t length) {
    uint8_t sum = 0;
    r->bits_left = 0;
    for (uint32_t i = 0; i < length; i++) {
        uint8_t index = 0;
        for (uint32_t b = 0; b < bits; b++) {
            index = (uint8_t)(index << 1 | read_bit(r));
        }
        sum = (uint8_t)(sum + mm_u8(r->src, steps + index));
        out[i] = sum;
    }
    if (r->bits_left <= 0) {
        r->pos++;
    }
}

// Builds the block of a packed instrument chunk: the records, the unpacked samples, then the tables as they are.
// Returns the block's bytes, 0 if the chunk is not a valid one.

static uint8_t* unpack_block(const MmBlock* src, uint32_t* block_size) {
    const Layout layout = read_layout(src);
    if (layout.count == 0 || layout.count > MM_INSTRUMENTS) {
        return nullptr;
    }
    const uint32_t table = layout.header;
    const uint32_t records_end = table + layout.count * RECORD_SIZE;
    uint32_t total = 0;
    for (uint32_t i = 0; i < layout.count; i++) {
        total += mm_u16(src, table + i * RECORD_SIZE);
    }
    const uint32_t bits = mm_u16(src, records_end);
    if (bits == 0 || bits > MAX_STEP_BITS) {
        return nullptr;
    }
    const uint32_t steps_at = records_end + 2;
    const uint32_t stream_at = steps_at + (1u << bits);
    if (stream_at > src->size) {
        return nullptr;
    }
    const uint32_t samples_at = records_end + SILENT_SIZE;
    uint8_t* samples = calloc(samples_at + total + 1, 1);
    if (samples == nullptr) {
        return nullptr;
    }
    memcpy(samples, src->data, records_end < src->size ? records_end : src->size);
    BitReader reader = { .src = src, .pos = stream_at };
    uint32_t at = samples_at;
    for (uint32_t i = 0; i < layout.count; i++) {
        const uint32_t length = mm_u16(src, table + i * RECORD_SIZE);
        if (length != 0) {
            unpack_sample(&reader, steps_at, bits, samples + at, length);
            at += length;
        }
    }
    at = (at + 1) & ~1u;
    uint32_t tables_total = 0;
    for (uint32_t i = 0; i < MM_PITCH_TABLES; i++) {
        tables_total += mm_u16(src, reader.pos + i * 2u);
    }
    const uint32_t capacity = at + MM_TABLE_SIZES_BYTES + tables_total;
    uint8_t* out = capacity <= MAX_BLOCK_SIZE ? realloc(samples, capacity) : nullptr;
    if (out == nullptr) {
        free(samples);
        return nullptr;
    }
    for (uint32_t i = 0; i < MM_TABLE_SIZES_BYTES + tables_total; i++) {
        out[at + i] = mm_u8(src, reader.pos + i);
    }
    *block_size = capacity;
    return out;
}

// Locates the records, samples and tables in the block.

static bool locate(MmInstruments* in) {
    const MmBlock* b = &in->block;
    const Layout layout = read_layout(b);
    if (layout.count == 0 || layout.count > MM_INSTRUMENTS) {
        return false;
    }
    const uint32_t records_end = layout.header + layout.count * RECORD_SIZE;
    in->silent = records_end;
    uint32_t at = records_end + SILENT_SIZE;
    for (uint32_t i = 0; i < layout.count; i++) {
        const uint32_t record = layout.header + i * RECORD_SIZE;
        in->record[i] = (MmSampleRecord) { .length = mm_u16(b, record),
                                           .first_length = mm_u16(b, record + 2),
                                           .loop_offset = mm_u16(b, record + 4),
                                           .loop_length = mm_u16(b, record + 6) };
        in->sample[i] = at;
        at += in->record[i].length;
    }
    at = (at + 1) & ~1u;
    uint32_t data = at + MM_TABLE_SIZES_BYTES;
    for (uint32_t i = 0; i < MM_PITCH_TABLES; i++) {
        const uint32_t size = mm_u16(b, at + i * 2u);
        in->pitch_table[i] = size != 0 ? data : 0;
        data += size;
    }
    return true;
}

// The 8 voice replayer halves every sample (arithmetic shift) so that two voices can be added.

static void halve_samples(MmInstruments* in) {
    for (uint32_t i = 0; i < MM_INSTRUMENTS; i++) {
        const uint32_t start = in->sample[i];
        for (uint32_t j = 0; j < in->record[i].length && start + j < in->block.size; j++) {
            in->storage[start + j] = (uint8_t)((int8_t)in->storage[start + j] >> 1);
        }
    }
}

bool mm_instruments_open(MmInstruments* in, const MmBlock* data, bool packed, bool halve) {
    *in = (MmInstruments) { 0 };
    if (data->size >= 2 * sizeof(uint32_t) && mm_u32(data, 0) == NO_INSTRUMENTS_MARKER
        && mm_u32(data, 4) == NO_INSTRUMENTS_MARKER) {
        return false;
    }
    // The block gets the Paula tail: a 4 voice sample can run past it with any AUDxLEN
    uint32_t size = data->size;
    if (packed) {
        uint8_t* unpacked = unpack_block(data, &size);
        in->storage = unpacked != nullptr ? nuade_paula_padded_copy(unpacked, size) : nullptr;
        free(unpacked);
    } else {
        in->storage = nuade_paula_padded_copy(data->data, size);
    }
    if (in->storage == nullptr) {
        return false;
    }
    in->block = (MmBlock) { .data = in->storage, .size = size };
    if (!locate(in)) {
        mm_instruments_close(in);
        return false;
    }
    if (halve) {
        halve_samples(in);
    }
    return true;
}

void mm_instruments_close(MmInstruments* in) {
    free(in->storage);
    *in = (MmInstruments) { 0 };
}

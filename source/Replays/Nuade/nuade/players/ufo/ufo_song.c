// SPDX-License-Identifier: MIT
// UFO player: the song file. The eagleplayer's check (FORM, DDAT, BODY, CHAN) and its chunk walk.
//
// Chunk sizes are not uniform, as the walk in the eagleplayer has them:
//   CHAN  tag, size, 1 unused byte, channel number, delay + events; the next chunk follows 8 + size after the tag
//   DRUM  tag + 0x54 bytes (not read)
//   BPM   tag, 1 unused byte, the tempo
//   BLCK  tag, size (counting its own four bytes), delay + events; the next chunk follows 4 + size after the tag
//   INST  tag, size (counting its own four bytes), 1 unused byte, instrument number, length, loop offset, 1 unused
//         byte, volume, sample bytes; the next chunk follows 4 + size after the tag
// Any other tag (END in practice) ends the walk.

#include "ufo_internal.h"

#include <string.h>

#define CHAN_HEADER_SIZE 10
#define DRUM_SIZE 0x58
#define BPM_SIZE 6
#define BLCK_HEADER_SIZE 8
#define INST_HEADER_SIZE 0x14
#define BPM_SCALE_NUMERATOR 24
#define BPM_SCALE_DENOMINATOR 60

typedef enum UfoChunkKind {
    UfoChunk_Other,
    UfoChunk_Chan,
    UfoChunk_Drum,
    UfoChunk_Bpm,
    UfoChunk_Blck,
    UfoChunk_Inst,
} UfoChunkKind;

typedef struct UfoChunkTag {
    const char* tag;
    UfoChunkKind kind;
} UfoChunkTag;

static const UfoChunkTag chunk_tags[] = {
    { "CHAN", UfoChunk_Chan }, { "DRUM", UfoChunk_Drum }, { "BPM ", UfoChunk_Bpm },
    { "BLCK", UfoChunk_Blck }, { "INST", UfoChunk_Inst },
};

static UfoChunkKind chunk_kind(const uint8_t* tag) {
    for (size_t i = 0; i < nuade_sizeof_array(chunk_tags); i++) {
        if (memcmp(tag, chunk_tags[i].tag, 4) == 0) {
            return chunk_tags[i].kind;
        }
    }
    return UfoChunk_Other;
}

uint8_t ufo_song_u8(const UfoSong* song, uint32_t offset, bool* fault) {
    if (offset >= song->size) {
        *fault = true;
        return 0;
    }
    return song->data[offset];
}

// u605e
uint16_t ufo_song_read_delay(const UfoSong* song, uint32_t* cursor, bool* fault, uint64_t* work) {
    uint16_t delay = 0;
    uint8_t byte;
    do {
        byte = ufo_song_u8(song, *cursor, fault);
        (*cursor)++;
        (*work)++;
        delay = (uint16_t)((delay << 7) + (byte & 0x7F));
    } while ((byte & 0x80) != 0 && !*fault);
    return delay;
}

static bool has_tags(const uint8_t* data, size_t size) {
    if (size < UFO_FIRST_CHUNK + 4) {
        return false;
    }
    // FORM <size> DDAT BODY <size> CHAN, the eagleplayer's Check2
    return memcmp(data, "FORM", 4) == 0 && memcmp(data + 8, "DDAT", 4) == 0 && memcmp(data + 12, "BODY", 4) == 0
           && memcmp(data + UFO_FIRST_CHUNK, "CHAN", 4) == 0;
}

// Offsets and sizes are 32 bit values of the file; a chunk end past 32 bits is not a chunk.

static bool next_chunk(uint32_t chunk, uint64_t distance, uint32_t* next) {
    const uint64_t offset = (uint64_t)chunk + distance;
    if (offset > UINT32_MAX) {
        return false;
    }
    *next = (uint32_t)offset;
    return true;
}

typedef struct ChanChunk {
    uint8_t channel;
    uint32_t stream; // first delay
    uint32_t next;
} ChanChunk;

static bool read_chan_chunk(const UfoSong* song, uint32_t chunk, ChanChunk* out) {
    if (song->size - chunk < CHAN_HEADER_SIZE) {
        return false;
    }
    out->channel = song->data[chunk + 9];
    out->stream = chunk + CHAN_HEADER_SIZE;
    return next_chunk(chunk, 8 + (uint64_t)nuade_read_be32(song->data + chunk + 4), &out->next);
}

typedef struct BpmChunk {
    uint8_t tempo;
    uint32_t next;
} BpmChunk;

static bool read_bpm_chunk(const UfoSong* song, uint32_t chunk, BpmChunk* out) {
    if (song->size - chunk < BPM_SIZE) {
        return false;
    }
    out->tempo = song->data[chunk + 5];
    return next_chunk(chunk, BPM_SIZE, &out->next);
}

typedef struct BlckChunk {
    uint32_t stream; // first delay
    uint32_t next;
} BlckChunk;

static bool read_blck_chunk(const UfoSong* song, uint32_t chunk, BlckChunk* out) {
    if (song->size - chunk < BLCK_HEADER_SIZE) {
        return false;
    }
    out->stream = chunk + BLCK_HEADER_SIZE;
    return next_chunk(chunk, 4 + (uint64_t)nuade_read_be32(song->data + chunk + 4), &out->next);
}

typedef struct InstChunk {
    uint8_t number;
    uint32_t length;
    uint32_t loop_offset;
    uint8_t volume;
    uint32_t sample_offset;
    uint32_t next;
} InstChunk;

static bool read_inst_chunk(const UfoSong* song, uint32_t chunk, InstChunk* out) {
    if (song->size - chunk < INST_HEADER_SIZE) {
        return false;
    }
    const uint8_t* p = song->data + chunk;
    out->number = p[9];
    out->length = nuade_read_be32(p + 10);
    out->loop_offset = nuade_read_be32(p + 14);
    out->volume = p[19];
    out->sample_offset = chunk + INST_HEADER_SIZE;
    return next_chunk(chunk, 4 + (uint64_t)nuade_read_be32(p + 4), &out->next);
}

// Can a sample record be played exactly as the replayer sets it up? False when it would run past its buffer or start
// Paula with a length of 0 words (65536 words of whatever follows).
static bool sample_fits(uint32_t buffer_size, uint32_t sample_offset, uint32_t length, uint32_t loop_offset) {
    if ((uint64_t)sample_offset + length > buffer_size) {
        return false;
    }
    // AUDxLEN of the first block, in words, and of the repeat; a length of 0 would play 65536 words
    if (((length >> 1) & 0xFFFF) == 0) {
        return false;
    }
    if (loop_offset != 0 && (loop_offset >= length || (((length - loop_offset) >> 1) & 0xFFFF) == 0)) {
        return false;
    }
    return true;
}

bool ufo_instrument_make(UfoInstrument* out, const uint8_t* buffer, uint32_t buffer_size, uint32_t sample_offset,
                         uint32_t length, uint32_t loop_offset, uint8_t volume) {
    if (!sample_fits(buffer_size, sample_offset, length, loop_offset)) {
        return false;
    }
    const int8_t* start = (const int8_t*)(buffer + sample_offset);
    *out = (UfoInstrument) {
        .start = start,
        .loop_start = start + loop_offset,
        .end = start + length,
        .loops = loop_offset != 0,
        .volume = volume,
    };
    return true;
}

// One chunk at `chunk`; false when the song is not one the replayer reads inside the file, `done` when the walk ends.

static bool walk_chunk(UfoSong* song, uint32_t chunk, uint32_t* next, bool* done) {
    switch (chunk_kind(song->data + chunk)) {
        case UfoChunk_Chan: {
            ChanChunk c;
            if (!read_chan_chunk(song, chunk, &c) || c.channel >= UFO_CHANNELS) {
                return false;
            }
            bool fault = false;
            uint64_t work = 0; // the parse reads each chunk once
            uint32_t cursor = c.stream;
            song->channel_delay[c.channel] = ufo_song_read_delay(song, &cursor, &fault, &work);
            song->channel_start[c.channel] = cursor;
            *next = c.next;
            return !fault;
        }
        case UfoChunk_Drum:
            return next_chunk(chunk, DRUM_SIZE, next);
        case UfoChunk_Bpm: {
            BpmChunk c;
            if (!read_bpm_chunk(song, chunk, &c)) {
                return false;
            }
            song->bpm = (uint8_t)((uint32_t)c.tempo * BPM_SCALE_NUMERATOR / BPM_SCALE_DENOMINATOR);
            *next = c.next;
            return true;
        }
        case UfoChunk_Blck: {
            BlckChunk c;
            if (song->block_count >= UFO_MAX_BLOCKS || !read_blck_chunk(song, chunk, &c)) {
                return false;
            }
            song->blocks[song->block_count++] = c.stream;
            *next = c.next;
            return true;
        }
        case UfoChunk_Inst: {
            InstChunk c;
            if (!read_inst_chunk(song, chunk, &c) || c.number >= UFO_INSTRUMENTS
                || !sample_fits(song->size, c.sample_offset, c.length, c.loop_offset)) {
                return false;
            }
            song->song_instruments[c.number] = (UfoSongInstrument) { .present = true,
                                                                     .sample_offset = c.sample_offset,
                                                                     .length = c.length,
                                                                     .loop_offset = c.loop_offset,
                                                                     .volume = c.volume };
            *next = c.next;
            return true;
        }
        case UfoChunk_Other:
            break;
    }
    *done = true;
    return true;
}

bool ufo_song_parse(UfoSong* song, const uint8_t* data, size_t size) {
    memset(song, 0, sizeof(*song));
    if (size >= INT32_MAX || !has_tags(data, size) || (uint64_t)8 + nuade_read_be32(data + 4) > size) {
        return false;
    }
    song->data = data;
    song->size = (uint32_t)size;
    uint32_t chunk = UFO_FIRST_CHUNK;
    bool done = false;
    while (!done) {
        if (song->size - 4 < chunk) { // the tag of the next chunk is not in the file
            return false;
        }
        uint32_t next = chunk;
        if (!walk_chunk(song, chunk, &next, &done)) {
            return false;
        }
        chunk = next;
    }
    return true;
}

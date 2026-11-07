// SPDX-License-Identifier: MIT
// NTSP-system player, translated from the UADE eagleplayer "NTSP-system player module V1.1" (Wanted Team).
//
// NtspModule: the magic "SPNT", the number of samples (bytes) the stream unpacks to, then the packed stream: five bytes
// unpack to nine samples. The replayer unpacks the stream into two chip buffers of NTSP_BUFFER_BYTES and plays the same
// buffer on all four channels (channels 0 and 1 at full volume, 2 and 3 at volume 1). The audio interrupt of channel 0
// fires at every buffer start: it unpacks the next part of the stream into the buffer that is not playing and points
// AUDxLC at it. When the stream is used up the rest of the buffer is zeroed, SongEnd is called and the stream starts
// again.

#include <nuade/players/ntsp.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define MAGIC 0x53504E54u // "SPNT"
#define HEADER_SIZE 8

// Packed bytes and samples of one block
#define BLOCK_PACKED 5
#define BLOCK_SAMPLES 9

#define BUFFER_BLOCKS (NTSP_BUFFER_BYTES / BLOCK_SAMPLES)

#define PERIOD 0xA1
#define VOLUME_LOUD 64
#define VOLUME_QUIET 1

// The module header and the checks the replayer makes on it

// The init rejects a module whose sample count is below the packed size it computes from it (5 / 9 of it plus 8, in
// 32-bit arithmetic).
static bool init_accepts(uint32_t samples) {
    const uint32_t packed = (uint32_t)(samples * BLOCK_PACKED) / BLOCK_SAMPLES + 8;
    return (int32_t)(samples - packed) >= 0;
}

// The packed bytes the replayer reads over a pass of the stream: whole blocks of the full buffers and of the last
// partial one.
static uint64_t stream_bytes_read(uint32_t samples) {
    const uint64_t blocks
        = (uint64_t)(samples / NTSP_BUFFER_BYTES) * BUFFER_BLOCKS + (samples % NTSP_BUFFER_BYTES) / BLOCK_SAMPLES;
    return blocks * BLOCK_PACKED;
}

// A sample count below one buffer (L < NTSP_BUFFER_BYTES) makes the stream counter wrap, so the replayer would unpack
// from memory past the module; so would a stream that is shorter than its sample count. Both are refused.
static bool module_read(const uint8_t* data, size_t size, NtspModule* out) {
    if (size < HEADER_SIZE || nuade_read_be32(data) != MAGIC) {
        return false;
    }
    const uint32_t samples = nuade_read_be32(data + 4);
    if (samples < NTSP_BUFFER_BYTES || !init_accepts(samples)) {
        return false;
    }
    if (stream_bytes_read(samples) > size - HEADER_SIZE) {
        return false;
    }
    *out = (NtspModule) { .stream = data + HEADER_SIZE, .samples = samples };
    return true;
}

// Unpacking

// A signed value of `bits` bits, as the replayer's lsl/asr pair extends it
static int sign_extend(uint32_t value, unsigned bits) {
    const uint32_t sign = 1u << (bits - 1);
    return (int)((value & (2 * sign - 1)) ^ sign) - (int)sign;
}

static int8_t scaled(int value, unsigned shift) {
    return (int8_t)(uint8_t)(value * (1 << shift));
}

// Five bytes to nine samples. The first byte holds a 5-bit sample and the shift (top three bits) of all of them; the
// other four hold two 4-bit samples each, high nibble first.
static void unpack_block(const uint8_t packed[BLOCK_PACKED], int8_t out[BLOCK_SAMPLES]) {
    const unsigned shift = packed[0] >> 5;
    out[0] = scaled(sign_extend(packed[0], 5), shift > 1 ? shift - 1 : 0);
    for (uint32_t i = 1; i < BLOCK_PACKED; i++) {
        out[i * 2 - 1] = scaled(sign_extend(packed[i] >> 4, 4), shift);
        out[i * 2] = scaled(sign_extend(packed[i], 4), shift);
    }
}

// Unpack `count` samples of the stream into `dest`: whole blocks only, and the stream counter drops by `count`
// whatever the blocks cover.
static void unpack(NtspPlayer* p, int8_t* dest, uint32_t count) {
    p->stream.remaining -= count;
    for (uint32_t blocks = count / BLOCK_SAMPLES; blocks > 0; blocks--) {
        unpack_block(p->module.stream + p->stream.pos, dest);
        dest += BLOCK_SAMPLES;
        p->stream.pos += BLOCK_PACKED;
    }
}

// Fill the free buffer from the stream. At the end of the stream the rest of the buffer is zeroed, the stream restarts
// and SongEnd is called.
static void fill_buffer(NtspPlayer* p) {
    int8_t* dest = p->buffers[p->free_buffer];
    uint32_t room = NTSP_BUFFER_BYTES;
    while (room != 0) {
        if (p->stream.remaining == 0) {
            memset(dest, 0, room);
            p->stream = (NtspStream) { .pos = 0, .remaining = p->module.samples };
            p->looped = true;
            return;
        }
        const uint32_t count = p->stream.remaining < room ? p->stream.remaining : room;
        unpack(p, dest, count);
        dest += count;
        room -= count;
    }
}

// Point all four channels at the buffer just filled; the other one is free for the next interrupt.
static void swap_buffers(NtspPlayer* p) {
    for (uint32_t i = 0; i < NTSP_VOICES; i++) {
        p->paula[i].lc = p->buffers[p->free_buffer];
    }
    p->free_buffer ^= 1;
}

// Public

bool ntsp_probe(const uint8_t* data, size_t size) {
    NtspModule module;
    return module_read(data, size, &module);
}

NtspPlayer* ntsp_create(const uint8_t* data, size_t size) {
    NtspPlayer* p = calloc(1, sizeof(*p));
    if (p == nullptr) {
        return nullptr;
    }
    p->data = malloc(size);
    if (p->data == nullptr) {
        free(p);
        return nullptr;
    }
    memcpy(p->data, data, size);
    if (!module_read(p->data, size, &p->module)) {
        ntsp_destroy(p);
        return nullptr;
    }
    return p;
}

void ntsp_destroy(NtspPlayer* player) {
    if (player != nullptr) {
        free(player->data);
        free(player);
    }
}

// InitSound: unpack the first buffer (a whole buffer whatever the stream length), then the channel registers.
// StartInt: DMA on for all four channels; its audio interrupt is the first buffer start.
bool ntsp_start(NtspPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    memset(player->buffers, 0, sizeof(player->buffers));
    player->stream = (NtspStream) { .pos = 0, .remaining = player->module.samples };
    player->free_buffer = 0;
    player->looped = false;
    memset(player->paula, 0, sizeof(player->paula));

    unpack(player, player->buffers[0], NTSP_BUFFER_BYTES);
    swap_buffers(player);
    for (uint32_t i = 0; i < NTSP_VOICES; i++) {
        NuadePaulaChannel* c = &player->paula[i];
        c->len_words = NTSP_BUFFER_BYTES / 2;
        c->period = PERIOD;
        c->volume = i < 2 ? VOLUME_LOUD : VOLUME_QUIET;
        nuade_paula_channel_dma_on(c);
    }
    fill_buffer(player);
    swap_buffers(player);

    NuadeVoiceState state;
    for (uint32_t i = 0; i < NTSP_VOICES; i++) {
        nuade_voice_stop(voices, i);
    }
    nuade_voice_get_state(voices, 0, &state);
    player->wraps_seen = state.wraps;
    return true;
}

NtspTickResult ntsp_tick(NtspPlayer* player, NuadeVoices* voices) {
    NuadeVoiceState state;
    nuade_voice_get_state(voices, 0, &state);
    for (uint32_t n = state.wraps - player->wraps_seen; n > 0; n--) {
        fill_buffer(player);
        swap_buffers(player);
    }
    player->wraps_seen = state.wraps;

    for (uint32_t i = 0; i < NTSP_VOICES; i++) {
        nuade_paula_channel_sync(&player->paula[i], i, voices);
    }
    const NtspTickResult result = { .looped = player->looped };
    player->looped = false;
    return result;
}

// SPDX-License-Identifier: MIT
// onEscapee player, translated from the UADE eagleplayer "onEscapee player module V1.1" (Wanted Team).
//
// The module is a sample stream: raw 8-bit data, every longword XORed with $AA55FF00, the first 24 longwords being the
// check pattern (which decodes to silence). The replayer plays it on all four Paula channels at once (the same two
// 1 KB buffers, period $DE, volumes 64, 64, 1, 1) and refills the buffer that just ended from the AUD0 interrupt, which
// fires at DMA start and at every buffer start. When the stream is used up it zero-fills the rest of the buffer, calls
// SongEnd and starts over.

#include <nuade/players/onescapee.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define CHUNK_WORDS (ON_CHUNK_BYTES / 2)
#define PERIOD 0xDE

// The check pattern: Check2 reads up to CHECK_LONGS + 1 longwords
#define CHECK_LONGS 24
#define XOR_KEY 0xAA55FF00u
#define XOR_KEY_ALT 0x55AA00FFu

static const uint8_t k_volumes[ONESCAPEE_VOICES] = { 64, 64, 1, 1 };

// Check2: `k` longwords of $AA55FF00, one longword of anything else, then the rest of the 24 + 1 compares against
// $55AA00FF (the second loop keeps the first loop's counter and pointer), or 24 longwords of $AA55FF00. It reads no
// further than the module's 25th longword (the caller has checked the size).
static bool check_pattern(const uint8_t* data) {
    uint32_t i = 0;
    while (i < CHECK_LONGS && nuade_read_be32(data + i * 4) == XOR_KEY) {
        i++;
    }
    if (i == CHECK_LONGS) {
        return true;
    }
    for (uint32_t j = i + 1; j <= CHECK_LONGS; j++) {
        if (nuade_read_be32(data + j * 4) != XOR_KEY_ALT) {
            return false;
        }
    }
    return true;
}

// InitSound decodes the first buffer without looking at the module size; a smaller module would run its remaining
// byte count below zero and play whatever follows it in memory.
bool onescapee_probe(const uint8_t* data, size_t size) {
    return data != nullptr && size >= ON_CHUNK_BYTES && size <= UINT32_MAX && check_pattern(data);
}

OnEscapeePlayer* onescapee_create(const uint8_t* data, size_t size) {
    if (!onescapee_probe(data, size)) {
        return nullptr;
    }
    OnEscapeePlayer* p = calloc(1, sizeof(OnEscapeePlayer));
    uint8_t* copy = malloc(size);
    if (p == nullptr || copy == nullptr) {
        free(p);
        free(copy);
        return nullptr;
    }
    memcpy(copy, data, size);
    p->data = copy;
    p->r.module = copy;
    p->r.module_size = (uint32_t)size & ~3u;
    return p;
}

void onescapee_destroy(OnEscapeePlayer* player) {
    if (player != nullptr) {
        free(player->data);
        free(player);
    }
}

// Copy `count` module bytes from the read position into `dest`, XORing every longword with the key. The original
// always decodes the whole count and reads past the module end when fewer bytes remain, but the refill overwrites
// those bytes with zeros at once.
static void decode(OnReplayer* r, int8_t* dest, uint32_t count) {
    const uint32_t key = XOR_KEY;
    for (uint32_t i = 0; i < count; i++) {
        dest[i] = (int8_t)(r->module[r->pos + i] ^ (uint8_t)(key >> (24 - 8 * (i & 3))));
    }
    r->pos += count;
    r->remaining -= count;
}

// Point AUDxLC of all four channels at the buffer that was just filled; the next refill takes the other one.
static void swap_buffers(OnReplayer* r) {
    const int8_t* lc = r->buffers[r->fill];
    for (uint32_t i = 0; i < ONESCAPEE_VOICES; i++) {
        r->paula[i].lc = lc;
    }
    r->fill ^= 1;
}

// Fill the buffer: module data while there is some, then zeros up to the buffer end and SongEnd (the stream starts over
// at the next refill).
static void fill_buffer(OnReplayer* r) {
    int8_t* dest = r->buffers[r->fill];
    uint32_t left = ON_CHUNK_BYTES;
    while (left != 0) {
        if (r->remaining == 0) {
            memset(dest, 0, left);
            r->pos = 0;
            r->remaining = r->module_size;
            r->looped = true;
            return;
        }
        const uint32_t n = r->remaining < left ? r->remaining : left;
        decode(r, dest, n);
        dest += n;
        left -= n;
    }
}

// The AUD0 interrupt handler.
static void audio_interrupt(OnReplayer* r) {
    fill_buffer(r);
    swap_buffers(r);
}

// InitSound and StartInt
static void init_sound(OnReplayer* r) {
    r->pos = 0;
    r->remaining = r->module_size;
    r->fill = 0;
    r->looped = false;
    decode(r, r->buffers[0], ON_CHUNK_BYTES);
    swap_buffers(r);
    for (uint32_t i = 0; i < ONESCAPEE_VOICES; i++) {
        NuadePaulaChannel* c = &r->paula[i];
        c->len_words = CHUNK_WORDS;
        c->period = PERIOD;
        c->volume = k_volumes[i];
    }
}

bool onescapee_start(OnEscapeePlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    OnReplayer* r = &player->r;
    memset(r->buffers, 0, sizeof(r->buffers));
    memset(r->paula, 0, sizeof(r->paula));
    for (uint32_t i = 0; i < ONESCAPEE_VOICES; i++) {
        nuade_voice_stop(voices, i);
    }
    init_sound(r);
    for (uint32_t i = 0; i < ONESCAPEE_VOICES; i++) {
        nuade_paula_channel_dma_on(&r->paula[i]);
    }
    NuadeVoiceState state;
    nuade_voice_get_state(voices, 0, &state);
    r->wraps_seen = state.wraps;
    return true;
}

OnEscapeeTickResult onescapee_tick(OnEscapeePlayer* player, NuadeVoices* voices) {
    OnReplayer* r = &player->r;
    r->looped = false;
    if (r->paula[0].dma_started) {
        audio_interrupt(r); // DMA start is a buffer start: its interrupt refills the other buffer
    }
    // Buffer starts the mixer played since the last tick are the interrupts that fired in between.
    NuadeVoiceState state;
    nuade_voice_get_state(voices, 0, &state);
    for (uint32_t n = state.wraps - r->wraps_seen; n > 0; n--) {
        audio_interrupt(r);
    }
    r->wraps_seen = state.wraps;
    for (uint32_t i = 0; i < ONESCAPEE_VOICES; i++) {
        nuade_paula_channel_sync(&r->paula[i], i, voices);
    }
    return (OnEscapeeTickResult) { .looped = r->looped };
}

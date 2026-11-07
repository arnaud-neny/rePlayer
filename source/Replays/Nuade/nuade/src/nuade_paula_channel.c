#include "nuade_paula_channel.h"

#include "nuade_base.h"
#include "nuade_paula.h"
#include <stdlib.h>
#include <string.h>

void nuade_paula_channel_dma_on(NuadePaulaChannel* c) {
    if (!c->dma) {
        c->dma = true;
        c->dma_started = true;
        c->start_lc = c->lc;
        c->start_len_words = c->len_words;
    }
}

void nuade_paula_channel_dma_off(NuadePaulaChannel* c) {
    c->dma = false;
    c->dma_started = false;
}

static NuadeSample channel_sample(const int8_t* lc, uint16_t len_words) {
    const uint32_t bytes = len_words != 0 ? (uint32_t)len_words * 2u : NUADE_PAULA_MAX_SAMPLE_BYTES;
    return (NuadeSample) {
        .data = lc,
        .format = NuadeSampleFormat_S8,
        .length = bytes,
        .loop_start = 0,
        .loop_length = bytes,
    };
}

static void channel_silence(NuadePaulaChannel* c, uint32_t voice, NuadeVoices* voices) {
    nuade_voice_stop(voices, voice);
    c->voice_on = false;
    c->sent_lc = nullptr;
}

void nuade_paula_channel_sync(NuadePaulaChannel* c, uint32_t voice, NuadeVoices* voices) {
    // Period 0 (never written, or written as 0) steps at Paula's longest period, ~54 samples a second
    nuade_voice_set_pitch(voices, voice, nuade_paula_period_hz(c->period));
    nuade_voice_set_volume(voices, voice, (float)nuade_paula_volume(c->volume) / 64.0f);

    if (!c->dma) {
        if (c->voice_on) {
            channel_silence(c, voice, voices);
        }
        c->sent_lc = nullptr;
        c->dma_started = false;
        return;
    }
    if (c->dma_started) {
        c->dma_started = false;
        if (c->start_lc == nullptr) {
            // DMA on with nothing to play
            channel_silence(c, voice, voices);
            return;
        }
        // DMA on: the channel plays what it latched from the start
        const NuadeSample start = channel_sample(c->start_lc, c->start_len_words);
        nuade_voice_play(voices, voice, &start, 0);
        c->voice_on = true;
        c->sent_lc = c->start_lc;
        c->sent_len_words = c->start_len_words;
    }
    if (c->lc != nullptr && (c->lc != c->sent_lc || c->len_words != c->sent_len_words)) {
        // AUDxLC/AUDxLEN rewritten while playing (or after the DMA start): the next repeat takes the new sample
        const NuadeSample repeat = channel_sample(c->lc, c->len_words);
        nuade_voice_queue(voices, voice, &repeat);
        c->voice_on = true;
        c->sent_lc = c->lc;
        c->sent_len_words = c->len_words;
    }
}

uint8_t* nuade_paula_padded_copy(const void* data, size_t size) {
    if (size > SIZE_MAX - NUADE_PAULA_MAX_SAMPLE_BYTES) {
        return nullptr;
    }
    uint8_t* memory = calloc(1, size + NUADE_PAULA_MAX_SAMPLE_BYTES);
    if (memory != nullptr && data != nullptr && size != 0) {
        memcpy(memory, data, size);
    }
    return memory;
}

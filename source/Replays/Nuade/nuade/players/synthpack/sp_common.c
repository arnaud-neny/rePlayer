// SPDX-License-Identifier: MIT
// Synth Pack: file access, the Paula channels and the envelope.

#include "sp_internal.h"
#include <stdlib.h>

bool sp_memory_init(SpMemory* memory, const uint8_t* data, size_t size) {
    if (size > UINT32_MAX / 2) {
        return false;
    }
    memory->data = nuade_paula_padded_copy(data, size);
    memory->size = (uint32_t)size;
    return memory->data != nullptr;
}

void sp_memory_free(SpMemory* memory) {
    free(memory->data);
    memory->data = nullptr;
    memory->size = 0;
}

bool sp_memory_has(const SpMemory* memory, uint32_t offset, uint32_t length) {
    return length <= memory->size && offset <= memory->size - length;
}

uint8_t sp_u8(const SpMemory* memory, uint32_t offset) {
    return offset < memory->size ? memory->data[offset] : 0;
}

int8_t sp_s8(const SpMemory* memory, uint32_t offset) {
    return (int8_t)sp_u8(memory, offset);
}

uint16_t sp_u16(const SpMemory* memory, uint32_t offset) {
    return (uint16_t)((sp_u8(memory, offset) << 8) | sp_u8(memory, offset + 1));
}

uint32_t sp_u32(const SpMemory* memory, uint32_t offset) {
    return ((uint32_t)sp_u16(memory, offset) << 16) | sp_u16(memory, offset + 2);
}

uint16_t sp_lsr16(uint16_t value, unsigned count) {
    return (count & 63) > 15 ? 0 : (uint16_t)(value >> (count & 63));
}

int16_t sp_asr16(int16_t value, unsigned count) {
    const unsigned shift = count & 63;
    if (shift > 15) {
        return value < 0 ? -1 : 0;
    }
    return (int16_t)(value >> shift);
}

void sp_hardware_reset(SpHardware* hardware) {
    static const int8_t silence[2] = { 0, 0 };
    for (int i = 0; i < SP_VOICES; i++) {
        SpChannel* channel = &hardware->channel[i];
        *channel = (SpChannel) { 0 };
        channel->paula.lc = silence;
        channel->paula.len_words = 1;
        channel->paula.volume = 0;
        nuade_paula_channel_dma_on(&channel->paula);
    }
}

void sp_hardware_begin_tick(SpHardware* hardware) {
    for (int i = 0; i < SP_VOICES; i++) {
        SpChannel* channel = &hardware->channel[i];
        if (channel->start_dma) {
            nuade_paula_channel_dma_on(&channel->paula);
            channel->start_dma = false;
        }
    }
}

void sp_hardware_end_tick(SpHardware* hardware) {
    for (int i = 0; i < SP_VOICES; i++) {
        SpChannel* channel = &hardware->channel[i];
        if (channel->latched) {
            nuade_paula_channel_dma_off(&channel->paula);
            channel->start_dma = true;
            channel->latched = false;
        }
    }
}

void sp_hardware_sync(SpHardware* hardware, NuadeVoices* voices) {
    for (int i = 0; i < SP_VOICES; i++) {
        nuade_paula_channel_sync(&hardware->channel[i].paula, (uint32_t)i, voices);
    }
}

void sp_channel_set_lc(SpChannel* channel, const SpMemory* set, uint32_t offset) {
    // The padded copy holds any offset a validated song produces; a wild one points at the start.
    const uint32_t even = offset & ~1u;
    channel->paula.lc = (const int8_t*)(set->data + (even <= set->size ? even : 0));
}

void sp_channel_set_sample(SpChannel* channel, const SpMemory* set, uint32_t offset, uint16_t length_words) {
    sp_channel_set_lc(channel, set, offset);
    channel->paula.len_words = length_words;
}

uint8_t sp_paula_volume(unsigned volume) {
    return (uint8_t)(volume & 0x7F);
}

// A level moves by this much each step of an envelope phase.
#define SP_ENVELOPE_STEP 8

// The step of one envelope phase happens every (counter / 2 + 1) ticks; levels are compared as signed bytes.

void sp_adsr_step(SpAdsr* adsr, const SpEnvelope* envelope, bool released) {
    switch (adsr->phase) {
        case SpAdsrPhase_Attack:
            adsr->counter = (int16_t)(adsr->counter - 2);
            if (adsr->counter >= 0) {
                return;
            }
            adsr->counter = envelope->attack_rate;
            if (envelope->attack_rate != 0 && (int8_t)adsr->level < (int8_t)envelope->attack_level) {
                adsr->level = (int16_t)(adsr->level + SP_ENVELOPE_STEP);
                return;
            }
            adsr->phase = SpAdsrPhase_Decay;
            adsr->counter = envelope->decay_rate;
            adsr->level = envelope->attack_level;
            break;
        case SpAdsrPhase_Decay:
            adsr->counter = (int16_t)(adsr->counter - 2);
            if (adsr->counter >= 0) {
                return;
            }
            adsr->counter = envelope->decay_rate;
            if (envelope->decay_rate != 0 && (int8_t)adsr->level > (int8_t)envelope->sustain_level) {
                adsr->level = (int16_t)(adsr->level - SP_ENVELOPE_STEP);
                return;
            }
            adsr->phase = SpAdsrPhase_Sustain;
            adsr->level = envelope->sustain_level;
            break;
        case SpAdsrPhase_Sustain:
            if (released) {
                adsr->phase = SpAdsrPhase_Release;
                adsr->counter = envelope->release_rate;
            }
            break;
        case SpAdsrPhase_Release:
            adsr->counter = (int16_t)(adsr->counter - 2);
            if (adsr->counter >= 0) {
                return;
            }
            adsr->counter = envelope->release_rate;
            if (envelope->release_rate != 0 && adsr->level > 0) {
                adsr->level = (int16_t)(adsr->level - SP_ENVELOPE_STEP);
                return;
            }
            adsr->phase = SpAdsrPhase_Idle;
            adsr->level = 0;
            break;
        case SpAdsrPhase_Idle:
            break;
    }
}

void sp_adsr_release(SpAdsr* adsr) {
    adsr->phase = SpAdsrPhase_Sustain;
}

void sp_adsr_start(SpAdsr* adsr) {
    *adsr = (SpAdsr) { .phase = SpAdsrPhase_Attack, .counter = 0, .level = 0 };
}

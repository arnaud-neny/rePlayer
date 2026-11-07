// SPDX-License-Identifier: MIT
// Richard Joseph Player: the module, the sample file and the state of the replayer.

#pragma once

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <nuade/players/richardjoseph.h>

#define RJP_INSTRUMENT_SIZE 32      // bytes of an instrument record
#define RJP_ZERO_LENGTH_WORDS 65536 // what AUDxLEN 0 plays
#define RJP_SAMPLE_BASE 4 // the sample file's first long is not sample data; all sample offsets count from after it

// A module or sample file in memory. Every read past its end gives 0 (the original reads whatever lies in memory
// there; no song does).

uint8_t rjp_u8(const RjpImage* image, uint64_t offset);
uint16_t rjp_u16(const RjpImage* image, uint64_t offset);
uint32_t rjp_u32(const RjpImage* image, uint64_t offset);

// The module: "RJP?" "SMOD", then seven sections, each a size long and its data.

// The instrument with this number; one past the last record is instrument 0, as the driver treats it.
RjpInstrument rjp_read_instrument(const RjpModule* module, uint8_t number);

// A volume envelope: a ramp from `start` to `peak`, a ramp to `sustain`, the sustain, and the release ramp to 0.
typedef struct RjpEnvelopeShape {
    uint8_t start;
    uint8_t peak;
    uint8_t attack_ticks;
    uint8_t sustain;
    uint8_t decay_ticks;
    uint8_t release_ticks;
} RjpEnvelopeShape;

RjpEnvelopeShape rjp_read_envelope(const RjpModule* module, uint32_t block);

// The offset in the module of the envelope block that instrument `instrument` points at.
uint32_t rjp_envelope_block(const RjpModule* module, const RjpInstrument* instrument);

// The offset in `patterns` of a pattern's first command, and the offset in `sequences` of a pattern list.
uint32_t rjp_pattern_start(const RjpModule* module, uint8_t pattern);
uint32_t rjp_sequence_start(const RjpModule* module, uint8_t sequence);

// The sequence number of a voice in a subsong (0 = the voice is not used).
uint8_t rjp_song_sequence(const RjpModule* module, uint32_t subsong, uint32_t voice);

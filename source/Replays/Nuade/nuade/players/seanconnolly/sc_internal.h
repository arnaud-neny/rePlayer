// SPDX-License-Identifier: MIT
// Sean Connolly: the module image, the known replayer versions and what the replayers share.
//
// A module is the composer's player followed by its data, loaded at address 0 of its own address space. The first
// words are `bra.w` to the replayer's routines (init at +0, then the interrupt routine and the others). The replayer
// addresses its tables with `lea d16(pc),An`; the displacements of those instructions are the only places where
// modules of one replayer version differ in code (the data behind the code is laid out by the assembler). The port
// identifies a version by the code with those displacement words zeroed and reads the table addresses from them.
// The replayer's variables sit in front of the code: most are fields of the player structs; the noise generator's
// buffer and seed stay in the image because a quirk of the generator writes across them.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nuade_paula_channel.h"
#include <nuade/players/seanconnolly.h>

#define SCO_MAX_MODULE_BYTES (4u << 20)
#define SCO_SONG_BYTES 16u    // one subsong in the song table: the stream offset of each voice
#define SCO_MAX_SUBSONGS 128u // the init routine of the sequencer build masks the subsong number with 0x7F
// The raster position (VHPOSR word) the sample build reads for its noise seed at init, as UADE's harness runs it.
#define SCO_V2_VHPOS 0x7F03u
// The CIA-B time of day (low byte, counting raster lines) the sequencer build reads for its noise seed at init, as
// UADE's harness runs it.
#define SCO_V3_TOD 59u

// The tables a replayer's code names. A role is a table as one piece of code uses it: two roles can address the same
// bytes (a module may lay its wave forms and its pitch sequences out in one block).

bool sco_module_load(ScoModule* module, const uint8_t* data, size_t size);
void sco_module_free(ScoModule* module);

// The sequencer build: whether every pattern of the module only does what the port models.
bool sc3_modeled(const ScoModule* module);

// Whether a table, stream or pattern read landed in the replayer's variables: the original reads live state there that
// the port keeps in structs, so the song cannot be reproduced from then on.
bool sco_module_var_read(const ScoModule* module);

// Puts the replayer's variables in the image back to what the file has (a new playing of the song starts from them).
void sco_module_reset(ScoModule* module);

// Reads of the module's initial variable bytes (the sampler's register blocks): not table reads, never flagged.
uint16_t sco_peek16(const ScoModule* module, uint32_t at);
uint32_t sco_peek32(const ScoModule* module, uint32_t at);

// Reads outside the module give 0 (the original would read whatever follows its module).
uint8_t sco_u8(const ScoModule* module, uint32_t at);
uint16_t sco_u16(const ScoModule* module, uint32_t at);
uint32_t sco_u32(const ScoModule* module, uint32_t at);
// Reads at `base + (int16)index`, the way the code indexes with a data register used as a word.
uint8_t sco_u8_indexed(const ScoModule* module, uint32_t base, uint16_t index);
uint16_t sco_u16_indexed(const ScoModule* module, uint32_t base, uint16_t index);
uint32_t sco_u32_indexed(const ScoModule* module, uint32_t base, uint16_t index);

// Writes into the image's variables (ignored outside the image).
void sco_write8(ScoModule* module, uint32_t at, uint8_t value);
void sco_write16(ScoModule* module, uint32_t at, uint16_t value);

// The hardware write of a voice's registers; the glue's volume patch masks the volume to 7 bits.
void sco_registers_write(const ScoModule* module, const ScoRegisters* registers, NuadePaulaChannel* paula);

// A sample of the sample table: its first byte and the byte behind its last. `entry_offset` is the byte offset of its
// long in the table; the entry behind it holds the end.
typedef struct ScoSampleBounds {
    uint32_t start;
    uint32_t end;
} ScoSampleBounds;

ScoSampleBounds sco_sample_bounds(const ScoModule* module, uint32_t entry_offset);

// AUDxLC for an address of the module's space: bit 0 dropped as Paula does, inside the padded image.
const int8_t* sco_chip(const ScoModule* module, uint32_t at);

// The generator's seed words.
static inline uint32_t sco_seed_address(const ScoModule* module) {
    return (uint32_t)((int32_t)module->entry + module->version->seed_at);
}

// The noise buffer.
static inline uint32_t sco_noise_address(const ScoModule* module) {
    return (uint32_t)((int32_t)module->entry + module->version->noise_at);
}
